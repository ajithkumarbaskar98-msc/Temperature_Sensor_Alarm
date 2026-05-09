#include <stm32l432xx.h>
uint16_t transferSPI16(SPI_TypeDef *spi, uint16_t data);
uint8_t transferSPI8(SPI_TypeDef *spi, uint8_t data);
void initSPI(SPI_TypeDef *spi);
uint8_t spi_exchange(SPI_TypeDef *spi,uint8_t d_out[], uint32_t d_out_len, uint8_t d_in[], uint32_t d_in_len);
