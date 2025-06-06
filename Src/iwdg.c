/**
  ******************************************************************************
  * @file    iwdg.c
  * @brief   This file provides code for the configuration
  *          of the IWDG instances.
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

/* Includes ------------------------------------------------------------------*/
#include "iwdg.h"

/* USER CODE BEGIN 0 */

/* External function declarations --------------------------------------------*/
extern void Error_Handler(void);

/* Private variables ---------------------------------------------------------*/
static uint32_t iwdg_feed_count = 0;           // 喂狗计数器
static uint32_t iwdg_last_feed_time = 0;       // 最后喂狗时间
static uint8_t iwdg_auto_feed_enabled = 1;     // 自动喂狗使能标志
static uint8_t iwdg_enabled = 0;               // 看门狗使能状态
static Reset_Cause_t last_reset_cause = RESET_CAUSE_UNKNOWN;  // 最后复位原因

/* 软件序列监控相关变量 */
static uint8_t task_checklist = 0;             // 任务清单变量
static uint8_t sequence_monitor_enabled = 1;   // 序列监控使能标志
static uint32_t sequence_start_time = 0;       // 序列开始时间
static uint32_t sequence_timeout_ms = 5000;    // 序列超时时间(5秒)
static uint32_t sequence_complete_count = 0;   // 序列完成计数
static uint32_t sequence_timeout_count = 0;    // 序列超时计数
static uint32_t sequence_incomplete_count = 0; // 序列不完整计数
static uint8_t last_task_checklist = 0;        // 上次任务清单状态

/* 乱序执行相关变量 */
static uint8_t scrambled_execution_enabled = 1; // 乱序执行使能标志
static uint32_t scramble_random_seed = 0;       // 随机种子
static uint8_t scramble_dummy_buffer[SCRAMBLE_DUMMY_BUFFER_SIZE]; // 虚拟缓冲区
static ScrambleStats_t scramble_stats = {0};    // 乱序执行统计
static volatile uint32_t scramble_dummy_result = 0; // 虚拟计算结果（防止编译器优化）

/* USER CODE END 0 */

IWDG_HandleTypeDef hiwdg;

/* IWDG init function */
void MX_IWDG_Init(void)
{
  /* USER CODE BEGIN IWDG_Init 0 */
  
  // 检测复位原因
  last_reset_cause = IWDG_GetResetCause();
  
  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
  hiwdg.Init.Reload = 1000;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */
  
  // 初始化看门狗相关变量
  iwdg_feed_count = 0;
  iwdg_last_feed_time = HAL_GetTick();
  iwdg_auto_feed_enabled = 1;
  iwdg_enabled = 0;  // 初始化后未启动
  
  // 初始化软件序列监控
  IWDG_SequenceMonitor_Init();
  
  // 初始化乱序执行
  ScrambledExecution_Init();
  
  /* USER CODE END IWDG_Init 2 */

}

/* USER CODE BEGIN 1 */

/**
  * @brief  启动看门狗
  * @retval IWDG_Status_t 操作状态
  */
IWDG_Status_t IWDG_Start(void)
{
    HAL_StatusTypeDef hal_status;
    
    // 启动看门狗
    hal_status = HAL_IWDG_Start(&hiwdg);
    
    if(hal_status == HAL_OK) {
        iwdg_enabled = 1;
        iwdg_last_feed_time = HAL_GetTick();
        iwdg_feed_count = 0;
        return IWDG_STATUS_OK;
    } else {
        return IWDG_STATUS_ERROR;
    }
}

/**
  * @brief  喂看门狗
  * @retval IWDG_Status_t 操作状态
  */
IWDG_Status_t IWDG_Feed(void)
{
    HAL_StatusTypeDef hal_status;
    
    if(!iwdg_enabled) {
        return IWDG_STATUS_ERROR;
    }
    
    // 刷新看门狗计数器
    hal_status = HAL_IWDG_Refresh(&hiwdg);
    
    if(hal_status == HAL_OK) {
        iwdg_feed_count++;
        iwdg_last_feed_time = HAL_GetTick();
        return IWDG_STATUS_OK;
    } else {
        return IWDG_STATUS_ERROR;
    }
}

