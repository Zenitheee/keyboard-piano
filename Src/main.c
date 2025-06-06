/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body - Keyboard Piano Implementation
  ******************************************************************************
  *
  * COPYRIGHT(c) 2015 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "gpio.h"
#include "i2c.h"

/* USER CODE BEGIN Includes */
#include <string.h>
#include "zlg7290.h"
#include "iwdg.h"
#include "state_machine.h"
#include "data_validator.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

// 定义9个音阶的频率 (Hz) - 对应键盘1-9
typedef struct {
    uint16_t frequency;
} Note_t;

// 标准音阶频率定义 (C4-C5音阶) - 基于A4=440Hz的十二平均律
const Note_t notes[9] = {
    {262},  // C4 - 键盘1 (Do)
    {294},  // D4 - 键盘2 (Re)  
    {330},  // E4 - 键盘3 (Mi)
    {349},  // F4 - 键盘4 (Fa)
    {392},  // G4 - 键盘5 (Sol)
    {440},  // A4 - 键盘6 (La)
    {494},  // B4 - 键盘7 (Si)
    {523},  // C5 - 键盘8 (Do)
    {587}   // D5 - 键盘9 (Re)
};

// 七段数码管编码表 (0-9)
const uint8_t seg7code[10] = {
    0xFC,  // 0
    0x0C,  // 1
    0xDA,  // 2
    0xF2,  // 3
    0x66,  // 4
    0xB6,  // 5
    0xBE,  // 6
    0xE0,  // 7
    0xFE,  // 8
    0xE6   // 9
};

// 显示缓冲区 - 8位数码管显示
uint8_t display_buffer[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t display_position = 0;  // 当前显示位置 (0-7)

// 热启动状态保存结构
typedef struct {
    uint32_t magic_number;      // 魔数，用于验证数据有效性
    uint8_t current_note;       // 当前播放的音符
    uint8_t is_playing;         // 是否正在播放
    uint32_t play_duration;     // 播放持续时间
    uint8_t display_buffer[8];  // 显示缓冲区状态
    uint8_t display_position;   // 显示位置状态
    uint32_t checksum;          // 校验和
} HotStartState_t;

#define MAGIC_NUMBER 0xDEADBEEF
#define BACKUP_SRAM_BASE 0x40024000  // STM32F4的备份SRAM地址

// 全局变量
volatile uint8_t current_key = 0xFF;  // 当前按键，0xFF表示无按键
volatile uint8_t key_pressed = 0;
volatile uint32_t note_timer = 0;
volatile uint8_t is_playing = 0;

// ZLG7290键盘相关变量
volatile uint8_t key_flag = 0;  // 按键中断标志
uint8_t key_buffer[1] = {0};    // 按键值缓冲区

// I2C通信错误处理相关变量
static uint32_t i2c_error_count = 0;           // I2C错误计数
static uint32_t last_i2c_error_time = 0;       // 最后I2C错误时间
static uint8_t i2c_communication_ok = 1;       // I2C通信状态标志

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/
void Play_Note(uint8_t note_index);
void Stop_Note(void);
void Save_HotStart_State(void);
uint8_t Restore_HotStart_State(void);
uint32_t Calculate_Checksum(HotStartState_t* state);
void Init_Backup_SRAM(void);
uint32_t Get_Microseconds(void);
void Display_Add_Digit(uint8_t digit);
void Display_Update(void);
void Display_Clear(void);
void Display_Init(void);
void Handle_I2C_Error(I2C_Status_t status);
void Check_I2C_Health(void);
void IWDG_SystemInit(void);
void IWDG_SystemTask(void);
void ScrambledExecution_PrintStats(void);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_IWDG_Init();

  /* USER CODE BEGIN 2 */
  
  // 初始化看门狗系统
  IWDG_SystemInit();
  
  // 初始化备份SRAM
  Init_Backup_SRAM();
  
  // 初始化I2C错误统计
  I2C_Init_ErrorStats();
  
  // 初始化显示
  Display_Init();
  
  // 初始化时间戳
  note_timer = HAL_GetTick();
  
  // 尝试恢复热启动状态
  uint8_t is_hot_start = 0;
  
  // 先验证备份SRAM中的数据
  if (Validate_HotStart_Data((void*)BACKUP_SRAM_BASE) != DATA_VALID) {
      DATA_VALIDATION_LOG("Hot start data invalid at startup, attempting repair\n");
      Repair_HotStartData(); // 尝试修复
  }
  
  if(Restore_HotStart_State()) {
      // 热启动成功，继续之前的状态
      is_hot_start = 1;
      // 确保数据有效后再更新显示
      if (Validate_DisplayBuffer(display_buffer, 8) == DATA_VALID) {
          Display_Update(); // 恢复显示状态
      } else {
          Repair_DisplayBuffer(display_buffer, 8);
          Display_Update();
      }
  }
  
  // 初始化状态机
  StateMachine_Init();
  
  // 设置热启动标志
  if(is_hot_start) {
      StateMachine_SetHotStartFlag(1);
  }
  
  // 启动测试 - 只在冷启动时执行蜂鸣，热启动时跳过
  // if(!is_hot_start) {
  //     // 冷启动 - 执行启动测试蜂鸣
  //     for(int i = 0; i < 1000; i++) {
  //         HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET);
  //         for(volatile int j = 0; j < 500; j++) __NOP();
  //         HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET);
  //         for(volatile int j = 0; j < 500; j++) __NOP();
  //     }
  // }
  // 热启动时完全跳过蜂鸣，用户无感知

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
  /* USER CODE END WHILE */
			
  /* USER CODE BEGIN 3 */		
    // 运行状态机
    StateMachine_Run();
    
    // 看门狗任务 - 自动喂狗和状态监控
    IWDG_SystemTask();
    
    // 进入CPU休眠模式，等待中断唤醒
    // 使用Sleep模式：CPU停止，外设继续工作，任何中断都可以唤醒
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
    
    // CPU被中断唤醒后会继续执行到这里
    // 可能的唤醒源：
    // 1. SysTick中断（1ms定时器）
    // 2. GPIO中断（PD13按键中断）
    // 3. 其他使能的中断
  }
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  __PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

}

