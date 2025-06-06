#include "state_machine.h"
#include "data_validator.h"
#include <string.h>
#include <stdio.h>

/* 备份SRAM基地址 - 从main.c中声明 */
#define BACKUP_SRAM_BASE 0x40024000  // STM32F4的备份SRAM地址

/* 状态机全局实例 */
static StateMachine_t state_machine;

/* 热启动标志 */
static uint8_t hot_start_flag = 0;

/* 外部函数声明（来自main.c）*/
extern void Play_Note(uint8_t note_index);
extern void Stop_Note(void);
extern void Save_HotStart_State(void);
extern void Display_Add_Digit(uint8_t digit);
extern void Display_Update(void);
extern void Display_Clear(void);
extern void Handle_I2C_Error(I2C_Status_t status);
extern void Check_I2C_Health(void);

/* 外部函数声明（来自iwdg.c）*/
extern void IWDG_SequenceMonitor_SetCheckpoint(uint8_t checkpoint);
extern void ScrambledExecution_Execute(void);
extern void ScrambledExecution_ExecuteInCriticalSection(void);
extern void ScrambledExecution_UpdateSeed(uint32_t new_seed);

/**
 * @brief 状态机初始化
 */
void StateMachine_Init(void) {
    // 验证数据的正确性
    if (Validate_HotStart_Data((void*)BACKUP_SRAM_BASE) != DATA_VALID) {
        // 数据无效，尝试修复
        Repair_HotStartData();
        DATA_VALIDATION_LOG("HotStart data repaired before state machine init\n");
    }
    
    memset(&state_machine, 0, sizeof(StateMachine_t));
    
    state_machine.current_state = STATE_IDLE;
    state_machine.previous_state = STATE_IDLE;
    state_machine.current_event = EVENT_NONE;
    state_machine.state_entry_time = HAL_GetTick();
    
    // 同步全局状态到状态机（支持热启动）- 添加数据验证
    state_machine.current_key = Safe_Get_KeyValue(current_key, 0xFF);
    state_machine.previous_key = Safe_Get_KeyValue(current_key, 0xFF);
    state_machine.is_playing = (is_playing > 1) ? 0 : is_playing;
    
    // 初始化定时器
    uint32_t current_time = HAL_GetTick();
    state_machine.last_50ms_timer = current_time;
    state_machine.last_100ms_timer = current_time;
    state_machine.last_5s_timer = current_time;
    
    STATE_DEBUG_PRINT("State Machine Initialized\n");
}

/**
 * @brief 设置热启动标志
 * @param is_hot_start 是否为热启动
 */
void StateMachine_SetHotStartFlag(uint8_t is_hot_start) {
    hot_start_flag = is_hot_start;
}

/**
 * @brief 状态机主运行函数
 */
void StateMachine_Run(void) {
    // 获取当前事件
    state_machine.current_event = StateMachine_GetEvent();
    
    // 根据当前状态调用对应的处理函数
    switch(state_machine.current_state) {
        case STATE_IDLE:
            State_Idle_Handler();
            break;
        case STATE_KEY_DETECT:
            State_KeyDetect_Handler();
            break;
        case STATE_KEY_PROCESS:
            State_KeyProcess_Handler();
            break;
        case STATE_AUDIO_PLAY:
            State_AudioPlay_Handler();
            break;
        case STATE_DISPLAY_UPDATE:
            State_DisplayUpdate_Handler();
            break;
        case STATE_SYSTEM_MAINTAIN:
            State_SystemMaintain_Handler();
            break;
        case STATE_ERROR_HANDLE:
            State_ErrorHandle_Handler();
            break;
        default:
            // 未知状态，重置到空闲状态
            StateMachine_SetState(STATE_IDLE);
            break;
    }
}

/**
 * @brief 获取系统事件
 * @return 检测到的事件
 */
SystemEvent_t StateMachine_GetEvent(void) {
    uint32_t current_time = HAL_GetTick();
    
    // 检查按键中断事件（优先级最高）
    if(key_flag == 1) {
        return EVENT_KEY_INTERRUPT;
    }
    
    // 检查定时器事件
    if(current_time - state_machine.last_50ms_timer >= 50) {
        return EVENT_TIMER_50MS;
    }
    
    if(current_time - state_machine.last_100ms_timer >= 100) {
        return EVENT_TIMER_100MS;
    }
    
    if(current_time - state_machine.last_5s_timer >= 5000) {
        return EVENT_TIMER_5S;
    }
    
    return EVENT_NONE;
}

