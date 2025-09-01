/**
 * @file LightManager.cpp
 * @brief 简化的智能光照管理器实现
 * @version 2.0.0 (重构版本)
 * @date 2025-07-21
 */

#include <Arduino.h>
#include <LightManager.h>
#include <LogManager/LogManager.h>

// 全局光照管理器实例
LightManager* globalLightManager = nullptr;

// ==================== 构造函数和析构函数 ====================

LightManager::LightManager() 
    : _lightHistory(nullptr), _historySize(LIGHT_DATA_HISTORY_SIZE), 
      _historyIndex(0), _historyCount(0), _initialized(false),
      _lastUpdateTime(0), _lastMaintenanceTime(0), 
      _totalSamples(0), _modeSwitchCount(0) {
    
    _lightHistory = new float[_historySize];
    if (_lightHistory) {
        for (size_t i = 0; i < _historySize; i++) {
            _lightHistory[i] = 0.0f;
        }
    }
}

LightManager::~LightManager() {
    if (_lightHistory) {
        delete[] _lightHistory;
        _lightHistory = nullptr;
    }
}

// ==================== 初始化和基本操作 ====================

bool LightManager::begin() {
    if (_initialized) {
        LOG_WARNING("光照管理器已经初始化");
        return true;
    }
    
    if (!ENABLE_LIGHT_MANAGEMENT) {
        LOG_NOTICE("光照管理功能已禁用");
        return false;
    }
    
    if (!_lightHistory) {
        LOG_ERROR("光照管理器内存分配失败");
        return false;
    }
    
    LOG_NOTICE("初始化智能光照管理系统");
    
    // 初始化状态
    _lastAnalysis = LightAnalysisResult();
    _modeSwitchState = ModeSwitchState();
    
    // 初始化时间戳
    unsigned long currentTime = millis();
    _lastUpdateTime = currentTime;
    _lastMaintenanceTime = currentTime;
    
    _initialized = true;
    
    LOG_NOTICE("智能光照管理系统初始化成功");
    return true;
}

bool LightManager::updateLightData(float lightIntensity) {
    if (!_initialized) {
        LOG_ERROR("光照管理器未初始化");
        return false;
    }
    
    // 基本数据验证
    if (lightIntensity < 0.0f || lightIntensity > 100000.0f) {
        LOG_WARNING("无效的光照数据: %.2f", lightIntensity);
        return false;
    }
    
    unsigned long currentTime = millis();
    
    // 检查更新频率限制
    if (currentTime - _lastUpdateTime < LIGHT_MANAGEMENT_UPDATE_INTERVAL_MS) {
        return true; // 跳过此次更新
    }
    
    // 添加到历史数据
    addToHistory(lightIntensity);
    
    // 执行光照分析
    LightAnalysisResult newAnalysis;
    newAnalysis.currentLevel = analyzeLightLevel(lightIntensity);
    newAnalysis.dayNightMode = analyzeDayNightMode(lightIntensity);
    newAnalysis.averageIntensity = calculateAverageIntensity();
    newAnalysis.analysisTime = currentTime;
    newAnalysis.isReliable = (_historyCount >= 3); // 至少需要3个样本
    
    // 更新模式切换状态
    if (ENABLE_AUTO_MODE_SWITCHING && _modeSwitchState.currentMode == DeviceMode::AUTO) {
        DeviceMode targetMode = determineTargetMode(newAnalysis);
        if (shouldSwitchMode(targetMode)) {
            executeModeSwitch(targetMode);
        }
    }
    
    // 更新分析结果
    _lastAnalysis = newAnalysis;
    _lastUpdateTime = currentTime;
    _totalSamples++;
    
    return true;
}

// ==================== 分析方法 ====================

LightLevel LightManager::analyzeLightLevel(float intensity) const {
    if (intensity <= LIGHT_LEVEL_EXTREME_DARK_MAX) {
        return LightLevel::EXTREME_DARK;
    } else if (intensity <= LIGHT_LEVEL_DARK_MAX) {
        return LightLevel::DARK;
    } else if (intensity <= LIGHT_LEVEL_INDOOR_MAX) {
        return LightLevel::INDOOR;
    } else if (intensity <= LIGHT_LEVEL_BRIGHT_MAX) {
        return LightLevel::BRIGHT;
    } else {
        return LightLevel::INTENSE;
    }
}

DayNightMode LightManager::analyzeDayNightMode(float intensity) const {
    if (intensity >= DAYNIGHT_THRESHOLD_DAY) {
        return DayNightMode::DAY;
    } else if (intensity <= DAYNIGHT_THRESHOLD_NIGHT) {
        return DayNightMode::NIGHT;
    } else {
        return DayNightMode::UNKNOWN;
    }
}

float LightManager::calculateAverageIntensity() const {
    if (_historyCount == 0) {
        return 0.0f;
    }
    
    float sum = 0.0f;
    for (size_t i = 0; i < _historyCount; i++) {
        sum += _lightHistory[i];
    }
    
    return sum / _historyCount;
}

// ==================== 模式切换方法 ====================

