#ifndef __NETWORK_DHCP_H
#define __NETWORK_DHCP_H

#define OP_REQUEST 1
#define OP_REPLY 2

struct BOOTP {
	uint8_t operation;
	uint8_t htype;
	uint8_t hlen;
	uint8_t hops;
	uint32_t xid;
	uint16_t secs;
	uint16_t identifier;
	uint32_t ciaddr;
	uint32_t yiaddr;
	uint32_t siaddr;
	uint32_t giaddr;
	uint8_t chaddr[16];
	uint8_t shname[64];
	uint8_t file[128];
	uint32_t magic_cookie;
	uint8_t vend[1];
}__attribute__((packed));

struct dhcp_state{
	uint8_t	cur_status;
	uint32_t server_ip;
	uint32_t client_ip;
	uint16_t max_msg_size;
	uint32_t ip_lease_time;
	uint32_t subnet_mask;
	uint32_t domain_name_server;
	uint32_t broadcast_address;
	uint32_t router;
};

// DHCP OPTIONS
enum DHO {
	DHO_PAD = 0,
	DHO_SUBNETMASK = 1,
	DHO_ROUTER = 3,
	DHO_DNSSERVER = 6,
	DHO_DNSDOMAIN = 15,
	DHO_IPADDRESS = 50,
	DHO_LEASETIME = 51,
	DHO_MSGTYPE = 53,
	DHO_SERVERID = 54,
	DHO_PARAREQLIST = 55,
	DHO_ERRORMSG = 56,
	DHO_MAXMSGSIZE = 57,
	DHO_RENEWALTIME = 58,
	DHO_REBINDTIME = 59,
	DHO_CLIENTID = 61,
	DHO_DNSSEARCH = 119,
	DHO_END = 255
};

enum DHCP {
	DHCP_NONE,
	DHCP_DISCOVER,
	DHCP_OFFER,
	DHCP_REQUEST,
	DHCP_DECLINE,
	DHCP_ACK,
	DHCP_NAK,
	DHCP_RELEASE,
	DHCP_INFORM,
	DHCP_ORCERENEW,
	DHCP_LEASEQUERY,
	DHCP_LEASEUNASSIGNED,
	DHCP_LEASEUNKNOWN,
	DHCP_LEASEACTIVE
};

#endif
