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
#include "crc.h"
#include "i2c.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32g0xx_hal_flash_ex.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {NOEVENT = 0, //not enevt happen
              EVENT_OPCOD_NOTYET_READ = 1,//operation code not been read
              EVENT_OPCOD_READ =2,//operation code has been readed
              EVENT_OPCOD_SEND =3,//Feedback the status of MCU 
              EVENT_OPCOD_BUSY_RECEIVE =4//I2C is in Busy of receive status
             // EVENT_OPCOD_BUSY_SEND =5 //i2C is busy of send
}EventStatus;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
typedef  void (*pFunction)(void);
/*this address is define */
#define APPLICATION_ADDRESS  0x08002800
#define FW_LENGTH (((uint32_t)0xF800) - ((uint32_t)0x2800))
#define FW_CRC_ADDR (((uint32_t)0x0800F800) - 4) 

/* Error codes used to make the red led blinking */
#define ERROR_ERASE 0x01
#define ERROR_PROG  0x02
#define ERROR_HALF_PROG 0x04
#define ERROR_PROG_FLAG 0x08
#define ERROR_WRITE_PROTECTION 0x10
#define ERROR_FETCH_DURING_ERASE 0x20
#define ERROR_FETCH_DURING_PROG 0x40
#define ERROR_SIZE 0x80
#define ERROR_ALIGNMENT 0x100
#define ERROR_NOT_ZERO 0x200
#define ERROR_UNKNOWN 0x400
#define ERROR_I2C       0x01
#define ERROR_HSI_TIMEOUT 0x55
#define ERROR_PLL_TIMEOUT 0xAA
#define ERROR_CLKSWITCH_TIMEOUT 0xBB

#define DELAY_TIME 500
#define LED_FLASH_TIME 200

#define  OPC_WREN       (uint8_t)(0x06)
#define  OPC_USRCD      (uint8_t)(0x77)

#define I2C_MAX_NUMS 2056

#define STM32G0xx_PAGE_SIZE 0x800
#define STM32G0xx_FLASH_PAGE0_STARTADDR 0x8000000
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint32_t lastTime = 0;

volatile uint16_t I2C_Receive_Counter=0;
volatile uint8_t Receive_Buffer[I2C_MAX_NUMS] = {0};
volatile EventStatus i2c_event= NOEVENT;
volatile uint16_t error = 0;
volatile uint32_t NbrOfPage = 0x00;
volatile uint32_t EraseCounter = 0x00, Address = 0x00;
volatile uint32_t JumpAddress = 0;
volatile pFunction JumpToApplication;
volatile uint8_t opcode = 0;

