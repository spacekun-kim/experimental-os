#include "socket.h"
#include "fs.h"
#include "file.h"
#include "global.h"
#include "list.h"
#include "interrupt.h"
#include "string.h"
#include "ksoftirqd.h"
#include "stdio.h"
#include "stdio-kernel.h"

uint32_t mf_src_ip;
uint32_t mf_dest_ip;

#define MSG_LIST_SIZE 16
#define MSG_DATA_SIZE 1518
struct list msg_free_list;
struct list msg_send_list;
struct list msg_waitmf_list;
struct list msg_waitarp_list;

uint16_t kernel_ports[KERNEL_PORT_SIZE] = {68};
uint8_t listening_sockets[BOUND_MAX_SIZE] = {BLANK_POS};

void print_raw_data(struct Message* msg) {
	int32_t i;
	printk("\nprint raw data from offset[%d], size=%d\n", msg->data_offset, msg->data_size);
	for(i = msg->data_offset; i < (msg->data_offset + msg->data_size); ++i) {
		if(!((i - msg->data_offset) % 16)) {
			if((i - msg->data_offset) > 0) {
				printk("\n");
			}
		}
		if(msg->data[i] < 16) {
			printk("0");
		}
		printk("%x", msg->data[i]);
	}
	printk("\n");
}

uint16_t htons(uint16_t hostshort) {
	return ((hostshort & 0x00ff) << 8) | (hostshort >> 8);
}

uint32_t htonl(uint32_t hostlong) {
	return ((hostlong & 0x000000ff) << 24)
			 | ((hostlong & 0x0000ff00) << 8)
			 | ((hostlong & 0x00ff0000) >> 8)
			 | ((hostlong & 0xff000000) >> 24);
}

uint16_t get_sum(uint16_t* ptr, uint16_t length) {
	uint16_t ret = 0;
	for(int i = 0; i < length; ++i) {
		ret += *ptr;
	}
	return ret;
}

static int32_t listen_add(uint32_t local_fd) {

	uint32_t global_fd = fd_local2global(local_fd);
	int16_t blank_idx = -1;
	struct socket_* sock_existed;
	struct socket_* sock_toadd = (struct socket_*)file_table[global_fd].fd_inode;
	enum intr_status old_status = intr_disable();
	//make sure two threads don't get one idx
	int i;
	for(i = 0; i < BOUND_MAX_SIZE; ++i) {
		if(listening_sockets[i] == BLANK_POS) {
			if(blank_idx == -1) {
				blank_idx = i;
			}
		} else {
			sock_existed = (struct socket_*)file_table[listening_sockets[i]].fd_inode;
			if((sock_existed->ip == sock_toadd->ip) && (sock_existed->port == sock_toadd->port)) {
				// port occupied
				return -1;
			}
		}
	}

	listening_sockets[blank_idx] = global_fd;
	
	intr_set_status(old_status);
	return 0;
}

static void listen_remove(uint32_t global_fd) {
	int i;
	for(i = 0; i < BOUND_MAX_SIZE; ++i) {
		if(listening_sockets[i] == global_fd) {
			listening_sockets[i] = BLANK_POS;
			return;
		}
	}
}

struct socket_* listen_search(struct Message* msg) {
	int i;
	struct socket_* sock;
	for(i = 0; i < BOUND_MAX_SIZE; ++i) {
		if(listening_sockets[i] == BLANK_POS) {
			continue;
		}
		sock = (struct socket_*)file_table[listening_sockets[i]].fd_inode;
		if((sock->ip == msg->dest_ip) && (sock->port == msg->dest_port)) {
			return sock;
		}
	}

	return NULL;
}

