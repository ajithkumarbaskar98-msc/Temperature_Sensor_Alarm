#include "spi.h"
#include "eeng1030_lib.h"
void initSPI(SPI_TypeDef *spi)
{
	/*	I/O List
		PA7  : SPI1 MOSI : Alternative function 5		
		PA1  : SPI1 SCLK : Alternative function 5    
        PA11 : SPI1 MISO : Alternative function 5
*/
	int drain;	
    RCC->APB2ENR |= (1 << 12); // turn on SPI1
	// Now configure the SPI interface        
    pinMode(GPIOA,7,2);        
    pinMode(GPIOA,1,2);
    pinMode(GPIOA,11,2);
    selectAlternateFunction(GPIOA,7,5);        
    selectAlternateFunction(GPIOA,1,5);
    selectAlternateFunction(GPIOA,11,5);

	drain = spi->SR;				// dummy read of SR to clear MODF	
	// enable SSM, set SSI, enable SPI, PCLK/2, MSB First Master, Clock = 1 when idle, CPOL=1 (SPI mode 3 overall)   
	spi->CR1 = (1 << 9)+(1 << 8)+(1 << 6)+(1 << 2) +(1 << 1) + (1 << 0)+(1 << 3); // Assuming 80MHz default system clock set SPI speed to 20MHz 
	spi->CR2 = (1 << 10)+(1 << 9)+(1 << 8); 	// configure for 8 bit operation
}
uint8_t transferSPI8(SPI_TypeDef *spi,uint8_t data)
{
    uint8_t ReturnValue;
    volatile uint8_t *preg=(volatile uint8_t*)&spi->DR;	 // make sure no transfer is already under way
    while (((spi->SR & (1 << 7))!=0));
    *preg = data;
    while (((spi->SR & (1 << 7))!=0));// wait for transfer to finish
    ReturnValue = *preg;	
    return ReturnValue;
}
uint16_t transferSPI16(SPI_TypeDef *spi,uint16_t data)
{
    uint32_t ReturnValue;    	
    while (((spi->SR & (1 << 7))!=0));// make sure no transfer is already under way
    spi->DR = data;    
    while (((spi->SR & (1 << 7))!=0));    // wait for transfer to finish
	ReturnValue = spi->DR;	
    return (uint16_t)ReturnValue;
}
uint8_t spi_exchange(SPI_TypeDef *spi,uint8_t d_out[], uint32_t d_out_len, uint8_t d_in[], uint32_t d_in_len)
{
    unsigned Timeout = 1000000;
    unsigned index=0;
    uint8_t ReturnValue=0;    
    while(d_out_len--) {
        transferSPI8(spi,d_out[index]);
        index++;
    }
    index=0;
    while(d_in_len--)
    {
        
        d_in[index]=transferSPI8(spi,0xff);
        index++;
    }    
    return ReturnValue; 
}
