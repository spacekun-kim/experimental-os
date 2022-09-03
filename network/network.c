#include "network.h"
#include "ne2000.h"
#include "string.h"
#include "memory.h"
#include "debug.h"
#include "ethernet.h"
#include "socket.h"
#include "transport.h"
#include "stdio-kernel.h"

struct ARP_cache arp_cache;
uint16_t identifier = 1;

static void arp_adjust_idx(uint16_t i) {
	while((i > 0) && (arp_cache.records[i]->priority > arp_cache.records[i - 1]->priority)) {
		struct ARP_record* record_tmp = arp_cache.records[i];
		arp_cache.records[i] = arp_cache.records[i - 1];
		arp_cache.records[i - 1] = record_tmp;
		--i;
	}
}

static struct ARP_record* arp_search(uint32_t ip) {
	uint16_t i;
	struct ARP_record* record;
	for(i = 0; i < arp_cache.size; ++i) {
		record = arp_cache.records[i];
		ASSERT(record != NULL);
		if(record->ip == ip) {
			++record->priority;
			arp_adjust_idx(i);
			return record;
		}
	}
	return NULL;
}

static struct ARP_record* arp_add(uint32_t ip, uint8_t* mac) {
	struct ARP_record* record;
	uint16_t i;
	/* check if ip exists in cache */
	for(i = 0; i < arp_cache.size; ++i) {
		record = arp_cache.records[i];
		ASSERT(record != NULL);
		if(record->ip == ip) {
			memcpy(record->mac, mac, 6);
			return record;
		}
	}
	if(arp_cache.size < ARP_CACHE_SIZE) {
		record = (struct ARP_record*)sys_malloc_kernel(sizeof(struct ARP_record));
		record->priority = 0;
		arp_cache.records[arp_cache.size] = record;
		++arp_cache.size;
	} else {
		record = arp_cache.records[ARP_CACHE_SIZE];
		if(record == NULL) {
			record = (struct ARP_record*)sys_malloc_kernel(sizeof(struct ARP_record));
		}
		record->priority = arp_cache.records[arp_cache.size / 2]->priority;
		arp_adjust_idx(ARP_CACHE_SIZE);
	}
	record->ip = ip;
	memcpy(record->mac, mac, 6);
	record->ttl = ARP_TIMER_INITIAL;
	
	return record;
}

void arp_init(void) {
	arp_cache.size = 0;
	arp_add(0xffffffff, broadcast_mac);
}

void arp_send(struct Message** msg_ptr, uint16_t operation) {
	struct Message* msg = *msg_ptr;
	// make arp packet
	msg->data_offset = 14;
	msg->data_size = 28;
	struct ARP* arp = (struct ARP*)(msg->data + msg->data_offset);
	arp->hardware_type = 0x0100;
	arp->protocol = TYPE_IP; // IPv4
	arp->hard_len = 6;
	arp->prot_len = 4;
	arp->op = operation;
	memcpy(arp->src_mac, msg->src_mac, 6);
	arp->src_ip = msg->src_ip;
	memcpy(arp->dest_mac, msg->dest_mac, 6);
	arp->dest_ip = msg->dest_ip;
	// send
	ethernet_send(msg_ptr, TYPE_ARP);
}

static bool send_msg_waitarp(struct list_elem* elem, int32_t arp_rcd_addr) {
	struct Message* msg = (struct Message*)elem2entry(struct Message, tag, elem);
	struct ARP_record* arp_rcd = (struct ARP_record*)arp_rcd_addr;
	if(msg->dest_ip == arp_rcd->ip) {
		memcpy(msg->dest_mac, arp_rcd->mac, 6);
		ip_send(&msg);
	}
	return false;
}

