/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ZLG7290_H

#define __ZLG7290_H

#include "stm32f4xx_hal.h"
//定义ZLG7290内部寄存器地址及对应地址等
#define ZLG7290_SystemReg		0x00		//系统寄存器
#define ZLG7290_Key			0x01		//键值寄存器
#define ZLG7290_RepeatCnt		0x02		//连击次数寄存器
#define ZLG7290_FunctionKey		0x03		//功能键寄存器
#define ZLG7290_CmdBuf			0x07		//命令缓存起始地址
#define ZLG7290_CmdBuf0			0x07		//命令缓存0
#define ZLG7290_CmdBuf1			0x08		//命令缓存1
#define ZLG7290_FlashOnOff		0x0C		//闪烁控制寄存器
#define ZLG7290_ScanNum			0x0D		//扫描位数寄存器

#define ZLG7290_DpRam			0x10		//显示缓存起始地址

#define ZLG7290_DpRam0			0x10		//显示缓存0
#define ZLG7290_DpRam1			0x11		//显示缓存1
#define ZLG7290_DpRam2			0x12		//显示缓存2
#define ZLG7290_DpRam3			0x13		//显示缓存3
#define ZLG7290_DpRam4			0x14		//显示缓存4
#define ZLG7290_DpRam5			0x15		//显示缓存5
#define ZLG7290_DpRam6			0x16		//显示缓存6
#define ZLG7290_DpRam7			0x17		//显示缓存7
#define ADDR_24LC64     0x70

// ZLG7290 I2C地址定义
#define ZLG7290_ADDR_WRITE      0x70        // 写地址
#define ZLG7290_ADDR_READ       0x71        // 读地址

#define I2C_PAGESIZE    8

// I2C通信重试机制相关定义
#define I2C_MAX_RETRY_COUNT     3           // 最大重试次数
#define I2C_RETRY_DELAY_MS      10          // 重试间隔时间(毫秒)
#define I2C_RESET_DELAY_MS      50          // I2C复位延时(毫秒)

// 调试相关宏定义
// 取消注释下面的行以启用I2C验证调试信息
// #define DEBUG_I2C_VALIDATION

// I2C错误状态枚举
typedef enum {
    I2C_STATUS_OK = 0,                      // 通信成功
    I2C_STATUS_ERROR,                       // 通信错误
    I2C_STATUS_TIMEOUT,                     // 通信超时
    I2C_STATUS_BUSY,                        // 总线忙
    I2C_STATUS_MAX_RETRY_EXCEEDED           // 超过最大重试次数
} I2C_Status_t;

// I2C错误统计结构
typedef struct {
    uint32_t total_operations;              // 总操作次数
    uint32_t successful_operations;         // 成功操作次数
    uint32_t failed_operations;             // 失败操作次数
    uint32_t retry_operations;              // 重试操作次数
    uint32_t timeout_errors;                // 超时错误次数
    uint32_t bus_errors;                    // 总线错误次数
    uint32_t validation_operations;         // 验证操作次数
    uint32_t validation_first_match;        // 前两次读取一致次数
    uint32_t validation_third_match;        // 第三次读取匹配次数
    uint32_t validation_failures;           // 验证失败次数
    uint32_t last_error_time;               // 最后错误时间
    HAL_StatusTypeDef last_error_code;      // 最后错误代码
} I2C_ErrorStats_t;

void I2C_ZLG7290_Read(I2C_HandleTypeDef *I2Cx,uint8_t I2C_Addr,uint8_t addr,uint8_t *buf,uint8_t num);
void I2C_ZLG7290_Write(I2C_HandleTypeDef *I2Cx,uint8_t I2C_Addr,uint8_t addr,uint8_t *buf,uint8_t num);

// 新增的带重试机制的函数声明
I2C_Status_t I2C_ZLG7290_Read_WithRetry(I2C_HandleTypeDef *I2Cx, uint8_t I2C_Addr, 
                                         uint8_t addr, uint8_t *buf, uint8_t num);
I2C_Status_t I2C_ZLG7290_Write_WithRetry(I2C_HandleTypeDef *I2Cx, uint8_t I2C_Addr, 
                                          uint8_t addr, uint8_t *buf, uint8_t num);
I2C_Status_t I2C_ZLG7290_WriteOneByte_WithRetry(I2C_HandleTypeDef *I2Cx, uint8_t I2C_Addr, 
                                                 uint8_t addr, uint8_t value);

// I2C总线管理函数
void I2C_Bus_Reset(I2C_HandleTypeDef *I2Cx);
void I2C_Error_Recovery(I2C_HandleTypeDef *I2Cx);
HAL_StatusTypeDef I2C_Check_Bus_Status(I2C_HandleTypeDef *I2Cx);

// 错误统计和诊断函数
void I2C_Init_ErrorStats(void);
I2C_ErrorStats_t* I2C_Get_ErrorStats(void);
void I2C_Reset_ErrorStats(void);
void I2C_Print_ErrorStats(void);

// 新的键盘读取验证机制
I2C_Status_t I2C_ZLG7290_Read_WithValidation(I2C_HandleTypeDef *I2Cx, uint8_t I2C_Addr, 
                                              uint8_t addr, uint8_t *buf, uint8_t num);

#endif /* __ZLG7290_H */

/**
* @}
*/

/**
* @}
*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/ 