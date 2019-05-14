#ifndef __OPTIONS_H__
#define __OPTIONS_H__

#include <netinet/in.h>

typedef enum {
	MODULE_CLIENT = 0,
	MODULE_CONTROLLER
} module_mode;

struct module_opts {
	in_addr_t srcaddr;
	module_mode mode;
};

int
parse_cmdline (int argc, char **argv, struct module_opts * options);

#endif /* __OPTIONS_H__ */
