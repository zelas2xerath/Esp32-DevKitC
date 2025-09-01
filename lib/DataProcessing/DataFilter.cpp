/**
 * @file DataFilter.cpp
 * @brief 简化的传感器数据滤波器实现
 * @version 2.0.0 (重构版本)
 * @date 2025-07-21
 */

#include <Arduino.h>
#include <DataFilter.h>
#include <LogManager/LogManager.h>

// 全局滤波器管理器实例
DataFilterManager* globalDataFilterManager = nullptr;

// ==================== SimpleDataFilter实现 ====================

SimpleDataFilter::SimpleDataFilter(size_t windowSize, float outlierThreshold)
    : _windowSize(windowSize), _outlierThreshold(outlierThreshold),
      _index(0), _count(0), _lastValue(0.0f), _initialized(false) {
    
    _buffer = new float[_windowSize];
    if (_buffer) {
        for (size_t i = 0; i < _windowSize; i++) {
            _buffer[i] = 0.0f;
        }
        _initialized = true;
    } else {
        LOG_ERROR("滤波器内存分配失败");
    }
}

SimpleDataFilter::~SimpleDataFilter() {
    if (_buffer) {
        delete[] _buffer;
        _buffer = nullptr;
    }
}

float SimpleDataFilter::filter(float rawValue) {
    if (!_initialized) {
        return rawValue;
    }
    
    // 异常值检测
    if (_count > 0 && isOutlier(rawValue)) {
        LOG_WARNING("检测到异常值: %.2f, 使用上次值: %.2f", rawValue, _lastValue);
        return _lastValue;
    }
    
    // 添加到缓冲区
    _buffer[_index] = rawValue;
    _index = (_index + 1) % _windowSize;
    if (_count < _windowSize) {
        _count++;
    }
    
    // 计算移动平均
    float filteredValue = calculateAverage();
    _lastValue = filteredValue;
    
    return filteredValue;
}

void SimpleDataFilter::reset() {
    if (!_initialized) return;
    
    _index = 0;
    _count = 0;
    _lastValue = 0.0f;
    
    for (size_t i = 0; i < _windowSize; i++) {
        _buffer[i] = 0.0f;
    }
}

float SimpleDataFilter::calculateAverage() const {
    if (_count == 0) return 0.0f;
    
    float sum = 0.0f;
    for (size_t i = 0; i < _count; i++) {
        sum += _buffer[i];
    }
    
    return sum / _count;
}

bool SimpleDataFilter::isOutlier(float value) const {
    if (_count < 2) return false;
    
    float average = calculateAverage();
    float diff = std::abs(value - average);
    
    return diff > _outlierThreshold;
}

// ==================== DataFilterManager实现 ====================

DataFilterManager::DataFilterManager()
    : _smsFilter(nullptr), _wdsFilter(nullptr), _lisFilter(nullptr),
      _ecsTempFilter(nullptr), _ecsHumidityFilter(nullptr), _ecsPressureFilter(nullptr),
      _initialized(false), _lastMaintenanceTime(0) {
}

DataFilterManager::~DataFilterManager() {
    delete _smsFilter;
    delete _wdsFilter;
    delete _lisFilter;
    delete _ecsTempFilter;
    delete _ecsHumidityFilter;
    delete _ecsPressureFilter;
}

