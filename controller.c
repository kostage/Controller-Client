
#include "controller.h"

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

#define MAX_CLIENT_NUM (16)
#define POLL_WAIT_MS (1000U)
#define REQUEST_PERIOD_SEC (5)

static int
controller_connect_client_mk_sock(struct module_instance *this_module,
				  struct module_instance *client,
				  int tout_ms);
static int
controller_client_request(struct module_instance *this_module,
			  struct module_instance *client);

static void
controller_client_request_all(struct module_instance *this_module);

static float
controller_filter_temp(struct temp_filter *filter, float temp_value);

static void
controller_disconnect(struct module_instance *this_module,
		      struct module_instance *client);

static void controller_disconnect_all(struct module_instance *this_module);

static int
controller_client_reply(struct module_instance *this_module,
			struct module_instance *client);

static void
controller_client_reply_all(struct module_instance *this_module);

static void
controller_populate_poller(struct module_instance *this_module,
			   struct pollfd *pollfd);

static int
controller_add_client(struct module_instance *this_module, int sock);


module_state controller_state_func(struct module_instance *this_module)
{
    module_state new_state_num = CONTROLLER_STATE;
    int mc_listen_sock = -1;
    int srv_listen_sock = -1;
    int mc_adv_sock = -1;

    struct pollfd pollfd[MAX_CLIENT_NUM + 2];

    mc_listen_sock = multicast_listen_mk_sock(this_module);

    if (mc_listen_sock < 0) {
	printf("multicast_listen_mk_sock() fail\n");
	new_state_num = FAILURE_STATE;
	goto exit;
    }

    if (!this_module->primary_controller) {
	mc_adv_sock = multicast_adv_mk_sock(this_module);
	if (mc_adv_sock < 0) {
	    printf("multicast_adv_mk_sock() fail\n");
	    new_state_num = FAILURE_STATE;
	    goto exit;
	}
	srv_listen_sock = client_listen_server_mk_sock(this_module);
	if (srv_listen_sock < 0) {
	     printf("client_listen_server_mk_sock() fail\n");
	     goto exit;
	}
    }

    time_t request_time = time(NULL);

    while (new_state_num == CONTROLLER_STATE) {
	int ret;
	struct pollfd *pollfdp = &pollfd[0];
	int poll_len;
	if (this_module->primary_controller) {
	    poll_len = this_module->list_size + 1;
	} else {
	    poll_len = this_module->list_size + 2;
	}

	/* poll new clients advertising */
	pollfdp->fd = mc_listen_sock;
	pollfdp->events = POLLIN;
	pollfdp->revents = 0;
	++pollfdp;

	if (!this_module->primary_controller) {
	    pollfdp->fd = srv_listen_sock;
	    pollfdp->events = POLLIN;
	    pollfdp->revents = 0;
	    ++pollfdp;
	}

	controller_populate_poller(this_module, pollfdp);

	pollfdp = &pollfd[0];

	if ((ret = poll(pollfdp, poll_len, POLL_WAIT_MS)) < 0) {
	     /*FAIL*/ perror("poll error");
	    goto exit;
	} else if (ret > 0) {
	    /* some activity in sockets */
	    /* peer broadcasting */
	    if (pollfdp->revents & POLLIN) {
		/* function may silently fail - don't care
		   no client is added in case of connect failure */
		if (controller_add_client(this_module, pollfd[0].fd)) {
		    /* client list changed - populate poller again */
		    continue;
		}
	    }
	    if (!this_module->primary_controller &&
		(++pollfdp)->revents & POLLIN) {
		new_state_num = CLIENT_CONNECTED_STATE;
		break;
	    }
	    /* serve clients */
	    controller_client_reply_all(this_module);
	}
	if ((time(NULL) - request_time) > REQUEST_PERIOD_SEC) {
	    controller_client_request_all(this_module);
	    if (!this_module->primary_controller) {
		unicast_advertise(this_module, mc_adv_sock);
	    }
	    request_time = time(NULL);
	}
    }
    /* if incoming connection from controller
       then accept it or fail */
    if (new_state_num == CLIENT_CONNECTED_STATE) {
	this_module->srv_sock = accept(srv_listen_sock, NULL, NULL);
	if (this_module->srv_sock < 0) {
	    perror("accept");
	    new_state_num = FAILURE_STATE;
	    goto exit;
	}
    }
  exit:
    controller_disconnect_all(this_module);
    if (mc_adv_sock > 0)
	close(mc_adv_sock);
    if (mc_listen_sock > 0)
	close(mc_listen_sock);
    if (srv_listen_sock > 0)
	close(srv_listen_sock);

    return new_state_num;
}

/* lambda func */
int
controller_client_request(struct module_instance *this_module,
			  struct module_instance *client)
{
    if (client->ctrl_state == CLIENT_CTRL_REQ_STATE) {
	// not replied yet
	if (++client->err_cnt >= CLIENT_MAX_ERR_CNT) {
	    controller_disconnect(this_module, client);
	}
    }
    if (send(client->srv_sock, GET_DATA_CMD, strlen(GET_DATA_CMD), 0) < 0) {
	if (++client->err_cnt >= CLIENT_MAX_ERR_CNT) {
	    controller_disconnect(this_module, client);
	}
    }
    client->ctrl_state = CLIENT_CTRL_REQ_STATE;
    return (0);
}

