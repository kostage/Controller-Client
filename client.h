#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "module.h"

module_state
client_advertise_state_func(struct module_instance * this_module);

module_state
client_noadv_listen_state_func(struct module_instance *this_module);

module_state
client_connected_state_func(struct module_instance *this_module);

#endif				// _CLIENT_H_
