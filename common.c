
#include "common.h"

#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "client_list.h"

#define MULTICAST_GROUP "239.255.0.1"

int multicast_listen_mk_sock(struct module_instance *this_module)
{
    int sock = -1, enable = 1;
    struct sockaddr_in bc_addr;

    memset(&bc_addr, 0, sizeof(bc_addr));
    bc_addr.sin_family = AF_INET;
    bc_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bc_addr.sin_port = htons(CLIENT_BC_PORT);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("socket error");
	return (-1);
    }
    if (bind(sock, (struct sockaddr *) &bc_addr, sizeof(bc_addr)) < 0) {
	perror("bind error");
	goto fail_exit;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))
	< 0) {
	perror("setsockopt error");
	goto fail_exit;
    }
    struct in_addr outgoing_addr;
    outgoing_addr.s_addr = this_module->addr.s_addr;

    if (setsockopt(sock,
		   IPPROTO_IP,
		   IP_MULTICAST_IF,
		   &outgoing_addr, sizeof(outgoing_addr)) < 0) {
	perror("setsockopt error");
	goto fail_exit;
    }

    u_char loop = 0;
    if (setsockopt
	(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
	perror("setsockopt error");
	goto fail_exit;
    }
    struct ip_mreq mreq;
    struct in_addr inaddr;
    inet_aton(MULTICAST_GROUP, &inaddr);
    mreq.imr_multiaddr.s_addr = inaddr.s_addr;
    mreq.imr_interface.s_addr = this_module->addr.s_addr;
    if (setsockopt(sock,
		   IPPROTO_IP,
		   IP_ADD_MEMBERSHIP, (char *) &mreq, sizeof(mreq)
	) < 0) {
	perror("setsockopt multicast");
	goto fail_exit;
    }

    return sock;
  fail_exit:
    if (sock > 0)
	close(sock);
    return (-1);
}

int multicast_adv_mk_sock(struct module_instance *this_module)
{
    int sock = -1, enable = 1;
    struct sockaddr_in bc_addr;

    memset(&bc_addr, 0, sizeof(bc_addr));
    bc_addr.sin_family = AF_INET;
    bc_addr.sin_addr.s_addr = this_module->addr.s_addr;
    bc_addr.sin_port = 0;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("socket");
	return (-1);
    }
    if (bind(sock, (struct sockaddr *) &bc_addr, sizeof(bc_addr)) < 0) {
	perror("bind");
	goto fail_exit;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))
	< 0) {
	perror("setsockopt");
	goto fail_exit;
    }
    return sock;
  fail_exit:
    if (sock > 0)
	close(sock);
    return (-1);
}

static int
advertise(struct module_instance *this_module, int bc_sock, in_addr_t dest)
{
    char buf[] = "Hello";
    struct sockaddr_in bc_addr;

    memset(&bc_addr, 0, sizeof(bc_addr));
    bc_addr.sin_family = AF_INET;
    bc_addr.sin_addr.s_addr = dest;
    bc_addr.sin_port = htons(CLIENT_BC_PORT);

    if (sendto(bc_sock, buf, sizeof(buf), 0,
	       (struct sockaddr *) &bc_addr, sizeof(bc_addr)) <= 0) {
	perror("sendto");
	return (-1);
    }
    printf("Advertise\n");
    return (0);
}

int multicast_advertise(struct module_instance *this_module, int bc_sock)
{
    struct in_addr mc_group_addr;

    if (inet_aton(MULTICAST_GROUP, &mc_group_addr) == 0) {
	perror("inet_aton");
	return (-1);
    }

    return advertise(this_module, bc_sock, mc_group_addr.s_addr);
}

int unicast_advertise(struct module_instance *this_module, int bc_sock)
{
    return advertise(this_module,
		     bc_sock, this_module->primary_addr.s_addr);
}

void
multicast_receive_adv(struct module_instance *this_module, int bc_sock)
{
    char buf[64];
    struct sockaddr_in peer_addr;
    socklen_t addrlen = sizeof(peer_addr);

    memset(&peer_addr, 0, sizeof(peer_addr));

    if (recvfrom(bc_sock,
		 buf,
		 sizeof(buf) - 1,
		 MSG_DONTWAIT,
		 (struct sockaddr *) &peer_addr, &addrlen) > 0) {
	// remember peer_addr here
	struct module_instance new_peer;
	new_peer.addr.s_addr = peer_addr.sin_addr.s_addr;
	if (client_list_add_client(this_module, &new_peer)) {
	    printf("peer detected: %s\n", inet_ntoa(peer_addr.sin_addr));
	}
    }
}

int client_listen_server_mk_sock(struct module_instance *this_module)
{
    int sock = -1, enable = 1;
    struct sockaddr_in client_addr;

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = this_module->addr.s_addr;
    client_addr.sin_port = htons(CLIENT_UC_PORT);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket error");
	return (-1);
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))
	< 0) {
	perror("setsockopt reuseaddr error");
	goto fail_exit;
    }

    if (bind(sock, (struct sockaddr *) &client_addr, sizeof(client_addr)) <
	0) {
	perror("bind error");
	goto fail_exit;
    }
    if (listen(sock, 1) < 0) {	/* tell kernel we're a server */
	perror("listen error");
	goto fail_exit;
    }
    return sock;
  fail_exit:
    if (sock > 0)
	close(sock);
    return (-1);
}
