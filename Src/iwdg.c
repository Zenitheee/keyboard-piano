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

/* USER CODE END 1 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/ 