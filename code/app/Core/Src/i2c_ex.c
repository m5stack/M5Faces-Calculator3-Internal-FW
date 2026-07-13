/* Includes ------------------------------------------------------------------*/
#include "i2c_ex.h"
#include <stdio.h>
#include <string.h>
#include "i2c.h"
#include "main.h"

#define UNUSED(X) (void)X      /* To avoid gcc/g++ warnings */

__IO uint8_t aReceiveBuffer[I2C_RECEIVE_BUFFER_LEN];
__IO uint8_t tx_buffer[I2C_RECEIVE_BUFFER_LEN];
__IO uint16_t ubReceiveIndex = 0;
volatile uint8_t i2c_addr = 0;
volatile uint16_t tx_buffer_index = 0;
volatile uint16_t tx_len = 0;
volatile uint8_t i2c_slave_state = I2C_SLAVE_STATE_IDLE;
volatile uint32_t i2c_ex_timeout_start = 0;

void set_i2c_slave_address(uint8_t addr)
{
  i2c_addr = (addr << 1);
}

__weak void Slave_Complete_Callback(uint8_t *rx_data, uint16_t len)
{
	/* Prevent unused argument(s) compilation warning */
	UNUSED(rx_data);
	UNUSED(len);  
}

void i2c2_it_enable(void)
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

void i2c2_tx_rx_it_enable(void)
{
	LL_I2C_EnableIT_TX(I2C2);
	LL_I2C_EnableIT_RX(I2C2);  
}

void i2c2_it_disable(void)
{
  LL_I2C_DisableIT_ADDR(I2C2);
	LL_I2C_DisableIT_TX(I2C2);
	LL_I2C_DisableIT_RX(I2C2);
  LL_I2C_DisableIT_NACK(I2C2);
  LL_I2C_DisableIT_ERR(I2C2);
  LL_I2C_DisableIT_STOP(I2C2);
}

void i2c2_reset(void)
{
  LL_I2C_DeInit(I2C2);
  LL_I2C_Disable(I2C2);
  i2c2_it_disable();
  user_i2c_init();    
  i2c2_it_enable();  
}

void i2c2_set_send_data(uint8_t *tx_ptr, uint16_t len) {
  if (len > I2C_RECEIVE_BUFFER_LEN) {
    len = I2C_RECEIVE_BUFFER_LEN;
	}

  if (len == 0 || tx_ptr == NULL) {
    return;
  }
  memcpy((void *)tx_buffer, tx_ptr, len);
  tx_buffer_index = 0;
  tx_len = len;
}

void Slave_Reception_Callback(void)
{
  if (ubReceiveIndex >= I2C_RECEIVE_BUFFER_LEN) {
    ubReceiveIndex = 0;
  }  
  /* Read character in Receive Data register.
  RXNE flag is cleared by reading data in RXDR register */
  aReceiveBuffer[ubReceiveIndex] = LL_I2C_ReceiveData8(I2C2);
  ubReceiveIndex++;
}

void Slave_Ready_To_Transmit_Callback(void)
{
  if (tx_buffer_index >= I2C_RECEIVE_BUFFER_LEN) {
    tx_buffer_index = 0;
  }  
  /* Send the Byte requested by the Master */
  LL_I2C_TransmitData8(I2C2, tx_buffer[tx_buffer_index]);
  tx_buffer_index++;
}

