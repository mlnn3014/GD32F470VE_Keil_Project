/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2025/06/05
* Note:
*/
#ifndef MCU_CMIC_GD32F470VET6_H
#define MCU_CMIC_GD32F470VET6_H

#include "gd32f4xx.h"
#include "gd32f4xx_sdio.h"
#include "gd32f4xx_dma.h"
#include "systick.h"
#include "adc_bsp.h"
#include "dac_bsp.h"
#include "rtc_bsp.h"
#include "usart_bsp.h"
#include "led_bsp.h"
#include "btn_bsp.h"

#include "button.h"
#include "gd25qxx.h"
#include "gd30ad3344.h"
#include "oled.h"
#include "sdio_sdcard.h"
#include "ff.h"
#include "diskio.h"

#include "sd_app.h"
#include "led_app.h"
#include "adc_app.h"
#include "dac_app.h"
#include "oled_app.h"
#include "usart_app.h"
#include "rtc_app.h"
#include "btn_app.h"
#include "scheduler.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
/* gd25qxx */

#define SPI_PORT              GPIOB
#define SPI_CLK_PORT          RCU_GPIOB

#define SPI_NSS               GPIO_PIN_12
#define SPI_SCK               GPIO_PIN_13
#define SPI_MISO              GPIO_PIN_14
#define SPI_MOSI              GPIO_PIN_15

// FUNCTION
void bsp_gd25qxx_init(void);

/***************************************************************************************************************/

/* gd30ad3344 */

#define SPI3_PORT              GPIOE
#define SPI3_CLK_PORT          RCU_GPIOE

#define SPI3_NSS               GPIO_PIN_4
#define SPI3_SCK               GPIO_PIN_2
#define SPI3_MISO              GPIO_PIN_5
#define SPI3_MOSI              GPIO_PIN_6

// FUNCTION
void bsp_gd30ad3344_init(void);

/***************************************************************************************************************/

#ifdef __cplusplus
  }
#endif

#endif /* MCU_CMIC_GD32F470VET6_H */
