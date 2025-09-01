/**
 * @file DataFilterConfig.h
 * @brief 简化的数据滤波器配置文件
 * @version 2.0.0 (重构版本)
 * @date 2025-07-21
 * 
 * 重构说明：
 * - 移除过度复杂的配置参数
 * - 保留核心的滤波配置
 * - 简化配置结构，提升可维护性
 */

#pragma once

// ==================== 滤波功能总开关 ====================

/**
 * @brief 数据滤波功能控制
 */
#define ENABLE_DATA_FILTERING           false    // 总开关：是否启用数据滤波

// ==================== 基础滤波参数配置 ====================

/**
 * @brief SMS土壤湿度传感器滤波配置
 */
#define SMS_FILTER_WINDOW_SIZE          5       // 滤波窗口大小
#define SMS_OUTLIER_THRESHOLD           200.0f  // 异常值阈值

/**
 * @brief WDS水位传感器滤波配置
 */
#define WDS_FILTER_WINDOW_SIZE          5       // 滤波窗口大小
#define WDS_OUTLIER_THRESHOLD           20.0f   // 异常值阈值（cm）

/**
 * @brief LIS光照传感器滤波配置
 */
#define LIS_FILTER_WINDOW_SIZE          5       // 滤波窗口大小
#define LIS_OUTLIER_THRESHOLD           1000.0f // 异常值阈值（lux）

/**
 * @brief ECS环境传感器滤波配置
 */
#define ECS_FILTER_WINDOW_SIZE          5       // 滤波窗口大小
#define ECS_TEMP_OUTLIER_THRESHOLD      10.0f   // 温度异常值阈值（°C）
#define ECS_HUMIDITY_OUTLIER_THRESHOLD  20.0f   // 湿度异常值阈值（%）
#define ECS_PRESSURE_OUTLIER_THRESHOLD  50.0f   // 气压异常值阈值（hPa）

// ==================== 调试配置 ====================

/**
 * @brief 调试和日志配置
 */
#define ENABLE_FILTER_DEBUG_LOG         false   // 是否启用滤波器调试日志