bool DataFilterManager::begin() {
    if (_initialized) {
        LOG_WARNING("滤波器管理器已经初始化");
        return true;
    }
    
    if (!ENABLE_DATA_FILTERING) {
        LOG_NOTICE("数据滤波功能已禁用");
        return false;
    }
    
    LOG_NOTICE("初始化数据滤波器管理器");
    
    try {
        // 创建各传感器滤波器
        _smsFilter = new SimpleDataFilter(SMS_FILTER_WINDOW_SIZE, SMS_OUTLIER_THRESHOLD);
        _wdsFilter = new SimpleDataFilter(WDS_FILTER_WINDOW_SIZE, WDS_OUTLIER_THRESHOLD);
        _lisFilter = new SimpleDataFilter(LIS_FILTER_WINDOW_SIZE, LIS_OUTLIER_THRESHOLD);
        _ecsTempFilter = new SimpleDataFilter(ECS_FILTER_WINDOW_SIZE, ECS_TEMP_OUTLIER_THRESHOLD);
        _ecsHumidityFilter = new SimpleDataFilter(ECS_FILTER_WINDOW_SIZE, ECS_HUMIDITY_OUTLIER_THRESHOLD);
        _ecsPressureFilter = new SimpleDataFilter(ECS_FILTER_WINDOW_SIZE, ECS_PRESSURE_OUTLIER_THRESHOLD);
        
        // 检查初始化状态
        if (!_smsFilter || !_smsFilter->isInitialized() ||
            !_wdsFilter || !_wdsFilter->isInitialized() ||
            !_lisFilter || !_lisFilter->isInitialized() ||
            !_ecsTempFilter || !_ecsTempFilter->isInitialized() ||
            !_ecsHumidityFilter || !_ecsHumidityFilter->isInitialized() ||
            !_ecsPressureFilter || !_ecsPressureFilter->isInitialized()) {
            
            LOG_ERROR("滤波器初始化失败");
            return false;
        }
        
        _initialized = true;
        _lastMaintenanceTime = millis();
        
        LOG_NOTICE("数据滤波器管理器初始化成功");
        return true;
        
    } catch (...) {
        LOG_ERROR("滤波器管理器初始化异常");
        return false;
    }
}

int DataFilterManager::filterSMSData(int rawValue) {
    if (!_initialized || !_smsFilter) {
        return rawValue;
    }
    
    float filtered = _smsFilter->filter(static_cast<float>(rawValue));
    return static_cast<int>(filtered);
}

double DataFilterManager::filterWDSData(double rawValue) {
    if (!_initialized || !_wdsFilter) {
        return rawValue;
    }
    
    float filtered = _wdsFilter->filter(static_cast<float>(rawValue));
    return static_cast<double>(filtered);
}

float DataFilterManager::filterLISData(float rawValue) {
    if (!_initialized || !_lisFilter) {
        return rawValue;
    }
    
    return _lisFilter->filter(rawValue);
}

bool DataFilterManager::filterECSData(float rawTemp, float rawHumidity, float rawPressure,
                                      float& filteredTemp, float& filteredHumidity, float& filteredPressure) {
    if (!_initialized || !_ecsTempFilter || !_ecsHumidityFilter || !_ecsPressureFilter) {
        filteredTemp = rawTemp;
        filteredHumidity = rawHumidity;
        filteredPressure = rawPressure;
        return false;
    }
    
    filteredTemp = _ecsTempFilter->filter(rawTemp);
    filteredHumidity = _ecsHumidityFilter->filter(rawHumidity);
    filteredPressure = _ecsPressureFilter->filter(rawPressure);
    
    return true;
}

void DataFilterManager::resetAllFilters() {
    if (!_initialized) return;
    
    if (_smsFilter) _smsFilter->reset();
    if (_wdsFilter) _wdsFilter->reset();
    if (_lisFilter) _lisFilter->reset();
    if (_ecsTempFilter) _ecsTempFilter->reset();
    if (_ecsHumidityFilter) _ecsHumidityFilter->reset();
    if (_ecsPressureFilter) _ecsPressureFilter->reset();
    
    LOG_NOTICE("所有滤波器已重置");
}

void DataFilterManager::maintenance() {
    if (!_initialized) return;
    
    unsigned long currentTime = millis();
    
    // 每5分钟执行一次维护
    if (currentTime - _lastMaintenanceTime > 300000) {
        LOG_TRACE("执行滤波器维护操作");
        _lastMaintenanceTime = currentTime;
    }
}

String DataFilterManager::getStatusSummary() const {
    if (!_initialized) {
        return "滤波器管理器未初始化";
    }
    
    String summary = "滤波器状态: ";
    summary += _smsFilter && _smsFilter->isInitialized() ? "SMS✓ " : "SMS✗ ";
    summary += _wdsFilter && _wdsFilter->isInitialized() ? "WDS✓ " : "WDS✗ ";
    summary += _lisFilter && _lisFilter->isInitialized() ? "LIS✓ " : "LIS✗ ";
    summary += _ecsTempFilter && _ecsTempFilter->isInitialized() ? "ECS✓" : "ECS✗";
    
    return summary;
}