/* USER CODE BEGIN 4 */

/**
 * @brief 获取微秒级时间戳
 * @return 当前时间戳（微秒）
 */
uint32_t Get_Microseconds(void) {
    uint32_t ms = HAL_GetTick();
    uint32_t st = SysTick->VAL;
    uint32_t pending = (SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) ? 1 : 0;
    uint32_t us = ms * 1000;
    
    // 计算SysTick计数器对应的微秒数
    // SysTick从LOAD值递减到0，所以实际经过的时间是(LOAD - VAL)
    us += ((SysTick->LOAD - st) * 1000) / SysTick->LOAD;
    
    // 如果有pending的SysTick中断，说明ms可能还没更新
    if (pending) {
        us += 1000;
    }
    
    return us;
}

/**
 * @brief 播放指定音符
 * @param note_index: 音符索引 (0-8)
 */
void Play_Note(uint8_t note_index) {
    // 验证热启动数据的正确性
    if (Validate_HotStart_Data((void*)BACKUP_SRAM_BASE) != DATA_VALID) {
        // 热启动数据无效，尝试修复
        Repair_HotStartData();
        DATA_VALIDATION_LOG("HotStart data repaired before playing note\n");
    }
    
    // 验证音符索引的合法性
    note_index = Safe_Get_NoteIndex(note_index, 0);
    if(note_index >= 9) return;
    
    const Note_t* note = &notes[note_index];
    static uint32_t last_toggle_time = 0;
    static uint8_t pin_state = 0;
    static uint8_t last_note_index = 0xFF; // 记录上次播放的音符
    
    // 如果切换到新音符，重置状态
    if(last_note_index != note_index) {
        last_note_index = note_index;
        last_toggle_time = Get_Microseconds();
        pin_state = 0;
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET);
        return;
    }
    
    // 获取当前微秒时间戳
    uint32_t current_us = Get_Microseconds();
    
    // 计算半周期时间 (微秒)
    // 频率 = 1/周期，半周期 = 1000000/(2*频率) 微秒
    uint32_t half_period_us = 1000000 / (2 * note->frequency);
    
    // 处理时间戳溢出的情况
    uint32_t time_diff;
    if(current_us >= last_toggle_time) {
        time_diff = current_us - last_toggle_time;
    } else {
        // 时间戳溢出
        time_diff = (0xFFFFFFFF - last_toggle_time) + current_us + 1;
    }
    
    if(time_diff >= half_period_us) {
        pin_state = !pin_state;
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, pin_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
        last_toggle_time = current_us;
    }
}

