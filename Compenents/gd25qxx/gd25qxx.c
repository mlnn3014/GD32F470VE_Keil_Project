#include "mcu_cmic_gd32f470vet6.h"

#define WRITE 0x02 /* write to memory instruction */
#define WRSR 0x01  /* write status register instruction */
#define WREN 0x06  /* write enable instruction */

#define READ 0x03 /* read from memory instruction */
#define RDSR 0x05 /* read status register instruction  */
#define RDID 0x9F /* read identification */
#define SE 0x20   /* sector erase instruction */
#define BE 0xC7   /* bulk erase instruction */

#define WIP_FLAG 0x01 /* write in progress(wip)flag */
#define DUMMY_BYTE 0xA5

extern uint8_t spi1_send_array[ARRAYSIZE];    // SPI1 DMA 发送缓冲区
extern uint8_t spi1_receive_array[ARRAYSIZE]; // SPI1 DMA 接收缓冲�?
/**
 * @brief Initializes the SPI Flash chip.
 * @note This function assumes that the SPI2 peripheral and CS GPIO (PB12)
 *       have already been initialized elsewhere in the application code.
 *       It primarily ensures the CS pin is high (chip deselected) initially.
 *       You can add a Flash ID read here for an initial check if desired.
 */
void spi_flash_init(void)
{
    /* SPI Flash 片选信号默认为高电平（取消选中状态） */
    SPI_FLASH_CS_HIGH();
    
    /* 使能 SPI1 */
    spi_enable(SPI_FLASH);
    
    /* 可选：读取 Flash ID 来验证通信是否正常 */
    // uint32_t id = spi_flash_read_id();
    // Optional: check ID or print it for debug.
}

void spi_flash_sector_erase(uint32_t sector_addr)
{
    spi_flash_write_enable();

    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(SE);
    spi_flash_send_byte_dma((sector_addr & 0xFF0000) >> 16);
    spi_flash_send_byte_dma((sector_addr & 0xFF00) >> 8);
    spi_flash_send_byte_dma(sector_addr & 0xFF);
    SPI_FLASH_CS_HIGH();

    spi_flash_wait_for_write_end();
}

void spi_flash_bulk_erase(void)
{
    spi_flash_write_enable();

    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(BE);
    SPI_FLASH_CS_HIGH();

    spi_flash_wait_for_write_end();
}

void spi_flash_page_write(uint8_t *pbuffer, uint32_t write_addr, uint16_t num_byte_to_write)
{
    spi_flash_write_enable();

    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(WRITE);
    spi_flash_send_byte_dma((write_addr & 0xFF0000) >> 16);
    spi_flash_send_byte_dma((write_addr & 0xFF00) >> 8);
    spi_flash_send_byte_dma(write_addr & 0xFF);

    while (num_byte_to_write--)
    {
        spi_flash_send_byte_dma(*pbuffer);
        pbuffer++;
    }

    SPI_FLASH_CS_HIGH();
    spi_flash_wait_for_write_end();
}