/**
 * @brief 设置新状态
 * @param new_state 新状态
 */
void StateMachine_SetState(SystemState_t new_state) {
    // 验证新状态的合法性
    if (Validate_SystemState(new_state) != DATA_VALID) {
        DATA_VALIDATION_LOG("Invalid state transition attempted: %d\n", new_state);
        new_state = STATE_IDLE; // 默认回到空闲状态
    }
    
    if(state_machine.current_state != new_state) {
        state_machine.previous_state = state_machine.current_state;
        state_machine.current_state = new_state;
        state_machine.state_entry_time = HAL_GetTick();
        
        STATE_DEBUG_PRINT("State: %d -> %d, Event: %d\n", 
                         state_machine.previous_state, 
                         state_machine.current_state, 
                         state_machine.current_event);
    }
}

/**
 * @brief 空闲状态处理函数
 */
void State_Idle_Handler(void) {
    switch(state_machine.current_event) {
        case EVENT_KEY_INTERRUPT:
            // 按键中断触发，转到按键检测状态
            StateMachine_SetState(STATE_KEY_DETECT);
            break;
            
        case EVENT_TIMER_50MS:
            // 50ms定时器事件，执行按键轮询检测
            state_machine.last_50ms_timer = HAL_GetTick();
            StateMachine_SetState(STATE_KEY_DETECT);
            break;
            
        case EVENT_TIMER_100MS:
            // 100ms定时器事件，执行系统维护
            state_machine.last_100ms_timer = HAL_GetTick();
            StateMachine_SetState(STATE_SYSTEM_MAINTAIN);
            break;
            
        case EVENT_TIMER_5S:
            // 5秒定时器事件，检查I2C健康状态
            state_machine.last_5s_timer = HAL_GetTick();
            Check_I2C_Health();
            // 保持在空闲状态
            break;
            
        default:
            // 如果正在播放音符，继续播放
            if(state_machine.is_playing && state_machine.current_key != 0xFF) {
                // 验证数据的正确性
                if (Validate_HotStart_Data((void*)BACKUP_SRAM_BASE) != DATA_VALID) {
                    // 数据无效，尝试修复
                    Repair_HotStartData();
                    DATA_VALIDATION_LOG("HotStart data repaired before playing note in idle state\n");
                }
                
                // 确保键值有效后再播放
                uint8_t safe_key = Safe_Get_KeyValue(state_machine.current_key, 0xFF);
                if(IS_VALID_NOTE_KEY(safe_key)) {
                    Play_Note(safe_key - 1);
                }
            }
            break;
    }
}

/**
 * @brief 按键检测状态处理函数
 */
void State_KeyDetect_Handler(void) {
    // 在关键状态开始前执行乱序执行
    SCRAMBLED_EXECUTE_CRITICAL();
    
    // 验证数据的正确性
    if (Validate_HotStart_Data((void*)BACKUP_SRAM_BASE) != DATA_VALID) {
        // 数据无效，尝试修复
        Repair_HotStartData();
        DATA_VALIDATION_LOG("HotStart data repaired before key detection\n");
    }
    
    // 在I2C通信前执行乱序执行
    SCRAMBLED_EXECUTE();
    
    I2C_Status_t i2c_status;
    
    // 清除按键中断标志
    if(state_machine.current_event == EVENT_KEY_INTERRUPT) {
        key_flag = 0;
    }
    
    // 使用新的验证机制读取按键值
    // 读取两次键盘值,如果一致则使用,如果不一致则读取第三次,
    // 第三次读取的值如果与前两次的任意一次相同则使用第三次的数据,
    // 如果仍然不一致则算作读取失败
    i2c_status = I2C_ZLG7290_Read_WithValidation(&hi2c1, ZLG7290_ADDR_READ, 
                                                 ZLG7290_Key, state_machine.key_buffer, 1);
    
    if(i2c_status == I2C_STATUS_OK) {
        // I2C通信成功后执行乱序执行
        SCRAMBLED_EXECUTE();
        
        // I2C通信成功，重置重试计数
        state_machine.i2c_retry_count = 0;
        
        // 设置任务A检查点 - 按键检测完成
        IWDG_SequenceMonitor_SetCheckpoint(TASK_CHECKPOINT_A);
        
        // 使用按键值更新随机种子
        ScrambledExecution_UpdateSeed(state_machine.key_buffer[0]);
        
        StateMachine_SetState(STATE_KEY_PROCESS);
    } else {
        // I2C通信失败
        state_machine.i2c_retry_count++;
        
        if(state_machine.i2c_retry_count < I2C_MAX_RETRY_COUNT) {
            // 还未达到最大重试次数，保持在检测状态重试
            HAL_Delay(I2C_RETRY_DELAY_MS);
        } else {
            // 超过最大重试次数，转到错误处理状态
            state_machine.i2c_retry_count = 0;
            StateMachine_SetState(STATE_ERROR_HANDLE);
        }
    }
}

