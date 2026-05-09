# Temperature_Sensor_Alarm
STM32L432KC temperature monitor on a Nucleo-32 board reads the MCU’s internal sensor, compensates via VREFINT, and outputs data to an SPI LCD, USART2, PWM buzzer, and status LED. A TIM2 rotary encoder sets a 0–100 °C alert threshold live. Encoder push-button enables shutdown, then software reset.
