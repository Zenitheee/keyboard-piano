/**
  ******************************************************************************
  * @file    iwdg.h
  * @brief   This file contains all the function prototypes for
  *          the iwdg.c file
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __IWDG_H__
#define __IWDG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */

/* 看门狗超时时间配置 */
#define IWDG_TIMEOUT_MS         2000    // 看门狗超时时间：2秒
#define IWDG_FEED_INTERVAL_MS   1000    // 喂狗间隔：1秒

/* 调试相关宏定义 */
// 取消注释下面的行以启用序列监控调试信息
// #define DEBUG_SEQUENCE_MONITOR

/* 乱序执行相关定义 */
// 取消注释下面的行以启用乱序执行功能
#define ENABLE_SCRAMBLED_EXECUTION

// 取消注释下面的行以启用乱序执行调试信息
// #define DEBUG_SCRAMBLED_EXECUTION

/* 乱序执行配置 */
#define SCRAMBLE_MIN_OPERATIONS     5       // 最小随机操作数
#define SCRAMBLE_MAX_OPERATIONS     20      // 最大随机操作数
#define SCRAMBLE_DUMMY_BUFFER_SIZE  32      // 虚拟缓冲区大小

/* 看门狗状态定义 */
typedef enum {
    IWDG_STATUS_OK = 0,
    IWDG_STATUS_ERROR,
    IWDG_STATUS_TIMEOUT
} IWDG_Status_t;

/* 看门狗复位原因 */
typedef enum {
    RESET_CAUSE_UNKNOWN = 0,
    RESET_CAUSE_POWER_ON,
    RESET_CAUSE_EXTERNAL,
    RESET_CAUSE_SOFTWARE,
    RESET_CAUSE_IWDG,
    RESET_CAUSE_WWDG,
    RESET_CAUSE_LOW_POWER
} Reset_Cause_t;

/* 软件序列监控相关定义 */
/* 任务检查点位掩码定义 */
#define TASK_CHECKPOINT_A       0x01    // 任务A检查点 - 按键检测
#define TASK_CHECKPOINT_B       0x02    // 任务B检查点 - 按键处理
#define TASK_CHECKPOINT_C       0x04    // 任务C检查点 - 音频播放
#define TASK_CHECKPOINT_D       0x08    // 任务D检查点 - 显示更新
#define TASK_CHECKPOINT_E       0x10    // 任务E检查点 - 系统维护

/* 完整任务序列掩码 */
#define TASK_SEQUENCE_COMPLETE  (TASK_CHECKPOINT_A | TASK_CHECKPOINT_B | TASK_CHECKPOINT_C | TASK_CHECKPOINT_D | TASK_CHECKPOINT_E)

/* 最小必要任务序列掩码（关键任务） */
#define TASK_SEQUENCE_CRITICAL  (TASK_CHECKPOINT_A | TASK_CHECKPOINT_B | TASK_CHECKPOINT_E)

/* 序列监控状态 */
typedef enum {
    SEQUENCE_MONITOR_OK = 0,
    SEQUENCE_MONITOR_INCOMPLETE,
    SEQUENCE_MONITOR_TIMEOUT,
    SEQUENCE_MONITOR_DISABLED
} SequenceMonitor_Status_t;

/* 乱序执行操作类型 */
typedef enum {
    SCRAMBLE_OP_DUMMY_CALC = 0,     // 虚拟计算操作
    SCRAMBLE_OP_MEMORY_ACCESS,      // 内存访问操作
    SCRAMBLE_OP_LOOP_DELAY,         // 循环延时操作
    SCRAMBLE_OP_BITWISE,            // 位运算操作
    SCRAMBLE_OP_ARITHMETIC,         // 算术运算操作
    SCRAMBLE_OP_CONDITIONAL,        // 条件分支操作
    SCRAMBLE_OP_COUNT               // 操作类型总数
} ScrambleOperation_t;

/* 乱序执行统计 */
typedef struct {
    uint32_t total_scrambles;       // 总乱序执行次数
    uint32_t operations_executed;   // 执行的操作总数
    uint32_t avg_operations;        // 平均操作数
    uint32_t last_scramble_time;    // 最后乱序执行时间
    uint32_t operation_counts[SCRAMBLE_OP_COUNT]; // 各类操作计数
} ScrambleStats_t;

/* USER CODE END Private defines */

extern IWDG_HandleTypeDef hiwdg;

/* USER CODE BEGIN Prototypes */

/* 看门狗初始化和控制函数 */
void MX_IWDG_Init(void);
IWDG_Status_t IWDG_Start(void);
IWDG_Status_t IWDG_Feed(void);
IWDG_Status_t IWDG_Stop(void);

/* 看门狗状态和诊断函数 */
HAL_IWDG_StateTypeDef IWDG_GetState(void);
uint8_t IWDG_IsEnabled(void);
Reset_Cause_t IWDG_GetResetCause(void);
const char* IWDG_GetResetCauseString(Reset_Cause_t cause);

/* 看门狗管理函数 */
void IWDG_Task(void);
void IWDG_EnableAutoFeed(uint8_t enable);
uint8_t IWDG_IsAutoFeedEnabled(void);

/* 调试和监控函数 */
uint32_t IWDG_GetFeedCount(void);
uint32_t IWDG_GetLastFeedTime(void);
void IWDG_ResetFeedCount(void);
Reset_Cause_t IWDG_GetLastResetCause(void);

/* 软件序列监控函数 */
void IWDG_SequenceMonitor_Init(void);
void IWDG_SequenceMonitor_SetCheckpoint(uint8_t checkpoint);
void IWDG_SequenceMonitor_ClearCheckpoint(uint8_t checkpoint);
void IWDG_SequenceMonitor_Reset(void);
SequenceMonitor_Status_t IWDG_SequenceMonitor_Check(void);
uint8_t IWDG_SequenceMonitor_GetTaskChecklist(void);
void IWDG_SequenceMonitor_Enable(uint8_t enable);
uint8_t IWDG_SequenceMonitor_IsEnabled(void);
IWDG_Status_t IWDG_Feed_WithSequenceCheck(void);
void IWDG_SequenceMonitor_PrintStatus(void);

/* 乱序执行函数 */
void ScrambledExecution_Init(void);
void ScrambledExecution_Execute(void);
void ScrambledExecution_ExecuteInCriticalSection(void);
uint32_t ScrambledExecution_GetRandomSeed(void);
void ScrambledExecution_UpdateSeed(uint32_t new_seed);
ScrambleStats_t* ScrambledExecution_GetStats(void);
void ScrambledExecution_ResetStats(void);
void ScrambledExecution_PrintStats(void);
void ScrambledExecution_Enable(uint8_t enable);
uint8_t ScrambledExecution_IsEnabled(void);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __IWDG_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/ 