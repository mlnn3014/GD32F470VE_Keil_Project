#include "usart_bsp.h"

#include "gd32f4xx.h"
#include "ring_buffer.h"

#define USART_BSP_PERIPH        USART0
#define USART_BSP_BAUDRATE      115200U
#define USART_BSP_DATA_REG      ((uint32_t)&USART_DATA(USART_BSP_PERIPH))

#define USART_BSP_GPIO_CLOCK    RCU_GPIOA
#define USART_BSP_GPIO_PORT     GPIOA
#define USART_BSP_TX_PIN        GPIO_PIN_9
#define USART_BSP_RX_PIN        GPIO_PIN_10
#define USART_BSP_GPIO_AF       GPIO_AF_7

#define USART_BSP_DMA_PERIPH    DMA1
#define USART_BSP_DMA_CLOCK     RCU_DMA1
#define USART_BSP_RX_DMA_CH     DMA_CH2
#define USART_BSP_TX_DMA_CH     DMA_CH7
#define USART_BSP_DMA_SUBPERI   DMA_SUBPERI4

#define USART_BSP_RX_DMA_SIZE   256U
#define USART_BSP_RX_RING_SIZE  2048U
#define USART_BSP_TX_RING_SIZE  2048U

#if ((USART_BSP_RX_DMA_SIZE & (USART_BSP_RX_DMA_SIZE - 1U)) != 0U)
#error "USART_BSP_RX_DMA_SIZE must be a power of 2"
#endif

#define USART_BSP_RX_DMA_MASK   (USART_BSP_RX_DMA_SIZE - 1U)

static uint8_t usart_rx_dma_buffer[USART_BSP_RX_DMA_SIZE];
static volatile uint16_t usart_rx_dma_read_index;
static volatile uint8_t usart_rx_poll_busy;

static uint8_t usart_rx_ring_buffer[USART_BSP_RX_RING_SIZE];
static ring_buffer_t usart_rx_ring;

static uint8_t usart_tx_ring_buffer[USART_BSP_TX_RING_SIZE];
static ring_buffer_t usart_tx_ring;
static volatile uint16_t usart_tx_dma_length;
static volatile uint8_t usart_tx_dma_busy;

static volatile uint32_t usart_rx_overflow_count;
static volatile uint32_t usart_tx_overflow_count;