void controller_client_request_all(struct module_instance *this_module)
{
    struct module_instance *client;
    struct module_instance *tmp;
    if (!list_empty(&this_module->list)) {
	list_for_each_entry_safe(client, tmp, &this_module->list, list) {
	    controller_client_request(this_module, client);
	}
    }
}

float controller_filter_temp(struct temp_filter *filter, float temp_value)
{
    return temp_value;
}

/* lambda func */
void
controller_disconnect(struct module_instance *this_module,
		      struct module_instance *client)
{
    close(client->srv_sock);
    client_list_remove(client);
    --this_module->list_size;
    printf("Client %s disconnect\n", inet_ntoa(client->addr));
}

void controller_disconnect_all(struct module_instance *this_module)
{
    struct module_instance *client;
    struct module_instance *tmp;
    if (!list_empty(&this_module->list)) {
	list_for_each_entry_safe(client, tmp, &this_module->list, list) {
	    controller_disconnect(this_module, client);
	}
    }
}

void
controller_populate_poller(struct module_instance *this_module,
			   struct pollfd *pollfd)
{
    struct module_instance *client;
    if (!list_empty(&this_module->list)) {
	list_for_each_entry(client, &this_module->list, list) {
	    pollfd->fd = client->srv_sock;
	    pollfd->events = POLLIN;
	    pollfd->revents = 0;
	    client->pollfd = pollfd;
	    pollfd++;
	}
    }
}

/* lambda func */
int
controller_client_reply(struct module_instance *this_module,
			struct module_instance *client)
{
    char buf[64];
    ssize_t nread;
    if (client->pollfd->revents & POLLIN) {
	int ret;
	float temp, light_power;
	if ((nread = recv(client->srv_sock,
			  buf, sizeof(buf), MSG_DONTWAIT)) <= 0) {
	    /* error or disconnected */
	    printf("Client %s recv error\n", inet_ntoa(client->addr));
	    goto disconnect_client;
	}

	buf[nread - 1] = '\0';
	ret = sscanf(buf, "%f,%f", &temp, &light_power);
	if (ret < 0) {
	    perror("sscanf error");
	    if (++client->err_cnt >= CLIENT_MAX_ERR_CNT) {
		goto disconnect_client;
	    }
	}
	client->temp = controller_filter_temp(&client->tfilter, temp);
	client->light_power = light_power;

	printf("Client %s, t = %.2f, l = %.2f\n",
	       inet_ntoa(client->addr), client->temp, client->light_power);
    }
    /* reply received = ready to new request */
    client->ctrl_state = CLIENT_CTRL_RDY_STATE;
    return (0);

  disconnect_client:
    controller_disconnect(this_module, client);
    return (0);
}

void controller_client_reply_all(struct module_instance *this_module)
{
    struct module_instance *client;
    struct module_instance *tmp;
    if (!list_empty(&this_module->list)) {
	list_for_each_entry_safe(client, tmp, &this_module->list, list) {
	    controller_client_reply(this_module, client);
	}
    }
}

int controller_add_client(struct module_instance *this_module, int sock)
{
    char buf[64];
    int nread;
    struct sockaddr_in peer_addr;
    socklen_t addrlen = sizeof(peer_addr);
    struct module_instance new_peer;

    memset(&peer_addr, 0, sizeof(peer_addr));

    nread = recvfrom(sock,
		     buf,
		     sizeof(buf) - 1,
		     MSG_DONTWAIT,
		     (struct sockaddr *) &peer_addr, &addrlen);
    if (nread <= 0) {
	perror("recvfrom error");
	return (-1);
    }
    if (this_module->list_size == MAX_CLIENT_NUM) {
	return (0);
    }

    MODULE_INIT(&new_peer);

    // remember peer_addr here
    new_peer.addr.s_addr = peer_addr.sin_addr.s_addr;
    if (client_list_lookup_by_inaddr(this_module, new_peer.addr.s_addr)) {
	return (0);
    }

    new_peer.srv_sock =
	controller_connect_client_mk_sock(this_module, &new_peer, 1000);

    if (new_peer.srv_sock < 0) {
	printf("controller_connect_client_mk_sock() error\n");
	return (0);
    }

    /* add client if its not already in the list */
    client_list_add_client(this_module, &new_peer);

    printf("Client detected: %s\n", inet_ntoa(new_peer.addr));
    ++this_module->list_size;
    return (1);
}

int
controller_connect_client_mk_sock(struct module_instance *this_module,
				  struct module_instance *client,
				  int tout_ms)
{
    int sock = -1;
    struct sockaddr_in srv_addr, client_addr;
    struct timeval timeout;
    timeout.tv_sec = tout_ms * 1000;
    timeout.tv_usec = 0;

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = this_module->addr.s_addr;	// specific iface
    srv_addr.sin_port = 0;	// any port

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = client->addr.s_addr;
    client_addr.sin_port = htons(CLIENT_UC_PORT);	// any port

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket error");
	return (-1);
    }

    if (bind(sock, (struct sockaddr *) &srv_addr, sizeof(srv_addr)) < 0) {
	perror("bind error");
	goto fail_exit;
    }

    if ((setsockopt
	 (sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout))) < 0) {
	perror("setsockopt tout error");
	goto fail_exit;
    }

    if (connect
	(sock, (struct sockaddr *) &client_addr,
	 sizeof(client_addr)) < 0) {
	perror("connect error");
	goto fail_exit;
    }
    return sock;

  fail_exit:
    close(sock);
    return (-1);
}
