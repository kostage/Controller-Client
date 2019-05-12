
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
controller_connect_client_mk_sock(struct module_instance * this_module,
				  struct module_instance * client,
				  int tout_ms);
static int
controller_client_request(struct module_instance * client,
			  void * ignored);

static void
controller_client_request_all(struct module_instance * this_module);

static float
controller_filter_temp(struct temp_filter * filter,
		       float temp_value);

static int
controller_disconnect(struct module_instance * client,
		      void * ignored);

static int
controller_client_reply(struct module_instance * client,
			void * ignored);

static void
controller_disconnect_clients(struct module_instance * this_module);

static void
controller_client_reply_all(struct module_instance * this_module);

static void
controller_populate_poller(struct module_instance * this_module,
			   struct pollfd * pollfd);

static int
controller_add_client(struct module_instance * this_module, int sock);


module_state
controller_state_func(struct module_instance * this_module)
{
	module_state new_state_num = CONTROLLER_STATE;
	int srv_adv_sock = -1;
	int client_num = 0;
	struct pollfd pollfd[MAX_CLIENT_NUM + 1];

	srv_adv_sock =
		multicast_listen_mk_sock(this_module);

	if (srv_adv_sock < 0) {
		printf("common_listen_adv_mk_sock() fail\n");
		return FAILURE_STATE;
	}

	time_t request_time = time(NULL);

	while(new_state_num == CONTROLLER_STATE)
	{
		int ret;
		/* poll new clients advertising */
		pollfd[0].fd = srv_adv_sock;
		pollfd[0].events = POLLIN;
		pollfd[0].revents = 0;

		controller_populate_poller(this_module, &pollfd[1]);

		if ((ret = poll(pollfd, client_num + 1, POLL_WAIT_MS)) < 0) {
			/*FAIL*/
			perror("poll error");
			goto exit;
		} else if (ret > 0) {
			/* some activity in sockets */
			/* peer broadcasting */
			if (pollfd[0].revents & POLLIN)
			{
				/* function may silently fail - don't care
				   no client is added in case of connect failure */
				controller_add_client(this_module, pollfd[0].fd);
			}
			/* serve clients */
			controller_client_reply_all(this_module);
		}
		if ((time(NULL) - request_time) > REQUEST_PERIOD_SEC) {
			controller_client_request_all(this_module);
			request_time = time(NULL);
		}
	}
exit:
	controller_disconnect_clients(this_module);
	close(srv_adv_sock);
	return new_state_num;
}

/* lambda func */
int
controller_client_request(struct module_instance * client,
			  void * ignored)
{
	if (client->ctrl_state == CLIENT_CTRL_REQ_STATE) {
		// not replied yet
		if (++client->err_cnt >= CLIENT_MAX_ERR_CNT)
			controller_disconnect(client, NULL);
	}
	if (send(client->srv_sock,
		 GET_DATA_CMD,
		 strlen(GET_DATA_CMD),
		 0) < 0)
	{
		if (++client->err_cnt >= CLIENT_MAX_ERR_CNT)
			controller_disconnect(client, NULL);
	}
	client->ctrl_state = CLIENT_CTRL_REQ_STATE;
	return (0);
}

void
controller_client_request_all(struct module_instance * this_module)
{
	client_list_foreach(this_module,
			    controller_client_request,
			    NULL);
}

float
controller_filter_temp(struct temp_filter * filter,
		       float temp_value)
{
	return temp_value;
}

/* lambda func */
int
controller_disconnect(struct module_instance * client,
		      void * ignored)
{
	close(client->srv_sock);
	client_list_remove(client);
	return (0);
}

void
controller_populate_poller(struct module_instance * this_module,
			   struct pollfd * pollfd)
{
	struct module_instance * client;
	if (!list_empty(&this_module->list))
	{
		/* client may be removed in lamda */
		list_for_each_entry(client, &this_module->list, list)
		{
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
controller_client_reply(struct module_instance * client,
			void * ignored)
{
	char buf[64];
	ssize_t nread;
	if (client->pollfd->revents & POLLIN)
	{
		int ret;
		float temp, light_power;
		if ((nread = recv(client->srv_sock,
				  buf,
				  sizeof(buf),
				  MSG_DONTWAIT)) <= 0)
		{
			/* error or disconected */
			perror("recv error\n");
			goto disconnect_client;
		}

		buf[nread - 1] = '\0';
		ret = sscanf(buf, "%f,%f", &temp, &light_power);
		if (ret < 0) {
			perror("sscanf error");
			if (++client->err_cnt >= CLIENT_MAX_ERR_CNT) {
				printf("client disconnect due to errors\n");
				goto disconnect_client;
			}
		}
		client->temp = controller_filter_temp(&client->tfilter,
						      temp);
		client->light_power = light_power;
	}
	/* reply received = ready to new request */
	client->ctrl_state = CLIENT_CTRL_RDY_STATE;
	return (0);

disconnect_client:
	controller_disconnect(client, NULL);
	return (0);
}

void
controller_client_reply_all(struct module_instance * this_module)
{
	client_list_foreach(this_module,
			    controller_client_reply,
			    NULL);
}

void
controller_disconnect_clients(struct module_instance * this_module)
{
	client_list_foreach(this_module,
			    controller_disconnect,
			    NULL);
}

int
controller_add_client(struct module_instance * this_module, int sock)
{
	char buf[64];
	int nread;
	static int client_num = 0;
	struct sockaddr_in peer_addr;
	socklen_t addrlen = sizeof(peer_addr);
	struct module_instance new_peer;

	memset(&peer_addr, 0, sizeof(peer_addr));

	nread = recvfrom(sock,
			 buf,
			 sizeof(buf)-1,
			 MSG_DONTWAIT,
			 (struct sockaddr *)&peer_addr, &addrlen);
	if (nread <= 0) {
		perror("recvfrom error");
		return (-1);
	} else if (nread == 0) {
		return (0);
	}
	if (client_num == MAX_CLIENT_NUM) {
		return (0);
	}
	// remember peer_addr here
	new_peer.addr = peer_addr.sin_addr.s_addr;
	if (client_list_lookup_by_inaddr(this_module, new_peer.addr)) {
		return (0);
	}

	new_peer.srv_sock =
		controller_connect_client_mk_sock(this_module,
						  &new_peer,
						  1000);

	if (new_peer.srv_sock < 0) {
		printf("controller_connect_client_mk_sock() error\n");
		return (0);
	}

	/* add client if its not already in the list */
	client_list_add_client(this_module, &new_peer);

	printf("peer detected\n");
	++client_num;
	return (1);
}

int
controller_connect_client_mk_sock(struct module_instance * this_module,
				  struct module_instance * client,
				  int tout_ms)
{
	int sock;
	struct sockaddr_in srv_addr, client_addr;
	struct timeval timeout;
	timeout.tv_sec  = tout_ms * 1000;
	timeout.tv_usec = 0;

	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = this_module->addr; // specific iface
	srv_addr.sin_port = 0; // any port
  
	memset(&client_addr, 0, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = client->addr;
	client_addr.sin_port = htons(CLIENT_UC_PORT); // any port

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket error");
		return (-1);
	}

	if (bind(sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
		perror("bind error");
		goto fail_exit;
	}

	if ((setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout))) < 0) {
		perror("setsockopt tout error");
		goto fail_exit;
	}

	if (connect(sock, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
		perror("connect error");
		goto fail_exit;
	}
	return sock;

fail_exit:
	close(sock);
	return (-1);
}
