#include "module.h"

#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "options.h"
#include "common.h"
#include "client.h"
#include "controller.h"


typedef module_state (*state_func)(struct module_instance *);

/* Decalarations of state funcs,
   each returns new state num */

static state_func
state_func_array[NUM_STATES] = {
	client_advertise_state_func,
	client_connected_state_func,
	controller_state_func
};

static char *
state_func_names[] =
{
	"CLIENT_ADVERTISE_STATE",
	"CLIENT_CONNECTED_STATE",
	"CONTROLLER_STATE"
};

static void
print_state(module_state state_num)
{
	printf("Switch to %s\n", state_func_names[state_num]);
}

static void
execute_module(struct module_instance * this_module,
	       module_state initial_state)
{
	module_state state_num = initial_state;
	state_func func = state_func_array[state_num];
	while(state_num != FAILURE_STATE)
	{
		print_state(state_num);
		state_num = func(this_module);
		if (state_num == FAILURE_STATE) {
			printf("State machine failure!\n");
		}
		func = state_func_array[state_num];
	}
}

/*****************************************/
/**************** MAIN *******************/
/*****************************************/
int main(int argc, char *argv[])
{
	struct module_opts options;
	struct module_instance this_module;
	module_state initial_state;

	if (parse_cmdline(argc, argv, &options) < 0) {
		printf("Incorrect command line arguments!\n");
		exit(1);
	}

	INIT_LIST_HEAD(&this_module.list);
	this_module.srv_sock = -1;
	this_module.pollfd = NULL;
	this_module.ctrl_state = CLIENT_CTRL_FAILURE_STATE;
	this_module.err_cnt = 0;
	this_module.addr = options.srcaddr;
	this_module.primary_controller = options.primary_controller;
	TEMP_FILTER_INIT(this_module.tfilter);
	this_module.temp = 0.0f;
	this_module.light_power = 0.0f;
	this_module.brightness = 0.0f;

	initial_state = (options.mode == MODULE_CLIENT) ?
		CLIENT_ADVERTISE_STATE : CONTROLLER_STATE;

	execute_module(&this_module, initial_state);

	exit(0);
}
