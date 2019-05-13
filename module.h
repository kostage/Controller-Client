
#ifndef _MODULE_H_
#define _MODULE_H_

#include <netinet/ip.h>
#include <poll.h>

#include "list.h"

#define CLIENT_UC_PORT (37000)
#define CLIENT_BC_PORT (38000)
#define GET_DATA_CMD "getdata"
#define CLIENT_MAX_ERR_CNT (2)

typedef enum {
	CLIENT_ADVERTISE_STATE = 0,
	CLIENT_CONNECTED_STATE,
	CONTROLLER_STATE,
	NUM_STATES,
	FAILURE_STATE = -1
} module_state;

/* state from controller point of view */
typedef enum {
	CLIENT_CTRL_REQ_STATE = 0,
	CLIENT_CTRL_RDY_STATE,
	CLIENT_CTRL_FAILURE_STATE = -1
} client_ctrl_state;

struct temp_filter {
	int temp;
};

struct module_instance {
	struct list_head list;
	int list_size;
	int srv_sock;
	struct pollfd * pollfd;
	client_ctrl_state ctrl_state;
	int err_cnt;
	in_addr_t addr;
	int primary_controller;
	struct temp_filter tfilter;
	float temp;
	float light_power;
	float brightness;
};

#endif // _MODULE_H_