static uint32_t usart_enter_critical(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

static void usart_exit_critical(uint32_t primask)
{
    if (primask == 0U) {
        __enable_irq();
    }
}

static void uart_rx_push_locked(uint8_t data)
{
    if (ring_buffer_is_full(&usart_rx_ring) != 0U) {
        usart_rx_overflow_count++;
        return;
    }

    (void)ring_buffer_write(&usart_rx_ring, &data, 1U);
}

static void usart_rx_dma_config(void)
{
    dma_single_data_parameter_struct dma_init;

    dma_deinit(USART_BSP_DMA_PERIPH, USART_BSP_RX_DMA_CH);
    dma_single_data_para_struct_init(&dma_init);
    dma_init.direction = DMA_PERIPH_TO_MEMORY;
    dma_init.memory0_addr = (uint32_t)usart_rx_dma_buffer;
    dma_init.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init.periph_addr = USART_BSP_DATA_REG;
    dma_init.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init.circular_mode = DMA_CIRCULAR_MODE_ENABLE;
    dma_init.number = USART_BSP_RX_DMA_SIZE;
    dma_init.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(USART_BSP_DMA_PERIPH, USART_BSP_RX_DMA_CH, &dma_init);
    dma_channel_subperipheral_select(USART_BSP_DMA_PERIPH, USART_BSP_RX_DMA_CH,
                                     USART_BSP_DMA_SUBPERI);
    dma_channel_enable(USART_BSP_DMA_PERIPH, USART_BSP_RX_DMA_CH);
}

static void usart_tx_dma_config(void)
{
    dma_single_data_parameter_struct dma_init;

    dma_deinit(USART_BSP_DMA_PERIPH, USART_BSP_TX_DMA_CH);
    dma_single_data_para_struct_init(&dma_init);
    dma_init.direction = DMA_MEMORY_TO_PERIPH;
    dma_init.memory0_addr = (uint32_t)usart_tx_ring_buffer;
    dma_init.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init.periph_addr = USART_BSP_DATA_REG;
    dma_init.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init.circular_mode = DMA_CIRCULAR_MODE_DISABLE;
    dma_init.number = 1U;
    dma_init.priority = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(USART_BSP_DMA_PERIPH, USART_BSP_TX_DMA_CH, &dma_init);
    dma_channel_subperipheral_select(USART_BSP_DMA_PERIPH, USART_BSP_TX_DMA_CH,
                                     USART_BSP_DMA_SUBPERI);
    dma_interrupt_enable(USART_BSP_DMA_PERIPH, USART_BSP_TX_DMA_CH, DMA_INT_FTF);
}

static void usart_tx_start_dma(void)
{
    uint16_t length;

    if ((usart_tx_dma_busy != 0U) || (ring_buffer_available(&usart_tx_ring) == 0U)) {
        return;
    }

    length = ring_buffer_read_linear(&usart_tx_ring);

    usart_tx_dma_length = length;
    usart_tx_dma_busy = 1U;

    dma_channel_disable(USART_BSP_DMA_PERIPH, USART_BSP_TX_DMA_CH);
    dma_flag_clear(USART_BSP_DMA_PERIPH, USART_BSP_TX_DMA_CH,
                   DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_TAE | DMA_FLAG_SDE | DMA_FLAG_FEE);
    dma_memory_address_config(USART_BSP_DMA_PERIPH, USART_BSP_TX_DMA_CH, DMA_MEMORY_0,
                              (uint32_t)ring_buffer_read_ptr(&usart_tx_ring));
    dma_transfer_number_config(USART_BSP_DMA_PERIPH, USART_BSP_TX_DMA_CH, length);
    dma_channel_enable(USART_BSP_DMA_PERIPH, USART_BSP_TX_DMA_CH);
}

static void usart_tx_finish_dma(void)
{
    uint16_t length = usart_tx_dma_length;

    ring_buffer_drop(&usart_tx_ring, length);
    usart_tx_dma_length = 0U;
    usart_tx_dma_busy = 0U;

    usart_tx_start_dma();
}

void uart_init(void)
{
    ring_buffer_init(&usart_rx_ring, usart_rx_ring_buffer, USART_BSP_RX_RING_SIZE);
    ring_buffer_init(&usart_tx_ring, usart_tx_ring_buffer, USART_BSP_TX_RING_SIZE);

    rcu_periph_clock_enable(USART_BSP_GPIO_CLOCK);
    rcu_periph_clock_enable(RCU_USART0);
    rcu_periph_clock_enable(USART_BSP_DMA_CLOCK);

    gpio_af_set(USART_BSP_GPIO_PORT, USART_BSP_GPIO_AF, USART_BSP_TX_PIN | USART_BSP_RX_PIN);
    gpio_mode_set(USART_BSP_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  USART_BSP_TX_PIN | USART_BSP_RX_PIN);
    gpio_output_options_set(USART_BSP_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            USART_BSP_TX_PIN | USART_BSP_RX_PIN);

    usart_deinit(USART_BSP_PERIPH);
    usart_baudrate_set(USART_BSP_PERIPH, USART_BSP_BAUDRATE);
    usart_receive_config(USART_BSP_PERIPH, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART_BSP_PERIPH, USART_TRANSMIT_ENABLE);

    usart_rx_dma_config();
    usart_tx_dma_config();

    usart_dma_receive_config(USART_BSP_PERIPH, USART_RECEIVE_DMA_ENABLE);
    usart_dma_transmit_config(USART_BSP_PERIPH, USART_TRANSMIT_DMA_ENABLE);
    usart_enable(USART_BSP_PERIPH);

    nvic_irq_enable(USART0_IRQn, 0U, 0U);
    nvic_irq_enable(DMA1_Channel7_IRQn, 1U, 0U);
    usart_interrupt_enable(USART_BSP_PERIPH, USART_INT_IDLE);
}

uint16_t uart_write(const uint8_t *data, uint16_t length)
{
    uint16_t written = 0U;
    uint32_t primask;

    if ((data == 0) || (length == 0U)) {
        return 0U;
    }

    primask = usart_enter_critical();
    written = ring_buffer_write(&usart_tx_ring, data, length);
    if (written < length) {
        usart_tx_overflow_count += (uint32_t)(length - written);
    }

    usart_tx_start_dma();
    usart_exit_critical(primask);

    return written;
}

uint16_t uart_read(uint8_t *data, uint16_t length)
{
    uint16_t read_count = 0U;
    uint32_t primask;

    if ((data == 0) || (length == 0U)) {
        return 0U;
    }

    uart_poll();

    primask = usart_enter_critical();
    read_count = ring_buffer_read(&usart_rx_ring, data, length);
    usart_exit_critical(primask);

    return read_count;
}

uint8_t uart_read_byte(uint8_t *data)
{
    return (uart_read(data, 1U) == 1U) ? 1U : 0U;
}

uint16_t uart_available(void)
{
    uint16_t available;
    uint32_t primask;

    uart_poll();
    primask = usart_enter_critical();
    available = ring_buffer_available(&usart_rx_ring);
    usart_exit_critical(primask);

    return available;
}

uart_status_t uart_status(void)
{
    uart_status_t status;
    uint32_t primask;

    uart_poll();
    primask = usart_enter_critical();
    status.rx_overflow_count = usart_rx_overflow_count;
    status.tx_overflow_count = usart_tx_overflow_count;
    status.rx_available = ring_buffer_available(&usart_rx_ring);
    status.tx_pending = ring_buffer_available(&usart_tx_ring);
    usart_exit_critical(primask);

    return status;
}

void uart_poll(void)
{
    uint16_t write_index;
    uint32_t primask;

    write_index = (uint16_t)(USART_BSP_RX_DMA_SIZE -
                             dma_transfer_number_get(USART_BSP_DMA_PERIPH, USART_BSP_RX_DMA_CH));
    write_index &= USART_BSP_RX_DMA_MASK;

    primask = usart_enter_critical();
    if (usart_rx_poll_busy != 0U) {
        usart_exit_critical(primask);
        return;
    }
    usart_rx_poll_busy = 1U;
    usart_exit_critical(primask);

    while (usart_rx_dma_read_index != write_index) {
        uint8_t data = usart_rx_dma_buffer[usart_rx_dma_read_index];

        primask = usart_enter_critical();
        uart_rx_push_locked(data);
        usart_rx_dma_read_index = (uint16_t)((usart_rx_dma_read_index + 1U) &
                                             USART_BSP_RX_DMA_MASK);
        usart_exit_critical(primask);
    }

    primask = usart_enter_critical();
    usart_rx_poll_busy = 0U;
    usart_exit_critical(primask);
}

void uart_irq_handler(void)
{
    if (usart_interrupt_flag_get(USART_BSP_PERIPH, USART_INT_FLAG_IDLE) != RESET) {
        (void)usart_data_receive(USART_BSP_PERIPH);
        uart_poll();
    }
}

void uart_tx_dma_irq_handler(void)
{
    if (dma_interrupt_flag_get(USART_BSP_DMA_PERIPH, USART_BSP_TX_DMA_CH,
                               DMA_INT_FLAG_FTF) != RESET) {
        dma_interrupt_flag_clear(USART_BSP_DMA_PERIPH, USART_BSP_TX_DMA_CH,
                                 DMA_INT_FLAG_FTF);
        dma_channel_disable(USART_BSP_DMA_PERIPH, USART_BSP_TX_DMA_CH);
        usart_tx_finish_dma();
    }
}