void spi_flash_buffer_write(uint8_t *pbuffer, uint32_t write_addr, uint16_t num_byte_to_write)
{
    uint8_t num_of_page = 0, num_of_single = 0, addr = 0, count = 0, temp = 0;

    addr = write_addr % SPI_FLASH_PAGE_SIZE;
    count = SPI_FLASH_PAGE_SIZE - addr;
    num_of_page = num_byte_to_write / SPI_FLASH_PAGE_SIZE;
    num_of_single = num_byte_to_write % SPI_FLASH_PAGE_SIZE;

    if (0 == addr)
    {
        if (0 == num_of_page)
        {
            spi_flash_page_write(pbuffer, write_addr, num_byte_to_write);
        }
        else
        {
            while (num_of_page--)
            {
                spi_flash_page_write(pbuffer, write_addr, SPI_FLASH_PAGE_SIZE);
                write_addr += SPI_FLASH_PAGE_SIZE;
                pbuffer += SPI_FLASH_PAGE_SIZE;
            }
            spi_flash_page_write(pbuffer, write_addr, num_of_single);
        }
    }
    else
    {
        if (0 == num_of_page)
        {
            if (num_of_single > count)
            {
                temp = num_of_single - count;
                spi_flash_page_write(pbuffer, write_addr, count);
                write_addr += count;
                pbuffer += count;
                spi_flash_page_write(pbuffer, write_addr, temp);
            }
            else
            {
                spi_flash_page_write(pbuffer, write_addr, num_byte_to_write);
            }
        }
        else
        {
            num_byte_to_write -= count;
            num_of_page = num_byte_to_write / SPI_FLASH_PAGE_SIZE;
            num_of_single = num_byte_to_write % SPI_FLASH_PAGE_SIZE;

            spi_flash_page_write(pbuffer, write_addr, count);
            write_addr += count;
            pbuffer += count;

            while (num_of_page--)
            {
                spi_flash_page_write(pbuffer, write_addr, SPI_FLASH_PAGE_SIZE);
                write_addr += SPI_FLASH_PAGE_SIZE;
                pbuffer += SPI_FLASH_PAGE_SIZE;
            }

            if (0 != num_of_single)
            {
                spi_flash_page_write(pbuffer, write_addr, num_of_single);
            }
        }
    }
}

void spi_flash_buffer_read(uint8_t *pbuffer, uint32_t read_addr, uint16_t num_byte_to_read)
{
    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(READ);
    spi_flash_send_byte_dma((read_addr & 0xFF0000) >> 16);
    spi_flash_send_byte_dma((read_addr & 0xFF00) >> 8);
    spi_flash_send_byte_dma(read_addr & 0xFF);

    while (num_byte_to_read--)
    {
        *pbuffer = spi_flash_send_byte_dma(DUMMY_BYTE);
        pbuffer++;
    }

    SPI_FLASH_CS_HIGH();
}

uint32_t spi_flash_read_id(void)
{
    uint32_t temp = 0, temp0 = 0, temp1 = 0, temp2 = 0;

    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(RDID);
    temp0 = spi_flash_send_byte_dma(DUMMY_BYTE);
    temp1 = spi_flash_send_byte_dma(DUMMY_BYTE);
    temp2 = spi_flash_send_byte_dma(DUMMY_BYTE);
    SPI_FLASH_CS_HIGH();

    temp = (temp0 << 16) | (temp1 << 8) | temp2;
    return temp;
}

void spi_flash_start_read_sequence(uint32_t read_addr)
{
    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(READ);
    spi_flash_send_byte_dma((read_addr & 0xFF0000) >> 16);
    spi_flash_send_byte_dma((read_addr & 0xFF00) >> 8);
    spi_flash_send_byte_dma(read_addr & 0xFF);
}

void spi_flash_write_enable(void)
{
    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(WREN);
    SPI_FLASH_CS_HIGH();
}

void spi_flash_wait_for_write_end(void)
{
    uint8_t flash_status = 0;

    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(RDSR);

    do
    {
        flash_status = spi_flash_send_byte_dma(DUMMY_BYTE);
    } while ((flash_status & WIP_FLAG) == 0x01);

    SPI_FLASH_CS_HIGH();
}

/**
 * @brief Transfer one byte with DMA.
 * @param byte byte to send
 * @return �?SPI 总线接收到的字节
 */