void I2C2_IRQHandler(void)
{
  /* USER CODE BEGIN I2C2_IRQn 0 */
  __disable_irq();
  /* Check ADDR flag value in ISR register */
  if(LL_I2C_IsActiveFlag_ADDR(I2C2))
  {
    /* Verify the Address Match with the OWN Slave address */
    if(LL_I2C_GetAddressMatchCode(I2C2) == i2c_addr)
    {
      i2c_ex_timeout_start = HAL_GetTick();
      /* Verify the transfer direction, a write direction, Slave enters receiver mode */
      if(LL_I2C_GetTransferDirection(I2C2) == LL_I2C_DIRECTION_WRITE)
      {
        ubReceiveIndex = 0;
        /* Clear ADDR flag value in ISR register */
        LL_I2C_ClearFlag_ADDR(I2C2);

        /* Enable Receive Interrupt */
        LL_I2C_EnableIT_RX(I2C2);

        i2c_slave_state = I2C_SLAVE_STATE_READ;
      }
      /* Verify the transfer direction, a read direction, Slave enters transmitter mode */
      else if(LL_I2C_GetTransferDirection(I2C2) == LL_I2C_DIRECTION_READ)
      {
        tx_buffer_index = 0;
        /* Clear ADDR flag value in ISR register */
        LL_I2C_ClearFlag_ADDR(I2C2);

        // 准备I2C数据
        if (ubReceiveIndex) {
          Slave_Complete_Callback((uint8_t *)aReceiveBuffer, ubReceiveIndex);
          ubReceiveIndex = 0; 
        }        

        /* Enable Transmit Interrupt */
        LL_I2C_EnableIT_TX(I2C2);

        i2c_slave_state = I2C_SLAVE_STATE_WRITE;
      }      
      else
      {
        /* Clear ADDR flag value in ISR register */
        LL_I2C_ClearFlag_ADDR(I2C2);
      }
    }
    else
    {
      /* Clear ADDR flag value in ISR register */
      LL_I2C_ClearFlag_ADDR(I2C2);
    }
  }
  /* Check NACK flag value in ISR register */
  else if(LL_I2C_IsActiveFlag_NACK(I2C2))
  {
    /* End of Transfer */
    LL_I2C_ClearFlag_NACK(I2C2);
    tx_buffer[0] = 0;
    HAL_GPIO_WritePin(G32_INT_GPIO_Port, G32_INT_Pin, GPIO_PIN_SET);
    i2c_slave_state = I2C_SLAVE_STATE_IDLE;
  } 
  /* Check TXIS flag value in ISR register */
  else if(LL_I2C_IsActiveFlag_TXIS(I2C2))
  {
    /* Call function Slave Ready to Transmit Callback */
    Slave_Ready_To_Transmit_Callback();
  }   
  /* Check RXNE flag value in ISR register */
  else if(LL_I2C_IsActiveFlag_RXNE(I2C2))
  {
    /* Call function Slave Reception Callback */
    Slave_Reception_Callback();
  }
  /* Check STOP flag value in ISR register */
  else if(LL_I2C_IsActiveFlag_STOP(I2C2))
  {
    /* End of Transfer */
    LL_I2C_ClearFlag_STOP(I2C2);

    /* Check TXE flag value in ISR register */
    if(!LL_I2C_IsActiveFlag_TXE(I2C2))
    {
      /* Flush the TXDR register */
      LL_I2C_ClearFlag_TXE(I2C2);
    }
    
    HAL_GPIO_WritePin(G32_INT_GPIO_Port, G32_INT_Pin, GPIO_PIN_SET);

    /* Call function Slave Complete Callback */
    if (ubReceiveIndex) {
      Slave_Complete_Callback((uint8_t *)aReceiveBuffer, ubReceiveIndex);
    }
    ubReceiveIndex = 0;
    i2c_slave_state = I2C_SLAVE_STATE_IDLE;
  }
  /* Check TXE flag value in ISR register */
  else if(!LL_I2C_IsActiveFlag_TXE(I2C2))
  {
    /* Flush the TXDR register */
    LL_I2C_ClearFlag_TXE(I2C2);    
    /* Do nothing */
    /* This Flag will be set by hardware when the TXDR register is empty */
    /* If needed, use LL_I2C_ClearFlag_TXE() interface to flush the TXDR register  */
  }  
  /* USER CODE END I2C2_IRQn 0 */
  if (LL_I2C_IsActiveFlag_BERR(I2C2)) {
    LL_I2C_ClearFlag_BERR(I2C2);
    i2c2_reset();
    i2c_slave_state = I2C_SLAVE_STATE_IDLE;
  }
  if (LL_I2C_IsActiveFlag_ARLO(I2C2)) {
    LL_I2C_ClearFlag_ARLO(I2C2);
    i2c2_reset();
    i2c_slave_state = I2C_SLAVE_STATE_IDLE;
  }
  /* USER CODE BEGIN I2C2_IRQn 1 */
  __enable_irq();
  /* USER CODE END I2C2_IRQn 1 */
}