/**
  * @brief  停止看门狗（注意：IWDG一旦启动无法停止，此函数仅用于状态管理）
  * @retval IWDG_Status_t 操作状态
  */
IWDG_Status_t IWDG_Stop(void)
{
    // 注意：独立看门狗一旦启动就无法停止，只能通过复位来停止
    // 这里只是更新状态标志，实际硬件看门狗仍在运行
    iwdg_enabled = 0;
    iwdg_auto_feed_enabled = 0;
    
    return IWDG_STATUS_OK;
}

/**
  * @brief  获取看门狗状态
  * @retval HAL_IWDG_StateTypeDef 看门狗HAL状态
  */
HAL_IWDG_StateTypeDef IWDG_GetState(void)
{
    return HAL_IWDG_GetState(&hiwdg);
}

/**
  * @brief  检查看门狗是否使能
  * @retval uint8_t 1-使能，0-未使能
  */
uint8_t IWDG_IsEnabled(void)
{
    return iwdg_enabled;
}

/**
  * @brief  获取复位原因
  * @retval Reset_Cause_t 复位原因
  */
Reset_Cause_t IWDG_GetResetCause(void)
{
    Reset_Cause_t cause = RESET_CAUSE_UNKNOWN;
    
    // 检查复位状态寄存器
    if(__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
        cause = RESET_CAUSE_IWDG;
    }
    else if(__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST)) {
        cause = RESET_CAUSE_WWDG;
    }
    else if(__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)) {
        cause = RESET_CAUSE_SOFTWARE;
    }
    else if(__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST)) {
        cause = RESET_CAUSE_POWER_ON;
    }
    else if(__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST)) {
        cause = RESET_CAUSE_EXTERNAL;
    }
    else if(__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST)) {
        cause = RESET_CAUSE_LOW_POWER;
    }
    
    // 清除复位标志
    __HAL_RCC_CLEAR_RESET_FLAGS();
    
    return cause;
}

/**
  * @brief  获取复位原因字符串
  * @param  cause 复位原因
  * @retval const char* 复位原因字符串
  */
const char* IWDG_GetResetCauseString(Reset_Cause_t cause)
{
    switch(cause) {
        case RESET_CAUSE_POWER_ON:
            return "Power-On Reset";
        case RESET_CAUSE_EXTERNAL:
            return "External Reset";
        case RESET_CAUSE_SOFTWARE:
            return "Software Reset";
        case RESET_CAUSE_IWDG:
            return "Independent Watchdog Reset";
        case RESET_CAUSE_WWDG:
            return "Window Watchdog Reset";
        case RESET_CAUSE_LOW_POWER:
            return "Low Power Reset";
        default:
            return "Unknown Reset";
    }
}

/**
  * @brief  看门狗任务函数，应在主循环中定期调用
  * @retval None
  */
void IWDG_Task(void)
{
    uint32_t current_time = HAL_GetTick();
    
    // 检查是否需要自动喂狗
    if(iwdg_enabled && iwdg_auto_feed_enabled) {
        if(current_time - iwdg_last_feed_time >= IWDG_FEED_INTERVAL_MS) {
            IWDG_Feed();
        }
    }
}

/**
  * @brief  使能/禁用自动喂狗
  * @param  enable 1-使能，0-禁用
  * @retval None
  */
void IWDG_EnableAutoFeed(uint8_t enable)
{
    iwdg_auto_feed_enabled = enable;
}

/**
  * @brief  检查自动喂狗是否使能
  * @retval uint8_t 1-使能，0-禁用
  */
uint8_t IWDG_IsAutoFeedEnabled(void)
{
    return iwdg_auto_feed_enabled;
}

/**
  * @brief  获取喂狗计数
  * @retval uint32_t 喂狗次数
  */
uint32_t IWDG_GetFeedCount(void)
{
    return iwdg_feed_count;
}

/**
  * @brief  获取最后喂狗时间
  * @retval uint32_t 最后喂狗时间戳
  */
