#include "dhcp.h"
#include "socket.h"
#include "transport.h"
#include "ethernet.h"

extern uint16_t identifier;
struct dhcp_state dhcp_stat;

void bootp_recv(struct Message** msg_ptr) {

}

void bootp_send(struct Message** msg_ptr, uint8_t operation) {
	struct Message* msg = *msg_ptr;
	msg->src_ip = dhcp_stat.client_ip;
	msg->src_port = 68;
	msg->dest_ip = dhcp_stat.server_ip;
	msg->dest_oprt = 67;
	msg->data_offset -= 240;
	msg->data_size += 240;
	
	struct BOOTP* bootp = (struct BOOTP*)(msg->data + msg->data_offset);
	memset(bootp, 0, 240);
	bootp->operation = operation;
	bootp->htype = 1;
	bootp->hlen = 6;
	bootp->hops = 0;
	bootp->xid = identifier;
	bootp->secs = 1;
	bootp->identifier = 0;
	memcpy(bootp->chaddr, mac, 6);
	bootp->magic_cookie = 0x63825363;
	udp_send(msg_ptr);
}

void dhcp_recv(struct Message** msg_ptr) {
	struct Message* msg = *msg_tr;
	uint8_t* ptr = msg->data + msg->data_offset;
}

uint8_t* dhcp_add_option(uint8_t* ptr, uint8_t option, uint8_t length, uint8_t* data) {
	*ptr = option;
	++ptr;
	if(length > 0) {
		ptr = length;
		++ptr;
		memcpy(ptr, data, length);
		ptr += length;
	}
	return ptr;
}

void dhcp_send(struct Message** msg_ptr, uint8_t dhcp) {
	struct Message* msg = *msg_ptr;
	uint8_t* ptr = msg->data + msg->data_offset;
	ptr = dhcp_add_option(ptr, DHO_MSGTYPE, 1, &dhcp);
	uint8_t buf[32] = {0};
	switch(dhcp) {
		case DHCP_DISCOVER:
			break;
		case DHCP_REQUEST:
			buf[0] = 1;
			memcpy(buf + 1, mac, 6);
			ptr = dhcp_add_option(ptr, DHO_CLIENTID, 7, buf);
			/* para */
			/* serverId leaseTime subnetMask domainNameServer broadcastAddress router */
			buf[0] = 1;
			buf[1] = 3;
			buf[2] = 6;
			buf[3] = 28;
			ptr = dhcp_add_option(ptr, DHO_PARAREQLIST, 4, buf);
			buf[0] = 0x02;
			buf[1] = 0x40;
			ptr = dhcp_add_option(ptr, DHO_MAXMSGSIZE, 2, buf);
			ptr = dhcp_add_option(ptr, DHO_IPADDRESS, 4, dhcp_stat.client_ip);
	}
	ptr = dhcp_add_option(ptr, DHO_END, 0, NULL);
	msg->data_size += ptr - msg->data - msg->data_offset;
	bootp_send(msg_ptr);
}

void dhcp_init(void) {
	int32_t fd = sys_socket(AF_INET, PRO_UDP);
	sys_bind(fd, dhcp_stat.client_ip, 68);
	sys_listen(fd);
	sys_connect(fd, dhcp_stat.server_ip, 67);
	// start a thread to be dhcp_client
	//
}
