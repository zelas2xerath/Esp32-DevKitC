/**
 * @file MSP20.cpp
 * @brief MSP20水深压力传感器驱动库实现
 * @author zelas2xerath
 * @date 2025-08-21
 * @version 2.1.0
 *
 * MSP20水深压力传感器精简驱动库的具体实现
 * 专注核心功能，简洁高效的实现
 */

#include <MSP20.h>
#include <algorithm>
#include <cmath>

// 日志系统依赖
#ifdef LOG_TRACE
    // 使用现有的日志系统
#else
    // 如果没有日志系统，定义空宏
    #define LOG_TRACE(...)
    #define LOG_NOTICE(...)
    #define LOG_WARNING(...)
    #define LOG_ERROR(...)
#endif

// ==================== 构造函数 ====================

MSP20::MSP20(const uint8_t pin, const uint8_t samplesCount) :
    _pin(pin),
    _samplesCount(std::max(MIN_SAMPLES, std::min(samplesCount, MAX_SAMPLES))),
    _isInitialized(false),
    _zeroOffset(0.0),
    _scaleFactor(1.0) {

    LOG_TRACE("MSP20: 构造函数 - 引脚:%d, 采样次数:%d", _pin, _samplesCount);
}

// ==================== 初始化与配置 ====================

bool MSP20::begin() {
    LOG_NOTICE("MSP20: 开始初始化传感器，引脚:%d", _pin);

    // 验证引脚有效性
    if (_pin > 39) {  // ESP32 ADC引脚范围检查
        LOG_ERROR("MSP20: 无效的ADC引脚:%d", _pin);
        return false;
    }

    // 初始化成功
    _isInitialized = true;

    LOG_NOTICE("MSP20: 传感器初始化成功 - 引脚:%d", _pin);
    return true;
}

// ==================== 数据读取接口 ====================

double MSP20::readVoltage() {
    if (!_isInitialized) {
        LOG_WARNING("MSP20: 传感器未初始化，无法读取电压值");
        return 0.0;
    }

    const double adcValue = readStableADC();
    if (adcValue == 0.0) {
        return 0.0;  // 读取失败
    }

    const double voltage = adcToVoltage(static_cast<uint16_t>(adcValue));

    // 验证电压值有效性
    if (!validateReading(voltage, MIN_VALID_VOLTAGE, MAX_VALID_VOLTAGE)) {
        LOG_WARNING("MSP20: 电压值超出有效范围:%.3fV", voltage);
        return 0.0;
    }

    LOG_TRACE("MSP20: 电压值:%.3fV", voltage);
    return voltage;
}

double MSP20::readPressure() {
    const double voltage = readVoltage();
    if (voltage == 0.0) {
        return 0.0;  // 读取失败
    }

    const double pressure = voltageToPressure(voltage);

    LOG_TRACE("MSP20: 压力值:%.2fkPa", pressure);
    return pressure;
}

double MSP20::readWaterDepth() {
    const double voltage = readVoltage();
    if (voltage == 0.0) {
        return 0.0;  // 读取失败
    }

    // 直接从电压计算水深值（使用正确公式）
    double depth = voltageToDepth(voltage);

    // 应用校准参数
    depth = applyCalibration(depth);

    // 验证水深值有效性
    if (!validateReading(depth, MIN_VALID_DEPTH, MAX_VALID_DEPTH)) {
        LOG_WARNING("MSP20: 水深值超出有效范围:%.2fcm", depth);
        return 0.0;
    }

    LOG_TRACE("MSP20: 水深值:%.2fcm", depth);
    return depth;
}

double MSP20::readWaterDepthMeters() {
    const double depthCm = readWaterDepth();
    const double depthM = depthCm / 100.0;

    LOG_TRACE("MSP20: 水深值:%.3fm", depthM);
    return depthM;
}

// ==================== 校准功能 ====================

bool MSP20::calibrate(const double zeroOffset, const double scaleFactor) {
    if (!_isInitialized) {
        LOG_WARNING("MSP20: 传感器未初始化，无法校准");
        return false;
    }

    if (scaleFactor <= 0.0) {
        LOG_WARNING("MSP20: 无效的比例因子:%.3f (必须大于0)", scaleFactor);
        return false;
    }

    _zeroOffset = zeroOffset;
    _scaleFactor = scaleFactor;

    LOG_NOTICE("MSP20: 校准参数设置 - 零点偏移:%.2fcm, 比例因子:%.3f",
               _zeroOffset, _scaleFactor);
    return true;
}

// ==================== 私有方法实现 ====================

double MSP20::adcToVoltage(const uint16_t adcValue) const {
    return (static_cast<double>(adcValue) / ADC_RESOLUTION) * WORKING_VOLTAGE;
}

double MSP20::voltageToPressure(const double voltage) const {
    // 压力 = 电压 * 6.2672 - 0.44178
    return voltage * PRESSURE_SLOPE + PRESSURE_OFFSET;
}

double MSP20::voltageToDepth(const double voltage) const {
    // 水深 = 电压 * 63.1 + 5.9739
    return voltage * DEPTH_SLOPE + DEPTH_OFFSET;
}

double MSP20::readStableADC() const {
    double total = 0.0;

    for (uint8_t i = 0; i < _samplesCount; i++) {
        total += analogRead(_pin);
        if (i < _samplesCount - 1) {
            delayMicroseconds(50);  // 简单的采样间隔
        }
    }

    return total / _samplesCount;
}

bool MSP20::validateReading(const double value, const double minValue, const double maxValue) const {
    return !isnan(value) && !isinf(value) && value >= minValue && value <= maxValue;
}

double MSP20::applyCalibration(const double rawValue) const {
    return (rawValue + _zeroOffset) * _scaleFactor;
}