uint8_t spi_flash_send_byte_dma(uint8_t byte)
{
    /* 将数据放入发送缓冲区 */
    spi1_send_array[0] = byte;
    
    /* 配置发�?DMA，只发送一个字�?*/
    dma_single_data_parameter_struct dma_init_struct;
    
    /* 配置 DMA 发送通道 */
    dma_deinit(DMA0, DMA_CH4);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_FLASH);
    dma_init_struct.memory0_addr        = (uint32_t)spi1_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = 1; /* 只发送一个字�?*/
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(DMA0, DMA_CH4, &dma_init_struct);
    dma_channel_subperipheral_select(DMA0, DMA_CH4, DMA_SUBPERI0);
    
    /* 配置 DMA 接收通道 */
    dma_deinit(DMA0, DMA_CH3);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_FLASH);
    dma_init_struct.memory0_addr        = (uint32_t)spi1_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(DMA0, DMA_CH3, &dma_init_struct);
    dma_channel_subperipheral_select(DMA0, DMA_CH3, DMA_SUBPERI0);
    
    /* 启用接收和发送的 DMA 通道 */
    dma_channel_enable(DMA0, DMA_CH3);
    dma_channel_enable(DMA0, DMA_CH4);
    
    /* 启用 SPI �?DMA 接收和发送功�?*/
    spi_dma_enable(SPI_FLASH, SPI_DMA_RECEIVE);
    spi_dma_enable(SPI_FLASH, SPI_DMA_TRANSMIT);
    
    /* 等待 DMA 传输完成 */
    while(RESET == dma_flag_get(DMA0, DMA_CH3, DMA_FLAG_FTF));
    
    /* 禁用 DMA */
    spi_dma_disable(SPI_FLASH, SPI_DMA_RECEIVE);
    spi_dma_disable(SPI_FLASH, SPI_DMA_TRANSMIT);
    dma_channel_disable(DMA0, DMA_CH3);
    dma_channel_disable(DMA0, DMA_CH4);
    
    /* 清除 DMA 标志 */
    dma_flag_clear(DMA0, DMA_CH3, DMA_FLAG_FTF);
    dma_flag_clear(DMA0, DMA_CH4, DMA_FLAG_FTF);
    
    /* 返回接收到的数据 */
    return spi1_receive_array[0];
}

/**
 * @brief 使用 DMA 发送并接收一个半字（16位数据）
 * @param half_word 要发送的半字
 * @return �?SPI 总线接收到的半字
 */
uint16_t spi_flash_send_halfword_dma(uint16_t half_word)
{
    uint16_t rx_data;
    
    /* 先发送高8�?*/
    spi1_send_array[0] = (uint8_t)(half_word >> 8);
    spi1_send_array[1] = (uint8_t)half_word;
    
    /* 配置 DMA 参数 */
    dma_single_data_parameter_struct dma_init_struct;
    
    /* 配置 DMA 发送通道 */
    dma_deinit(DMA0, DMA_CH4);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_FLASH);
    dma_init_struct.memory0_addr        = (uint32_t)spi1_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = 2; /* 发�?个字�?*/
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(DMA0, DMA_CH4, &dma_init_struct);
    dma_channel_subperipheral_select(DMA0, DMA_CH4, DMA_SUBPERI0);
    
    /* 配置 DMA 接收通道 */
    dma_deinit(DMA0, DMA_CH3);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_FLASH);
    dma_init_struct.memory0_addr        = (uint32_t)spi1_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(DMA0, DMA_CH3, &dma_init_struct);
    dma_channel_subperipheral_select(DMA0, DMA_CH3, DMA_SUBPERI0);
    
    /* 启用接收和发送的 DMA 通道 */
    dma_channel_enable(DMA0, DMA_CH3);
    dma_channel_enable(DMA0, DMA_CH4);
    
    /* 启用 SPI �?DMA 接收和发送功�?*/
    spi_dma_enable(SPI_FLASH, SPI_DMA_RECEIVE);
    spi_dma_enable(SPI_FLASH, SPI_DMA_TRANSMIT);
    
    /* 等待 DMA 传输完成 */
    while(RESET == dma_flag_get(DMA0, DMA_CH3, DMA_FLAG_FTF));
    
    /* 禁用 DMA */
    spi_dma_disable(SPI_FLASH, SPI_DMA_RECEIVE);
    spi_dma_disable(SPI_FLASH, SPI_DMA_TRANSMIT);
    dma_channel_disable(DMA0, DMA_CH3);
    dma_channel_disable(DMA0, DMA_CH4);
    
    /* 清除 DMA 标志 */
    dma_flag_clear(DMA0, DMA_CH3, DMA_FLAG_FTF);
    dma_flag_clear(DMA0, DMA_CH4, DMA_FLAG_FTF);
    
    /* 组合接收到的数据 */
    rx_data = (uint16_t)(spi1_receive_array[0] << 8);
    rx_data |= spi1_receive_array[1];
    
    return rx_data;
}