volatile uint8_t gpio_init_flag = 0;
volatile uint32_t bootloader_led_delay = 0;
volatile uint8_t bootloader_flash_led_flag = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static uint8_t iwdg_is_enabled(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void i2c2_it_disable(void)
{
  LL_I2C_DisableIT_ADDR(I2C2);
	LL_I2C_DisableIT_TX(I2C2);
	LL_I2C_DisableIT_RX(I2C2);
  LL_I2C_DisableIT_NACK(I2C2);
  LL_I2C_DisableIT_ERR(I2C2);
  LL_I2C_DisableIT_STOP(I2C2);
}

static void i2c2_it_enable(void)
{
  LL_I2C_Enable(I2C2);

  /* (6) Enable I2C2 address match/error interrupts:
   *  - Enable Address Match Interrupt
   *  - Enable Not acknowledge received interrupt
   *  - Enable Error interrupts
   *  - Enable Stop interrupt
   */
  LL_I2C_EnableIT_ADDR(I2C2);  
  LL_I2C_EnableIT_NACK(I2C2);
  LL_I2C_EnableIT_ERR(I2C2);
  LL_I2C_EnableIT_STOP(I2C2);
}

static void i2c2_reset(void)
{
  LL_I2C_DeInit(I2C2);
  LL_I2C_Disable(I2C2);
  i2c2_it_disable();
  MX_I2C2_Init();
  i2c2_it_enable();  
}

uint8_t compute_fw_crc32(void)
{
    uint32_t crcsum, crc_read, len;
    uint8_t *pdata = (uint8_t*)APPLICATION_ADDRESS;

    len = FW_LENGTH - 4;
    crc_read = *(uint32_t*)FW_CRC_ADDR;

    crcsum = HAL_CRC_Calculate(&hcrc, (uint32_t *)pdata, len)^0xffffffff;

    if (crc_read == crcsum)
        return 1;
    else
        return 0;
}

/**
  * @brief  Gets the page of a given address
  * @param  Addr: Address of the FLASH Memory
  * @retval The page of a given address
  */
static uint32_t GetPage(uint32_t Addr)
{
  return (Addr - STM32G0xx_FLASH_PAGE0_STARTADDR) / FLASH_PAGE_SIZE;
}

/**
  * Brief   This function handles I2C2 interrupt request.
  * Param   None
  * Retval  I2C2 always as slave of i2c conmunication
  */
void I2C2_IRQHandler(void)
{
	uint32_t I2C_InterruptStatus = I2C2->ISR; /* Get interrupt status */
  
  if((I2C_InterruptStatus & I2C_ISR_ADDR) == I2C_ISR_ADDR) /* Check address match */
  {
    I2C2->ICR |= I2C_ICR_ADDRCF;                        /* Clear address match flag*/
    
    if((I2C2->ISR & I2C_ISR_DIR) == I2C_ISR_DIR) /* Check if transfer direction is read (slave transmitter) */
    {
      I2C2->CR1 |= I2C_CR1_TXIE;        /* Set transmit IT /status*/
      i2c_event=EVENT_OPCOD_SEND;              /* Set I2C  entor transmit mode*/
      
    }
    else   /*Write operation, slave receive status*/
    {
      I2C2->CR1 |= I2C_CR1_RXIE; /* Set receive IT /status*/
      i2c_event=EVENT_OPCOD_BUSY_RECEIVE; /* Set I2C  entor receive mode*/
    }
    
    LL_I2C_EnableIT_ERR(I2C2);//Enable ERR interrupt
    I2C2->CR1 |=I2C_CR1_STOPIE;//Enable STOP interrupt
   
  }
  else if((I2C_InterruptStatus & I2C_ISR_TXIS) == I2C_ISR_TXIS)
  {
    LL_I2C_TransmitData8(I2C2, 1);
    //add some application code in this place   
  }
      /*check RXDR is not empty*/
  else if((I2C_InterruptStatus & I2C_ISR_RXNE) == I2C_ISR_RXNE)
  {
    if (I2C_Receive_Counter >= I2C_MAX_NUMS) {
      I2C_Receive_Counter = 0;
    }    
    //I2C_ISR_RXNE add you code in this place Tomas Li add
    Receive_Buffer[I2C_Receive_Counter]= I2C2->RXDR;
    I2C_Receive_Counter++;
    i2c_event=EVENT_OPCOD_BUSY_RECEIVE;//slave is busy for receive data

  }
  else if(LL_I2C_IsActiveFlag_NACK(I2C2))
  {
    /* End of Transfer */
    LL_I2C_ClearFlag_NACK(I2C2);
  } 
  /*check Stop event happen */
  if((I2C_InterruptStatus & I2C_ISR_STOPF) == I2C_ISR_STOPF)
  {
    
      I2C2->ICR |=I2C_ICR_STOPCF;//clear the STOP interrupt Flag
      
#if 1
    switch(i2c_event){
    case EVENT_OPCOD_BUSY_RECEIVE://slave receive status Stop flag
      I2C_Receive_Counter=0;
      i2c_event = EVENT_OPCOD_NOTYET_READ;
  
      I2C2->CR1 &= ~(I2C_CR1_STOPIE); //Disable all interrupt.except Error interrupt
      I2C2->CR2 |= I2C_CR2_NACK;//set feedback Nack in next event
      I2C2->CR1 |= I2C_CR1_ADDRIE;      
        break;
    case EVENT_OPCOD_SEND: //slave send stop
      I2C2->ICR |=I2C_ICR_STOPCF | I2C_ICR_NACKCF |I2C_ICR_BERRCF;//clear the STOP interrupt Flag
      I2C2->CR1 |=I2C_CR1_ADDRIE;
      i2c_event = NOEVENT;
      
        break;
    default:
      
        break;  
    
    }
#endif
    
  } 
  else
  {
    error = ERROR_I2C; /* Report an error */
     
  }
  if (LL_I2C_IsActiveFlag_BERR(I2C2)) {
    LL_I2C_ClearFlag_BERR(I2C2);
    i2c2_reset();
  }
  if (LL_I2C_IsActiveFlag_ARLO(I2C2)) {
    LL_I2C_ClearFlag_ARLO(I2C2);
    i2c2_reset();
  }   
}

uint8_t Reset_AllPeriph(void)
{
  uint8_t status = HAL_ERROR;

  // disable i2c
  LL_I2C_DeInit(I2C2);
  LL_I2C_DisableAutoEndMode(I2C2);
  LL_I2C_Disable(I2C2);
  LL_I2C_DisableIT_ADDR(I2C2);

  SysTick->CTRL=0;
  SYSCFG->CFGR1 &= SYSCFG_CFGR1_MEM_MODE;
  NVIC->ICER[0] = 0xFFFFFFFF;
  NVIC->ICPR[0] = 0xFFFFFFFF;
  /* Set EXTICRx registers to reset value */
  EXTI->EXTICR[0] = 0;
  EXTI->EXTICR[1] = 0;
  EXTI->EXTICR[2] = 0;
  EXTI->EXTICR[3] = 0;
  /* Set CFGR2 register to reset value: clear SRAM parity error flag */
  SYSCFG->CFGR2 |= (uint32_t) SYSCFG_CFGR2_SRAM_PE; 
  // set clock to default
  status = HAL_RCC_DeInit(); 
  __HAL_SYSCFG_CLEAR_FLAG();
	status |= HAL_CRC_DeInit(&hcrc);
	// HAL_DeInit();
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;  

  return status;
}

void Jump_APP(void)
{
  uint32_t jump_app_timeout = 0;

  if (compute_fw_crc32()) {
    /*check the application address context whether avilible*/
    if (((*(__IO uint32_t*)APPLICATION_ADDRESS) & 0x2FFE0000 ) == 0x20000000)
    {
      while (Reset_AllPeriph() != HAL_OK)
      {
        jump_app_timeout++;
        if (jump_app_timeout > 20) {
          jump_app_timeout = 0;
          HAL_NVIC_SystemReset();
          HAL_Delay(1000);
        }
      }
      
      SCB->VTOR = APPLICATION_ADDRESS;
      /* Jump to user application */
      JumpAddress = *(__IO uint32_t*) (APPLICATION_ADDRESS +4);
      JumpToApplication = (pFunction) JumpAddress;
      /* Initialize user application's Stack Pointer */
      __set_MSP(*(__IO uint32_t*) APPLICATION_ADDRESS);
      JumpToApplication();
    }
  }
  else {
    NVIC_SystemReset();
  }
}

void Write_Code(void)
{
    uint16_t Number_Bytes_Transferred = 0;
    uint32_t Add_Flash, end_add_flash;
    uint64_t Data = 0;
    uint16_t Data_index = 8;    
    uint32_t PageError = 0;                    //设置PageError,如果出现错误这个变量会被设置为出错的FLASH地址
    FLASH_EraseInitTypeDef My_Flash;
    uint32_t PageNum = 0;
    uint32_t opt_timeout = 0;

    Add_Flash = Receive_Buffer[1]<<24|              
                Receive_Buffer[2]<<16|
                Receive_Buffer[3]<<8|
                Receive_Buffer[4]<<0;
    end_add_flash = Add_Flash + 2048;

    // return if address illegal
    if (Add_Flash < APPLICATION_ADDRESS) {
      return;
    }    
  
    Number_Bytes_Transferred=(Receive_Buffer[5]<<8)+ Receive_Buffer[6];
    
    if(Number_Bytes_Transferred > 0)
    {
      if(__HAL_FLASH_GET_FLAG(FLASH_FLAG_CFGBSY) != 0x00U) {
        *(uint32_t *)(Add_Flash + 600) = 12323;
        FLASH->SR = FLASH_SR_CLEAR;
      }
step_unlock:
      if (iwdg_is_enabled())
        IWDG->KR = 0xAAAA;  // 喂狗
      if(HAL_FLASH_Unlock() != HAL_OK) {
        opt_timeout++;
        if (opt_timeout > 100) {
          HAL_FLASH_Lock();
          HAL_NVIC_SystemReset();
        }        
        goto step_unlock;
      }

      PageNum = GetPage(Add_Flash);

      My_Flash.TypeErase = FLASH_TYPEERASE_PAGES;  //标明Flash执行页面只做擦除操作
      My_Flash.Page        = PageNum;
      My_Flash.NbPages = 1;                        //说明要擦除的页数，此参数必须是Min_Data = 1和Max_Data =(�????大页�????-初始页的�????)之间的�??              

      opt_timeout = 0;
step_erase:      
      if (iwdg_is_enabled())
        IWDG->KR = 0xAAAA;  // 喂狗
      FLASH_WaitForLastOperation(50);
      if (HAL_FLASHEx_Erase(&My_Flash, &PageError) != HAL_OK) {
        opt_timeout++;
        if (opt_timeout > 100) {
          HAL_FLASH_Lock();
          HAL_NVIC_SystemReset();
        }        
        goto step_erase;
      }  //调用擦除函数擦除 

      while (Add_Flash < end_add_flash) {
        Data = Receive_Buffer[Data_index] | ((uint64_t)Receive_Buffer[Data_index+1] << 8) \
        | ((uint64_t)Receive_Buffer[Data_index+2] << 16) | ((uint64_t)Receive_Buffer[Data_index+3] << 24) \
        | ((uint64_t)Receive_Buffer[Data_index+4] << 32) | ((uint64_t)Receive_Buffer[Data_index+5] << 40) \
        | ((uint64_t)Receive_Buffer[Data_index+6] << 48) | ((uint64_t)Receive_Buffer[Data_index+7] << 56);	

        opt_timeout = 0;
step_write:   
        if (iwdg_is_enabled())
          IWDG->KR = 0xAAAA;  // 喂狗    
        FLASH_WaitForLastOperation(50);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, Add_Flash, Data) != HAL_OK)
        {
          opt_timeout++;
          if (opt_timeout > 100) {
            HAL_FLASH_Lock();
            HAL_NVIC_SystemReset();
          }           
          goto step_write;
        }			
        Add_Flash = Add_Flash + 8;
        Data_index = Data_index + 8;        
      }
      if (HAL_FLASH_Lock() != HAL_OK) {
        HAL_NVIC_SystemReset();
      }
      FLASH_WaitForLastOperation(50);          
    }
    for (int i = 0; i < sizeof(Receive_Buffer); i++) {
      Receive_Buffer[i] = 0;
    } 
}

