#ifndef __KERNEL_INIT_H
#define __KERNEL_INIT_H
#include "global.h"
extern struct lock init_done;
void init_all(void);
#endif
