#ifndef __NETWORK_ETHERNET_H
#define __NETWORK_ETHERNET_H

#include "stdint.h"
#include "socket.h"

#define ETHERNET_DATA_MAX_LEN 1500

#define TYPE_IP 0x0008
#define TYPE_ARP 0x0608

struct Ethernet {
	uint8_t dest_mac[6];
	uint8_t src_mac[6];
	uint16_t type;
	uint8_t data[1];
}__attribute__((packed));

extern uint8_t mac[6];
extern uint8_t broadcast_mac[6];

void ethernet_send(struct Message** msg_ptr, uint16_t type);
void ethernet_recv(struct Message** msg_ptr);
#endif
