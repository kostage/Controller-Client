
#include "client.h"

#include <stdio.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "client_list.h"
#include "common.h"
#include "module.h"

#define POLL_WAIT_MS (1000U)
#define CLIENT_ADV_PERIOD_SEC (5U)
#define CLIENT_WAIT_TIMEOUT_SEC (10U)
#define CLIENT_ACTIVITY_TIMEOUT_SEC (30U)

typedef enum {
    TIME_IDLE,
    TIME_ADVERTISE,
    TIME_SELECT_CONTROLLER
} client_timing;

static
    struct client_adv_timer {
    time_t start_time;
    time_t last_adv_time;
} adver_timer;

static time_t last_activity_time;

static void refresh_activity();
static int connection_inactive_check();
static int client_has_min_ipaddr(struct module_instance *this_module);
static void client_init_adv_time(struct client_adv_timer *ctimer);
static client_timing
client_check_adv_time(struct client_adv_timer *ctimer);
static int
client_process_request(struct module_instance *this_module, char *request);

/* CLIENT_ADVERTISE_STATE func and helpers definitions */
module_state
client_advertise_state_func(struct module_instance *this_module)
{
    module_state new_state_num = CLIENT_ADVERTISE_STATE;
    int srv_listen_sock = -1;
    int mc_listen_sock = -1;
    struct pollfd pollfd[2];

    if ((srv_listen_sock = client_listen_server_mk_sock(this_module)) < 0) {
	printf("client_listen_server_mk_sock() fail\n");
	goto exit;
    }

    if ((mc_listen_sock = multicast_listen_mk_sock(this_module)) < 0) {
	printf("multicast_listen_mk_sock() fail\n");
	new_state_num = FAILURE_STATE;
	goto exit;
    }

    client_init_adv_time(&adver_timer);

    while (new_state_num == CLIENT_ADVERTISE_STATE) {
	int ret;
	client_timing ctiming = TIME_IDLE;

	pollfd[0].fd = srv_listen_sock;
	pollfd[0].events = POLLIN;
	pollfd[0].revents = 0;

	pollfd[1].fd = mc_listen_sock;
	pollfd[1].events = POLLIN;
	pollfd[1].revents = 0;

	if ((ret = poll(pollfd, 2, POLL_WAIT_MS)) < 0) {
	     /*FAIL*/ perror("poll error");
	    new_state_num = FAILURE_STATE;
	    break;
	} else if (ret > 0) {
	    /* some activity in sockets */
	    /* peer broadcasting */
	    if (pollfd[1].revents & POLLIN) {
		multicast_receive_adv(this_module, pollfd[1].fd);
	    }
	    /* incoming connection from controller */
	    if (pollfd[0].revents & POLLIN) {
		new_state_num = CLIENT_CONNECTED_STATE;
		break;
	    }
	}
	/* after polling sockets it's time to attempt
	   either advertise client or become a controller */
	if ((ctiming =
	     client_check_adv_time(&adver_timer)) == TIME_ADVERTISE) {
	    if (multicast_advertise(this_module, mc_listen_sock) < 0) {
		new_state_num = FAILURE_STATE;
		break;
	    }
	} else if (ctiming == TIME_SELECT_CONTROLLER &&
		   !client_list_empty(this_module) &&
		   client_has_min_ipaddr(this_module)) {
	    client_list_clear(this_module);
	    new_state_num = CONTROLLER_STATE;
	    break;
	}
    }				// while(true)

    /* if incoming connection from controller
       then accept it or fail */
    if (new_state_num == CLIENT_CONNECTED_STATE) {
	this_module->srv_sock = accept(srv_listen_sock, NULL, NULL);
	if (this_module->srv_sock < 0) {
	    perror("accept error");
	    new_state_num = FAILURE_STATE;
	    goto exit;
	}
    }
  exit:
    if (mc_listen_sock > 0)
	close(mc_listen_sock);
    if (srv_listen_sock > 0)
	close(srv_listen_sock);
    return new_state_num;
}


/* CLIENT_CONNECTED_STATE function definition */
module_state
client_connected_state_func(struct module_instance * this_module)
{
    struct pollfd pollfd[1];
    char buf[64];
    ssize_t nread;
    module_state new_state_num = CLIENT_CONNECTED_STATE;

    refresh_activity();

    while (new_state_num == CLIENT_CONNECTED_STATE) {
	pollfd[0].fd = this_module->srv_sock;
	pollfd[0].events = POLLIN;
	pollfd[0].revents = 0;

	if (poll(pollfd, 1, POLL_WAIT_MS) < 0) {
	    perror("poll error");
	    new_state_num = FAILURE_STATE;
	    break;
	}
	if (pollfd[0].revents & POLLHUP) {
	    /* unlikely - sockets never give pollhup */
	    new_state_num = CLIENT_ADVERTISE_STATE;
	    break;
	}
	if (pollfd[0].revents & POLLIN) {
	    /* read request from controller */
	    if ((nread =
		 recv(pollfd[0].fd, buf, sizeof(buf), MSG_DONTWAIT)) < 0) {
		perror("recv");
		new_state_num = FAILURE_STATE;
	    } else if (nread == 0) {
		/* controller closed the connection */
		new_state_num = CLIENT_ADVERTISE_STATE;
	    } else {		/* process client's request */
		int ret;
		if ((ret = client_process_request(this_module, buf)) < 0)
		    new_state_num = FAILURE_STATE;
		else if (ret > 0)
		    refresh_activity();
	    }
	} else {
	    /* no request - check inactive timeout */
	    if (connection_inactive_check() != 0) {
		printf("connection incative\n");
		new_state_num = CLIENT_ADVERTISE_STATE;
		break;
	    } else {
		continue;
	    }
	}
    }
    close(this_module->srv_sock);
    return new_state_num;
}