/**
 * @brief 按键处理状态处理函数
 */
void State_KeyProcess_Handler(void) {
    // 在按键处理前执行乱序执行
    SCRAMBLED_EXECUTE_CRITICAL();
    
    // 验证数据的正确性
    if (Validate_HotStart_Data((void*)BACKUP_SRAM_BASE) != DATA_VALID) {
        // 数据无效，尝试修复
        Repair_HotStartData();
        DATA_VALIDATION_LOG("HotStart data repaired before key processing\n");
    }
    
    // 获取按键编号
    uint8_t key_number = Get_Key_Number(state_machine.key_buffer[0]);
    
    // 在按键处理过程中执行乱序执行
    SCRAMBLED_EXECUTE();
    
    // 处理按键输入
    KeyProcessResult_t result = Process_Key_Input(key_number);
    
    // 设置任务B检查点 - 按键处理完成
    IWDG_SequenceMonitor_SetCheckpoint(TASK_CHECKPOINT_B);
    
    switch(result) {
        case KEY_RESULT_CLEAR:
            // 清空键按下
            StateMachine_SetState(STATE_DISPLAY_UPDATE);
            break;
            
        case KEY_RESULT_NOTE:
            // 音符键按下
            StateMachine_SetState(STATE_AUDIO_PLAY);
            break;
            
        case KEY_RESULT_RELEASE:
            // 按键释放
            StateMachine_SetState(STATE_SYSTEM_MAINTAIN);
            break;
            
        case KEY_RESULT_NONE:
        default:
            // 无状态变化，返回空闲状态
            StateMachine_SetState(STATE_IDLE);
            break;
    }
}

/**
 * @brief 音频播放状态处理函数
 */
void State_AudioPlay_Handler(void) {
    // 在音频播放前执行乱序执行（关键操作）
    SCRAMBLED_EXECUTE_CRITICAL();
    
    // 验证数据的正确性
    if (Validate_HotStart_Data((void*)BACKUP_SRAM_BASE) != DATA_VALID) {
        // 数据无效，尝试修复
        Repair_HotStartData();
        DATA_VALIDATION_LOG("HotStart data repaired before audio play\n");
    }
    
    // 验证当前按键的合法性
    uint8_t safe_key = Safe_Get_KeyValue(state_machine.current_key, 0xFF);
    
    // 开始播放音符
    if(IS_VALID_NOTE_KEY(safe_key)) {
        state_machine.is_playing = 1;
        is_playing = 1;  // 更新全局变量
        // 音符索引 = 按键值 - 1
        uint8_t note_index = Safe_Get_NoteIndex(safe_key - 1, 0);
        
        // 在播放音符前执行乱序执行
        SCRAMBLED_EXECUTE();
        
        Play_Note(note_index);
        
        // 在播放音符后执行乱序执行
        SCRAMBLED_EXECUTE();
        
        // 设置任务C检查点 - 音频播放完成
        IWDG_SequenceMonitor_SetCheckpoint(TASK_CHECKPOINT_C);
        
        // 使用音符信息更新随机种子
        ScrambledExecution_UpdateSeed(safe_key ^ note_index);
    }
    
    // 转到显示更新状态
    StateMachine_SetState(STATE_DISPLAY_UPDATE);
}

/**
 * @brief 显示更新状态处理函数
 */
void State_DisplayUpdate_Handler(void) {
    // 验证数据的正确性
    if (Validate_HotStart_Data((void*)BACKUP_SRAM_BASE) != DATA_VALID) {
        // 数据无效，尝试修复
        Repair_HotStartData();
        DATA_VALIDATION_LOG("HotStart data repaired before display update\n");
    }
    
    // 更新显示
    Display_Update();
    
    // 设置任务D检查点 - 显示更新完成
    IWDG_SequenceMonitor_SetCheckpoint(TASK_CHECKPOINT_D);
    
    // 显示更新完成，返回空闲状态
    StateMachine_SetState(STATE_IDLE);
}