void arp_recv(struct Message** msg_ptr) {
	struct Message* msg = *msg_ptr;
	struct ARP* arp = (struct ARP*)(msg->data + msg->data_offset);
		
	/* only accept IPv4 packet */
	if(arp->protocol != TYPE_IP) {
		return;
	}
	if(arp->op == OP_ARP_REQUEST) {
		if(arp->dest_ip == NET_IP) {
			arp_add(arp->src_ip, arp->src_mac);

			msg->src_ip = arp->dest_ip;
			memcpy(msg->src_mac, mac, 6);
			msg->dest_ip = arp->src_ip;
			memcpy(msg->dest_mac, arp->src_mac, 6);
			
			arp_send(msg_ptr, OP_ARP_REPLY);
		}
	} else if(arp->op == OP_ARP_REPLY){
		/*	
		printk("arp_reply received\n");	
		printk("arp->src_ip = %x\n", arp->src_ip);
		printk("arp->src_mac = %x%x%x%x%x%x\n,", arp->src_mac[5], arp->src_mac[4], arp->src_mac[3], arp->src_mac[2], arp->src_mac[1], arp->src_mac[0]);
		printk("arp->dest_ip = %x\n", arp->dest_ip);
		printk("arp->dest_mac = %x%x%x%x%x%x\n,", arp->dest_mac[5], arp->dest_mac[4], arp->dest_mac[3], arp->dest_mac[2], arp->dest_mac[1], arp->dest_mac[0]);
		asm volatile ("xchg %bx, %bx");
		*/
		struct ARP_record* arp_rcd = arp_add(arp->src_ip, arp->src_mac);
		// wake up message which is waiting for the arp reply
		if(!list_empty(&msg_waitarp_list)) {
			list_traversal(&msg_waitarp_list, send_msg_waitarp, (int32_t)arp_rcd);
			struct list_elem* elem = msg_waitarp_list.head.next;
			while(elem != &msg_waitarp_list.tail) {
				struct Message* msg = (struct Message*)elem2entry(struct Message, tag, elem);
				if(msg->status == STATUS_FREEMSG) {
					struct list_elem* next_elem = elem->next;
					list_remove(elem);
					message_free(msg);
					elem = next_elem;
				}
			}
		}
	}
	msg->status = STATUS_FREEMSG;
}

static void ip_send_(struct Message** msg_ptr, uint16_t flags_and_offset) {
/* IPv4``
 * version: 4
 * IHL: 5
 * DS: 0
 * ECN: 0
 * length: length + 20
 * ident:
 * ttl: 64
 * protocol: 17(UDP)/6(TCP)
 * checksum:
 */
	struct Message* msg = *msg_ptr;
	msg->data_offset -= 20;
	msg->data_size += 20;

	struct IP* ip_pack = (struct IP*)(msg->data + msg->data_offset);
	ip_pack->version_IHL = (4 << 4) + 5;
	ip_pack->type = 0;
	ip_pack->length = htons(msg->data_size);
	ip_pack->ident = identifier;
	ip_pack->flags_and_offset = htons(flags_and_offset);
	ip_pack->ttl = 64;
	ip_pack->protocol = msg->protocol;
	ip_pack->src_ip = msg->src_ip;
	ip_pack->dest_ip = msg->dest_ip;
	ip_pack->checksum = 0;
	ip_pack->checksum = ~(get_sum((uint16_t*)ip_pack, 5) + 1);
	
	uint8_t* dest_mac = arp_search(msg->dest_ip)->mac;
	memcpy(msg->dest_mac, dest_mac, 6);
	memcpy(msg->src_mac, mac, 6);
	ethernet_send(msg_ptr, TYPE_IP);
}

