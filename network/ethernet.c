#include "ethernet.h"
#include "ne2000.h"
#include "network.h"
#include "string.h"
#include "CRC32.h"
#include "debug.h"
#include "stdio-kernel.h"
#include "socket.h"

uint8_t mac[6] = {0x99, 0x00, 0x00, 0x20, 0xc4, 0xb0};
uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void ethernet_send(struct Message** msg_ptr, uint16_t type) {
	struct Ethernet* eth_pack;
	// uint32_t* fcs;
	struct Message* msg = *msg_ptr;

	if(msg->data_size < 48) {
		uint16_t pad_len = 48 - msg->data_size;
		memset(msg->data + msg->data_offset + msg->data_size, 0, pad_len);
		msg->data_size = 48;
	}
	msg->data_offset -= 14;
	msg->data_size += 14;
	ASSERT(msg->data_offset >= 0);
	eth_pack = (struct Ethernet*)(msg->data + msg->data_offset);
	memcpy(eth_pack->dest_mac, msg->dest_mac, 6);
	memcpy(eth_pack->src_mac, msg->src_mac, 6);
	eth_pack->type = type;
	/*
	fcs = (uint32_t*)(msg->data + msg->data_offset + msg->data_size - 4);
	*fcs = CRC32_compute(msg->data + msg->data_offset, msg->data_size);
	*fcs = htonl(*fcs);
	*/
	printk("send\n");
	print_raw_data(msg);
	ne2000_send(msg->data, msg->data_size);
}

void ethernet_recv(struct Message** msg_ptr) {
	struct Message* msg = *msg_ptr;
	/* raw packet */
	msg->data_size = ne2000_recv(msg->data);
	msg->data_size -= 4;
	/*
		uint8_t* ptr = msg->data;
	printk("recv from %x:%x:%x:%x:%x:%x\n", ptr[6],ptr[7],ptr[8],ptr[9],ptr[10],ptr[11]);
	*/
	if(msg->data_size == 0) {
		msg->status = STATUS_FREEMSG;
		return;
	}
	/* ethernet frame */
	struct Ethernet* frame = (struct Ethernet*)msg->data;
	/* if dest_mac == local_mac || dest_mac == broadcast_mac */
	if(	 (!memcmp(mac				, &frame->dest_mac, 6)) 
		|| (!memcmp(broadcast_mac, &frame->dest_mac, 6))) {
		if(!(memcmp(mac, &frame->dest_mac, 6))) {
			printk("recv\n");
			print_raw_data(msg);
		}
		/*
		uint32_t* fcs = (uint32_t*)(msg->data + msg->data_size - 4);
		uint8_t* data = frame->data;
		if(CRC32_compute(data, msg->data_size - 18) != *fcs) {
			// CRC check failed
		}
		*/
		if(msg->data_size & 1) {
			*(msg->data + msg->data_offset) = 0;
		}
		/* deliver_packet */
		msg->data_size -= 18;
		msg->data_offset = 14;
		if(frame->type == TYPE_IP) {
			ip_recv(msg_ptr);
		} else if(frame->type == TYPE_ARP) {
			arp_recv(msg_ptr);
		} else {
			// unknown protocol
			msg->status = STATUS_FREEMSG;
		}
	} else {
		msg->status = STATUS_FREEMSG;
	}
}
