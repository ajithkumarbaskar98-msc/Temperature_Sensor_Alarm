/*******************************************************************************
 * STM32L432KC -- Temperature Monitor (Rotary Encoder Edition)
 *
 * Threshold is set by a rotary encoder.
 * Turning CW raises the threshold; CCW lowers it. Range: 0 - 100 degC.
 *
 * Pressing the encoder SW (PB4, active LOW) a SECOND time while in the STOPPED state triggers 
 * NVIC_SystemReset() — a full software reset that re-runs setup() and restarts all peripherals.
 *
 * PIN ASSIGNMENTS
 * ─────────────────────────────────────────────────────
 * Rotary encoder
 *   PA0  : TIM2_CH1  (AF1)  — CLK / phase A
 *   PB3  : TIM2_CH2  (AF1)  — DT  / phase B
 *   PB4  : GPIO input        — SW  (push button on encoder, active LOW)
 *                              First  press → stop_all()  (halt)
 *                              Second press → NVIC_SystemReset() (restart)
 *
 * Buzzer
 *   PA8  : TIM1_CH1  (AF1)  — ~3 kHz PWM buzzer output
 *
 * LED
 *   PB5  : GPIO output       — alert LED (active HIGH)
 *
 * USART2 TX
 *   PA2  : USART2_TX (AF7)
 *
 * SPI1 / LCD
 *   PA1  : SPI1_SCK  (AF5)
 *   PA7  : SPI1_MOSI (AF5)
 *   PA4  : LCD CS    (GPIO output)
 *   PA5  : LCD D/C   (GPIO output)
 *   PA6  : LCD RESET (GPIO output)
 ******************************************************************************/

#include <stdio.h>
#include <errno.h>
#include <sys/unistd.h>
#include "eeng1030_lib.h"
#include "display.h"

/* Factory calibration constants in flash */
#define TS_CAL1      (*((volatile uint16_t *)0x1FFF75A8))
#define TS_CAL2      (*((volatile uint16_t *)0x1FFF75CA))
#define VREFINT_CAL  (*((volatile uint16_t *)0x1FFF75AA))

/* Encoder step-to-temperature mapping
   TIM2 counts in quadrature (x1 mode): each detent = 1 count.
   ARR = 199 → counter wraps 0..199.
   We map that linearly to 0..100 degC (divide by 2).            */
#define ENCODER_MAX   200u
#define TEMP_MAX      100.0f

/* Function prototypes */
void setup(void);
void initADC(void);
void initSerial(uint32_t baudrate);
void initTIM1_PWM(void);
void initEncoder(void);
void buzzer_on(void);
void buzzer_off(void);
void stop_all(void);
int  readADC(int chan);
float readTemperature(void);
void updateDisplay(float temp, int breached, float threshold);
void eputc(char c);
float readThreshold(void);

volatile int alert_active = 0;

int main(void)
{
    setup();
    init_display();

    fillRectangle(0, 0, 160, 80, RGBToWord(0, 0, 0));
    printText("Temp Monitor", 5, 0, RGBToWord(255, 255, 255), 0);

    printf("\r\n=== STM32L432KC Temperature Monitor (Encoder) ===\r\n");
    printf("Threshold : rotary encoder on PA0/PB3, range 0-100 C\r\n");
    printf("Buzzer    : PA8 TIM1_CH1 ~3kHz\r\n");
    printf("LED       : PB5\r\n");
    printf("Display   : SPI LCD 160x80\r\n");
    printf("TS_CAL1   (30C  @ 3.0V): %u\r\n", (unsigned)TS_CAL1);
    printf("TS_CAL2   (130C @ 3.0V): %u\r\n", (unsigned)TS_CAL2);
    printf("VREFINT_CAL    (@ 3.0V): %u\r\n", (unsigned)VREFINT_CAL);
    printf("-------------------------------------------------\r\n");

    int sample = 0;

    while (1)
    {
        /*
         * Encoder push-button on PB4 (active LOW) — shutdown.
         * 5 ms double-sample debounce.
         */
        if ((GPIOB->IDR & (1 << 4)) == 0)
        {
            delay_ms(5);
            if ((GPIOB->IDR & (1 << 4)) == 0)
                stop_all();
        }

        float threshold = readThreshold();  /* from TIM2 encoder count */
        float temp      = readTemperature();
        sample++;

        if (temp >= threshold)
            alert_active = 1;

        int whole = (int)temp;
        int frac  = (int)((temp - whole) * 100);
        if (frac < 0) frac = -frac;

        int thr_whole = (int)threshold;
        int thr_frac  = (int)((threshold - thr_whole) * 100);

        printf("[%4d] Temp: %d.%02d C  Thresh: %d.%02d C",
               sample, whole, frac, thr_whole, thr_frac);
        if (temp >= threshold)
            printf("  <<< THRESHOLD! [LED+BUZZER]\r\n");
        else if (alert_active)
            printf("      (cooling)  [LED blinking]\r\n");
        else
            printf("      (normal)\r\n");

        updateDisplay(temp, (temp >= threshold), threshold);

        if (alert_active)
        {
            GPIOB->ODR |= (1 << 5);       /* PB5 HIGH — LED on  */
            if (temp >= threshold)
                buzzer_on();
            delay_ms(250);

            GPIOB->ODR &= ~(1 << 5);      /* PB5 LOW  — LED off */
            buzzer_off();
            delay_ms(250);
        }
        else
        {
            GPIOB->ODR &= ~(1 << 5);
            buzzer_off();
            delay_ms(500);
        }
    }
}

