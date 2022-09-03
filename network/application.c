#include "application.h"
#include "socket.h"
#include "network.h"
#include "stdio-kernel.h"
#include "stdio.h"
#include "interrupt.h"
#include "signal.h"

static void handler(uint32_t val) {
	printf("handler called! val=%x\n", val);
}

void sys_test(void) {
	/*
	int32_t fd = sys_socket(AF_INET, PRO_ICMP);
	sys_bind(fd, NET_IP, 0);
	sys_listen(fd);
	sys_connect(fd, 0x0800a8c0, 0);
	char buf[1024] = {0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69};
	socket_write(fd, buf, 32);
	printk("icmp ping sent.\n");
	*/
	sig_t sig = sys_sigregister(handler);
	
	sys_sendsignal_(running_thread(), sig, 1);
}

