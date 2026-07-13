/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    flash.c
  * @brief   This file provides code for the configuration
  *          of the flash instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "flash.h"

// 宏定义提高可维护性
#define MAX_RETRY_COUNT     20      // 单次操作最大重试次数
#define RESET_DELAY_MS      1000    // 复位前延时
#define FLASH_TIMEOUT_MS    100     // FLASH操作超时时间

static void system_reset(void);
static HAL_StatusTypeDef flash_recover_lock(void);

// 错误处理统一宏
#define CHECK_AND_RESET(op, err) \
    do { \
        if ((err) != HAL_OK) { \
            flash_recover_lock(); \
            system_reset(); \
        } \
    } while(0)
/*******************************************************************************
* Function Name  : doseFlashHasPackedMessage
* Description    : Does flash has packed messages   
* Input          : None
* Output         : 
* Return         : ture/false
*******************************************************************************/
bool doseFlashHasPackedMessage(void)
{
    uint16_t length;
    uint16_t getHead;    

    /*Is head matched*/ 
    getHead = (uint16_t)(*(uint16_t*)(STM32G0xx_FLASH_PAGE31_STARTADDR ));      
    if( EEPPROM_PACKAGEHEAD != getHead )
    {
        return false;
    }
    
    /*Is length zero*/
    length = (*(uint16_t*)(STM32G0xx_FLASH_PAGE31_STARTADDR+2));
    if( 0 == length)
    {
        return false;
    }
    
    return true;
}
/*******************************************************************************
* Function Name  : getValuablePackedMessageLengthofFlash
* Description    : Get valuable packed message length of flash 
* Input          : None
* Output         : 
* Return         : valuable length
*******************************************************************************/
uint16_t getValuablePackedMessageLengthofFlash( void )
{
    uint16_t length;
         
    /*Is head matched*/       
    if( EEPPROM_PACKAGEHEAD != (*(uint16_t*)(STM32G0xx_FLASH_PAGE31_STARTADDR )) )
    {
        return 0;
    }
    
    /*Get length*/
    length = (uint16_t)(*(uint16_t*)(STM32G0xx_FLASH_PAGE31_STARTADDR+2));   
    
    return length;
}
/*******************************************************************************
* Function Name  : readPackedMessageFromFlash
* Description    : Read packed message form flash
* Input          : buff:point to first location of received buffer.length:Maxmum length of reception
* Output         : 
* Return         : reception length
*******************************************************************************/
uint16_t readPackedMessageFromFlash( uint8_t *buff , uint16_t length)
{
    int i;
    uint16_t getLength;
    
    if( !doseFlashHasPackedMessage() )
        return 0;
    
    /*Get valuable length*/
    getLength = getValuablePackedMessageLengthofFlash();
    
    /*Read out message*/
    for(i=0;i<MIN(getLength,length);i++)
    {
        buff[i]= *(uint8_t*)(STM32G0xx_FLASH_PAGE31_STARTADDR+8+i);
    }     
    
    return MIN(getLength,length);
}
/*******************************************************************************
* Function Name  : Flash_eeprom_WriteWithPacked
* Description    : Write a group of datas to flash.
* Input          : buff:pointer of first data, length: write length
* Output         : 
* Return         : true/false
*******************************************************************************/
bool writeMessageToFlash( uint8_t *buff , uint16_t length)
{
    uint64_t temp;
    int i;
    FLASH_EraseInitTypeDef My_Flash;
    uint32_t flash_lock_timeout = 0;
    
    /*Protection*/
    if( (length+4) > STM32G0xx_PAGE_SIZE )
    {
        return false;
    }
    
    if (HAL_FLASH_Unlock() != HAL_OK) {
        return false;
    }

    My_Flash.TypeErase = FLASH_TYPEERASE_PAGES;  
    My_Flash.Page        = 31;
    My_Flash.NbPages = 1;                        
    
    uint32_t PageError = 0;                    
    if (HAL_FLASHEx_Erase(&My_Flash, &PageError) != HAL_OK) {
        // if flash cannot lock, reset system
        flash_lock_timeout = 0;
        while(HAL_FLASH_Lock() != HAL_OK) {
            flash_lock_timeout++;
            if (flash_lock_timeout > 20) {
                HAL_NVIC_SystemReset();
                flash_lock_timeout = 0;
                HAL_Delay(1000);
            }
        }
        return false;
    }  

    // define head and length
    temp = EEPPROM_PACKAGEHEAD |  (uint64_t)length << 16;    
    
    /*Write head and length*/
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, STM32G0xx_FLASH_PAGE31_STARTADDR, temp) != HAL_OK) {
        // if flash cannot lock, reset system
        flash_lock_timeout = 0;
        while(HAL_FLASH_Lock() != HAL_OK) {
            flash_lock_timeout++;
            if (flash_lock_timeout > 20) {
                HAL_NVIC_SystemReset();
                flash_lock_timeout = 0;
                HAL_Delay(1000);
            }
        }
        return false;
    }
    
    /*Write datas*/
    for(i=0 ;i<length/8 ;i++)
    {
        temp = buff[8*i] | (uint64_t)buff[8*i+1]<<8 | (uint64_t)buff[8*i+2]<<16 | (uint64_t)buff[8*i+3]<<24\
        | (uint64_t)buff[8*i+4]<<32 | (uint64_t)buff[8*i+5]<<40 | (uint64_t)buff[8*i+6]<<48 | (uint64_t)buff[8*i+7]<<56;

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, STM32G0xx_FLASH_PAGE31_STARTADDR+8+8*i, temp) != HAL_OK) {
            // if flash cannot lock, reset system
            flash_lock_timeout = 0;
            while(HAL_FLASH_Lock() != HAL_OK) {
                flash_lock_timeout++;
                if (flash_lock_timeout > 20) {
                    HAL_NVIC_SystemReset();
                    flash_lock_timeout = 0;
                    HAL_Delay(1000);
                }
            }
            return false;            
        }
    }  
    
    /*Read out and check*/
    for(i=0 ;i<length ;i++)
    {
        if( *(uint8_t*)(STM32G0xx_FLASH_PAGE31_STARTADDR+8+i) != buff[i] )
        {
            // if flash cannot lock, reset system
            flash_lock_timeout = 0;
            while(HAL_FLASH_Lock() != HAL_OK) {
                flash_lock_timeout++;
                if (flash_lock_timeout > 20) {
                    HAL_NVIC_SystemReset();
                    flash_lock_timeout = 0;
                    HAL_Delay(1000);
                }
            }
            return false;
        }
    }    
    
    // if flash cannot lock, reset system
    flash_lock_timeout = 0;
    while(HAL_FLASH_Lock() != HAL_OK) {
        flash_lock_timeout++;
        if (flash_lock_timeout > 20) {
            HAL_NVIC_SystemReset();
            flash_lock_timeout = 0;
            HAL_Delay(1000);
        }
    }
    return true;    
}

