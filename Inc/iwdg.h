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

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __IWDG_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/ 