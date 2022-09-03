#ifndef __NETWORK_NETWORK_H
#define __NETWORK_NETWORK_H

#include "stdint.h"
#include "global.h"
#include "socket.h"

#define FRAGMENT_SIZE 1480

#define OP_ARP_REQUEST 0x0100
#define OP_ARP_REPLY 0x0200
#define OP_RARP_REQUEST 0x0300
#define OP_RARP_REPLY 0x0400

#define PROTOCOL_IP_ICMP 0x01
#define PROTOCOL_IP_TCP 0x06
#define PROTOCOL_IP_UDP 0x11

/* 192.168.0.10 */
#define NET_IP 0x0a00a8c0

#define ARP_CACHE_SIZE 31
#define ARP_TIMER_INITIAL 1200

struct ARP_record {
	uint32_t ip;
	uint8_t mac[6];
	uint32_t ttl;
	uint32_t priority;
}__attribute__((packed));

struct ARP_cache {
	struct ARP_record* records[ARP_CACHE_SIZE + 1];
	
	uint16_t size;
};

struct ARP {
	uint16_t hardware_type;
	uint16_t protocol;
	uint8_t hard_len;
	uint8_t prot_len;
	uint16_t op;
	uint8_t src_mac[6];
	uint32_t src_ip;
	uint8_t dest_mac[6];
	uint32_t dest_ip;
}__attribute__((packed));

struct IP {
	uint8_t version_IHL; // version 4, IHL 4
	uint8_t type; // DS 6, ECN 2
	uint16_t length;
	uint16_t ident;
	uint16_t flags_and_offset; // flags(R|DF|MF) 3, offset 13(data offset bytes/8)
	uint8_t ttl;
	uint8_t protocol;
	uint16_t checksum;
	uint32_t src_ip;
	uint32_t dest_ip;
	uint8_t data[1];
}__attribute__((packed));


void arp_init(void);
void arp_send(struct Message** msg_ptr, uint16_t operation);
void arp_recv(struct Message** msg_ptr);
void ip_send(struct Message** msg_ptr);
void ip_recv(struct Message** msg_ptr);

#endif
