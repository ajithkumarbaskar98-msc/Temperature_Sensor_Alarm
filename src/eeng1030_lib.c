#include <stdint.h>
#include <stm32l432xx.h>

/* millisecond counter incremented by SysTick ISR */
volatile uint32_t milliseconds = 0;

/* SysTick ISR — fires every 1 ms when SysTick is configured for 80 MHz */
void SysTick_Handler(void)
{
    milliseconds++;
}

/* Millisecond delay — sleeps the CPU between ticks to save power */
void delay_ms(volatile uint32_t dly)
{
    uint32_t end = milliseconds + dly;
    while (milliseconds != end)
        __asm(" wfi ");
}

/* Busy-loop delay — used before SysTick is running (e.g. ADC regulator settle) */
void delay(volatile uint32_t dly)
{
    while (dly--);
}

void initClocks(void)
{
    /* Switch system clock from 4 MHz MSI to 80 MHz PLL.
       VCO input = 4 MHz (MSI), N=80 → VCO=320 MHz, R=4 → PLLCLK=80 MHz */
    RCC->CR &= ~(1 << 24);                          /* PLL off                    */
    RCC->PLLCFGR = (1 << 25) | (1 << 24)           /* PLLR = /4, PLLREN          */
                 | (1 << 22) | (1 << 21)            /* PLLQ = /8, PLLQEN disabled */
                 | (1 << 17)                        /* PLLP /17 (SAI, unused)     */
                 | (80 << 8)                        /* PLLN = 80                  */
                 | (1 << 0);                        /* PLLSRC = MSI               */
    RCC->CR |= (1 << 24);                           /* PLL on                     */
    while ((RCC->CR & (1 << 25)) == 0);             /* wait for PLL lock          */
    FLASH->ACR &= ~(7u);
    FLASH->ACR |=  (1 << 2);                        /* 4 wait states for 80 MHz   */
    RCC->CFGR  |=  (1 << 1) | (1 << 0);            /* select PLL as sysclk       */
}

void enablePullUp(GPIO_TypeDef *Port, uint32_t BitNumber)
{
    Port->PUPDR &= ~(3u << (BitNumber * 2));
    Port->PUPDR |=  (1u << (BitNumber * 2));
}

void pinMode(GPIO_TypeDef *Port, uint32_t BitNumber, uint32_t Mode)
{
    /* 00=input  01=output  10=alternate  11=analog */
    uint32_t val = Port->MODER;
    val &= ~(3u  << (BitNumber * 2));
    val |=  (Mode << (BitNumber * 2));
    Port->MODER = val;
}

void selectAlternateFunction(GPIO_TypeDef *Port, uint32_t BitNumber, uint32_t AF)
{
    /* The alternative function control is spread across two 32 bit registers AFR[0] and AFR[1]
        There are 4 bits for each port bit. */
    if (BitNumber < 8)
    {
        Port->AFR[0] &= ~(0x0Fu << (4 * BitNumber));
        Port->AFR[0] |=  (AF    << (4 * BitNumber));
    }
    else
    {
        BitNumber -= 8;
        Port->AFR[1] &= ~(0x0Fu << (4 * BitNumber));
        Port->AFR[1] |=  (AF    << (4 * BitNumber));
    }
}