DeviceMode LightManager::determineTargetMode(const LightAnalysisResult& analysis) const {
    float intensity = analysis.averageIntensity;
    
    if (intensity <= MODE_SWITCH_TO_ENERGY_SAVING) {
        return DeviceMode::ENERGY_SAVING;
    } else if (intensity >= MODE_SWITCH_TO_HIGH_BRIGHTNESS) {
        return DeviceMode::HIGH_BRIGHTNESS;
    } else {
        return DeviceMode::NORMAL;
    }
}

bool LightManager::shouldSwitchMode(DeviceMode targetMode) {
    if (_modeSwitchState.currentMode == targetMode) {
        _modeSwitchState.isSwitching = false;
        _modeSwitchState.confirmationCount = 0;
        return false;
    }
    
    unsigned long currentTime = millis();
    
    // 检查目标模式是否改变
    if (_modeSwitchState.targetMode != targetMode) {
        _modeSwitchState.targetMode = targetMode;
        _modeSwitchState.confirmationCount = 1;
        _modeSwitchState.switchStartTime = currentTime;
        _modeSwitchState.isSwitching = true;
        return false;
    }
    
    // 检查延迟时间
    if ((currentTime - _modeSwitchState.switchStartTime) < MODE_SWITCH_DELAY_MS) {
        return false;
    }
    
    // 增加确认计数
    _modeSwitchState.confirmationCount++;
    
    // 检查是否达到确认次数要求
    return (_modeSwitchState.confirmationCount >= MODE_SWITCH_CONFIRMATION_COUNT);
}

void LightManager::executeModeSwitch(DeviceMode newMode) {
    DeviceMode oldMode = _modeSwitchState.currentMode;
    _modeSwitchState.currentMode = newMode;
    _modeSwitchState.isSwitching = false;
    _modeSwitchState.confirmationCount = 0;
    
    _modeSwitchCount++;
    
    LOG_NOTICE("设备模式切换: %s -> %s", 
              getDeviceModeString(oldMode).c_str(),
              getDeviceModeString(newMode).c_str());
}

// ==================== 公共接口方法 ====================

const LightAnalysisResult& LightManager::getCurrentAnalysis() const {
    return _lastAnalysis;
}

DeviceMode LightManager::getCurrentMode() const {
    return _modeSwitchState.currentMode;
}

bool LightManager::setManualMode(DeviceMode mode) {
    if (!_initialized) {
        return false;
    }
    
    if (mode == DeviceMode::AUTO) {
        return enableAutoMode();
    }
    
    DeviceMode oldMode = _modeSwitchState.currentMode;
    _modeSwitchState.currentMode = mode;
    _modeSwitchState.isSwitching = false;
    _modeSwitchState.confirmationCount = 0;
    
    LOG_NOTICE("手动设置设备模式: %s -> %s", 
              getDeviceModeString(oldMode).c_str(),
              getDeviceModeString(mode).c_str());
    
    return true;
}

bool LightManager::enableAutoMode() {
    if (!_initialized) {
        return false;
    }
    
    _modeSwitchState.currentMode = DeviceMode::AUTO;
    _modeSwitchState.isSwitching = false;
    _modeSwitchState.confirmationCount = 0;
    
    LOG_NOTICE("启用自动模式");
    return true;
}

// ==================== 静态字符串转换方法 ====================

String LightManager::getLightLevelString(LightLevel level) {
    switch (level) {
        case LightLevel::EXTREME_DARK: return "极暗";
        case LightLevel::DARK: return "暗";
        case LightLevel::INDOOR: return "室内光";
        case LightLevel::BRIGHT: return "明亮";
        case LightLevel::INTENSE: return "强光";
        default: return "未知";
    }
}

String LightManager::getDayNightModeString(DayNightMode mode) {
    switch (mode) {
        case DayNightMode::DAY: return "白天";
        case DayNightMode::NIGHT: return "夜间";
        default: return "未知";
    }
}

String LightManager::getDeviceModeString(DeviceMode mode) {
    switch (mode) {
        case DeviceMode::ENERGY_SAVING: return "节能模式";
        case DeviceMode::NORMAL: return "正常模式";
        case DeviceMode::HIGH_BRIGHTNESS: return "高亮模式";
        case DeviceMode::AUTO: return "自动模式";
        default: return "未知模式";
    }
}

// ==================== 辅助方法 ====================

void LightManager::addToHistory(float intensity) {
    if (!_lightHistory) return;
    
    _lightHistory[_historyIndex] = intensity;
    _historyIndex = (_historyIndex + 1) % _historySize;
    
    if (_historyCount < _historySize) {
        _historyCount++;
    }
}

String LightManager::getStatusSummary() const {
    if (!_initialized) {
        return "光照管理器未初始化";
    }
    
    String summary = "光照管理器状态: ";
    summary += "等级=" + getLightLevelString(_lastAnalysis.currentLevel);
    summary += ", 模式=" + getDeviceModeString(_modeSwitchState.currentMode);
    summary += ", 样本=" + String(_totalSamples);
    summary += ", 切换=" + String(_modeSwitchCount);
    
    return summary;
}

void LightManager::maintenance() {
    if (!_initialized) return;
    
    unsigned long currentTime = millis();
    
    // 每5分钟执行一次维护
    if (currentTime - _lastMaintenanceTime > 300000) {
        LOG_TRACE("执行光照管理器维护操作");
        _lastMaintenanceTime = currentTime;
    }
}