/**
 * @brief 停止播放音符
 */
void Stop_Note(void) {
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET);
}

/**
 * @brief 初始化备份SRAM
 */
void Init_Backup_SRAM(void) {
    // 使能PWR时钟
    __HAL_RCC_PWR_CLK_ENABLE();
    
    // 使能备份域访问
    HAL_PWR_EnableBkUpAccess();
    
    // 使能备份SRAM时钟
    __HAL_RCC_BKPSRAM_CLK_ENABLE();
}

/**
 * @brief 计算校验和
 */
uint32_t Calculate_Checksum(HotStartState_t* state) {
    uint32_t checksum = 0;
    uint8_t* data = (uint8_t*)state;
    
    // 计算除checksum字段外的所有数据的校验和
    for(int i = 0; i < sizeof(HotStartState_t) - sizeof(uint32_t); i++) {
        checksum += data[i];
    }
    
    return checksum;
}

/**
 * @brief 保存热启动状态
 */
void Save_HotStart_State(void) {
    // 验证热启动数据结构是否已有效
    if (Validate_HotStart_Data((void*)BACKUP_SRAM_BASE) != DATA_VALID) {
        // 如果当前备份区域数据无效，记录日志
        DATA_VALIDATION_LOG("Current backup SRAM data invalid before saving\n");
    }
    
    HotStartState_t* backup_state = (HotStartState_t*)BACKUP_SRAM_BASE;
    
    // 准备状态数据
    HotStartState_t current_state;
    current_state.magic_number = MAGIC_NUMBER;
    current_state.current_note = current_key;
    current_state.is_playing = is_playing;
    
    // 安全获取时间差，避免HAL_GetTick()问题
    uint32_t current_tick = HAL_GetTick();
    if(current_tick >= note_timer) {
        current_state.play_duration = current_tick - note_timer;
    } else {
        current_state.play_duration = 0; // 时间溢出或未初始化
    }
    
    // 保存显示状态
    memcpy(current_state.display_buffer, display_buffer, 8);
    current_state.display_position = display_position;
    
    // 计算校验和
    current_state.checksum = Calculate_Checksum(&current_state);
    
    // 验证数据完整性，确保我们即将保存的数据是有效的
    if (Validate_HotStart_Data(&current_state) != DATA_VALID) {
        DATA_VALIDATION_LOG("Invalid hot start data prepared, not saving\n");
        return; // 不保存无效数据
    }
    
    // 写入备份SRAM
    memcpy(backup_state, &current_state, sizeof(HotStartState_t));
}

/**
 * @brief 恢复热启动状态
 * @return 1: 恢复成功, 0: 恢复失败
 */
uint8_t Restore_HotStart_State(void) {
    // 验证热启动数据的完整性 - 多次检验确保数据完整性
    if (Validate_HotStart_Data((void*)BACKUP_SRAM_BASE) != DATA_VALID) {
        DATA_VALIDATION_LOG("Hot start data validation failed during restore\n");
        
        // 尝试修复数据
        Repair_HotStartData();
        
        // 修复后再次验证
        if (Validate_HotStart_Data((void*)BACKUP_SRAM_BASE) != DATA_VALID) {
            DATA_VALIDATION_LOG("Hot start data still invalid after repair\n");
            return 0; // 修复失败，数据无效
        }
        
        DATA_VALIDATION_LOG("Hot start data repaired successfully\n");
    }
    
    HotStartState_t* backup_state = (HotStartState_t*)BACKUP_SRAM_BASE;
    
    // 提取安全的数据值
    uint8_t safe_current_key = Safe_Get_KeyValue(backup_state->current_note, 0xFF);
    uint8_t safe_is_playing = (backup_state->is_playing > 1) ? 0 : backup_state->is_playing;
    uint8_t safe_display_position = Safe_Get_DisplayPosition(backup_state->display_position, 0);
    
    // 恢复状态 - 确保无感知切换
    current_key = safe_current_key;
    is_playing = safe_is_playing;
    note_timer = HAL_GetTick(); // 重置计时器为当前时间，避免时间跳跃
    
    // 恢复显示状态
    memcpy(display_buffer, backup_state->display_buffer, 8);
    display_position = safe_display_position;
    
    // 验证并修复恢复的显示数据
    if (Validate_DisplayBuffer(display_buffer, 8) != DATA_VALID) {
        Repair_DisplayBuffer(display_buffer, 8);
        DATA_VALIDATION_LOG("Hot start display buffer repaired\n");
    }
    
    // 如果之前正在播放，继续播放（无缝恢复）
    if(is_playing && IS_VALID_NOTE_KEY(current_key)) {
        // 恢复播放状态，用户完全无感知
        key_pressed = 1;
        // 注意：不在这里调用Play_Note，让主循环自然接管
    }
    
    return 1; // 恢复成功
}

