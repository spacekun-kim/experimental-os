#ifndef __DEVICE_CTIMER_H
#define __DEVICE_CTIMER_H
#include "stdint.h"

#define NEW_TIMER 0
#define DEL_TIMER 1

struct timer* new_timer(uint32_t m_seconds);
void set_timer_interval(struct timer* tmr, uint32_t m_seconds);
void set_timer_enable(struct timer* tmr, bool timer_switch);
void set_timer_event(struct timer* tmr, void(*func_)(void));
void remove_timer(struct timer* tmr);
#endif
