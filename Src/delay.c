#include "stm32f4xx.h"
#include <stdint.h>

// ================= Delay Function =================
void delay_ms(uint32_t ms)
{
    SysTick->LOAD = 16000 - 1;  // 1 ms tick @16 MHz
    SysTick->VAL = 0;
    SysTick->CTRL = 5;          // Enable SysTick, processor clock

    for(uint32_t i = 0; i < ms; i++)
        while(!(SysTick->CTRL & (1<<16))); // Wait for COUNTFLAG
}

void delay_us(uint32_t us)
{
    SysTick->LOAD = 16 - 1;  // 1 µs delay @16 MHz
    SysTick->VAL = 0;
    SysTick->CTRL = 5;          // Enable SysTick, processor clock

    for(uint32_t i = 0; i < us; i++)
        while(!(SysTick->CTRL & (1<<16))); // Wait for COUNTFLAG
}
