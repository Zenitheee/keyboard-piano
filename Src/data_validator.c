#include "data_validator.h"
#include "state_machine.h"
#include <string.h>

/* 外部变量声明 */
extern uint8_t display_buffer[8];
extern uint8_t display_position;
extern const uint8_t seg7code[10];
extern uint32_t Calculate_Checksum(void* state);
/* 使用和main.c相同的魔数值 */
#define MAGIC_NUMBER 0xDEADBEEF

/* 热启动状态结构定义 - 与main.c中相同 */
typedef struct {
    uint32_t magic_number;      // 魔数，用于验证数据有效性
    uint8_t current_note;       // 当前播放的音符
    uint8_t is_playing;         // 是否正在播放
    uint32_t play_duration;     // 播放持续时间
    uint8_t display_buffer[8];  // 显示缓冲区状态
    uint8_t display_position;   // 显示位置状态
    uint32_t checksum;          // 校验和
} HotStartState_t;

/**
 * @brief 验证按键值的合法性
 * @param key 按键值
 * @return 验证结果
 */
DataValidationResult_t Validate_KeyValue(uint8_t key) {
    if (IS_VALID_KEY(key)) {
        return DATA_VALID;
    }
    DATA_VALIDATION_LOG("Invalid key value: 0x%02X\n", key);
    return DATA_INVALID_RANGE;
}

/**
 * @brief 验证显示位置的合法性
 * @param position 显示位置
 * @return 验证结果
 */
DataValidationResult_t Validate_DisplayPosition(uint8_t position) {
    if (IS_VALID_DISPLAY_POS(position)) {
        return DATA_VALID;
    }
    DATA_VALIDATION_LOG("Invalid display position: %d\n", position);
    return DATA_INVALID_RANGE;
}

/**
 * @brief 验证显示缓冲区的合法性
 * @param buffer 显示缓冲区指针
 * @param size 缓冲区大小
 * @return 验证结果
 */
DataValidationResult_t Validate_DisplayBuffer(uint8_t* buffer, uint8_t size) {
    if (buffer == NULL) {
        DATA_VALIDATION_LOG("Display buffer is NULL\n");
        return DATA_INVALID_NULL;
    }
    
    if (size != 8) {
        DATA_VALIDATION_LOG("Invalid display buffer size: %d\n", size);
        return DATA_INVALID_RANGE;
    }
    
    // 检查缓冲区数据的合理性（每个字节应该是有效的7段码或0x00）
    for (uint8_t i = 0; i < size; i++) {
        if (buffer[i] != 0x00) {
            // 检查是否为有效的7段数码管编码
            uint8_t is_valid_seg7 = 0;
            for (uint8_t j = 0; j < 10; j++) {
                if (buffer[i] == seg7code[j]) {
                    is_valid_seg7 = 1;
                    break;
                }
            }
            if (!is_valid_seg7) {
                DATA_VALIDATION_LOG("Invalid seg7 code at position %d: 0x%02X\n", i, buffer[i]);
                return DATA_CORRUPTED;
            }
        }
    }
    
    return DATA_VALID;
}

/**
 * @brief 验证系统状态的合法性
 * @param state 系统状态
 * @return 验证结果
 */
DataValidationResult_t Validate_SystemState(uint8_t state) {
    if (IS_VALID_SYSTEM_STATE(state)) {
        return DATA_VALID;
    }
    DATA_VALIDATION_LOG("Invalid system state: %d\n", state);
    return DATA_INVALID_RANGE;
}

/**
 * @brief 验证系统事件的合法性
 * @param event 系统事件
 * @return 验证结果
 */
DataValidationResult_t Validate_SystemEvent(uint8_t event) {
    if (IS_VALID_SYSTEM_EVENT(event)) {
        return DATA_VALID;
    }
    DATA_VALIDATION_LOG("Invalid system event: %d\n", event);
    return DATA_INVALID_RANGE;
}

/**
 * @brief 验证音符索引的合法性
 * @param note_index 音符索引
 * @return 验证结果
 */
DataValidationResult_t Validate_NoteIndex(uint8_t note_index) {
    if (note_index < 9) {
        return DATA_VALID;
    }
    DATA_VALIDATION_LOG("Invalid note index: %d\n", note_index);
    return DATA_INVALID_RANGE;
}

/**
 * @brief 验证热启动数据的合法性
 * @param data 热启动数据指针 (HotStartState_t*)
 * @return 验证结果
 */
