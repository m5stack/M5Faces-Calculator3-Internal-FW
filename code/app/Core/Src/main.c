/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "iwdg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "pt_task.h"
#include "flash.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FACES_TYPE_ID_CALCULATOR (1)

#define FACES_TYPE_ID_KEYBOARD (2)

#define FACES_TYPE_ID_GAMEBOY (3)

#define DEVICE_ID (FACES_TYPE_ID_CALCULATOR)

#define UID_REGISTER_START (0xE0)
#define UID_REGISTER_END (0xEB)
#define UID_LENGTH (12)
#define DEVICE_ID_REGISTER (0xD0)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
PeriError_TypeDef peri_error;
volatile EventFlags_t g_events = {0};  // 全局事件标志

// i2c address
uint8_t i2c_address[1] = {0};
// flash data
uint8_t flash_data[FLASH_DATA_SIZE] = {0};
uint8_t g_uid[UID_LENGTH] = {0};
volatile uint8_t fm_version = FIRMWARE_VERSION;
volatile uint8_t i2c_slave_handle_tx_buf[48] = {0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint32_t millis(void) 
{
  return HAL_GetTick();
}

__STATIC_INLINE uint32_t GXT_SYSTICK_IsActiveCounterFlag(void)
{
  return ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == (SysTick_CTRL_COUNTFLAG_Msk));
}

static uint32_t getCurrentMicros(void)
{
  /* Ensure COUNTFLAG is reset by reading SysTick control and status register */
  GXT_SYSTICK_IsActiveCounterFlag();
  uint32_t m = HAL_GetTick();
  const uint32_t tms = SysTick->LOAD + 1;
  __IO uint32_t u = tms - SysTick->VAL;
  if (GXT_SYSTICK_IsActiveCounterFlag()) {
    m = HAL_GetTick();
    u = tms - SysTick->VAL;
  }
  return (m * 1000 + (u * 1000) / tms);
}

//获取系统时间，单位us
uint32_t micros(void)
{
  return getCurrentMicros();
}

void read_uid(void)
{
  uint32_t uid0 = HAL_GetUIDw0();
  uint32_t uid1 = HAL_GetUIDw1();
  uint32_t uid2 = HAL_GetUIDw2();

  memcpy(&g_uid[0], &uid0, 4);
  memcpy(&g_uid[4], &uid1, 4);
  memcpy(&g_uid[8], &uid2, 4);
}

void Slave_Complete_Callback(uint8_t *rx_data, uint16_t len)
{
  uint8_t rx_buf[48] = {0};
  uint8_t rx_mark[48] = {0};   

  if (len == 1) {
    if ((rx_data[0] >= UID_REGISTER_START) && (rx_data[0] <= UID_REGISTER_END)) {
      uint8_t uid_offset = rx_data[0] - UID_REGISTER_START;
      i2c2_set_send_data(&g_uid[uid_offset], UID_LENGTH - uid_offset);
    }
    else if (rx_data[0] == DEVICE_ID_REGISTER) {
      i2c_slave_handle_tx_buf[0] = DEVICE_ID;
      i2c2_set_send_data((uint8_t *)&i2c_slave_handle_tx_buf[0], 1);
    }

    if ((rx_data[0] >= 0xFB) && (rx_data[0] <= 0xFF))
    {
      i2c_slave_handle_tx_buf[0] = peri_error.value;
      i2c_slave_handle_tx_buf[1] = (*(uint8_t*)BOOTLOADER_VER_ADDR);
      i2c_slave_handle_tx_buf[2] = 0;
      i2c_slave_handle_tx_buf[3] = fm_version;
      i2c_slave_handle_tx_buf[4] = i2c_address[0];
      i2c2_set_send_data((uint8_t *)&i2c_slave_handle_tx_buf[rx_data[0]-0xFB], 0xFF-rx_data[0]+1);
    }    
  }  
  else if (len > 1) {
    if (rx_data[0] == 0xFD) {
      if (rx_data[1] == 1) {
        g_events.jump_to_bootloader = 1;
      }
    }
    else if (rx_data[0] == 0xFF)
    {
      if (len == 2) {
        if ((rx_data[1]) && (rx_data[1] < 128)) {
          if (i2c_address[0] != rx_data[1]) {
            i2c_address[0] = rx_data[1];
            g_events.i2c_addr_config_update = 1;
            g_events.flash_config_update = 1;
          }
        }
      }       
    }    
  }  
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  if (HAL_Init() != HAL_OK) {
    HAL_NVIC_SystemReset();
  }

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  // MX_I2C2_Init();
  MX_IWDG_Init();
  /* USER CODE BEGIN 2 */
  HAL_IWDG_Refresh(&hiwdg);
  init_flash_data();
  init_pt_task();
  read_uid();
  user_i2c_init();
  i2c2_it_enable();
  HAL_IWDG_Refresh(&hiwdg);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    main_scheduler(&main_pt);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
    HAL_NVIC_SystemReset();
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
