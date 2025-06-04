#include "zlg7290.h"
#include <stdio.h>

/* Maximum Timeout values for flags and events waiting loops. These timeouts are
not based on accurate values, they just guarantee that the application will 
not remain stuck if the I2C communication is corrupted.
You may modify these timeout values depending on CPU frequency and application
conditions (interrupts routines ...). */   
#define I2C_Open_FLAG_TIMEOUT         ((uint32_t)0x1000)

#define I2C_Open_LONG_TIMEOUT         ((uint32_t)0xffff)

__IO uint32_t  I2CTimeout = I2C_Open_LONG_TIMEOUT;

// I2C错误统计全局变量
static I2C_ErrorStats_t i2c_error_stats = {0};

/**
 * @brief 初始化I2C错误统计
 */
void I2C_Init_ErrorStats(void) {
    memset(&i2c_error_stats, 0, sizeof(I2C_ErrorStats_t));
}

/**
 * @brief 获取I2C错误统计
 * @return 指向错误统计结构的指针
 */
I2C_ErrorStats_t* I2C_Get_ErrorStats(void) {
    return &i2c_error_stats;
}

/**
 * @brief 重置I2C错误统计
 */
void I2C_Reset_ErrorStats(void) {
    I2C_Init_ErrorStats();
}

/**
 * @brief 打印I2C错误统计信息
 */
void I2C_Print_ErrorStats(void) {
    printf("\n=== I2C Error Statistics ===\n");
    printf("Total Operations: %lu\n", i2c_error_stats.total_operations);
    printf("Successful Operations: %lu\n", i2c_error_stats.successful_operations);
    printf("Failed Operations: %lu\n", i2c_error_stats.failed_operations);
    printf("Retry Operations: %lu\n", i2c_error_stats.retry_operations);
    printf("Timeout Errors: %lu\n", i2c_error_stats.timeout_errors);
    printf("Bus Errors: %lu\n", i2c_error_stats.bus_errors);
    printf("Last Error Code: %d\n", i2c_error_stats.last_error_code);
    printf("Success Rate: %.2f%%\n", 
           i2c_error_stats.total_operations > 0 ? 
           (float)i2c_error_stats.successful_operations * 100.0f / i2c_error_stats.total_operations : 0.0f);
    printf("============================\n\n");
}

/**
 * @brief 检查I2C总线状态
 * @param I2Cx I2C句柄
 * @return HAL状态
 */
HAL_StatusTypeDef I2C_Check_Bus_Status(I2C_HandleTypeDef *I2Cx) {
    // 检查I2C状态
    if (I2Cx->State == HAL_I2C_STATE_READY) {
        return HAL_OK;
    }
    
    // 检查是否有错误标志
    if (__HAL_I2C_GET_FLAG(I2Cx, I2C_FLAG_BERR) || 
        __HAL_I2C_GET_FLAG(I2Cx, I2C_FLAG_ARLO) || 
        __HAL_I2C_GET_FLAG(I2Cx, I2C_FLAG_AF)) {
        return HAL_ERROR;
    }
    
    return HAL_BUSY;
}

/**
 * @brief I2C总线复位
 * @param I2Cx I2C句柄
 */
void I2C_Bus_Reset(I2C_HandleTypeDef *I2Cx) {
    // 禁用I2C
    __HAL_I2C_DISABLE(I2Cx);
    
    // 清除所有错误标志
    __HAL_I2C_CLEAR_FLAG(I2Cx, I2C_FLAG_BERR);
    __HAL_I2C_CLEAR_FLAG(I2Cx, I2C_FLAG_ARLO);
    __HAL_I2C_CLEAR_FLAG(I2Cx, I2C_FLAG_AF);
    __HAL_I2C_CLEAR_FLAG(I2Cx, I2C_FLAG_OVR);
    
    // 延时
    HAL_Delay(I2C_RESET_DELAY_MS);
    
    // 重新启用I2C
    __HAL_I2C_ENABLE(I2Cx);
    
    // 重置I2C状态
    I2Cx->State = HAL_I2C_STATE_READY;
    I2Cx->ErrorCode = HAL_I2C_ERROR_NONE;
}

