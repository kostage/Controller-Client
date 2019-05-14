#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "options.h"

#define MODULE_CLIENT_STR    "client"
#define MODULE_CONTROLLER_STR    "controller"

static void
usage(void)
{
	printf("\tusage: \n"
	       "\tmodule -i src_ip -m client|controller\n");
}

int
parse_cmdline (int argc, char **argv, struct module_opts * options)
{
	int c, success = 1;
	struct in_addr ipaddr;
	memset(options, 0, sizeof(struct module_opts));
	/* default value for mode is CLIENT */
	options->mode = MODULE_CLIENT;

	while (1)
	{
		static struct option long_options[] =
			{
				{"iface", required_argument, 0, 'i'},
				{"mode", required_argument, 0, 'm'},
				{NULL}
			};
		/* getopt_long stores the option index here. */
		int option_index = 0;

		c = getopt_long(argc, argv, "i:m:",
				 long_options, &option_index);

		/* Detect the end of the options-> */
		if (c == -1)
			break;

		switch (c)
		{
		case 'i':
			if (inet_aton(optarg, &ipaddr) == 0) {
				printf ("invalid src iface addr\n");
				success = 0;
				goto errout;
			}
			options->srcaddr = ipaddr.s_addr;
			break;

		case 'm':
			if (strcmp(optarg, MODULE_CLIENT_STR) == 0) {
				options->mode = MODULE_CLIENT;
			} else if (strcmp(optarg, MODULE_CONTROLLER_STR) == 0) {
				options->mode = MODULE_CONTROLLER;
			} else {
				printf ("mode %s not supported\n", optarg);
				success = 0;
				goto errout;
			}
			break;

		case '?':
			success = 0;
			goto errout;

		default:
			success = 0;
			goto errout;
		}
	}
	if (!options->srcaddr) {
		printf("src ip addr is mandatory!\n");
		success = 0;
	}
	if (!success) {
		goto errout;
	}

	return (0);
  
errout:
	printf("Wrong arguments provided!\n");
	usage();
	return (-1);
}

