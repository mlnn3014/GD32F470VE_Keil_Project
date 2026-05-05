/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2025/06/05
* Note:
*/
#include "mcu_cmic_gd32f470vet6.h"


/* SPI3 DMA 鐩稿叧缂撳啿鍖?*/
uint8_t spi3_send_array[ARRAYSIZE] = {0};    // SPI0 DMA 鍙戦€佺紦鍐插尯
uint8_t spi3_receive_array[ARRAYSIZE] = {0}; // SPI0 DMA 鎺ユ敹缂撳啿鍖?
/* SPI1 DMA 鐩稿叧缂撳啿鍖?*/
uint8_t spi1_send_array[ARRAYSIZE] = {0};    // SPI1 DMA 鍙戦€佺紦鍐插尯
uint8_t spi1_receive_array[ARRAYSIZE] = {0}; // SPI1 DMA 鎺ユ敹缂撳啿鍖?
void bsp_gd25qxx_init(void)
{
    rcu_periph_clock_enable(SPI_CLK_PORT);
    rcu_periph_clock_enable(RCU_SPI1);
    rcu_periph_clock_enable(RCU_DMA0);
    
    /* configure SPI1 GPIO */
    gpio_af_set(SPI_PORT, GPIO_AF_5, SPI_SCK | SPI_MISO | SPI_MOSI);
    gpio_mode_set(SPI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SPI_SCK | SPI_MISO | SPI_MOSI);
    gpio_output_options_set(SPI_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPI_SCK | SPI_MISO | SPI_MOSI);

    /* set SPI1_NSS as GPIO*/
    gpio_mode_set(SPI_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI_NSS);
    gpio_output_options_set(SPI_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPI_NSS);
    
    spi_parameter_struct spi_init_struct;

    /* configure SPI1 parameter */
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_8;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(SPI1, &spi_init_struct);

    /* 鍒濆鍖?SPI Flash */
    spi_flash_init();
}

void bsp_gd30ad3344_init(void)
{
    rcu_periph_clock_enable(SPI3_CLK_PORT);
    rcu_periph_clock_enable(RCU_SPI3);
    rcu_periph_clock_enable(RCU_DMA1);
    
    /* configure SPI0 GPIO */
    gpio_af_set(SPI3_PORT, GPIO_AF_5, SPI3_SCK | SPI3_MISO | SPI3_MOSI);
    gpio_mode_set(SPI3_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SPI3_SCK | SPI3_MISO | SPI3_MOSI);
    gpio_output_options_set(SPI3_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPI3_SCK | SPI3_MISO | SPI3_MOSI);

    /* set SPI0_NSS as GPIO*/
    gpio_mode_set(SPI3_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI3_NSS);
    gpio_output_options_set(SPI3_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPI3_NSS);
    
    spi_parameter_struct spi_init_struct;

    /* configure SPI1 parameter */
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_LOW_PH_2EDGE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_8;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(SPI3, &spi_init_struct);

    /* 鍒濆鍖?SPI gd30ad3344 */
    GD30AD3344_Init();
}