/*------------------------------------------------------------------------------
 * updateDisplay()
 * Row y=20 : temperature  (cyan)
 * Row y=40 : threshold from encoder (yellow)
 * Row y=60 : status (red / green)
 *----------------------------------------------------------------------------*/
void updateDisplay(float temp, int breached, float threshold)
{
    char buf[20];
    int whole = (int)temp;
    int frac  = (int)((temp - whole) * 100);
    if (frac < 0) frac = -frac;

    fillRectangle(0, 18, 160, 17, RGBToWord(0, 0, 0));
    sprintf(buf, "Temp:%3d.%02d C", whole, frac);
    printText(buf, 5, 20, RGBToWord(0, 255, 255), 0);

    int thr_whole = (int)threshold;
    int thr_frac  = (int)((threshold - thr_whole) * 100);
    fillRectangle(0, 38, 160, 17, RGBToWord(0, 0, 0));
    sprintf(buf, "Thr:%3d.%02d C", thr_whole, thr_frac);
    printText(buf, 5, 40, RGBToWord(255, 255, 0), 0);

    fillRectangle(0, 58, 160, 17, RGBToWord(0, 0, 0));
    if (breached)
        printText("THRESH BREACHED", 5, 60, RGBToWord(255, 0,   0), 0);
    else
        printText("Normal",          5, 60, RGBToWord(0,   255, 0), 0);
}

/*------------------------------------------------------------------------------
 * stop_all()
 *
 * Halts all outputs and displays a STOPPED screen.
 * Waits for a second press of the encoder button (PB4, active LOW) then
 * calls NVIC_SystemReset() to perform a full software reset.
 *----------------------------------------------------------------------------*/
void stop_all(void)
{
    /* ── 1. Silence all outputs ─────────────────────────────────────────── */
    buzzer_off();
    GPIOB->ODR &= ~(1 << 5);   /* LED off (PB5) */

    TIM1->CR1  &= ~(1 << 0);   /* stop TIM1 (buzzer) */
    TIM2->CR1  &= ~(1 << 0);   /* stop TIM2 (encoder) */

    ADC1->CR   |=  (1 << 1);   /* ADDIS — disable ADC */
    while (ADC1->CR & (1 << 0));

    /* ── 2. Update display ───────────────────────────────────────────────── */
    fillRectangle(0, 0, 160, 80, RGBToWord(0, 0, 0));
    printText("STOPPED",          30, 20, RGBToWord(255,   0,   0), 0);
    printText("Hold btn: RESET",   8, 45, RGBToWord(255, 255, 255), 0);

    /* ── 3. Serial log ───────────────────────────────────────────────────── */
    printf("\r\n[STOPPED] Button pressed.\r\n");
    printf("LED off. Buzzer off. TIM1/TIM2 stopped. ADC disabled.\r\n");
    printf("Hold encoder button (PB4) to reset.\r\n");

    /* ── 4. Wait for button release (clear the stop-press bounce) ────────── */
    while ((GPIOB->IDR & (1 << 4)) == 0);
    delay_ms(20);

    /* ── 5. Poll for second button press → software reset ───────────────── */
    while (1)
    {
        if ((GPIOB->IDR & (1 << 4)) == 0)
        {
            delay_ms(20);
            if ((GPIOB->IDR & (1 << 4)) == 0)
            {
                printf("[RESET] Encoder button held — executing NVIC_SystemReset().\r\n");

                GPIOB->ODR |=  (1 << 5);   /* brief LED flash */
                delay_ms(200);
                GPIOB->ODR &= ~(1 << 5);

                NVIC_SystemReset();
                /* never reached */
            }
        }
    }
}

/*------------------------------------------------------------------------------
 * readTemperature()
 *----------------------------------------------------------------------------*/
float readTemperature(void)
{
    int ts_raw   = readADC(17);
    int vref_raw = readADC(0);

    float factor    = (float)VREFINT_CAL / (float)vref_raw;
    float corrected = (float)ts_raw * factor;

    return ((corrected - (float)TS_CAL1) * (130.0f - 30.0f)
            / (float)((int)TS_CAL2 - (int)TS_CAL1)) + 30.0f;
}

/*------------------------------------------------------------------------------
 * readThreshold()
 *----------------------------------------------------------------------------*/
float readThreshold(void)
{
    uint32_t cnt = TIM2->CNT;                          /* 0 .. 199 */
    return (float)cnt * (TEMP_MAX / (float)(ENCODER_MAX - 1));
}

