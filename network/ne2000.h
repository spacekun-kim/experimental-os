#ifndef __NETWORK_NE2000_H
#define __NETWORK_NE2000_H
#include "stdint.h"

void intr_network_handler(void);
void ne2000_init(void);
void ne2000_send(uint8_t* packet, uint16_t length);
uint16_t ne2000_recv(uint8_t* packet);
#endif
