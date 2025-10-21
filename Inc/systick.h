#ifndef SYSTICK_H_
#define SYSTICK_H_

#include "stm32f4xx.h"
#include <stdint.h>

void systick_init();
uint32_t system_get_tick_ms(void);
void systick_delay_ms(uint32_t delay);

#endif  // SYSTICK_H_