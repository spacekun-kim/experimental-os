#ifndef __NETWORK_CRC32_H
#define __NETWORK_CRC32_H
#include "stdint.h"
uint32_t CRC32_compute(uint8_t* message, uint32_t msg_len);
#endif
