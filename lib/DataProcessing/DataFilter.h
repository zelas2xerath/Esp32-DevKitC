/**
 * @file DataFilter.h
 * @brief 简化的传感器数据滤波器 - 核心滤波功能
 * @version 2.0.0 (重构版本)
 * @date 2025-07-21
 * 
 * 重构说明：
 * - 移除过度设计的模板和复杂配置
 * - 保留核心的移动平均和异常值检测功能
 * - 简化API接口，提升可维护性
 * - 统一使用LogManager进行日志记录
 */

#pragma once

#include <DataFilterConfig.h>
#include <cstddef>  // 包含size_t定义

// 前向声明，避免头文件包含冲突
class String;

// ==================== 简化的滤波器类 ====================

/**
 * @brief 简化的数据滤波器
 * 
 * 提供基本的数据滤波功能：
 * - 移动平均滤波
 * - 异常值检测和处理
 * - 数据有效性验证
 */
class SimpleDataFilter {
    float* _buffer;           // 数据缓冲区
    size_t _windowSize;       // 窗口大小
    size_t _index;            // 当前索引
    size_t _count;            // 有效数据数量
    float _lastValue;         // 上次输出值
    float _outlierThreshold;  // 异常值阈值
    bool _initialized;        // 是否已初始化

public:
    /**
     * @brief 构造函数
     * @param windowSize 滤波窗口大小
     * @param outlierThreshold 异常值阈值
     */
    SimpleDataFilter(size_t windowSize = 5, float outlierThreshold = 1000.0f);
    
    /**
     * @brief 析构函数
     */
    ~SimpleDataFilter();
    
    /**
     * @brief 滤波处理
     * @param rawValue 原始数据
     * @return 滤波后的数据
     */
    float filter(float rawValue);
    
    /**
     * @brief 重置滤波器
     */
    void reset();
    
    /**
     * @brief 检查是否已初始化
     */
    bool isInitialized() const { return _initialized; }

private:
    /**
     * @brief 计算移动平均值
     */
    float calculateAverage() const;
    
    /**
     * @brief 检测异常值
     */
    bool isOutlier(float value) const;
};

// ==================== 滤波器管理器 ====================

/**
 * @brief 简化的滤波器管理器
 * 
 * 管理所有传感器的滤波器实例
 */
class DataFilterManager {
private:
    SimpleDataFilter* _smsFilter;    // SMS滤波器
    SimpleDataFilter* _wdsFilter;    // WDS滤波器
    SimpleDataFilter* _lisFilter;    // LIS滤波器
    SimpleDataFilter* _ecsTempFilter;    // ECS温度滤波器
    SimpleDataFilter* _ecsHumidityFilter; // ECS湿度滤波器
    SimpleDataFilter* _ecsPressureFilter; // ECS气压滤波器
    
    bool _initialized;               // 是否已初始化
    unsigned long _lastMaintenanceTime; // 上次维护时间

public:
    /**
     * @brief 构造函数
     */
    DataFilterManager();
    
    /**
     * @brief 析构函数
     */
    ~DataFilterManager();
    
    /**
     * @brief 初始化滤波器管理器
     * @return true 成功，false 失败
     */
    bool begin();
    
    /**
     * @brief SMS数据滤波
     * @param rawValue 原始湿度值
     * @return 滤波后的湿度值
     */
    int filterSMSData(int rawValue);
    
    /**
     * @brief WDS数据滤波
     * @param rawValue 原始水位值
     * @return 滤波后的水位值
     */
    double filterWDSData(double rawValue);
    
    /**
     * @brief LIS数据滤波
     * @param rawValue 原始光照值
     * @return 滤波后的光照值
     */
    float filterLISData(float rawValue);
    
    /**
     * @brief ECS数据滤波
     * @param rawTemp 原始温度
     * @param rawHumidity 原始湿度
     * @param rawPressure 原始气压
     * @param filteredTemp 滤波后温度
     * @param filteredHumidity 滤波后湿度
     * @param filteredPressure 滤波后气压
     * @return true 成功，false 失败
     */
    bool filterECSData(float rawTemp, float rawHumidity, float rawPressure,
                       float& filteredTemp, float& filteredHumidity, float& filteredPressure);
    
    /**
     * @brief 重置所有滤波器
     */
    void resetAllFilters();
    
    /**
     * @brief 维护操作
     */
    void maintenance();
    
    /**
     * @brief 检查是否已初始化
     */
    bool isInitialized() const { return _initialized; }
    
    /**
     * @brief 获取状态摘要
     */
    String getStatusSummary() const;
};

// ==================== 全局实例 ====================

extern DataFilterManager* globalDataFilterManager;
