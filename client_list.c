
#include "client_list.h"

#include <stdlib.h>

struct module_instance *client_list_lookup_by_inaddr(const struct
						     module_instance
						     *client_list,
						     in_addr_t cmp_addr)
{
    struct module_instance *client;
    /* look if addr is already on the list */
    list_find_first(client,
		    &client_list->list, list, addr.s_addr, cmp_addr);
    return client;
}

int
client_list_add_client(struct module_instance *client_list,
		       struct module_instance *new_client)
{
    in_addr_t cmp_addr = new_client->addr.s_addr;
    struct module_instance *client =
	client_list_lookup_by_inaddr(client_list, cmp_addr);
    /* if exist do nothing */
    if (client)
	return (0);

    client = malloc(sizeof(*client));
    if (!client) {
	perror("out of memory");
	return (-1);
    }

    *client = *new_client;
    list_add_tail(&client->list, &client_list->list);
    /* report 1 client instance addition */
    return (1);
}

int
client_list_remove_by_inaddr(struct module_instance *client_list,
			     in_addr_t cmp_addr)
{
    struct module_instance *client =
	client_list_lookup_by_inaddr(client_list, cmp_addr);
    if (client) {
	list_del(&client->list);
	free(client);
	return (1);
    }
    return (0);
}

void client_list_remove(struct module_instance *client)
{
    if (client) {
	list_del(&client->list);
	free(client);
    }
}

int
client_list_pop(struct module_instance *client_list,
		struct module_instance *ret)
{
    if (!list_empty(&client_list->list)) {
	/* copy first item */
	struct module_instance *first_item;
	first_item = list_first_entry(&client_list->list,
				      struct module_instance, list);
	/* remove it from the list */
	list_del(&first_item->list);
	*ret = *first_item;
	free(first_item);
	return (1);
    }
    return (0);
}

void client_list_clear(struct module_instance *client_list)
{
    struct module_instance dummy;
    while (client_list_pop(client_list, &dummy));
}

int
client_list_foreach(struct module_instance *client_list,
		    lambda_func func, void *arg)
{
    struct module_instance *client;
    struct module_instance *temp;
    if (!list_empty(&client_list->list)) {
	/* client may be removed in lamda */
	list_for_each_entry_safe(client, temp, &client_list->list, list) {
	    int ret;
	    ret = func(client, arg);
	    /* ret < 0 - stop conditiion */
	    if (ret < 0)
		return ret;
	}
    }
    return (0);
}

int client_list_empty(struct module_instance *client_list)
{
    return list_empty(&client_list->list);
}