/**
 * @brief 使用 DMA 发送和接收多个字节
 * @param tx_buffer 发送缓冲区
 * @param rx_buffer 接收缓冲�? * @param size 传输大小
 */
void spi_flash_transmit_receive_dma(uint8_t *tx_buffer, uint8_t *rx_buffer, uint16_t size)
{
    /* 检查传输大小是否超过缓冲区 */
    if (size > ARRAYSIZE) {
        size = ARRAYSIZE;
    }
    
    /* 准备发送数�?*/
    for (uint16_t i = 0; i < size; i++) {
        spi1_send_array[i] = tx_buffer[i];
    }
    
    /* 配置 DMA 参数 */
    dma_single_data_parameter_struct dma_init_struct;
    
    /* 配置 DMA 发送通道 */
    dma_deinit(DMA0, DMA_CH4);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_FLASH);
    dma_init_struct.memory0_addr        = (uint32_t)spi1_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = size;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(DMA0, DMA_CH4, &dma_init_struct);
    dma_channel_subperipheral_select(DMA0, DMA_CH4, DMA_SUBPERI0);
    
    /* 配置 DMA 接收通道 */
    dma_deinit(DMA0, DMA_CH3);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_FLASH);
    dma_init_struct.memory0_addr        = (uint32_t)spi1_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(DMA0, DMA_CH3, &dma_init_struct);
    dma_channel_subperipheral_select(DMA0, DMA_CH3, DMA_SUBPERI0);
    
    /* 启用接收和发送的 DMA 通道 */
    dma_channel_enable(DMA0, DMA_CH3);
    dma_channel_enable(DMA0, DMA_CH4);
    
    /* 启用 SPI �?DMA 接收和发送功�?*/
    spi_dma_enable(SPI_FLASH, SPI_DMA_RECEIVE);
    spi_dma_enable(SPI_FLASH, SPI_DMA_TRANSMIT);
    
    /* 等待 DMA 传输完成 */
    while(RESET == dma_flag_get(DMA0, DMA_CH3, DMA_FLAG_FTF));
    
    /* 禁用 DMA */
    spi_dma_disable(SPI_FLASH, SPI_DMA_RECEIVE);
    spi_dma_disable(SPI_FLASH, SPI_DMA_TRANSMIT);
    dma_channel_disable(DMA0, DMA_CH3);
    dma_channel_disable(DMA0, DMA_CH4);
    
    /* 清除 DMA 标志 */
    dma_flag_clear(DMA0, DMA_CH3, DMA_FLAG_FTF);
    dma_flag_clear(DMA0, DMA_CH4, DMA_FLAG_FTF);
    
    /* 复制接收到的数据到接收缓冲区 */
    for (uint16_t i = 0; i < size; i++) {
        rx_buffer[i] = spi1_receive_array[i];
    }
}

/**
 * @brief 等待 DMA 传输完成
 */
void spi_flash_wait_for_dma_end(void)
{
    /* 等待 DMA 传输完成 */
    while(RESET == dma_flag_get(DMA0, DMA_CH3, DMA_FLAG_FTF));
    
    /* 清除 DMA 标志 */
    dma_flag_clear(DMA0, DMA_CH3, DMA_FLAG_FTF);
    dma_flag_clear(DMA0, DMA_CH4, DMA_FLAG_FTF);
}