/**
 * @brief 初始化显示
 */
void Display_Init(void) {
    // 清空显示缓冲区
    for(uint8_t i = 0; i < 8; i++) {
        display_buffer[i] = 0x00;
    }
    display_position = 0;
    
    // 更新显示
    Display_Update();
}

/**
 * @brief 添加数字到显示缓冲区
 * @param digit: 要显示的数字 (1-9)
 */
void Display_Add_Digit(uint8_t digit) {
    // 验证热启动数据的正确性
    if (Validate_HotStart_Data((void*)BACKUP_SRAM_BASE) != DATA_VALID) {
        // 热启动数据无效，尝试修复
        Repair_HotStartData();
        DATA_VALIDATION_LOG("HotStart data repaired before adding digit to display\n");
    }
    
    // 验证输入数字的合法性
    if(!IS_VALID_NOTE_KEY(digit)) {
        DATA_VALIDATION_LOG("Invalid digit for display: %d\n", digit);
        return;
    }
    
    // 验证显示位置的安全性
    display_position = Safe_Get_DisplayPosition(display_position, 0);
    
    // 如果显示位置已满，左移所有数字
    if(display_position >= 8) {
        // 左移显示缓冲区
        for(uint8_t i = 0; i < 7; i++) {
            display_buffer[i] = display_buffer[i + 1];
        }
        display_position = 7;
    }
    
    // 在当前位置添加新数字
    if(IS_VALID_ARRAY_INDEX(digit, 10) && IS_VALID_ARRAY_INDEX(display_position, 8)) {
        display_buffer[display_position] = seg7code[digit];
        display_position++;
    }
    Save_HotStart_State();
    // 更新显示
    Display_Update();
}

/**
 * @brief 更新ZLG7290显示
 */
void Display_Update(void) {
    // 验证热启动数据的正确性
    if (Validate_HotStart_Data((void*)BACKUP_SRAM_BASE) != DATA_VALID) {
        // 热启动数据无效，尝试修复
        Repair_HotStartData();
        DATA_VALIDATION_LOG("HotStart data repaired before display update\n");
    }
    
    // 验证显示缓冲区的合法性
    if (Validate_DisplayBuffer(display_buffer, 8) != DATA_VALID) {
        // 如果数据无效，尝试修复
        Repair_DisplayBuffer(display_buffer, 8);
    }
    
    // 通过I2C写入显示缓冲区到ZLG7290
    I2C_ZLG7290_Write(&hi2c1, ZLG7290_ADDR_WRITE, ZLG7290_DpRam0, display_buffer, 8);
}

/**
 * @brief 清空显示
 */
void Display_Clear(void) {
    // 清空显示缓冲区
    for(uint8_t i = 0; i < 8; i++) {
        display_buffer[i] = 0x00;
    }
    display_position = 0;
    
    // 更新显示
    Display_Update();
}

/**
 * @brief 处理I2C通信错误
 * @param status I2C错误状态
 */