/**
 * @brief I2C错误恢复
 * @param I2Cx I2C句柄
 */
void I2C_Error_Recovery(I2C_HandleTypeDef *I2Cx) {
    // 执行总线复位
    I2C_Bus_Reset(I2Cx);
    
    // 如果仍有问题，尝试重新初始化I2C
    if (I2C_Check_Bus_Status(I2Cx) != HAL_OK) {
        HAL_I2C_DeInit(I2Cx);
        HAL_Delay(10);
        HAL_I2C_Init(I2Cx);
    }
}

/**
 * @brief 带重试机制的I2C读取函数
 * @param I2Cx I2C句柄
 * @param I2C_Addr 设备地址
 * @param addr 寄存器地址
 * @param buf 数据缓冲区
 * @param num 数据长度
 * @return I2C操作状态
 */
I2C_Status_t I2C_ZLG7290_Read_WithRetry(I2C_HandleTypeDef *I2Cx, uint8_t I2C_Addr, 
                                         uint8_t addr, uint8_t *buf, uint8_t num) {
    HAL_StatusTypeDef hal_status;
    uint8_t retry_count = 0;
    
    // 更新统计
    i2c_error_stats.total_operations++;
    
    while (retry_count <= I2C_MAX_RETRY_COUNT) {
        // 检查总线状态
        if (I2C_Check_Bus_Status(I2Cx) != HAL_OK) {
            I2C_Error_Recovery(I2Cx);
        }
        
        // 尝试I2C读取
        hal_status = HAL_I2C_Mem_Read(I2Cx, I2C_Addr, addr, I2C_MEMADD_SIZE_8BIT, 
                                      buf, num, I2CTimeout);
        
        if (hal_status == HAL_OK) {
            // 成功
            i2c_error_stats.successful_operations++;
            if (retry_count > 0) {
                i2c_error_stats.retry_operations++;
            }
            return I2C_STATUS_OK;
        }
        
        // 记录错误信息
        i2c_error_stats.last_error_code = hal_status;
        i2c_error_stats.last_error_time = HAL_GetTick();
        
        // 根据错误类型更新统计
        switch (hal_status) {
            case HAL_TIMEOUT:
                i2c_error_stats.timeout_errors++;
                break;
            case HAL_ERROR:
                i2c_error_stats.bus_errors++;
                break;
            default:
                break;
        }
        
        retry_count++;
        
        if (retry_count <= I2C_MAX_RETRY_COUNT) {
            // 重试前的恢复操作
            I2C_Error_Recovery(I2Cx);
            HAL_Delay(I2C_RETRY_DELAY_MS);
        }
    }
    
    // 所有重试都失败
    i2c_error_stats.failed_operations++;
    return I2C_STATUS_MAX_RETRY_EXCEEDED;
}

/**
 * @brief 带重试机制的I2C单字节写入函数
 * @param I2Cx I2C句柄
 * @param I2C_Addr 设备地址
 * @param addr 寄存器地址
 * @param value 要写入的值
 * @return I2C操作状态
 */