void test_spi_flash(void)
{
    uint32_t flash_id;
    uint8_t write_buffer[SPI_FLASH_PAGE_SIZE];
    uint8_t read_buffer[SPI_FLASH_PAGE_SIZE];

    uint32_t test_addr = 0x000000; // Test address, choose a sector start

    uart_printf(DEBUG_USART, "SPI FLASH Test Start\r\n");

    // 1. Initialize SPI Flash driver (mainly CS pin state)
    spi_flash_init();
    uart_printf(DEBUG_USART, "SPI Flash Initialized.\r\n");

    // 2. Read Flash ID
    flash_id = spi_flash_read_id();
    uart_printf(DEBUG_USART, "Flash ID: 0x%lX\r\n", flash_id);
    // You can check the ID against your chip manual, e.g., GD25Q64 ID might be 0xC84017

    // 3. Erase a sector (typically 4KB)
    // Note: Erase operation takes time
    uart_printf(DEBUG_USART, "Erasing sector at address 0x%lX...\r\n", test_addr);
    spi_flash_sector_erase(test_addr);
    uart_printf(DEBUG_USART, "Sector erased.\r\n");

    // (Optional) Verify erase: read a page and check if all bytes are 0xFF
    spi_flash_buffer_read(read_buffer, test_addr, SPI_FLASH_PAGE_SIZE);
    int erased_check_ok = 1;
    for (int i = 0; i < SPI_FLASH_PAGE_SIZE; i++)
    {
        if (read_buffer[i] != 0xFF)
        {
            erased_check_ok = 0;
            break;
        }
    }
    if (erased_check_ok)
    {
        uart_printf(DEBUG_USART, "Erase check PASSED. Sector is all 0xFF.\r\n");
    }
    else
    {
        uart_printf(DEBUG_USART, "Erase check FAILED.\r\n");
    }

    // 4. Prepare data to write (one page)
    const char *message = "Hello from STM32 to SPI FLASH! Microunion Studio Test - 12345.";
    uint16_t data_len = strlen(message);
    if (data_len >= SPI_FLASH_PAGE_SIZE)
    {
        data_len = SPI_FLASH_PAGE_SIZE - 1; // Ensure not exceeding page size
    }
    memset(write_buffer, 0, SPI_FLASH_PAGE_SIZE);
    memcpy(write_buffer, message, data_len);
    write_buffer[data_len] = '\0'; // Ensure string termination

    uart_printf(DEBUG_USART, "Writing data to address 0x%lX: \"%s\"\r\n", test_addr, write_buffer);
    // Use spi_flash_buffer_write (can handle cross-page, but here we're writing within one page)
    // Or use spi_flash_page_write directly if certain it's within one page
    spi_flash_buffer_write(write_buffer, test_addr, SPI_FLASH_PAGE_SIZE); // Write entire page with padding
    uart_printf(DEBUG_USART, "Data written.\r\n");

    // 5. Read back the written data
    uart_printf(DEBUG_USART, "Reading data from address 0x%lX...\r\n", test_addr);
    memset(read_buffer, 0x00, SPI_FLASH_PAGE_SIZE);
    spi_flash_buffer_read(read_buffer, test_addr, SPI_FLASH_PAGE_SIZE);
//    spi_flash_buffer_read(read_2_buffer, test_addr, SPI_FLASH_PAGE_SIZE + 1);
    uart_printf(DEBUG_USART, "Data read: \"%.*s\"\r\n", SPI_FLASH_PAGE_SIZE, read_buffer);
//    uart_printf(&huart1, "write_buffer \"%p\"\r\n", (void *)write_buffer);
//    uart_printf(&huart1, "read_buffer \"%p\"\r\n", (void *)read_buffer);
    // 6. Verify data
    if (memcmp(write_buffer, read_buffer, SPI_FLASH_PAGE_SIZE) == 0)
    {
        uart_printf(DEBUG_USART, "Data VERIFIED! Write and Read successful.\r\n");
    }
    else
    {
        uart_printf(DEBUG_USART, "Data VERIFICATION FAILED!\r\n");
    }

    uart_printf(DEBUG_USART, "SPI FLASH Test End\r\n");
}