void Handle_I2C_Error(I2C_Status_t status) {
    i2c_error_count++;
    last_i2c_error_time = HAL_GetTick();
    i2c_communication_ok = 0;
    
    // 根据错误类型采取不同的处理策略
    switch(status) {
        case I2C_STATUS_TIMEOUT:
            // 超时错误 - 可能是总线被占用
            // 尝试总线复位
            I2C_Bus_Reset(&hi2c1);
            break;
            
        case I2C_STATUS_ERROR:
            // 通信错误 - 可能是硬件问题
            // 执行错误恢复
            I2C_Error_Recovery(&hi2c1);
            break;
            
        case I2C_STATUS_MAX_RETRY_EXCEEDED:
            // 超过最大重试次数 - 严重错误
            // 可能需要重新初始化I2C
            HAL_I2C_DeInit(&hi2c1);
            HAL_Delay(100);
            HAL_I2C_Init(&hi2c1);
            break;
            
        default:
            break;
    }
    
    // 如果错误过于频繁，可以考虑降级操作
    if(i2c_error_count > 10 && (HAL_GetTick() - last_i2c_error_time) < 1000) {
        // 错误频率过高，暂时禁用I2C操作
        HAL_Delay(1000);
        i2c_error_count = 0; // 重置错误计数
    }
}

/**
 * @brief 检查I2C健康状态
 */
void Check_I2C_Health(void) {
    I2C_ErrorStats_t* stats = I2C_Get_ErrorStats();
    
    // 计算成功率
    if(stats->total_operations > 0) {
        float success_rate = (float)stats->successful_operations * 100.0f / stats->total_operations;
        
        // 如果成功率低于90%，打印警告
        if(success_rate < 90.0f) {
            printf("Warning: I2C success rate is %.2f%% (below 90%%)\n", success_rate);
            I2C_Print_ErrorStats();
        }
        
        // 如果成功率低于50%，执行恢复操作
        if(success_rate < 50.0f) {
            printf("Critical: I2C success rate is %.2f%%, performing recovery\n", success_rate);
            I2C_Error_Recovery(&hi2c1);
            I2C_Reset_ErrorStats(); // 重置统计，给系统一个新的开始
        }
    }
}

/**
 * @brief 看门狗系统初始化
 */
void IWDG_SystemInit(void) {
    // 获取复位原因（仅用于内部记录，不显示）
    Reset_Cause_t reset_cause = IWDG_GetLastResetCause();
    
    // 如果是看门狗复位，记录到备份SRAM
    if(reset_cause == RESET_CAUSE_IWDG) {
        // 可以在这里添加看门狗复位的特殊处理逻辑
        // 例如：记录复位次数、发送告警等
        // 但不显示任何信息，保持用户体验流畅
    }
    
    // 启动看门狗（2秒超时）
    if(IWDG_Start() == IWDG_STATUS_OK) {
        // 看门狗启动成功
        // 静默启动，不影响用户体验
    }
}

/**
 * @brief 看门狗系统任务
 */
void IWDG_SystemTask(void) {
    // 使用序列监控的喂狗机制
    uint32_t current_time = HAL_GetTick();
    static uint32_t last_feed_attempt = 0;
    
    // 检查是否需要尝试喂狗
    if(IWDG_IsEnabled() && (current_time - last_feed_attempt >= IWDG_FEED_INTERVAL_MS)) {
        // 使用带序列检查的喂狗函数
        IWDG_Status_t feed_status = IWDG_Feed_WithSequenceCheck();
        
        last_feed_attempt = current_time;
        
        // 可选：记录喂狗状态
        #ifdef DEBUG_SEQUENCE_MONITOR
        if (feed_status == IWDG_STATUS_OK) {
            printf("Watchdog fed successfully\n");
        } else {
            printf("Watchdog feed denied by sequence monitor\n");
        }
        #endif
    }
    
    // 可以在这里添加额外的看门狗监控逻辑
    static uint32_t last_monitor_info_time = 0;
    
    // 每30秒显示一次序列监控统计信息（调试用）
    #ifdef DEBUG_SEQUENCE_MONITOR
    if(current_time - last_monitor_info_time > 30000) {
        last_monitor_info_time = current_time;
        IWDG_SequenceMonitor_PrintStatus();
    }
    #endif
    
    // 每60秒显示一次乱序执行统计信息（调试用）
    #ifdef DEBUG_SCRAMBLED_EXECUTION
    static uint32_t last_scramble_info_time = 0;
    if(current_time - last_scramble_info_time > 60000) {
        last_scramble_info_time = current_time;
        ScrambledExecution_PrintStats();
    }
    #endif
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler */
  /* User can add his own implementation to report the HAL error return state */
  while(1) 
  {
  }
  /* USER CODE END Error_Handler */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