/*------------------------------------------------------------------------------
 * readADC()
 *----------------------------------------------------------------------------*/
int readADC(int chan)
{
    ADC1->SQR1  = (chan << 6);
    ADC1->ISR   = 0xFFFFFFFF;

    ADC1->CR   |= (1 << 0);
    while (!(ADC1->ISR & (1 << 0)));

    ADC1->CR   |= (1 << 2);
    while (!(ADC1->ISR & (1 << 2)));

    int result  = ADC1->DR;

    ADC1->CR   |= (1 << 1);
    while (ADC1->CR & (1 << 0));

    return result;
}

/*------------------------------------------------------------------------------
 * buzzer_on() / buzzer_off()
 *----------------------------------------------------------------------------*/
void buzzer_on(void)
{
    TIM1->CCMR1 = (TIM1->CCMR1 & ~(7 << 4)) | (6 << 4);
}

void buzzer_off(void)
{
    TIM1->CCMR1 = (TIM1->CCMR1 & ~(7 << 4)) | (4 << 4);
}

/*------------------------------------------------------------------------------
 * initEncoder()
 *----------------------------------------------------------------------------*/
void initEncoder(void)
{
    pinMode(GPIOA, 0, 2);
    selectAlternateFunction(GPIOA, 0, 1);
    enablePullUp(GPIOA, 0);

    pinMode(GPIOB, 3, 2);
    selectAlternateFunction(GPIOB, 3, 1);
    enablePullUp(GPIOB, 3);

    pinMode(GPIOB, 4, 0);
    enablePullUp(GPIOB, 4);

    TIM2->ARR   = ENCODER_MAX - 1;
    TIM2->CNT   = 0;
    TIM2->SMCR  = 0x01;
    TIM2->CCMR1 = (1 << 0) | (0x0F << 4) | (1 << 8) | (0x0F << 12);
    TIM2->CCER  = 0;
    TIM2->CR1  |= (1 << 0);
}

/*------------------------------------------------------------------------------
 * initTIM1_PWM()
 *----------------------------------------------------------------------------*/
void initTIM1_PWM(void)
{
    pinMode(GPIOA, 8, 2);
    selectAlternateFunction(GPIOA, 8, 1);

    TIM1->PSC   = 79;
    TIM1->ARR   = 332;
    TIM1->CCR1  = 166;
    TIM1->CCMR1 = (4 << 4) | (1 << 3);
    TIM1->CCER  = (1 << 0);
    TIM1->BDTR  = (1 << 15);
    TIM1->EGR   = (1 << 0);
    TIM1->CR1   = (1 << 0);
}

/*------------------------------------------------------------------------------
 * setup()
 *----------------------------------------------------------------------------*/
void setup(void)
{
    initClocks();

    SysTick->LOAD = 80000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = 7;
    __asm(" cpsie i ");

    RCC->AHB2ENR  |= (1 << 0) | (1 << 1) | (1 << 13);
    RCC->APB1ENR1 |= (1 << 17) | (1 << 0);
    RCC->APB2ENR  |= (1 << 11);

    GPIOB->MODER &= ~(3u << (5 * 2));
    GPIOB->MODER |=  (1u << (5 * 2));   /* PB5 = output */

    GPIOA->MODER  &= ~(3u << (2 * 2));
    GPIOA->MODER  |=  (2u << (2 * 2));
    GPIOA->AFR[0] &= ~(0xFu << (2 * 4));
    GPIOA->AFR[0] |=  (7u   << (2 * 4));

    initSerial(9600);
    initADC();
    initEncoder();
    initTIM1_PWM();
}

/*------------------------------------------------------------------------------
 * initADC()
 *----------------------------------------------------------------------------*/
void initADC(void)
{
    RCC->CCIPR        |= (1 << 29) | (1 << 28);
    ADC1_COMMON->CCR   = (1 << 16) | (1 << 22) | (1 << 23);
    ADC1->CR           = 0;
    ADC1->CR           = (1 << 28);
    delay(100);
    ADC1->CR          |= (1 << 31);
    while (ADC1->CR & (1 << 31));
    ADC1->SMPR1        = (7 << 0);
    ADC1->SMPR2        = (7 << 21);
    ADC1->CFGR         = (1 << 31);
}

/*------------------------------------------------------------------------------
 * initSerial()
 *----------------------------------------------------------------------------*/
void initSerial(uint32_t baudrate)
{
    USART2->CR1 = 0;
    USART2->CR2 = 0;
    USART2->CR3 = (1 << 12);
    USART2->BRR = 80000000 / baudrate;
    USART2->CR1 = (1 << 3);
    USART2->CR1|= (1 << 0);
}

void eputc(char c)
{
    while ((USART2->ISR & (1 << 7)) == 0);
    USART2->TDR = c;
}

int _write(int file, char *data, int len)
{
    if ((file != STDOUT_FILENO) && (file != STDERR_FILENO))
    {
        errno = EBADF;
        return -1;
    }
    while (len--)
        eputc(*data++);
    return 0;
}