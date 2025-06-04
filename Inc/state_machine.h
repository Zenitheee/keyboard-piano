#ifndef __STATE_MACHINE_H
#define __STATE_MACHINE_H

#include "stm32f4xx_hal.h"
#include "zlg7290.h"

/* 系统状态枚举 */
typedef enum {
    STATE_IDLE,           // 空闲状态 - 等待事件
    STATE_KEY_DETECT,     // 按键检测状态 - 读取I2C按键值
    STATE_KEY_PROCESS,    // 按键处理状态 - 处理按键逻辑
    STATE_AUDIO_PLAY,     // 音频播放状态 - 处理音频输出
    STATE_DISPLAY_UPDATE, // 显示更新状态 - 更新显示内容
    STATE_SYSTEM_MAINTAIN,// 系统维护状态 - 保存状态、维护
    STATE_ERROR_HANDLE    // 错误处理状态 - 处理各种错误
} SystemState_t;

/* 系统事件枚举 */
typedef enum {
    EVENT_NONE,           // 无事件
    EVENT_KEY_INTERRUPT,  // 按键中断事件
    EVENT_TIMER_50MS,     // 50ms定时器事件
    EVENT_TIMER_100MS,    // 100ms定时器事件
    EVENT_TIMER_5S,       // 5秒定时器事件
    EVENT_I2C_SUCCESS,    // I2C通信成功事件
    EVENT_I2C_ERROR,      // I2C通信错误事件
    EVENT_KEY_PRESS,      // 按键按下事件
    EVENT_KEY_RELEASE,    // 按键释放事件
    EVENT_AUDIO_START,    // 音频开始事件
    EVENT_AUDIO_STOP      // 音频停止事件
} SystemEvent_t;

/* 按键处理结果枚举 */
typedef enum {
    KEY_RESULT_NONE,      // 无变化
    KEY_RESULT_NEW_PRESS, // 新按键按下
    KEY_RESULT_RELEASE,   // 按键释放
    KEY_RESULT_CLEAR,     // 清空键按下
    KEY_RESULT_NOTE       // 音符键按下
} KeyProcessResult_t;

/* 状态机上下文结构 */
typedef struct {
    SystemState_t current_state;        // 当前状态
    SystemState_t previous_state;       // 前一个状态
    SystemEvent_t current_event;        // 当前事件
    uint32_t state_entry_time;          // 状态进入时间
    uint8_t i2c_retry_count;            // I2C重试计数
    uint8_t key_buffer[1];              // 按键值缓冲区
    uint8_t current_key;                // 当前按键值
    uint8_t previous_key;               // 前一个按键值
    uint8_t is_playing;                 // 播放状态
    uint32_t last_50ms_timer;           // 50ms定时器
    uint32_t last_100ms_timer;          // 100ms定时器
    uint32_t last_5s_timer;             // 5秒定时器
} StateMachine_t;

/* 外部变量声明 */
extern volatile uint8_t key_flag;      // 按键中断标志
extern volatile uint8_t is_playing;    // 全局播放状态
extern volatile uint8_t current_key;   // 全局当前按键
extern I2C_HandleTypeDef hi2c1;        // I2C句柄

/* 函数声明 */
void StateMachine_Init(void);
void StateMachine_Run(void);
SystemEvent_t StateMachine_GetEvent(void);
void StateMachine_SetState(SystemState_t new_state);
void StateMachine_SetHotStartFlag(uint8_t is_hot_start);

/* 状态处理函数声明 */
void State_Idle_Handler(void);
void State_KeyDetect_Handler(void);
void State_KeyProcess_Handler(void);
void State_AudioPlay_Handler(void);
void State_DisplayUpdate_Handler(void);
void State_SystemMaintain_Handler(void);
void State_ErrorHandle_Handler(void);

/* 辅助函数声明 */
uint8_t Get_Key_Number(uint8_t key_value);
KeyProcessResult_t Process_Key_Input(uint8_t key_number);

/* 调试相关宏定义 */
#ifdef DEBUG_STATE_MACHINE
    #define STATE_DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
    #define STATE_DEBUG_PRINT(fmt, ...)
#endif

#endif /* __STATE_MACHINE_H */ 