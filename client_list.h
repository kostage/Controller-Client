#ifndef _CLIENT_LIST_H_
#define _CLIENT_LIST_H_

#include "module.h"

typedef int (*lambda_func) (struct module_instance *, void *);

struct module_instance *client_list_lookup_by_inaddr(const struct
						     module_instance
						     *client_list,
						     in_addr_t cmp_addr);

int
client_list_add_client(struct module_instance *new_client,
		       struct module_instance *client_list);

int
client_list_remove_by_inaddr(struct module_instance *client_list,
			     in_addr_t cmp_addr);

int
client_list_pop(struct module_instance *client_list,
		struct module_instance *ret);

void client_list_clear(struct module_instance *client_list);

int
client_list_foreach(struct module_instance *client_list,
		    lambda_func func, void *arg);
void client_list_remove(struct module_instance *client);

int client_list_empty(struct module_instance *client_list);

#endif				// _CLIENT_LIST_H_
