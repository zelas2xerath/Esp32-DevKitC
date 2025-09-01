/**
 * @file LightManager.h
 * @brief 简化的智能光照管理器 - 核心光照分析功能
 * @version 2.0.0 (重构版本)
 * @date 2025-07-21
 *
 */

#pragma once

#include <LightManagementConfig.h>
#include <cstddef>  // 包含size_t定义

// 前向声明，避免头文件包含冲突
class String;

// ==================== 简化的枚举定义 ====================

/**
 * @brief 光照等级枚举
 */
enum class LightLevel {
    EXTREME_DARK = 0,   // 极暗：0-10 lux
    DARK = 1,           // 暗：10-100 lux
    INDOOR = 2,         // 室内光：100-1000 lux
    BRIGHT = 3,         // 明亮：1000-10000 lux
    INTENSE = 4         // 强光：10000+ lux
};

/**
 * @brief 昼夜模式枚举
 */
enum class DayNightMode {
    DAY = 0,            // 白天模式
    NIGHT = 1,          // 夜间模式
    UNKNOWN = 255       // 未知模式
};

/**
 * @brief 设备工作模式枚举
 */
enum class DeviceMode {
    ENERGY_SAVING = 0,  // 节能模式
    NORMAL = 1,         // 正常模式
    HIGH_BRIGHTNESS = 2,// 高亮模式
    AUTO = 3            // 自动模式
};

// ==================== 简化的数据结构 ====================

/**
 * @brief 光照分析结果结构
 */
struct LightAnalysisResult {
    LightLevel currentLevel;        // 当前光照等级
    DayNightMode dayNightMode;     // 昼夜模式
    float averageIntensity;        // 平均光照强度
    unsigned long analysisTime;    // 分析时间戳
    bool isReliable;              // 分析结果是否可靠
    
    LightAnalysisResult() : currentLevel(LightLevel::EXTREME_DARK), 
                           dayNightMode(DayNightMode::UNKNOWN),
                           averageIntensity(0.0f), analysisTime(0), isReliable(false) {}
};

/**
 * @brief 模式切换状态结构
 */
struct ModeSwitchState {
    DeviceMode currentMode;         // 当前模式
    DeviceMode targetMode;          // 目标模式
    unsigned long switchStartTime;  // 切换开始时间
    int confirmationCount;          // 确认计数
    bool isSwitching;              // 是否正在切换
    
    ModeSwitchState() : currentMode(DeviceMode::AUTO), targetMode(DeviceMode::AUTO),
                       switchStartTime(0), confirmationCount(0), isSwitching(false) {}
};

// ==================== 简化的光照管理器类 ====================

/**
 * @brief 简化的智能光照管理器
 * 
 * 提供核心的光照分析和管理功能：
 * - 光照等级分析和分类
 * - 昼夜模式识别
 * - 自动模式切换
 * - 基本的统计功能
 */
class LightManager {
private:
    // 数据存储
    float* _lightHistory;               // 光照历史数据
    size_t _historySize;                // 历史数据大小
    size_t _historyIndex;               // 当前索引
    size_t _historyCount;               // 有效数据数量
    
    // 状态管理
    bool _initialized;                  // 是否已初始化
    LightAnalysisResult _lastAnalysis;  // 最后一次分析结果
    ModeSwitchState _modeSwitchState;   // 模式切换状态
    
    // 时间管理
    unsigned long _lastUpdateTime;      // 上次更新时间
    unsigned long _lastMaintenanceTime; // 上次维护时间
    
    // 统计信息
    unsigned long _totalSamples;        // 总样本数
    unsigned long _modeSwitchCount;     // 模式切换次数

public:
    /**
     * @brief 构造函数
     */
    LightManager();
    
    /**
     * @brief 析构函数
     */
    ~LightManager();
    
    /**
     * @brief 初始化光照管理器
     * @return true 初始化成功，false 初始化失败
     */
    bool begin();
    
    /**
     * @brief 更新光照数据并执行分析
     * @param lightIntensity 光照强度（lux）
     * @return true 更新成功，false 更新失败
     */
    bool updateLightData(float lightIntensity);
    
    /**
     * @brief 获取当前光照分析结果
     * @return 光照分析结果结构
     */
    const LightAnalysisResult& getCurrentAnalysis() const;
    
    /**
     * @brief 获取当前设备工作模式
     * @return 当前设备工作模式
     */
    DeviceMode getCurrentMode() const;
    
    /**
     * @brief 手动设置设备工作模式
     * @param mode 目标工作模式
     * @return true 设置成功，false 设置失败
     */
    bool setManualMode(DeviceMode mode);
    
    /**
     * @brief 启用自动模式
     * @return true 启用成功，false 启用失败
     */
    bool enableAutoMode();
    
    /**
     * @brief 获取光照等级字符串描述
     * @param level 光照等级
     * @return 光照等级的中文描述
     */
    static String getLightLevelString(LightLevel level);
    
    /**
     * @brief 获取昼夜模式字符串描述
     * @param mode 昼夜模式
     * @return 昼夜模式的中文描述
     */
    static String getDayNightModeString(DayNightMode mode);
    
    /**
     * @brief 获取设备模式字符串描述
     * @param mode 设备模式
     * @return 设备模式的中文描述
     */
    static String getDeviceModeString(DeviceMode mode);
    
    /**
     * @brief 获取状态摘要
     * @return 包含当前状态信息的字符串
     */
    String getStatusSummary() const;
    
    /**
     * @brief 执行维护操作
     */
    void maintenance();
    
    /**
     * @brief 检查是否已初始化
     * @return true 已初始化，false 未初始化
     */
    bool isInitialized() const { return _initialized; }

private:
    // 私有方法
    LightLevel analyzeLightLevel(float intensity) const;
    DayNightMode analyzeDayNightMode(float intensity) const;
    float calculateAverageIntensity() const;
    DeviceMode determineTargetMode(const LightAnalysisResult& analysis) const;
    bool shouldSwitchMode(DeviceMode targetMode);
    void executeModeSwitch(DeviceMode newMode);
    void addToHistory(float intensity);
};

// ==================== 全局光照管理器实例 ====================

extern LightManager* globalLightManager;