uint32_t IWDG_GetLastFeedTime(void)
{
    return iwdg_last_feed_time;
}

/**
  * @brief  重置喂狗计数
  * @retval None
  */
void IWDG_ResetFeedCount(void)
{
    iwdg_feed_count = 0;
}

/**
  * @brief  获取上次复位原因
  * @retval Reset_Cause_t 复位原因
  */
Reset_Cause_t IWDG_GetLastResetCause(void)
{
    return last_reset_cause;
}

/**
  * @brief  初始化软件序列监控
  * @retval None
  */
void IWDG_SequenceMonitor_Init(void)
{
    task_checklist = 0;
    sequence_monitor_enabled = 1;
    sequence_start_time = HAL_GetTick();
    sequence_complete_count = 0;
    sequence_timeout_count = 0;
    sequence_incomplete_count = 0;
    last_task_checklist = 0;
}

/**
  * @brief  设置任务检查点
  * @param  checkpoint 检查点位掩码
  * @retval None
  */
void IWDG_SequenceMonitor_SetCheckpoint(uint8_t checkpoint)
{
    if (!sequence_monitor_enabled) {
        return;
    }
    
    // 如果是新的序列开始（从0开始设置第一个检查点）
    if (task_checklist == 0) {
        sequence_start_time = HAL_GetTick();
    }
    
    // 设置检查点
    task_checklist |= checkpoint;
    
    // 记录调试信息
    #ifdef DEBUG_SEQUENCE_MONITOR
    printf("Checkpoint set: 0x%02X, Current checklist: 0x%02X\n", checkpoint, task_checklist);
    #endif
}

/**
  * @brief  清除任务检查点
  * @param  checkpoint 检查点位掩码
  * @retval None
  */
void IWDG_SequenceMonitor_ClearCheckpoint(uint8_t checkpoint)
{
    if (!sequence_monitor_enabled) {
        return;
    }
    
    task_checklist &= ~checkpoint;
}

/**
  * @brief  重置任务清单
  * @retval None
  */
void IWDG_SequenceMonitor_Reset(void)
{
    last_task_checklist = task_checklist;
    task_checklist = 0;
    sequence_start_time = HAL_GetTick();
}

/**
  * @brief  检查任务序列完成状态
  * @retval SequenceMonitor_Status_t 序列监控状态
  */
SequenceMonitor_Status_t IWDG_SequenceMonitor_Check(void)
{
    if (!sequence_monitor_enabled) {
        return SEQUENCE_MONITOR_DISABLED;
    }
    
    uint32_t current_time = HAL_GetTick();
    uint32_t elapsed_time = current_time - sequence_start_time;
    
    // 检查是否超时
    if (elapsed_time > sequence_timeout_ms) {
        sequence_timeout_count++;
        
        #ifdef DEBUG_SEQUENCE_MONITOR
        printf("Sequence timeout! Elapsed: %lu ms, Checklist: 0x%02X\n", elapsed_time, task_checklist);
        #endif
        
        return SEQUENCE_MONITOR_TIMEOUT;
    }
    
    // 检查关键任务是否完成
    if ((task_checklist & TASK_SEQUENCE_CRITICAL) == TASK_SEQUENCE_CRITICAL) {
        sequence_complete_count++;
        
        #ifdef DEBUG_SEQUENCE_MONITOR
        printf("Critical sequence complete! Checklist: 0x%02X\n", task_checklist);
        #endif
        
        return SEQUENCE_MONITOR_OK;
    }
    
    // 序列未完成
    return SEQUENCE_MONITOR_INCOMPLETE;
}

/**
  * @brief  获取当前任务清单状态
  * @retval uint8_t 任务清单位掩码
  */
uint8_t IWDG_SequenceMonitor_GetTaskChecklist(void)
{
    return task_checklist;
}

/**
  * @brief  使能/禁用序列监控
  * @param  enable 1-使能，0-禁用
  * @retval None
  */
void IWDG_SequenceMonitor_Enable(uint8_t enable)
{
    sequence_monitor_enabled = enable;
    
    if (enable) {
        // 重新启用时重置状态
        IWDG_SequenceMonitor_Reset();
    }
}

