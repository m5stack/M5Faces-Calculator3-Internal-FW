/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, A_Pin|B_Pin|C_Pin|D_Pin
                          |E_Pin|G32_INT_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : PAPin PAPin PAPin PAPin
                           PAPin PAPin */
  GPIO_InitStruct.Pin = A_Pin|B_Pin|C_Pin|D_Pin
                          |E_Pin|G32_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PBPin PBPin PBPin PBPin */
  GPIO_InitStruct.Pin = Col0_Pin|Col1_Pin|Col2_Pin|Col3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 2 */
void set_scan_row(uint8_t current_row) 
{
  // 关闭所有行（全部置高）
  HAL_GPIO_WritePin(GPIOA, A_Pin | B_Pin | C_Pin | D_Pin | E_Pin, GPIO_PIN_SET);

  // 根据行号激活当前行（拉低）
  if (current_row == 0) {
    HAL_GPIO_WritePin(GPIOA, A_Pin, GPIO_PIN_RESET);
  }
  else if (current_row == 1) {
    HAL_GPIO_WritePin(GPIOA, B_Pin, GPIO_PIN_RESET);
  }
  else if (current_row == 2) {
    HAL_GPIO_WritePin(GPIOA, C_Pin, GPIO_PIN_RESET);
  }
  else if (current_row == 3) {
    HAL_GPIO_WritePin(GPIOA, D_Pin, GPIO_PIN_RESET);
  }
  else if (current_row == 4) {
    HAL_GPIO_WritePin(GPIOA, E_Pin, GPIO_PIN_RESET);
  }
}

uint8_t read_columns(void) 
{
  uint8_t res = 0;

  if (HAL_GPIO_ReadPin(GPIOB, Col0_Pin) == GPIO_PIN_RESET) {
    res = 1;
  }
  if (HAL_GPIO_ReadPin(GPIOB, Col1_Pin) == GPIO_PIN_RESET) {
    res = 2;
  }
  if (HAL_GPIO_ReadPin(GPIOB, Col2_Pin) == GPIO_PIN_RESET) {
    res = 3;
  }
  if (HAL_GPIO_ReadPin(GPIOB, Col3_Pin) == GPIO_PIN_RESET) {
    res = 4;
  }

  return res;
}
/* USER CODE END 2 */
