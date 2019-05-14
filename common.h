
#ifndef _COMMON_H_
#define _COMMON_H_


#include "module.h"

int
multicast_listen_mk_sock(struct module_instance * this_module);

int
multicast_adv_mk_sock(struct module_instance * this_module);

int
multicast_advertise(struct module_instance * this_module,
		    int bc_sock);

int
unicast_advertise(struct module_instance * this_module,
		  int bc_sock);

void
multicast_receive_adv(struct module_instance * this_module,
		      int bc_sock);
int
client_listen_server_mk_sock(struct module_instance * this_module);

#endif // _COMMON_H_