/**
 * @brief 系统维护状态处理函数
 */
void State_SystemMaintain_Handler(void) {
    // 在系统维护前执行乱序执行
    SCRAMBLED_EXECUTE_CRITICAL();
    
    // 保存热启动状态
    Save_HotStart_State();
    
    // 在系统维护后执行乱序执行
    SCRAMBLED_EXECUTE();
    
    // 设置任务E检查点 - 系统维护完成
    IWDG_SequenceMonitor_SetCheckpoint(TASK_CHECKPOINT_E);
    
    // 使用当前时间更新随机种子
    ScrambledExecution_UpdateSeed(HAL_GetTick());
    
    // 返回空闲状态
    StateMachine_SetState(STATE_IDLE);
}

/**
 * @brief 错误处理状态处理函数
 */
void State_ErrorHandle_Handler(void) {
    // 处理I2C错误
    Handle_I2C_Error(I2C_STATUS_MAX_RETRY_EXCEEDED);
    
    // 错误处理完成，返回空闲状态
    StateMachine_SetState(STATE_IDLE);
}

/**
 * @brief 处理按键输入
 * @param key_number 按键编号
 * @return 处理结果
 */
KeyProcessResult_t Process_Key_Input(uint8_t key_number) {
    // 验证数据的正确性
    if (Validate_HotStart_Data((void*)BACKUP_SRAM_BASE) != DATA_VALID) {
        // 数据无效，尝试修复
        Repair_HotStartData();
        DATA_VALIDATION_LOG("HotStart data repaired before key input processing\n");
    }
    
    // 验证输入按键的合法性
    key_number = Safe_Get_KeyValue(key_number, 0xFF);
    
    // 保存前一个按键值
    state_machine.previous_key = state_machine.current_key;
    
    if(key_number != 0xFF && key_number != state_machine.current_key) {
        // 新按键按下
        state_machine.current_key = key_number;
        current_key = key_number;  // 更新全局变量
        
        if(IS_VALID_CLEAR_KEY(key_number)) {
            // 清空键按下
            Display_Clear();
            Stop_Note();
            state_machine.is_playing = 0;
            is_playing = 0;
            return KEY_RESULT_CLEAR;
        } else if(IS_VALID_NOTE_KEY(key_number)) {
            // 音符键按下
            // 检查是否为热启动后的第一次按键处理，如果是且按键相同则跳过显示添加
            if(hot_start_flag && key_number == state_machine.previous_key) {
                // 热启动时，如果是相同按键，只清除热启动标志，不添加显示
                hot_start_flag = 0;
            } else {
                // 正常情况下添加按键数字到显示
                Display_Add_Digit(key_number);
                hot_start_flag = 0;  // 清除热启动标志
            }
            
            Stop_Note();  // 停止当前音符
            return KEY_RESULT_NOTE;
        }
    }
    else if(key_number == 0xFF && state_machine.current_key != 0xFF) {
        // 按键释放
        state_machine.current_key = 0xFF;
        current_key = 0xFF;
        Stop_Note();
        state_machine.is_playing = 0;
        is_playing = 0;
        hot_start_flag = 0;  // 清除热启动标志
        return KEY_RESULT_RELEASE;
    }
    
    return KEY_RESULT_NONE;
}

/**
 * @brief ZLG7290按键值到键盘编号的映射
 * @param key_value ZLG7290按键值
 * @return 键盘编号(0-9)，0xFF表示无效按键
 */
uint8_t Get_Key_Number(uint8_t key_value) {
    // 验证数据的正确性
    if (Validate_HotStart_Data((void*)BACKUP_SRAM_BASE) != DATA_VALID) {
        // 数据无效，尝试修复
        Repair_HotStartData();
        DATA_VALIDATION_LOG("HotStart data repaired before key number mapping\n");
    }
    
    switch(key_value) {
        case 0x03: return 0;  // 键0 - 清空显示
        case 0x1C: return 1;  // 键1
        case 0x1B: return 2;  // 键2
        case 0x1A: return 3;  // 键3
        case 0x14: return 4;  // 键4
        case 0x13: return 5;  // 键5
        case 0x12: return 6;  // 键6
        case 0x0C: return 7;  // 键7
        case 0x0B: return 8;  // 键8
        case 0x0A: return 9;  // 键9
        default: return 0xFF; // 无效按键
    }
} 