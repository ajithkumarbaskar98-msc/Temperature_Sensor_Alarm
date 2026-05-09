#ifndef EENG1030_LIB_H
#define EENG1030_LIB_H

#include <stdint.h>
#include <stm32l432xx.h>

/* Clock and GPIO helpers */
void initClocks(void);
void enablePullUp(GPIO_TypeDef *Port, uint32_t BitNumber);
void pinMode(GPIO_TypeDef *Port, uint32_t BitNumber, uint32_t Mode);
void selectAlternateFunction(GPIO_TypeDef *Port, uint32_t BitNumber, uint32_t AF);

/* Timing — SysTick-based and busy-loop */
extern volatile uint32_t milliseconds;
void delay_ms(volatile uint32_t dly);
void delay(volatile uint32_t dly);

#endif