I2C_Status_t I2C_ZLG7290_WriteOneByte_WithRetry(I2C_HandleTypeDef *I2Cx, uint8_t I2C_Addr, 
                                                 uint8_t addr, uint8_t value) {
    HAL_StatusTypeDef hal_status;
    uint8_t retry_count = 0;
    
    // 更新统计
    i2c_error_stats.total_operations++;
    
    while (retry_count <= I2C_MAX_RETRY_COUNT) {
        // 检查总线状态
        if (I2C_Check_Bus_Status(I2Cx) != HAL_OK) {
            I2C_Error_Recovery(I2Cx);
        }
        
        // 尝试I2C写入
        hal_status = HAL_I2C_Mem_Write(I2Cx, I2C_Addr, addr, I2C_MEMADD_SIZE_8BIT, 
                                       &value, 1, I2CTimeout);
        
        if (hal_status == HAL_OK) {
            // 成功
            i2c_error_stats.successful_operations++;
            if (retry_count > 0) {
                i2c_error_stats.retry_operations++;
            }
            return I2C_STATUS_OK;
        }
        
        // 记录错误信息
        i2c_error_stats.last_error_code = hal_status;
        i2c_error_stats.last_error_time = HAL_GetTick();
        
        // 根据错误类型更新统计
        switch (hal_status) {
            case HAL_TIMEOUT:
                i2c_error_stats.timeout_errors++;
                break;
            case HAL_ERROR:
                i2c_error_stats.bus_errors++;
                break;
            default:
                break;
        }
        
        retry_count++;
        
        if (retry_count <= I2C_MAX_RETRY_COUNT) {
            // 重试前的恢复操作
            I2C_Error_Recovery(I2Cx);
            HAL_Delay(I2C_RETRY_DELAY_MS);
        }
    }
    
    // 所有重试都失败
    i2c_error_stats.failed_operations++;
    return I2C_STATUS_MAX_RETRY_EXCEEDED;
}

/**
 * @brief 带重试机制的I2C多字节写入函数
 * @param I2Cx I2C句柄
 * @param I2C_Addr 设备地址
 * @param addr 起始寄存器地址
 * @param buf 数据缓冲区
 * @param num 数据长度
 * @return I2C操作状态
 */
I2C_Status_t I2C_ZLG7290_Write_WithRetry(I2C_HandleTypeDef *I2Cx, uint8_t I2C_Addr, 
                                          uint8_t addr, uint8_t *buf, uint8_t num) {
    I2C_Status_t status;
    
    // 逐字节写入，每个字节都有重试机制
    for (uint8_t i = 0; i < num; i++) {
        status = I2C_ZLG7290_WriteOneByte_WithRetry(I2Cx, I2C_Addr, addr + i, buf[i]);
        
        if (status != I2C_STATUS_OK) {
            return status; // 如果任何一个字节写入失败，返回错误
        }
        
        // 字节间延时，确保ZLG7290有足够时间处理
        HAL_Delay(5);
    }
    
    return I2C_STATUS_OK;
}

/*******************************************************************************
* Function Name  : I2C_24C64_Read
* Description    : 
* Input          : 
* Output         : 
* Return         : 
* Attention      : None
*******************************************************************************/

void I2C_ZLG7290_Read(I2C_HandleTypeDef *I2Cx,uint8_t I2C_Addr,uint8_t addr,uint8_t *buf,uint8_t num)
{
    // 使用新的重试机制函数
    I2C_ZLG7290_Read_WithRetry(I2Cx, I2C_Addr, addr, buf, num);
}

/*******************************************************************************
* Function Name  : I2C_24C64_WriteOneByte
* Description    : 
* Input          : 
* Output         : None
* Return         : 
* Attention      : None
*******************************************************************************/

void I2C_ZLG7290_WriteOneByte(I2C_HandleTypeDef *I2Cx,uint8_t I2C_Addr,uint8_t addr,uint8_t value)
{   
    // 使用新的重试机制函数
    I2C_ZLG7290_WriteOneByte_WithRetry(I2Cx, I2C_Addr, addr, value);
}

/*******************************************************************************
* Function Name  : I2C_24C64_Write
* Description    : 
* Input          : 
* Output         : None
* Return         : 
* Attention      : None
*******************************************************************************/

void I2C_ZLG7290_Write(I2C_HandleTypeDef *I2Cx,uint8_t I2C_Addr,uint8_t addr,uint8_t *buf,uint8_t num)
{
    // 使用新的重试机制函数
    I2C_ZLG7290_Write_WithRetry(I2Cx, I2C_Addr, addr, buf, num);
}

/**
* @}
*/

/**
* @}
*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/ 