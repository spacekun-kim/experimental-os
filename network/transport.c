#include "transport.h"
#include "network.h"
#include "socket.h"
#include "stdio-kernel.h"
#include "thread.h"

uint16_t seq_num = 0;
/*
static void exchange_16(uint16_t* a, uint16_t* b) {
	*a ^= *b;
	*b ^= *a;
	*a ^= *b;
}
*/
static void exchange_32(uint32_t* a, uint32_t* b) {
	*a ^= *b;
	*b ^= *a;
	*a ^= *b;
}

static uint16_t pseudo_header_sum(struct Message* msg, uint16_t protocol) {
	uint16_t ret = 0;
	ret += msg->src_ip & 0x00ff;
	ret += (msg->src_ip & 0xff00) >> 8;
	ret += msg->dest_ip & 0x00ff;
	ret += (msg->dest_ip & 0xff00) >> 8;
	ret += protocol;
	ret += msg->data_size;
	return ret;
}

void tcp_recv(struct Message** msg_ptr) {
	struct Message* msg = *msg_ptr; 
	struct TCP* tcp = (struct TCP*)(msg->data + msg->data_offset);
	uint16_t checksum = pseudo_header_sum(msg, PROTOCOL_IP_TCP) + get_sum((uint16_t*)(msg->data + msg->data_offset), msg->data_size >> 1);
	if(checksum != 0) {
		// checksum failed
	}
	uint16_t tcp_header_length = (tcp->data_offset & 0xf0) >> 2;
	msg->src_port = tcp->src_port;
	msg->dest_port = tcp->dest_port;
	msg->data_size -= tcp_header_length;
	msg->data_offset += tcp_header_length;
	msg->protocol = PROTOCOL_TCP;
	msg->status = STATUS_DELIVER;
}

void udp_recv(struct Message** msg_ptr) {
	struct Message* msg = *msg_ptr;
	struct UDP* udp = (struct UDP*)(msg->data + msg->data_offset);
	uint16_t checksum = pseudo_header_sum(msg, PROTOCOL_IP_UDP) + get_sum((uint16_t*)(msg->data + msg->data_offset), msg->data_size >> 1);
	if(checksum != 0) {
		// checksum failed
	}
	msg->src_port = udp->src_port;
	msg->dest_port = udp->dest_port;
	msg->data_size = udp->length;
	msg->status = STATUS_DELIVER;
}

void icmp_recv(struct Message** msg_ptr) {
	struct Message* msg = *msg_ptr;
	struct ICMP* icmp = (struct ICMP*)(msg->data + msg->data_offset);
	if(get_sum((uint16_t*)icmp, msg->data_size >> 1) != 0) {
		// checksum failed
	}
	switch(icmp->type) {
		case 0: // ping reply
			printk("ping replyed\n");
			break;
		case 8: // ping request
			msg->data_size -= 8;
			msg->data_offset += 8;
			msg->mf_ident = 0;
			exchange_32(&msg->src_ip, &msg->dest_ip);
			icmp_send(msg_ptr);
			msg->status = STATUS_FREEMSG;
			break;
	}
}

void tcp_send(struct Message** msg_ptr) {
	struct Message* msg = *msg_ptr; 
	msg->data_offset -= 20;
	msg->data_size += 20;
	
	struct TCP* tcp = (struct TCP*)(msg->data - 20);
	tcp->src_port = msg->src_port;
	tcp->dest_port = msg->dest_port;
	tcp->checksum = 0;
	tcp->checksum = pseudo_header_sum(msg, PROTOCOL_IP_TCP);
	tcp->checksum += get_sum((uint16_t*)(msg->data + msg->data_offset), msg->data_size >> 1);
	tcp->checksum = ~(tcp->checksum + 1);
	ip_send(msg_ptr);
}

void udp_send(struct Message** msg_ptr) {
	struct Message* msg = *msg_ptr; 
	msg->data_offset -= 8;
	msg->data_size += 8;
	
	struct UDP* udp = (struct UDP*)(msg->data  + msg->data_offset);
	udp->src_port = msg->src_port;
	udp->dest_port = msg->dest_port;
	udp->length = msg->data_size;
	udp->checksum = 0;
	udp->checksum = get_sum((uint16_t*)(msg->data + msg->data_offset), msg->data_size >> 1);
	udp->checksum += pseudo_header_sum(msg, PROTOCOL_IP_UDP);
	udp->checksum = ~(udp->checksum + 1);
	ip_send(msg_ptr);
}

void icmp_send(struct Message** msg_ptr) {
	++seq_num;
	struct Message* msg = *msg_ptr; 
	msg->data_offset -= 8;
	msg->data_size += 8;
	
	struct ICMP* icmp = (struct ICMP*)(msg->data + msg->data_offset);
	icmp->type = (msg->mf_ident & 0xff00) >> 8;
	icmp->subtype = (msg->mf_ident & 0x00ff);
	icmp->identifier = htons(running_thread()->tid);
	icmp->sequence = htons(seq_num);
	icmp->checksum = ~(get_sum((uint16_t*)icmp, msg->data_size) + 1);
	ip_send(msg_ptr);
}