void ip_send(struct Message** msg_ptr) {
	// to solve situations of fragments and arp_record not found
	struct Message* msg = *msg_ptr;
	struct ARP_record* arp_rcd = arp_search(msg->dest_ip);
	
	if(arp_rcd == NULL) {
		list_append(&msg_waitarp_list, &msg->tag);
		// no arp record, send an arp request
		struct Message* arp_msg = sys_malloc_kernel(sizeof(struct Message));
		arp_msg->data = sys_malloc_kernel(64);
		arp_msg->src_ip = msg->src_ip;
		memcpy(arp_msg->src_mac, mac, 6);
		arp_msg->dest_ip = msg->dest_ip;
		memcpy(arp_msg->dest_mac, broadcast_mac, 6);
		arp_send(&arp_msg, OP_ARP_REQUEST);
		
		sys_free_kernel(arp_msg->data);
		sys_free_kernel(arp_msg);
		
		// put ip message into msg_waitarp_list
		msg->status = STATUS_WAIT;
		return;
	}
	
	uint16_t left_size = msg->data_size;
	uint32_t fcs_backup;
	uint32_t* fcs_ptr;
	uint32_t fragment_count = 0;
	uint16_t flags_and_offset = 0;
	
	while(left_size > FRAGMENT_SIZE) {
		++fragment_count;
		msg->data_size = FRAGMENT_SIZE;
		flags_and_offset = (1 << 13) | ((FRAGMENT_SIZE >> 3) * fragment_count);
		fcs_ptr = (uint32_t*)(msg->data + msg->data_offset + ETHERNET_DATA_MAX_LEN);
		fcs_backup = *fcs_ptr;
		ip_send_(msg_ptr, flags_and_offset);
		ethernet_send(msg_ptr, TYPE_IP);
		*fcs_ptr = fcs_backup;
		left_size -= ETHERNET_DATA_MAX_LEN;
		msg->data_offset += ETHERNET_DATA_MAX_LEN;
	}
	
	flags_and_offset = (FRAGMENT_SIZE >> 3) * fragment_count;
	msg->data_size = left_size;
	ip_send_(msg_ptr, flags_and_offset);

	++identifier;
	msg->status = STATUS_FREEMSG;
}

void ip_recv(struct Message** msg_ptr) {
	struct Message* msg = *msg_ptr;
	struct IP* ip_pack = (struct IP*)(msg->data + msg->data_offset);
	if(get_sum((uint16_t*)ip_pack, 6) != 0) {
		// check sum failed
	}
	msg->data_size -= 20;
	msg->data_offset += 20;
	msg->src_ip = ip_pack->src_ip;
	msg->dest_ip = ip_pack->dest_ip;
	msg->mf_ident = ip_pack->ident;
	msg->mf_offset = (ip_pack->flags_and_offset & 0x1fff) << 3;
	msg->protocol = ip_pack->protocol;
	msg->status = STATUS_NOTASK;

	struct list_elem* elem;
	struct Message* msg_mf;
	if((ip_pack->flags_and_offset & 0x2000 )) {
		if(msg->mf_offset == 0) { // start of mf
			msg_mf = get_message_from_list(&msg_free_list, 65535);
			memcpy((uint8_t*)msg_mf, (uint8_t*)msg, sizeof(struct Message));
			memcpy(msg_mf->data, msg->data, msg->data_size);
			
			msg->status = STATUS_FREEMSG;
			list_append(&msg_waitmf_list, &msg_mf->tag);
		} else { // part of mf
			elem = list_traversal(&msg_waitmf_list, find_mf, (int32_t)msg);
			if(elem != NULL) {
				msg_mf = (struct Message*)elem2entry(struct Message, tag, elem);
				memcpy(msg_mf->data + msg_mf->data_offset + msg->mf_offset, msg->data + msg->data_offset, msg->data_size);
				msg_mf->data_size += msg->data_size;
			}
			msg->status = STATUS_FREEMSG;
		}
	} else {
		if(msg->mf_offset != 0) { // mf over
			elem = list_traversal(&msg_waitmf_list, find_mf, (int32_t)msg);
			if(elem != NULL) {
				msg_mf = (struct Message*)elem2entry(struct Message, tag, elem);
				if(msg_mf->data_size != msg->mf_offset) {
					msg->status = STATUS_FREEMSG;
					return;
				}
				memcpy(msg_mf->data + msg_mf->data_offset + msg->mf_offset, msg->data + msg->data_offset, msg->data_size);
				msg_mf->data_size += msg->data_size;

				message_free(msg);
				*msg_ptr = msg_mf;
				msg = *msg_ptr;
			}
		} else {
			//no mf
		}

		if(ip_pack->protocol == PROTOCOL_IP_TCP) {
			tcp_recv(msg_ptr);
		} else if(ip_pack->protocol == PROTOCOL_IP_UDP) {
			udp_recv(msg_ptr);
		} else if(ip_pack->protocol == PROTOCOL_IP_ICMP) {
			icmp_recv(msg_ptr);
		} else {
		// unknown protocol
		}
	}
}