void init_flash_data(void) 
{   
  uint32_t flash_write_timeout = 0;

  if (!(readPackedMessageFromFlash(flash_data, FLASH_DATA_SIZE))) {
    i2c_address[0] = I2C_ADDRESS;

    flash_data[0] = i2c_address[0];
    flash_data[1] = peri_error.value;

    // flash 写入失败，flash错误位置1
    while(!writeMessageToFlash(flash_data , FLASH_DATA_SIZE)) {
      flash_write_timeout++;
      if (flash_write_timeout > 20) {       
        peri_error.flags.flash_err = 1;
        break;
      }
    }
  } else {
    i2c_address[0] = flash_data[0];
  }
}

uint8_t flash_data_write_back(void)
{
  uint32_t flash_write_timeout = 0;

  // if read flash ok
  if (readPackedMessageFromFlash(flash_data, FLASH_DATA_SIZE)) {
    flash_data[0] = i2c_address[0];
    flash_data[1] = peri_error.value;
    // flash写入失败，返回0，flash错误置1
    while(!writeMessageToFlash(flash_data , FLASH_DATA_SIZE)) {
      flash_write_timeout++;
      if (flash_write_timeout > 20) {
        peri_error.flags.flash_err = 1;
        flash_write_timeout = 0;
        return 0;
      }
    }
    // write success, return 1
    return 1;
  }
  else {
    peri_error.flags.flash_err = 1;
    return 0;
  }     
}

