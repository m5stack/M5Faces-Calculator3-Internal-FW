#include "pt_task.h"
#include "i2c.h"
#include "gpio.h"
#include "iwdg.h"
#include "power.h"
#include "flash.h"

struct pt main_pt;
struct pt jump_to_bootloader_setter_pt;
struct pt i2c_addr_config_setter_pt;
struct pt i2c_slave_timeout_handle_pt;
struct pt keyboard_task_handle_pt;
struct pt stm_feed_wdt_handle_pt;
struct pt flash_config_setter_pt;

#define LONG_PRESS_THRESHOLD  400
// 按键状态全局变量
volatile uint8_t tx_key_value = 0;
const uint8_t keymap[5][4] = {
    {'A', 'M', '%', '/'}, {'7', '8', '9', '*'}, 
    {'4', '5', '6', '-'}, {'1', '2', '3', '+'}, {'.', '0', '`', '='}
}; 

void init_pt_task(void)
{
  PT_INIT(&main_pt); 
  PT_INIT(&jump_to_bootloader_setter_pt);
  PT_INIT(&i2c_addr_config_setter_pt);
  PT_INIT(&i2c_slave_timeout_handle_pt);   
  PT_INIT(&flash_config_setter_pt);  
  PT_INIT(&keyboard_task_handle_pt);   
  PT_INIT(&stm_feed_wdt_handle_pt);   
}

PT_THREAD(flash_config_setter(struct pt *pt)) {
    PT_BEGIN(pt);
    while(1) {
        PT_WAIT_UNTIL(pt, g_events.flash_config_update == 1);  // 等待设置触发信号
        g_events.flash_config_update = 0; //清除标志
        
        __disable_irq();  // 屏蔽所有中断

        flash_data_write_back();
        
        __enable_irq();  // 打开所有中断

        PT_YIELD(pt);  // 主动让出CPU
    }
    PT_END(pt);
}

PT_THREAD(i2c_addr_config_setter(struct pt *pt)) {
    PT_BEGIN(pt);
    while(1) {
        PT_WAIT_UNTIL(pt, g_events.i2c_addr_config_update == 1);  // 等待设置触发信号
        g_events.i2c_addr_config_update = 0; //清除标志
        
        __disable_irq();  // 屏蔽所有中断

        user_i2c_init();
        i2c2_it_enable();
        
        __enable_irq();  // 打开所有中断

        PT_YIELD(pt);  // 主动让出CPU
    }
    PT_END(pt);
}

static PT_THREAD(jump_to_bootloader_setter(struct pt *pt)) {
    PT_BEGIN(pt);
    while(1) {
        PT_WAIT_UNTIL(pt, g_events.jump_to_bootloader == 1);  // 等待设置触发信号
        g_events.jump_to_bootloader = 0; //清除标志
        
        Jump_Bootloader();

        PT_YIELD(pt);  // 主动让出CPU
    }
    PT_END(pt);
}

static PT_THREAD(stm_feed_wdt_handle(struct pt *pt))
{
    PT_BEGIN(pt);

    while (1) {
        PT_TIMER_DELAY(pt, PT_MILLIS_SECOND(10));
        HAL_IWDG_Refresh(&hiwdg);
    }

    PT_END(pt);
}

static PT_THREAD(keyboard_task_handle(struct pt *pt)) {
    static uint8_t current_row = 0;
    static uint8_t col_state[5] = {0};
    static uint8_t is_long_pressed[5] = {0};   
    static uint32_t long_press_timer[5] = {0};
    static volatile uint8_t temp[5] = {0};

    PT_BEGIN(pt);
    while(1) {
        // 扫描当前行（优化为循环切换）
        set_scan_row(current_row);  // 拉低当前行，其他行置高
        PT_TIMER_DELAY(pt, 1);

        // 读取列状态
        col_state[current_row] = read_columns();
        if (col_state[current_row] != 0) {    // 检测到按键
            PT_TIMER_DELAY(pt, 1);
            if (col_state[current_row] != 0) {
                if (!is_long_pressed[current_row]) {
                    long_press_timer[current_row] = HAL_GetTick();
                    is_long_pressed[current_row] = 1;
                }
                temp[current_row] = keymap[current_row][col_state[current_row]-1];
                if (is_long_pressed[current_row]) {
                    if (HAL_GetTick() - long_press_timer[current_row] > LONG_PRESS_THRESHOLD) {
                        if (temp[current_row] == 'A') temp[current_row] = 0x08;
                        if (temp[current_row] == '=') temp[current_row] = 0x0D;
                    }
                }
            } else {
                is_long_pressed[current_row] = 0;
                temp[current_row] = 0;
            }
        } else {
            if (temp[current_row]) {
                tx_key_value = temp[current_row];
                tx_buffer[0] = tx_key_value;
                tx_buffer[1] = tx_key_value;
                ubReceiveIndex = 0;
                HAL_GPIO_WritePin(G32_INT_GPIO_Port, G32_INT_Pin, GPIO_PIN_RESET);
            }
            is_long_pressed[current_row] = 0;
            temp[current_row] = 0;
        }

        current_row = (current_row + 1) % 5;  // 循环扫描5行
        PT_YIELD(pt);  // 主动让出CPU
    }
    PT_END(pt);
}

static PT_THREAD(i2c_slave_timeout_handle(struct pt *pt)) {
    static uint32_t hal_get_tick_temp = 0;

    PT_BEGIN(pt);
    while(1) {
        if (i2c_slave_state != I2C_SLAVE_STATE_IDLE) {
        	hal_get_tick_temp = HAL_GetTick();
            if ((hal_get_tick_temp > i2c_ex_timeout_start) && (hal_get_tick_temp - i2c_ex_timeout_start > I2C_TIMEOUT_MS)) {
                i2c2_reset();
                i2c_slave_state = I2C_SLAVE_STATE_IDLE;
            }
        }
        PT_YIELD(pt);  // 主动让出CPU
    }
    PT_END(pt);
}

void main_scheduler(struct pt *pt) 
{
    PT_BEGIN(pt);
    while(1) {
        // 按优先级顺序调度   
        stm_feed_wdt_handle(&stm_feed_wdt_handle_pt);
        keyboard_task_handle(&keyboard_task_handle_pt);
        i2c_addr_config_setter(&i2c_addr_config_setter_pt);
        flash_config_setter(&flash_config_setter_pt);
        i2c_slave_timeout_handle(&i2c_slave_timeout_handle_pt);    
        jump_to_bootloader_setter(&jump_to_bootloader_setter_pt);    
    }
    PT_END(pt);
}
