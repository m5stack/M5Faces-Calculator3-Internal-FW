/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"

#include "stm32g0xx_ll_i2c.h"
#include "stm32g0xx_ll_bus.h"
#include "stm32g0xx_ll_cortex.h"
#include "stm32g0xx_ll_rcc.h"
#include "stm32g0xx_ll_system.h"
#include "stm32g0xx_ll_utils.h"
#include "stm32g0xx_ll_pwr.h"
#include "stm32g0xx_ll_gpio.h"
#include "stm32g0xx_ll_dma.h"

#include "stm32g0xx_ll_exti.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef union {
    struct {
        uint8_t flash_err  : 1;  // flash设置错误
        uint8_t rsvd          : 7;  // 保留位
    } flags;
    uint8_t value;  // 整体访问接口
} __attribute__((packed)) PeriError_TypeDef;

typedef struct {
    volatile uint8_t jump_to_bootloader   : 1;  // 跳转到bootloader
    volatile uint8_t i2c_addr_config_update   : 1;  // i2c地址配置更新
    volatile uint8_t flash_config_update   : 1;  // flash配置更新
    volatile uint8_t reserved        : 5;  // 预留位
} EventFlags_t;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
uint32_t millis(void);
uint32_t micros(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define A_Pin GPIO_PIN_0
#define A_GPIO_Port GPIOA
#define B_Pin GPIO_PIN_1
#define B_GPIO_Port GPIOA
#define C_Pin GPIO_PIN_2
#define C_GPIO_Port GPIOA
#define D_Pin GPIO_PIN_3
#define D_GPIO_Port GPIOA
#define E_Pin GPIO_PIN_4
#define E_GPIO_Port GPIOA
#define G32_INT_Pin GPIO_PIN_5
#define G32_INT_GPIO_Port GPIOA
#define Col0_Pin GPIO_PIN_0
#define Col0_GPIO_Port GPIOB
#define Col1_Pin GPIO_PIN_1
#define Col1_GPIO_Port GPIOB
#define Col2_Pin GPIO_PIN_3
#define Col2_GPIO_Port GPIOB
#define Col3_Pin GPIO_PIN_4
#define Col3_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define FLASH_DATA_SIZE 64
#define I2C_ADDRESS 0x08
#define FIRMWARE_VERSION 0x03

#define APPLICATION_ADDRESS     ((uint32_t)0x08002800) 
#define BOOTLOADER_ADDRESS     ((uint32_t)0x08000000) 
#define BOOTLOADER_VER_ADDR ((uint32_t)0x08002800 - 1)

extern PeriError_TypeDef peri_error;
extern volatile EventFlags_t g_events;  // 全局事件标志

// i2c address
extern uint8_t i2c_address[1];
// flash data
extern uint8_t flash_data[FLASH_DATA_SIZE];
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
