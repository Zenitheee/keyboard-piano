#ifndef __DATA_VALIDATOR_H
#define __DATA_VALIDATOR_H

#include "stm32f4xx_hal.h"

/* 数据验证结果枚举 */
typedef enum {
    DATA_VALID = 0,          // 数据有效
    DATA_INVALID_RANGE,      // 数据超出范围
    DATA_INVALID_MAGIC,      // 魔数无效
    DATA_INVALID_NULL,       // 空指针
    DATA_INVALID_CHECKSUM,   // 校验和错误
    DATA_CORRUPTED          // 数据损坏
} DataValidationResult_t;

/* 按键值验证宏 */
#define IS_VALID_KEY(key) ((key) <= 9 || (key) == 0xFF)
#define IS_VALID_NOTE_KEY(key) ((key) >= 1 && (key) <= 9)
#define IS_VALID_CLEAR_KEY(key) ((key) == 0)

/* 显示位置验证宏 */
#define IS_VALID_DISPLAY_POS(pos) ((pos) <= 8)

/* 状态验证宏 */
#define IS_VALID_SYSTEM_STATE(state) ((state) >= STATE_IDLE && (state) <= STATE_ERROR_HANDLE)
#define IS_VALID_SYSTEM_EVENT(event) ((event) >= EVENT_NONE && (event) <= EVENT_AUDIO_STOP)

/* 数组边界检查宏 */
#define IS_VALID_ARRAY_INDEX(index, max_size) ((index) < (max_size))

/* 数据验证函数声明 */
DataValidationResult_t Validate_KeyValue(uint8_t key);
DataValidationResult_t Validate_DisplayPosition(uint8_t position);
DataValidationResult_t Validate_DisplayBuffer(uint8_t* buffer, uint8_t size);
DataValidationResult_t Validate_SystemState(uint8_t state);
DataValidationResult_t Validate_SystemEvent(uint8_t event);
DataValidationResult_t Validate_NoteIndex(uint8_t note_index);

/* HotStartState data validation function */
DataValidationResult_t Validate_HotStart_Data(void* data);

/* 安全数据访问函数 */
uint8_t Safe_Get_KeyValue(uint8_t key, uint8_t default_value);
uint8_t Safe_Get_DisplayPosition(uint8_t position, uint8_t default_value);
uint8_t Safe_Get_NoteIndex(uint8_t index, uint8_t default_value);

/* 数据修复函数 */
void Repair_DisplayBuffer(uint8_t* buffer, uint8_t size);
void Repair_HotStartData(void);

/* 调试相关宏定义 */
#ifdef DEBUG_DATA_VALIDATION
    #define DATA_VALIDATION_LOG(fmt, ...) printf("[DATA_VAL] " fmt, ##__VA_ARGS__)
#else
    #define DATA_VALIDATION_LOG(fmt, ...)
#endif

#endif /* __DATA_VALIDATOR_H */ 