DataValidationResult_t Validate_HotStart_Data(void* data) {
    if (data == NULL) {
        DATA_VALIDATION_LOG("HotStart data is NULL\n");
        return DATA_INVALID_NULL;
    }
    
    // 类型转换
    HotStartState_t* state = (HotStartState_t*)data;
    
    // 验证魔数
    if (state->magic_number != MAGIC_NUMBER) {
        DATA_VALIDATION_LOG("Invalid magic number: 0x%08X\n", state->magic_number);
        return DATA_INVALID_MAGIC;
    }
    
    // 验证校验和
    uint32_t calculated_checksum = Calculate_Checksum(state);
    if (calculated_checksum != state->checksum) {
        DATA_VALIDATION_LOG("Checksum mismatch: expected 0x%08X, got 0x%08X\n", 
                           state->checksum, calculated_checksum);
        return DATA_INVALID_CHECKSUM;
    }
    
    // 验证各个字段
    if (Validate_KeyValue(state->current_note) != DATA_VALID) {
        DATA_VALIDATION_LOG("Invalid current_note in HotStart data: %d\n", state->current_note);
        return DATA_CORRUPTED;
    }
    
    if (state->is_playing > 1) {
        DATA_VALIDATION_LOG("Invalid is_playing in HotStart data: %d\n", state->is_playing);
        return DATA_CORRUPTED;
    }
    
    if (Validate_DisplayPosition(state->display_position) != DATA_VALID) {
        DATA_VALIDATION_LOG("Invalid display_position in HotStart data: %d\n", 
                           state->display_position);
        return DATA_CORRUPTED;
    }
    
    // 验证显示缓冲区
    if (Validate_DisplayBuffer(state->display_buffer, 8) != DATA_VALID) {
        DATA_VALIDATION_LOG("Invalid display buffer in HotStart data\n");
        return DATA_CORRUPTED;
    }
    
    return DATA_VALID;
}

/**
 * @brief 安全获取按键值
 * @param key 原始按键值
 * @param default_value 默认值
 * @return 安全的按键值
 */
uint8_t Safe_Get_KeyValue(uint8_t key, uint8_t default_value) {
    if (Validate_KeyValue(key) == DATA_VALID) {
        return key;
    }
    DATA_VALIDATION_LOG("Using default key value: 0x%02X\n", default_value);
    return default_value;
}

/**
 * @brief 安全获取显示位置
 * @param position 原始显示位置
 * @param default_value 默认值
 * @return 安全的显示位置
 */
uint8_t Safe_Get_DisplayPosition(uint8_t position, uint8_t default_value) {
    if (Validate_DisplayPosition(position) == DATA_VALID) {
        return position;
    }
    DATA_VALIDATION_LOG("Using default display position: %d\n", default_value);
    return default_value;
}

/**
 * @brief 安全获取音符索引
 * @param index 原始音符索引
 * @param default_value 默认值
 * @return 安全的音符索引
 */
uint8_t Safe_Get_NoteIndex(uint8_t index, uint8_t default_value) {
    if (Validate_NoteIndex(index) == DATA_VALID) {
        return index;
    }
    DATA_VALIDATION_LOG("Using default note index: %d\n", default_value);
    return default_value;
}

/**
 * @brief 修复显示缓冲区
 * @param buffer 显示缓冲区指针
 * @param size 缓冲区大小
 */
void Repair_DisplayBuffer(uint8_t* buffer, uint8_t size) {
    if (buffer == NULL || size != 8) {
        return;
    }
    
    // 修复无效的显示数据
    for (uint8_t i = 0; i < size; i++) {
        if (buffer[i] != 0x00) {
            // 检查是否为有效的7段数码管编码
            uint8_t is_valid_seg7 = 0;
            for (uint8_t j = 0; j < 10; j++) {
                if (buffer[i] == seg7code[j]) {
                    is_valid_seg7 = 1;
                    break;
                }
            }
            if (!is_valid_seg7) {
                // 无效数据，清零
                buffer[i] = 0x00;
                DATA_VALIDATION_LOG("Repaired invalid data at display position %d\n", i);
            }
        }
    }
}

/**
 * @brief 修复热启动数据
 */
void Repair_HotStartData(void) {
    // 验证和修复显示缓冲区
    if (Validate_DisplayBuffer(display_buffer, 8) != DATA_VALID) {
        Repair_DisplayBuffer(display_buffer, 8);
    }
    
    // 验证和修复显示位置
    if (Validate_DisplayPosition(display_position) != DATA_VALID) {
        display_position = Safe_Get_DisplayPosition(display_position, 0);
        DATA_VALIDATION_LOG("Repaired display position to: %d\n", display_position);
    }
    
    DATA_VALIDATION_LOG("Hot start data repair completed\n");
} 