/**
  * @brief  检查序列监控是否使能
  * @retval uint8_t 1-使能，0-禁用
  */
uint8_t IWDG_SequenceMonitor_IsEnabled(void)
{
    return sequence_monitor_enabled;
}

/**
  * @brief  带序列检查的喂狗函数
  * @retval IWDG_Status_t 操作状态
  */
IWDG_Status_t IWDG_Feed_WithSequenceCheck(void)
{
    if (!iwdg_enabled) {
        return IWDG_STATUS_ERROR;
    }
    
    // 如果序列监控未使能，直接喂狗
    if (!sequence_monitor_enabled) {
        return IWDG_Feed();
    }
    
    // 检查任务序列完成状态
    SequenceMonitor_Status_t sequence_status = IWDG_SequenceMonitor_Check();
    
    switch (sequence_status) {
        case SEQUENCE_MONITOR_OK:
            // 序列完成，允许喂狗并重置序列
            IWDG_SequenceMonitor_Reset();
            return IWDG_Feed();
            
        case SEQUENCE_MONITOR_INCOMPLETE:
            // 序列未完成，不喂狗
            sequence_incomplete_count++;
            
            #ifdef DEBUG_SEQUENCE_MONITOR
            printf("Sequence incomplete, feed denied. Checklist: 0x%02X\n", task_checklist);
            #endif
            
            return IWDG_STATUS_ERROR;
            
        case SEQUENCE_MONITOR_TIMEOUT:
            // 序列超时，强制重置并喂狗（避免系统死锁）
            IWDG_SequenceMonitor_Reset();
            
            #ifdef DEBUG_SEQUENCE_MONITOR
            printf("Sequence timeout, forced feed and reset\n");
            #endif
            
            return IWDG_Feed();
            
        case SEQUENCE_MONITOR_DISABLED:
        default:
            // 监控禁用，直接喂狗
            return IWDG_Feed();
    }
}

/**
  * @brief  打印序列监控状态信息
  * @retval None
  */
void IWDG_SequenceMonitor_PrintStatus(void)
{
    printf("\n=== Sequence Monitor Status ===\n");
    printf("Enabled: %s\n", sequence_monitor_enabled ? "Yes" : "No");
    printf("Current Checklist: 0x%02X\n", task_checklist);
    printf("Last Checklist: 0x%02X\n", last_task_checklist);
    printf("Sequence Complete Count: %lu\n", sequence_complete_count);
    printf("Sequence Timeout Count: %lu\n", sequence_timeout_count);
    printf("Sequence Incomplete Count: %lu\n", sequence_incomplete_count);
    printf("Timeout Setting: %lu ms\n", sequence_timeout_ms);
    
    uint32_t current_time = HAL_GetTick();
    uint32_t elapsed_time = current_time - sequence_start_time;
    printf("Current Sequence Elapsed: %lu ms\n", elapsed_time);
    
    printf("Critical Tasks Required: 0x%02X\n", TASK_SEQUENCE_CRITICAL);
    printf("Complete Tasks Required: 0x%02X\n", TASK_SEQUENCE_COMPLETE);
    
    // 分析当前状态
    SequenceMonitor_Status_t status = IWDG_SequenceMonitor_Check();
    printf("Current Status: ");
    switch (status) {
        case SEQUENCE_MONITOR_OK:
            printf("OK (Ready to feed)\n");
            break;
        case SEQUENCE_MONITOR_INCOMPLETE:
            printf("INCOMPLETE (Feed denied)\n");
            break;
        case SEQUENCE_MONITOR_TIMEOUT:
            printf("TIMEOUT (Forced feed)\n");
            break;
        case SEQUENCE_MONITOR_DISABLED:
            printf("DISABLED\n");
            break;
        default:
            printf("UNKNOWN\n");
            break;
    }
    
    printf("===============================\n\n");
}

/**
  * @brief  初始化乱序执行模块
  * @retval None
  */