/* helpers definition */

int
client_process_request(struct module_instance *this_module, char *request)
{
    char reply[64];
    int ret;
    if (strncmp(request, GET_DATA_CMD, strlen(GET_DATA_CMD)) == 0) {
	ret = snprintf(reply,
		       sizeof(reply),
		       "%.2f, %.2f",
		       this_module->temp, this_module->light_power);
	if (ret < 0 || ret >= sizeof(reply))
	    return (-1);
    } else {
	return 0;
    }
    if (send(this_module->srv_sock, reply, ret, 0) != ret) {
	perror("client reply error");
	return (-1);
    }
    return ret;
}

void refresh_activity()
{
    last_activity_time = time(NULL);
}

int connection_inactive_check()
{
    time_t cur_time = time(NULL);
    if ((cur_time - last_activity_time) > CLIENT_ACTIVITY_TIMEOUT_SEC) {
	return (1);
    }
    return (0);
}

static int
min_ip_addr_lambda(struct module_instance *client, void *cmp_ipaddr)
{
    // TODO: 
    if (client->addr.s_addr < *(in_addr_t *) cmp_ipaddr) {
	*(in_addr_t *) cmp_ipaddr = client->addr.s_addr;
	return (-1);
    }
    return (0);
}

int client_has_min_ipaddr(struct module_instance *this_module)
{
    in_addr_t cmp_addr = this_module->addr.s_addr;
    client_list_foreach(this_module, min_ip_addr_lambda, &cmp_addr);
    if (this_module->addr.s_addr <= cmp_addr) {
	return (1);
    }
    return (0);
}

void client_init_adv_time(struct client_adv_timer *ctimer)
{
    ctimer->start_time = time(NULL);
    ctimer->last_adv_time = ctimer->start_time;
}

client_timing client_check_adv_time(struct client_adv_timer *ctimer)
{
    time_t cur_time = time(NULL);
    if ((cur_time - ctimer->start_time) > CLIENT_WAIT_TIMEOUT_SEC) {
	ctimer->start_time = cur_time;
	return TIME_SELECT_CONTROLLER;
    }
    if ((cur_time - ctimer->last_adv_time) > CLIENT_ADV_PERIOD_SEC) {
	ctimer->last_adv_time = cur_time;
	return TIME_ADVERTISE;
    }
    return TIME_IDLE;
}


/* CLIENT_ADVERTISE_STATE func and helpers definitions */
module_state
client_noadv_listen_state_func(struct module_instance * this_module)
{
    module_state new_state_num = CLIENT_ADVERTISE_STATE;
    int srv_listen_sock = -1;
    int mc_send_sock = -1;
    struct pollfd pollfd[1];

    if ((srv_listen_sock = client_listen_server_mk_sock(this_module)) < 0) {
	printf("client_listen_server_mk_sock() fail\n");
	new_state_num = FAILURE_STATE;
	goto exit;
    }

    if ((mc_send_sock = multicast_adv_mk_sock(this_module)) < 0) {
	printf("multicast_send_mk_sock() fail\n");
	new_state_num = FAILURE_STATE;
	goto exit;
    }

    client_init_adv_time(&adver_timer);

    while (new_state_num == CLIENT_ADVERTISE_STATE) {
	int ret;
	client_timing ctiming = TIME_IDLE;

	pollfd[0].fd = srv_listen_sock;
	pollfd[0].events = POLLIN;
	pollfd[0].revents = 0;

	if ((ret = poll(pollfd, 1, POLL_WAIT_MS)) < 0) {
	     /*FAIL*/ perror("poll error");
	    new_state_num = FAILURE_STATE;
	    break;
	} else if (ret > 0) {
	    /* incoming connection from controller */
	    if (pollfd[0].revents & POLLIN) {
		new_state_num = CLIENT_CONNECTED_STATE;
		break;
	    }
	}
	/* after polling sockets it's time to attempt
	   either advertise client or become a controller */
	if ((ctiming =
	     client_check_adv_time(&adver_timer)) == TIME_ADVERTISE) {
	    if (multicast_advertise(this_module, mc_send_sock) < 0) {
		new_state_num = FAILURE_STATE;
		break;
	    }
	} else if (ctiming == TIME_SELECT_CONTROLLER &&
		   !client_list_empty(this_module) &&
		   client_has_min_ipaddr(this_module)) {
	    client_list_clear(this_module);
	    new_state_num = CONTROLLER_STATE;
	    break;
	}
    }				// while(...)

    /* if incoming connection from controller
       then accept it or fail */
    if (new_state_num == CLIENT_CONNECTED_STATE) {
	this_module->srv_sock = accept(srv_listen_sock, NULL, NULL);
	if (this_module->srv_sock < 0) {
	    perror("accept error");
	    new_state_num = FAILURE_STATE;
	    goto exit;
	}
    }
  exit:
    if (mc_send_sock > 0)
	close(mc_send_sock);
    if (srv_listen_sock > 0)
	close(srv_listen_sock);
    return new_state_num;
}
