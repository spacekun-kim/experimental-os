#ifndef __NETWORK_TRANSPORT_H
#define __NETWORK_TRANSPORT_H

#include "socket.h"

struct ICMP {
	uint8_t type;
	uint8_t subtype;
	uint16_t checksum;
	uint16_t identifier;
	uint16_t sequence;
	uint8_t data[1];
}__attribute__((packed));

struct UDP {
	uint16_t src_port;
	uint16_t dest_port;
	uint16_t length;
	uint16_t checksum;
	uint8_t data[1];
}__attribute__((packed));

struct TCP {
	uint16_t src_port;
	uint16_t dest_port;
	uint32_t seq;
	uint32_t ack;
	uint8_t data_offset;
	uint8_t control;
	uint16_t window;
	uint16_t checksum;
	uint16_t urgent_pointer;
	uint8_t data[1];
}__attribute__((packed));

#define FIN 0x01
#define SYN 0x02
#define RST 0x04
#define PSH 0x08
#define ACK 0x10
#define URG 0x20

#define PROTOCOL_ICMP 0x01
#define PROTOCOL_TCP 0x06
#define PROTOCOL_UDP 0x11

void tcp_recv(struct Message** msg_ptr);
void udp_recv(struct Message** msg_ptr);
void icmp_recv(struct Message** msg_ptr);
void tcp_send(struct Message** msg_ptr);
void udp_send(struct Message** msg_ptr);
void icmp_send(struct Message** msg_ptr);

#endif
