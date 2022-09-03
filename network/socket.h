#ifndef __NETWORK_SOCKET_H
#define __NETWORK_SOCKET_H

#include "stdint.h"
#include "list.h"

#define SOCKET_FLAG 0xfffe

#define STATUS_NOTASK 0
#define STATUS_FREEMSG 1
#define STATUS_DELIVER 2
#define STATUS_WAIT 3

#define BOUND_MAX_SIZE 16
#define BLANK_POS 0x00

#define MESSAGE_IN_LIST 0
#define NORM_MESSAGE 1
#define BIG_MESSAGE 2

struct Message {
	struct list_elem tag;
	uint16_t message_type;
	uint32_t src_ip;
	uint16_t src_port;
	uint8_t src_mac[6];
	uint32_t dest_ip;
	uint16_t dest_port;
	uint8_t dest_mac[6];
	uint16_t data_size;
	uint16_t data_offset;
	uint16_t mf_ident; // also icmp_type
	uint16_t mf_offset;
	uint8_t protocol;
	uint8_t status;
	uint8_t* data;
};

extern struct list msg_free_list;
extern struct list msg_send_list;
extern struct list msg_waitmf_list;
extern struct list msg_waitarp_list;

#define KERNEL_PORT_SIZE 1

extern uint16_t kernel_ports[KERNEL_PORT_SIZE];

#define AF_LOCAL 0
#define AF_INET 1
#define AF_INET6 2

#define PRO_LOCAL 0
#define PRO_ICMP 1
#define PRO_TCP 6
#define PRO_UDP 17

#define MAX_CONNECTION 16

struct connection {
	uint32_t ip;
	uint16_t port;
};

struct socket_ {
	uint8_t domain;
	uint8_t protocol;
	uint32_t ip;
	uint16_t port;
	uint16_t connection_count;
	struct connection connections[MAX_CONNECTION];
	struct list msg_recv_list;
};

void print_raw_data(struct Message* msg);
uint16_t htons(uint16_t hostshort);
uint32_t htonl(uint32_t hostlong);
uint16_t get_sum(uint16_t* ptr, uint16_t length);
struct socket_* listen_search(struct Message* msg);
void msg_init(void);
struct Message* get_message_from_list(struct list* msg_list, uint16_t size);
struct Message* get_a_message(void);
void message_free(struct Message* msg);
bool find_mf(struct list_elem* elem, int32_t msg_addr);
bool is_socket(uint32_t local_fd);
int32_t sys_socket(uint8_t domain, uint8_t protocol);
int32_t sys_bind(int32_t sockfd, uint32_t ip, uint16_t port);
int32_t sys_listen(int32_t sockfd);
int32_t sys_connect(int32_t sockfd, uint32_t ip, uint16_t port);
int32_t socket_read(int32_t sockfd, void* buf, uint32_t count);
int32_t socket_write(int32_t sockfd, const void* buf, uint32_t count);
int32_t socket_close(int32_t sockfd);
#endif
