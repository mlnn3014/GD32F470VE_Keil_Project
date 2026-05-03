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

/* USART */
#define DEBUG_USART               (USART0)
#define USART0_RDATA_ADDRESS      ((uint32_t)&USART_DATA(USART0))

#define USART_PORT                GPIOA
#define USARTI_CLK_PORT           RCU_GPIOA

#define USART_TX                  GPIO_PIN_9
#define USART_RX                  GPIO_PIN_10

// FUNCTION
void bsp_usart_init(void);

/***************************************************************************************************************/

/* ADC */
#define ADC1_PORT       GPIOC
#define ADC1_CLK_PORT   RCU_GPIOC

#define ADC1_PIN        GPIO_PIN_0

// FUNCTION
void bsp_adc_init(void);

/***************************************************************************************************************/

/* DAC */
#define CONVERT_NUM                     (1)
#define DAC0_R12DH_ADDRESS              (0x40007408)  /* 12??????DAC??????????? */

#define DAC1_PORT       GPIOA
#define DAC1_CLK_PORT   RCU_GPIOA

#define DAC1_PIN        GPIO_PIN_4

// FUNCTION
void bsp_dac_init(void);

/***************************************************************************************************************/

/* RTC */
#define RTC_CLOCK_SOURCE_LXTAL
#define BKP_VALUE    0x32F0

// FUNCTION
int bsp_rtc_init(void);

/***************************************************************************************************************/

#ifdef __cplusplus
  }
#endif

#endif /* MCU_CMIC_GD32F470VET6_H */