void iap_i2c(void)
{

  /*this is a endless loop for process the data from Host side*/
  while(1)
  {
    //Tomas_Li_Test();//Just for Test
    if (iwdg_is_enabled())
      IWDG->KR = 0xAAAA;  // 喂狗      
    if (HAL_GetTick() > lastTime) {
      Jump_APP();
    }

    if (i2c_event == EVENT_OPCOD_NOTYET_READ)
    {
      NVIC_DisableIRQ(I2C2_IRQn);
      i2c_event=NOEVENT;//changed the status
      /* Read opcode */
     switch (Receive_Buffer[0])
     {
         case OPC_WREN:
           lastTime = HAL_GetTick() + DELAY_TIME*1000;
           Write_Code();
           break;

         case OPC_USRCD:
       	   Jump_APP();
           break;

         default:
         break;
     }
      NVIC_EnableIRQ(I2C2_IRQn);            
      I2C2->CR1 |=I2C_CR1_ADDRIE;//Open address and Stop interrupt
      LL_I2C_Enable(I2C2);
      LL_I2C_EnableIT_ADDR(I2C2);      
    }
  }
}

static uint8_t iwdg_is_enabled(void) 
{
    // 检查状态寄存器是否配置完成
    if ((IWDG->SR & 0x03) != 0) {  // PVU或RVU为1，配置未完成
        return 0;
    }
    
    // 检查预分频和重装载值是否非默认值
    if ((IWDG->PR == 0) && (IWDG->RLR == 0xFFF)) {
        return 0;  // 未配置过IWDG
    }
    return 1;
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
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_CRC_Init();
  /* USER CODE BEGIN 2 */
  if (iwdg_is_enabled())
    IWDG->KR = 0xAAAA;  // 喂狗    
  __enable_irq();
  lastTime = HAL_GetTick() + DELAY_TIME;
  MX_I2C2_Init();
  LL_I2C_Enable(I2C2);
  LL_I2C_EnableIT_ADDR(I2C2);
  iap_i2c();    
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