void ScrambledExecution_Init(void)
{
    // 使用系统时钟作为初始随机种子
    scramble_random_seed = HAL_GetTick();
    
    // 初始化虚拟缓冲区
    for (uint8_t i = 0; i < SCRAMBLE_DUMMY_BUFFER_SIZE; i++) {
        scramble_dummy_buffer[i] = (uint8_t)(scramble_random_seed + i);
    }
    
    // 重置统计信息
    memset(&scramble_stats, 0, sizeof(ScrambleStats_t));
    scrambled_execution_enabled = 1;
    scramble_dummy_result = 0;
    
    #ifdef DEBUG_SCRAMBLED_EXECUTION
    printf("Scrambled execution initialized with seed: 0x%08lX\n", scramble_random_seed);
    #endif
}

/**
  * @brief  简单的线性同余随机数生成器
  * @retval uint32_t 随机数
  */
static uint32_t scramble_random(void)
{
    // 线性同余生成器: X(n+1) = (a * X(n) + c) mod m
    // 使用常见的参数: a=1664525, c=1013904223, m=2^32
    scramble_random_seed = scramble_random_seed * 1664525UL + 1013904223UL;
    return scramble_random_seed;
}

/**
  * @brief  执行虚拟计算操作
  * @retval None
  */
static void scramble_dummy_calculation(void)
{
    uint32_t temp = scramble_random();
    
    // 执行一些无意义但耗时的计算
    for (uint8_t i = 0; i < (temp % 10 + 1); i++) {
        scramble_dummy_result += temp * i;
        scramble_dummy_result ^= (temp >> i);
        scramble_dummy_result = (scramble_dummy_result << 1) | (scramble_dummy_result >> 31);
    }
    
    scramble_stats.operation_counts[SCRAMBLE_OP_DUMMY_CALC]++;
}

/**
  * @brief  执行内存访问操作
  * @retval None
  */
static void scramble_memory_access(void)
{
    uint32_t index = scramble_random() % SCRAMBLE_DUMMY_BUFFER_SIZE;
    uint8_t temp_value = scramble_dummy_buffer[index];
    
    // 执行一些内存读写操作
    for (uint8_t i = 0; i < 5; i++) {
        uint32_t next_index = (index + i) % SCRAMBLE_DUMMY_BUFFER_SIZE;
        scramble_dummy_buffer[next_index] = temp_value ^ (uint8_t)i;
        temp_value = scramble_dummy_buffer[next_index];
    }
    
    // 恢复原始值（避免影响系统状态）
    scramble_dummy_buffer[index] = (uint8_t)(scramble_random_seed + index);
    
    scramble_stats.operation_counts[SCRAMBLE_OP_MEMORY_ACCESS]++;
}

/**
  * @brief  执行循环延时操作
  * @retval None
  */
static void scramble_loop_delay(void)
{
    uint32_t loop_count = scramble_random() % 100 + 10;
    volatile uint32_t dummy = 0;
    
    // 执行空循环延时
    for (uint32_t i = 0; i < loop_count; i++) {
        dummy += i;
        __NOP(); // 空操作，防止编译器优化
    }
    
    scramble_dummy_result += dummy;
    scramble_stats.operation_counts[SCRAMBLE_OP_LOOP_DELAY]++;
}

/**
  * @brief  执行位运算操作
  * @retval None
  */
static void scramble_bitwise_operations(void)
{
    uint32_t value = scramble_random();
    
    // 执行各种位运算
    value ^= (value << 13);
    value ^= (value >> 17);
    value ^= (value << 5);
    value = ~value;
    value = (value << 16) | (value >> 16);
    
    scramble_dummy_result ^= value;
    scramble_stats.operation_counts[SCRAMBLE_OP_BITWISE]++;
}

/**
  * @brief  执行算术运算操作
  * @retval None
  */
static void scramble_arithmetic_operations(void)
{
    uint32_t a = scramble_random();
    uint32_t b = scramble_random();
    
    // 执行各种算术运算
    uint32_t result = a + b;
    result = result * 3;
    result = result / 2;
    result = result % 1000;
    result = result - a;
    
    scramble_dummy_result += result;
    scramble_stats.operation_counts[SCRAMBLE_OP_ARITHMETIC]++;
}

