#include "gd30_bsp.h"

#include "gd32f4xx.h"

#define GD30_SPI              SPI3
#define GD30_GPIO_PORT        GPIOE
#define GD30_GPIO_CLK         RCU_GPIOE
#define GD30_SPI_CLK          RCU_SPI3
#define GD30_PIN_CS           GPIO_PIN_4
#define GD30_PIN_SCK          GPIO_PIN_2
#define GD30_PIN_MISO         GPIO_PIN_5
#define GD30_PIN_MOSI         GPIO_PIN_6

static uint8_t gd30_transfer8(uint8_t value)
{
    while (RESET == spi_i2s_flag_get(GD30_SPI, SPI_FLAG_TBE)) {
    }
    spi_i2s_data_transmit(GD30_SPI, value);
    while (RESET == spi_i2s_flag_get(GD30_SPI, SPI_FLAG_RBNE)) {
    }
    return (uint8_t)spi_i2s_data_receive(GD30_SPI);
}

static void gd30_select(void)
{
    gpio_bit_reset(GD30_GPIO_PORT, GD30_PIN_CS);
}

static void gd30_deselect(void)
{
    gpio_bit_set(GD30_GPIO_PORT, GD30_PIN_CS);
}

void gd30_bus_init(void)
{
    spi_parameter_struct spi_init_struct;

    rcu_periph_clock_enable(GD30_GPIO_CLK);
    rcu_periph_clock_enable(GD30_SPI_CLK);

    gpio_af_set(GD30_GPIO_PORT, GPIO_AF_5, GD30_PIN_SCK | GD30_PIN_MISO | GD30_PIN_MOSI);
    gpio_mode_set(GD30_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  GD30_PIN_SCK | GD30_PIN_MISO | GD30_PIN_MOSI);
    gpio_output_options_set(GD30_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GD30_PIN_SCK | GD30_PIN_MISO | GD30_PIN_MOSI);

    gpio_mode_set(GD30_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GD30_PIN_CS);
    gpio_output_options_set(GD30_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GD30_PIN_CS);
    gd30_deselect();

    spi_i2s_deinit(GD30_SPI);
    spi_init_struct.trans_mode = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode = SPI_MASTER;
    spi_init_struct.frame_size = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_LOW_PH_2EDGE;
    spi_init_struct.nss = SPI_NSS_SOFT;
    spi_init_struct.prescale = SPI_PSC_8;
    spi_init_struct.endian = SPI_ENDIAN_MSB;
    spi_init(GD30_SPI, &spi_init_struct);
    spi_enable(GD30_SPI);
}

uint16_t gd30_transfer16(uint16_t value)
{
    uint16_t received;

    gd30_select();
    received = ((uint16_t)gd30_transfer8((uint8_t)(value >> 8)) << 8);
    received |= gd30_transfer8((uint8_t)value);
    while (SET == spi_i2s_flag_get(GD30_SPI, SPI_FLAG_TRANS)) {
    }
    gd30_deselect();

    return received;
}

void gd30_transfer16_sequence(const uint16_t *tx, uint16_t *rx, uint32_t count)
{
    uint32_t i;

    gd30_select();
    for (i = 0U; i < count; i++) {
        uint16_t value;
        uint16_t received;

        value = 0U;
        if (tx != 0) {
            value = tx[i];
        }

        received = ((uint16_t)gd30_transfer8((uint8_t)(value >> 8)) << 8);
        received |= gd30_transfer8((uint8_t)value);

        if (rx != 0) {
            rx[i] = received;
        }
    }
    while (SET == spi_i2s_flag_get(GD30_SPI, SPI_FLAG_TRANS)) {
    }
    gd30_deselect();
}