void msg_init(void) {
	/* init msg lists */
	list_init(&msg_free_list);
	list_init(&msg_send_list);
	list_init(&msg_waitmf_list);
	list_init(&msg_waitarp_list);

	struct Message* msg_mem = (struct Message*)sys_malloc_kernel(MSG_LIST_SIZE * sizeof(struct Message));
	uint8_t* data_mem = (uint8_t*)sys_malloc_kernel(MSG_LIST_SIZE * MSG_DATA_SIZE);
	for(int i = 0; i < MSG_LIST_SIZE; ++i) {
		msg_mem->message_type = MESSAGE_IN_LIST;
		message_free(msg_mem);
		msg_mem->data = data_mem;
		++msg_mem;
		data_mem += MSG_DATA_SIZE;
	}
}

struct Message* get_message_from_list(struct list* msg_list, uint16_t size) {
	if(msg_list == NULL) {
		struct Message* msg = sys_malloc_kernel(sizeof(struct Message));
		msg->data = sys_malloc_kernel(size <= MSG_DATA_SIZE ? MSG_DATA_SIZE : size);
		msg->message_type = size <= MSG_DATA_SIZE ? NORM_MESSAGE : BIG_MESSAGE;
		return msg;
	}

	if(size <= MSG_DATA_SIZE) {
		if(list_empty(msg_list)) {
			return (struct Message*)NULL;
		}
		struct list_elem* elem = list_pop(msg_list);
		return elem2entry(struct Message, tag, elem);
	} else {
		struct Message* msg = sys_malloc_kernel(sizeof(struct Message));
		msg->data = sys_malloc_kernel(size);
		msg->message_type = BIG_MESSAGE;
		return msg;
	}
}

struct Message* get_a_message(void) {
	return get_message_from_list(NULL, 0);
}

void message_free(struct Message* msg) {
	msg->src_ip = 0;
	msg->src_port = 0;
	msg->dest_ip = 0;
	msg->dest_port = 0;
	msg->data_size = 0;
	msg->data_offset = 0;
	msg->protocol = 0;
	msg->mf_ident = 0;
	msg->mf_offset = 0;
	msg->status = STATUS_NOTASK;
	switch(msg->message_type) {
		case BIG_MESSAGE:
			sys_free_kernel(msg->data);
			sys_free_kernel(msg);
			break;
		case MESSAGE_IN_LIST:
			list_append(&msg_free_list, &msg->tag);
			break;
		case NORM_MESSAGE:
			break;
	}
}

bool find_mf(struct list_elem* elem, int32_t msg_addr) {
	struct Message* mf_msg = elem2entry(struct Message, tag, elem);
	struct Message* msg = (struct Message*)msg_addr;
	if(msg->src_ip != mf_msg->src_ip) {
		return false;
	}
	if(msg->dest_ip != mf_msg->dest_ip) {
		return false;
	}
	if(msg->mf_ident != mf_msg->mf_ident) {
		return false;
	}
	return true;
}

bool is_socket(uint32_t local_fd) {
	uint32_t global_fd = fd_local2global(local_fd);
	return file_table[global_fd].fd_flag == SOCKET_FLAG;
}

int32_t sys_socket(uint8_t domain, uint8_t protocol) {
	uint32_t global_fd = get_free_slot_in_global();
		
	file_table[global_fd].fd_flag = SOCKET_FLAG;
	file_table[global_fd].fd_pos = 0;
	file_table[global_fd].fd_inode = sys_malloc_kernel(sizeof(struct socket_));
	if(file_table[global_fd].fd_inode == NULL) {
		return -1;
	}

	struct socket_* sock = (struct socket_*)file_table[global_fd].fd_inode;
	sock->domain = domain;
	if(domain == AF_LOCAL) {
		sock->protocol = PRO_LOCAL;
	} else {
		sock->protocol = protocol;
	}
	sock->ip = 0;
	sock->port = 0;
	sock->connection_count = 0;
	list_init(&(sock->msg_recv_list));
	
	return pcb_fd_install(global_fd);
}

int32_t sys_bind(int32_t sockfd, uint32_t ip, uint16_t port) {
	uint32_t global_fd = fd_local2global(sockfd);
	if(!is_socket(sockfd)) {
		return -1;
	}
	struct socket_* sock = (struct socket_*)file_table[global_fd].fd_inode;
	sock->ip = ip;
	sock->port = port;
	return 0; 
}