// 系统复位函数
static void system_reset(void) 
{
    HAL_Delay(RESET_DELAY_MS);
    HAL_NVIC_SystemReset();
}

// FLASH资源恢复函数
static HAL_StatusTypeDef flash_recover_lock(void) 
{
    HAL_StatusTypeDef status = HAL_OK;
    uint32_t start_tick = HAL_GetTick();
    
    // 优先锁定OB
    while (HAL_FLASH_OB_Lock() != HAL_OK) {
        if (HAL_GetTick() - start_tick > FLASH_TIMEOUT_MS) {
            status = HAL_ERROR;
            break;
        }
    }
    
    // 然后锁定FLASH
    start_tick = HAL_GetTick();
    while (HAL_FLASH_Lock() != HAL_OK) {
        if (HAL_GetTick() - start_tick > FLASH_TIMEOUT_MS) {
            status = HAL_ERROR;
            break;
        }
    }
    
    return status;
}

// 带超时的FLASH操作封装
static HAL_StatusTypeDef flash_op_with_timeout(HAL_StatusTypeDef (*op)(void)) 
{
    uint32_t start_tick = HAL_GetTick();
    HAL_StatusTypeDef status;
    
    do {
        status = op();
        if (status == HAL_OK) break;
    } while (HAL_GetTick() - start_tick <= FLASH_TIMEOUT_MS);
    
    return status;
}

// 修改函数签名，接受带参数的函数指针和参数
static HAL_StatusTypeDef flash_op_with_timeout_and_para(
    HAL_StatusTypeDef (*op)(FLASH_OBProgramInitTypeDef*), 
    FLASH_OBProgramInitTypeDef* p_arg
) {
    uint32_t start_tick = HAL_GetTick();
    HAL_StatusTypeDef status;

    do {
        status = op(p_arg);  // 传递参数给函数指针
        if (status == HAL_OK) break;
    } while (HAL_GetTick() - start_tick <= FLASH_TIMEOUT_MS);

    return status;
}

// 优化后的选项字节编程函数
HAL_StatusTypeDef iwdg_ob_program(void) 
{
    FLASH_OBProgramInitTypeDef ob_init = {0};
    HAL_StatusTypeDef status;
    
    // 1. 获取当前OB配置
    HAL_FLASHEx_OBGetConfig(&ob_init);
    
    // 2. 检查是否需要修改IWDG_STDBY位
    if (!(ob_init.USERConfig & FLASH_OPTR_IWDG_STDBY)) {
        return HAL_OK;  // 无需修改直接返回
    }
    
    // 3. 准备新配置
    ob_init.OptionType   = OPTIONBYTE_USER;
    ob_init.USERType     = OB_USER_IWDG_STDBY;
    ob_init.USERConfig   = OB_IWDG_STDBY_FREEZE;
    ob_init.RDPLevel     = OB_RDP_LEVEL0;
    
    // 4. 执行FLASH操作序列
    do {
        // 4.1 解锁FLASH
        status = flash_op_with_timeout(HAL_FLASH_Unlock);
        CHECK_AND_RESET(HAL_FLASH_Unlock, status);
        
        // 4.2 解锁OB
        status = flash_op_with_timeout(HAL_FLASH_OB_Unlock);
        CHECK_AND_RESET(HAL_FLASH_OB_Unlock, status);
        
        // 4.3 编程OB
        status = flash_op_with_timeout_and_para(HAL_FLASHEx_OBProgram, &ob_init);
        CHECK_AND_RESET(HAL_FLASHEx_OBProgram, status);
        
        // 4.4 启动OB重载
        status = flash_op_with_timeout(HAL_FLASH_OB_Launch);
        CHECK_AND_RESET(HAL_FLASH_OB_Launch, status);
        
    } while(0);
    
    // 5. 无论成功与否都尝试恢复锁定
    status = flash_recover_lock();
    
    return (status == HAL_OK) ? HAL_OK : HAL_ERROR;
}