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
    printf("--- Validation Statistics ---\n");
    printf("Validation Operations: %lu\n", i2c_error_stats.validation_operations);
    printf("First Two Match: %lu\n", i2c_error_stats.validation_first_match);
    printf("Third Match: %lu\n", i2c_error_stats.validation_third_match);
    printf("Validation Failures: %lu\n", i2c_error_stats.validation_failures);
    printf("Last Error Code: %d\n", i2c_error_stats.last_error_code);
    printf("Success Rate: %.2f%%\n", 
           i2c_error_stats.total_operations > 0 ? 
           (float)i2c_error_stats.successful_operations * 100.0f / i2c_error_stats.total_operations : 0.0f);
    if (i2c_error_stats.validation_operations > 0) {
        printf("Validation Success Rate: %.2f%%\n", 
               (float)(i2c_error_stats.validation_first_match + i2c_error_stats.validation_third_match) * 100.0f / i2c_error_stats.validation_operations);
    }
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

/**
 * @brief 带数据验证的键盘读取函数
 * 读取两次键盘值,如果一致则使用,如果不一致则读取第三次,
 * 第三次读取的值如果与前两次的任意一次相同则使用第三次的数据,
 * 如果仍然不一致则算作读取失败
 * @param I2Cx I2C句柄
 * @param I2C_Addr 设备地址
 * @param addr 寄存器地址
 * @param buf 数据缓冲区
 * @param num 数据长度
 * @return I2C操作状态
 */
I2C_Status_t I2C_ZLG7290_Read_WithValidation(I2C_HandleTypeDef *I2Cx, uint8_t I2C_Addr, 
                                              uint8_t addr, uint8_t *buf, uint8_t num) {
    HAL_StatusTypeDef hal_status;
    uint8_t read_buffer1[8] = {0};  // 第一次读取缓冲区
    uint8_t read_buffer2[8] = {0};  // 第二次读取缓冲区
    uint8_t read_buffer3[8] = {0};  // 第三次读取缓冲区
    uint8_t read_count = 0;
    
    // 更新统计
    i2c_error_stats.total_operations++;
    i2c_error_stats.validation_operations++;
    
    // 限制数据长度，防止缓冲区溢出
    if (num > 8) {
        num = 8;
    }
    
    // 第一次读取
    hal_status = HAL_I2C_Mem_Read(I2Cx, I2C_Addr, addr, I2C_MEMADD_SIZE_8BIT, 
                                  read_buffer1, num, I2CTimeout);
    if (hal_status != HAL_OK) {
        // 第一次读取失败，记录错误并返回
        i2c_error_stats.last_error_code = hal_status;
        i2c_error_stats.last_error_time = HAL_GetTick();
        i2c_error_stats.failed_operations++;
        
        if (hal_status == HAL_TIMEOUT) {
            i2c_error_stats.timeout_errors++;
        } else if (hal_status == HAL_ERROR) {
            i2c_error_stats.bus_errors++;
        }
        
        return I2C_STATUS_ERROR;
    }
    read_count++;
    
    // 短暂延时，确保ZLG7290状态稳定
    HAL_Delay(2);
    
    // 第二次读取
    hal_status = HAL_I2C_Mem_Read(I2Cx, I2C_Addr, addr, I2C_MEMADD_SIZE_8BIT, 
                                  read_buffer2, num, I2CTimeout);
    if (hal_status != HAL_OK) {
        // 第二次读取失败，记录错误并返回
        i2c_error_stats.last_error_code = hal_status;
        i2c_error_stats.last_error_time = HAL_GetTick();
        i2c_error_stats.failed_operations++;
        
        if (hal_status == HAL_TIMEOUT) {
            i2c_error_stats.timeout_errors++;
        } else if (hal_status == HAL_ERROR) {
            i2c_error_stats.bus_errors++;
        }
        
        return I2C_STATUS_ERROR;
    }
    read_count++;
    
    // 比较前两次读取结果
    uint8_t data_match = 1;
    for (uint8_t i = 0; i < num; i++) {
        if (read_buffer1[i] != read_buffer2[i]) {
            data_match = 0;
            break;
        }
    }
    
    if (data_match) {
        // 前两次读取一致，使用第二次的数据
        for (uint8_t i = 0; i < num; i++) {
            buf[i] = read_buffer2[i];
        }
        
        // 更新成功统计
        i2c_error_stats.successful_operations++;
        i2c_error_stats.validation_first_match++;
        return I2C_STATUS_OK;
    }
    
    // 前两次读取不一致，进行第三次读取
    HAL_Delay(2);
    
    hal_status = HAL_I2C_Mem_Read(I2Cx, I2C_Addr, addr, I2C_MEMADD_SIZE_8BIT, 
                                  read_buffer3, num, I2CTimeout);
    if (hal_status != HAL_OK) {
        // 第三次读取失败，记录错误并返回
        i2c_error_stats.last_error_code = hal_status;
        i2c_error_stats.last_error_time = HAL_GetTick();
        i2c_error_stats.failed_operations++;
        
        if (hal_status == HAL_TIMEOUT) {
            i2c_error_stats.timeout_errors++;
        } else if (hal_status == HAL_ERROR) {
            i2c_error_stats.bus_errors++;
        }
        
        return I2C_STATUS_ERROR;
    }
    read_count++;
    
    // 检查第三次读取是否与前两次中的任意一次匹配
    uint8_t match_with_first = 1;
    uint8_t match_with_second = 1;
    
    // 与第一次比较
    for (uint8_t i = 0; i < num; i++) {
        if (read_buffer3[i] != read_buffer1[i]) {
            match_with_first = 0;
            break;
        }
    }
    
    // 与第二次比较
    for (uint8_t i = 0; i < num; i++) {
        if (read_buffer3[i] != read_buffer2[i]) {
            match_with_second = 0;
            break;
        }
    }
    
    if (match_with_first || match_with_second) {
        // 第三次读取与前两次中的某一次匹配，使用第三次的数据
        for (uint8_t i = 0; i < num; i++) {
            buf[i] = read_buffer3[i];
        }
        
        // 更新统计（有重试但最终成功）
        i2c_error_stats.successful_operations++;
        i2c_error_stats.retry_operations++;
        i2c_error_stats.validation_third_match++;
        
        return I2C_STATUS_OK;
    }
    
    // 三次读取都不一致，读取失败
    i2c_error_stats.failed_operations++;
    i2c_error_stats.validation_failures++;
    
    // 记录调试信息（可选）
    #ifdef DEBUG_I2C_VALIDATION
    printf("I2C Read Validation Failed:\n");
    printf("Read1: ");
    for (uint8_t i = 0; i < num; i++) {
        printf("0x%02X ", read_buffer1[i]);
    }
    printf("\nRead2: ");
    for (uint8_t i = 0; i < num; i++) {
        printf("0x%02X ", read_buffer2[i]);
    }
    printf("\nRead3: ");
    for (uint8_t i = 0; i < num; i++) {
        printf("0x%02X ", read_buffer3[i]);
    }
    printf("\n");
    #endif
    
    return I2C_STATUS_ERROR;
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