int32_t sys_listen(int32_t sockfd) {
	if(!is_socket(sockfd)) {
		return -1;
	}
	return listen_add(sockfd);
}

int32_t sys_connect(int32_t sockfd, uint32_t ip, uint16_t port) {
	uint32_t global_fd = fd_local2global(sockfd);
	if(!is_socket(sockfd)) {
		return -1;
	}
	struct socket_* sock = (struct socket_*)file_table[global_fd].fd_inode;
	sock->connections[0].ip = ip;
	sock->connections[0].port = port;
	// if tcp, it needs hand shake here
	return 0;
}

int32_t socket_read(int32_t sockfd, void* buf, uint32_t count) {
	uint32_t global_fd = fd_local2global(sockfd);
	struct socket_* sock = (struct socket_*)file_table[global_fd].fd_inode;
	if(list_empty(&(sock->msg_recv_list))) {
		return 0;
	}
	
	struct Message* msg;
	int32_t read_bytes = 0;
	while(!list_empty(&(sock->msg_recv_list))) {
		msg = elem2entry(struct Message, tag, list_top(&(sock->msg_recv_list)));
		if(count >= msg->data_size) {
			if(msg->protocol == PRO_ICMP) {
				// for ping
				read_bytes = sprintf(buf, "ping return from %d\n", msg->src_ip);
			} else {
				memcpy((uint8_t*)buf, msg->data + msg->data_offset, msg->data_size);
			}
			list_pop(&(sock->msg_recv_list));
			message_free(msg);
			count -= msg->data_size;
			read_bytes += msg->data_size;
		} else {
			break;
		}
	}
	return read_bytes;
}

int32_t socket_write(int32_t sockfd, const void* buf, uint32_t count) {
	uint32_t global_fd = fd_local2global(sockfd);
	struct socket_* sock = (struct socket_*)file_table[global_fd].fd_inode;
	// tcp 18 + 20 + 20
	// udp 18 + 20 + 8
	// icmp 18 + 20 + 4
	uint32_t data_size = count & 1 ? count + 1 : count;
	switch(sock->protocol) {
		case PRO_LOCAL:
			break;
		case PRO_ICMP:
			data_size += 42;
			break;
		case PRO_TCP:
			data_size += 58;
			break;
		case PRO_UDP:
			data_size += 46;
	}
	struct Message* msg = get_message_from_list(&msg_free_list, data_size);
	msg->protocol = sock->protocol;
	msg->src_ip = sock->ip;
	msg->src_port = sock->port;
	msg->dest_ip = sock->connections[0].ip;
	msg->dest_port = sock->connections[0].port;
	msg->data_size = count;
	switch(sock->protocol) {
		case PRO_LOCAL:
			break;
		case PRO_ICMP:
			msg->data_offset = 42;
			// for ping
			msg->mf_ident = 0x0800;
			break;
		case PRO_TCP:
			msg->data_offset = 54;
			break;
		case PRO_UDP:
			msg->data_offset = 42;
	}
	memcpy(msg->data + msg->data_offset, buf, count);
	if(count & 1) {
		*(msg->data + msg->data_offset + msg->data_size) = 0;
	}
	list_append(&msg_send_list, &msg->tag);
	softirq_queue_put(NET_TX);
	return 0;
}

int32_t socket_close(int32_t sockfd) {
	uint32_t global_fd = fd_local2global(sockfd);
	struct socket_* sock = (struct socket_*)file_table[global_fd].fd_inode;
	
	struct list_elem* elem;
	struct Message* msg;
	listen_remove(global_fd);
	while(!list_empty(&sock->msg_recv_list)) {
		elem = list_pop(&sock->msg_recv_list);
		msg = elem2entry(struct Message, tag, elem);
		message_free(msg);
	}
	sys_free_kernel(sock);
	file_table[global_fd].fd_inode = NULL;
	return 0;
}
