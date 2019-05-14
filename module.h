
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
	struct in_addr addr;
	struct in_addr primary_addr;
	int primary_controller;
	struct temp_filter tfilter;
	float temp;
	float light_power;
	float brightness;
};

#define TEMP_FILTER_INIT(tfilter)

#define MODULE_INIT(this_module)  do {	    \
     INIT_LIST_HEAD(&(this_module)->list);   \
     (this_module)->list_size = 0;	     \
     (this_module)->srv_sock = -1;			     \
     (this_module)->pollfd = NULL;			      \
     (this_module)->ctrl_state = CLIENT_CTRL_FAILURE_STATE;	     \
     (this_module)->err_cnt = 0;				     \
     (this_module)->primary_controller = 0;			     \
     TEMP_FILTER_INIT((this_module)->tfilter);			     \
     (this_module)->temp = 0.0f;				     \
     (this_module)->light_power = 0.0f;				     \
     (this_module)->brightness = 0.0f;				     \
     } while(0);

#endif // _MODULE_H_