/**
  * @brief  执行条件分支操作
  * @retval None
  */
static void scramble_conditional_operations(void)
{
    uint32_t value = scramble_random();
    
    // 执行条件分支操作
    if (value & 0x01) {
        scramble_dummy_result += value;
    } else {
        scramble_dummy_result -= value;
    }
    
    if (value & 0x02) {
        scramble_dummy_result ^= 0xAAAAAAAA;
    }
    
    if (value & 0x04) {
        scramble_dummy_result = (scramble_dummy_result << 2) | (scramble_dummy_result >> 30);
    }
    
    scramble_stats.operation_counts[SCRAMBLE_OP_CONDITIONAL]++;
}

/**
  * @brief  执行单个随机操作
  * @retval None
  */
static void scramble_execute_single_operation(void)
{
    ScrambleOperation_t op = (ScrambleOperation_t)(scramble_random() % SCRAMBLE_OP_COUNT);
    
    switch (op) {
        case SCRAMBLE_OP_DUMMY_CALC:
            scramble_dummy_calculation();
            break;
        case SCRAMBLE_OP_MEMORY_ACCESS:
            scramble_memory_access();
            break;
        case SCRAMBLE_OP_LOOP_DELAY:
            scramble_loop_delay();
            break;
        case SCRAMBLE_OP_BITWISE:
            scramble_bitwise_operations();
            break;
        case SCRAMBLE_OP_ARITHMETIC:
            scramble_arithmetic_operations();
            break;
        case SCRAMBLE_OP_CONDITIONAL:
            scramble_conditional_operations();
            break;
        default:
            scramble_dummy_calculation();
            break;
    }
}

/**
  * @brief  执行乱序执行（普通模式）
  * @retval None
  */
void ScrambledExecution_Execute(void)
{
    #ifdef ENABLE_SCRAMBLED_EXECUTION
    if (!scrambled_execution_enabled) {
        return;
    }
    
    uint32_t operation_count = (scramble_random() % (SCRAMBLE_MAX_OPERATIONS - SCRAMBLE_MIN_OPERATIONS + 1)) + SCRAMBLE_MIN_OPERATIONS;
    
    #ifdef DEBUG_SCRAMBLED_EXECUTION
    printf("Executing %lu scrambled operations\n", operation_count);
    #endif
    
    for (uint32_t i = 0; i < operation_count; i++) {
        scramble_execute_single_operation();
    }
    
    // 更新统计信息
    scramble_stats.total_scrambles++;
    scramble_stats.operations_executed += operation_count;
    scramble_stats.avg_operations = scramble_stats.operations_executed / scramble_stats.total_scrambles;
    scramble_stats.last_scramble_time = HAL_GetTick();
    
    // 使用当前时间更新随机种子
    ScrambledExecution_UpdateSeed(HAL_GetTick());
    #endif
}

/**
  * @brief  在关键代码段中执行乱序执行（增强模式）
  * @retval None
  */
void ScrambledExecution_ExecuteInCriticalSection(void)
{
    #ifdef ENABLE_SCRAMBLED_EXECUTION
    if (!scrambled_execution_enabled) {
        return;
    }
    
    // 关键代码段使用更多的随机操作
    uint32_t operation_count = (scramble_random() % (SCRAMBLE_MAX_OPERATIONS * 2 - SCRAMBLE_MIN_OPERATIONS + 1)) + SCRAMBLE_MIN_OPERATIONS;
    
    #ifdef DEBUG_SCRAMBLED_EXECUTION
    printf("Executing %lu critical scrambled operations\n", operation_count);
    #endif
    
    // 在执行前后插入随机操作
    for (uint32_t i = 0; i < operation_count / 3; i++) {
        scramble_execute_single_operation();
    }
    
    // 中间执行一些特殊操作
    scramble_dummy_result ^= HAL_GetTick();
    
    for (uint32_t i = 0; i < operation_count / 3; i++) {
        scramble_execute_single_operation();
    }
    
    // 更新缓冲区内容
    uint32_t buffer_index = scramble_random() % SCRAMBLE_DUMMY_BUFFER_SIZE;
    scramble_dummy_buffer[buffer_index] = (uint8_t)HAL_GetTick();
    
    for (uint32_t i = 0; i < operation_count / 3; i++) {
        scramble_execute_single_operation();
    }
    
    // 更新统计信息
    scramble_stats.total_scrambles++;
    scramble_stats.operations_executed += operation_count;
    scramble_stats.avg_operations = scramble_stats.operations_executed / scramble_stats.total_scrambles;
    scramble_stats.last_scramble_time = HAL_GetTick();
    
    // 使用多个源更新随机种子
    ScrambledExecution_UpdateSeed(HAL_GetTick() ^ scramble_dummy_result);
    #endif
}

