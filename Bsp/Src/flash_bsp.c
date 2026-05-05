#include "flash_bsp.h"

#define FLASH_SPI              SPI1
#define FLASH_GPIO_PORT        GPIOB
#define FLASH_GPIO_CLK         RCU_GPIOB
#define FLASH_SPI_CLK          RCU_SPI1
#define FLASH_DMA_CLK          RCU_DMA0
#define FLASH_PIN_CS           GPIO_PIN_12
#define FLASH_PIN_SCK          GPIO_PIN_13
#define FLASH_PIN_MISO         GPIO_PIN_14
#define FLASH_PIN_MOSI         GPIO_PIN_15
#define FLASH_DMA              DMA0
#define FLASH_DMA_RX_CH        DMA_CH3
#define FLASH_DMA_TX_CH        DMA_CH4
#define FLASH_DMA_SUBPERI      DMA_SUBPERI0

static uint8_t flash_tx_byte;
static uint8_t flash_rx_byte;

void flash_bus_init(void)
{
    spi_parameter_struct spi_init_struct;

    rcu_periph_clock_enable(FLASH_GPIO_CLK);
    rcu_periph_clock_enable(FLASH_SPI_CLK);
    rcu_periph_clock_enable(FLASH_DMA_CLK);

    gpio_af_set(FLASH_GPIO_PORT, GPIO_AF_5, FLASH_PIN_SCK | FLASH_PIN_MISO | FLASH_PIN_MOSI);
    gpio_mode_set(FLASH_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  FLASH_PIN_SCK | FLASH_PIN_MISO | FLASH_PIN_MOSI);
    gpio_output_options_set(FLASH_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            FLASH_PIN_SCK | FLASH_PIN_MISO | FLASH_PIN_MOSI);

    gpio_mode_set(FLASH_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, FLASH_PIN_CS);
    gpio_output_options_set(FLASH_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, FLASH_PIN_CS);
    flash_bus_deselect();

    spi_i2s_deinit(FLASH_SPI);
    spi_init_struct.trans_mode = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode = SPI_MASTER;
    spi_init_struct.frame_size = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
    spi_init_struct.nss = SPI_NSS_SOFT;
    spi_init_struct.prescale = SPI_PSC_8;
    spi_init_struct.endian = SPI_ENDIAN_MSB;
    spi_init(FLASH_SPI, &spi_init_struct);
    spi_enable(FLASH_SPI);
}

void flash_bus_select(void)
{
    gpio_bit_reset(FLASH_GPIO_PORT, FLASH_PIN_CS);
}

void flash_bus_deselect(void)
{
    gpio_bit_set(FLASH_GPIO_PORT, FLASH_PIN_CS);
}

uint8_t flash_bus_transfer(uint8_t data)
{
    dma_single_data_parameter_struct dma_init_struct;

    flash_tx_byte = data;
    flash_rx_byte = 0U;

    dma_deinit(FLASH_DMA, FLASH_DMA_TX_CH);
    dma_init_struct.periph_addr = (uint32_t)&SPI_DATA(FLASH_SPI);
    dma_init_struct.memory0_addr = (uint32_t)&flash_tx_byte;
    dma_init_struct.direction = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_init_struct.number = 1U;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_DISABLE;
    dma_init_struct.circular_mode = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(FLASH_DMA, FLASH_DMA_TX_CH, &dma_init_struct);
    dma_channel_subperipheral_select(FLASH_DMA, FLASH_DMA_TX_CH, FLASH_DMA_SUBPERI);

    dma_deinit(FLASH_DMA, FLASH_DMA_RX_CH);
    dma_init_struct.memory0_addr = (uint32_t)&flash_rx_byte;
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_single_data_mode_init(FLASH_DMA, FLASH_DMA_RX_CH, &dma_init_struct);
    dma_channel_subperipheral_select(FLASH_DMA, FLASH_DMA_RX_CH, FLASH_DMA_SUBPERI);

    dma_channel_enable(FLASH_DMA, FLASH_DMA_RX_CH);
    dma_channel_enable(FLASH_DMA, FLASH_DMA_TX_CH);

    spi_dma_enable(FLASH_SPI, SPI_DMA_RECEIVE);
    spi_dma_enable(FLASH_SPI, SPI_DMA_TRANSMIT);

    while (RESET == dma_flag_get(FLASH_DMA, FLASH_DMA_RX_CH, DMA_FLAG_FTF)) {
    }

    spi_dma_disable(FLASH_SPI, SPI_DMA_RECEIVE);
    spi_dma_disable(FLASH_SPI, SPI_DMA_TRANSMIT);
    dma_channel_disable(FLASH_DMA, FLASH_DMA_RX_CH);
    dma_channel_disable(FLASH_DMA, FLASH_DMA_TX_CH);

    dma_flag_clear(FLASH_DMA, FLASH_DMA_RX_CH, DMA_FLAG_FTF);
    dma_flag_clear(FLASH_DMA, FLASH_DMA_TX_CH, DMA_FLAG_FTF);

    return flash_rx_byte;
}
