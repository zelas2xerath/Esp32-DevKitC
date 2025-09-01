/**
 * @file LightManagementConfig.h
 * @brief 简化的智能光照管理系统配置文件
 * @version 2.0.0 (重构版本)
 * @date 2025-07-21
 * 
 * 重构说明：
 * - 移除过度复杂的配置参数
 * - 保留核心的光照管理配置
 * - 简化配置结构，提升可维护性
 */

#pragma once

// ==================== 光照管理功能总开关 ====================

/**
 * @brief 光照管理功能控制
 */
#define ENABLE_LIGHT_MANAGEMENT         false    // 总开关：是否启用智能光照管理
#define ENABLE_AUTO_MODE_SWITCHING      true    // 自动模式切换

// ==================== 光照等级阈值配置 ====================

/**
 * @brief 光照等级分类阈值（单位：lux）
 */
#define LIGHT_LEVEL_EXTREME_DARK_MAX    10.0f   // 极暗：0-10 lux
#define LIGHT_LEVEL_DARK_MAX            100.0f  // 暗：10-100 lux
#define LIGHT_LEVEL_INDOOR_MAX          1000.0f // 室内光：100-1000 lux
#define LIGHT_LEVEL_BRIGHT_MAX          10000.0f// 明亮：1000-10000 lux

// 昼夜检测阈值
#define DAYNIGHT_THRESHOLD_DAY          50.0f   // 白天阈值
#define DAYNIGHT_THRESHOLD_NIGHT        20.0f   // 夜间阈值

// ==================== 模式切换配置 ====================

/**
 * @brief 模式切换阈值配置
 */
#define MODE_SWITCH_TO_ENERGY_SAVING    LIGHT_LEVEL_EXTREME_DARK_MAX  // 切换到节能模式的阈值
#define MODE_SWITCH_TO_HIGH_BRIGHTNESS  LIGHT_LEVEL_BRIGHT_MAX        // 切换到高亮模式的阈值

/**
 * @brief 模式切换防抖动配置
 */
#define MODE_SWITCH_DELAY_MS            3000    // 模式切换延迟时间（毫秒）
#define MODE_SWITCH_CONFIRMATION_COUNT  2       // 模式切换确认次数

// ==================== 性能配置 ====================

/**
 * @brief 性能配置
 */
#define LIGHT_MANAGEMENT_UPDATE_INTERVAL_MS     1000    // 光照管理更新间隔（毫秒）
#define LIGHT_DATA_HISTORY_SIZE                 10      // 光照数据历史记录大小