/**
  * @brief  获取当前随机种子
  * @retval uint32_t 随机种子
  */
uint32_t ScrambledExecution_GetRandomSeed(void)
{
    return scramble_random_seed;
}

/**
  * @brief  更新随机种子
  * @param  new_seed 新的随机种子
  * @retval None
  */
void ScrambledExecution_UpdateSeed(uint32_t new_seed)
{
    scramble_random_seed ^= new_seed;
    
    // 确保种子不为0
    if (scramble_random_seed == 0) {
        scramble_random_seed = HAL_GetTick() | 1;
    }
}

/**
  * @brief  获取乱序执行统计信息
  * @retval ScrambleStats_t* 统计信息指针
  */
ScrambleStats_t* ScrambledExecution_GetStats(void)
{
    return &scramble_stats;
}

/**
  * @brief  重置乱序执行统计信息
  * @retval None
  */
void ScrambledExecution_ResetStats(void)
{
    memset(&scramble_stats, 0, sizeof(ScrambleStats_t));
}

/**
  * @brief  打印乱序执行统计信息
  * @retval None
  */
void ScrambledExecution_PrintStats(void)
{
    printf("\n=== Scrambled Execution Statistics ===\n");
    printf("Enabled: %s\n", scrambled_execution_enabled ? "Yes" : "No");
    printf("Total Scrambles: %lu\n", scramble_stats.total_scrambles);
    printf("Total Operations: %lu\n", scramble_stats.operations_executed);
    printf("Average Operations per Scramble: %lu\n", scramble_stats.avg_operations);
    printf("Last Scramble Time: %lu ms\n", scramble_stats.last_scramble_time);
    printf("Current Random Seed: 0x%08lX\n", scramble_random_seed);
    printf("Dummy Result: 0x%08lX\n", scramble_dummy_result);
    
    printf("--- Operation Type Counts ---\n");
    printf("Dummy Calculations: %lu\n", scramble_stats.operation_counts[SCRAMBLE_OP_DUMMY_CALC]);
    printf("Memory Accesses: %lu\n", scramble_stats.operation_counts[SCRAMBLE_OP_MEMORY_ACCESS]);
    printf("Loop Delays: %lu\n", scramble_stats.operation_counts[SCRAMBLE_OP_LOOP_DELAY]);
    printf("Bitwise Operations: %lu\n", scramble_stats.operation_counts[SCRAMBLE_OP_BITWISE]);
    printf("Arithmetic Operations: %lu\n", scramble_stats.operation_counts[SCRAMBLE_OP_ARITHMETIC]);
    printf("Conditional Operations: %lu\n", scramble_stats.operation_counts[SCRAMBLE_OP_CONDITIONAL]);
    
    printf("=====================================\n\n");
}

/**
  * @brief  使能/禁用乱序执行
  * @param  enable 1-使能，0-禁用
  * @retval None
  */
void ScrambledExecution_Enable(uint8_t enable)
{
    scrambled_execution_enabled = enable;
    
    if (enable) {
        // 重新启用时重新初始化
        ScrambledExecution_Init();
    }
}

/**
  * @brief  检查乱序执行是否使能
  * @retval uint8_t 1-使能，0-禁用
  */
uint8_t ScrambledExecution_IsEnabled(void)
{
    return scrambled_execution_enabled;
}

/* USER CODE END 1 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/ 