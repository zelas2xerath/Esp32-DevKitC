/**
 * @file LIS.cpp
 * @brief 光照传感器硬件抽象类实现 - 专为RTOS架构优化的轻量级实现
 * @version 4.0.0 - RTOS架构适配版本
 */

#include <SensorManager.h>
#include <LogManager/LogManager.h>

/**
 * @brief LIS类构造函数
 *
 * 初始化光照传感器硬件抽象实例，设置基本参数。
 * 构造函数只进行基本的参数设置，不执行硬件初始化。
 *
 * @note 硬件初始化需要调用begin()方法完成
 * @note 在RTOS架构中，传感器配置通常是预定义的
 */
LIS::LIS() :
    _isInitialized(false),
    _readCount(0),
    _lastLightIntensity(0.0f),
    _lastReadTime(0),
    _lastErrorCode(ErrorCode::SUCCESS),
    _lastErrorMessage(""),
    _lastErrorTime(0) {

    LOG_TRACE("LIS: 光照传感器硬件抽象实例创建");
}

/**
 * @brief 初始化光照传感器硬件
 *
 * 执行传感器硬件初始化，包括BH1750传感器配置。
 * 在RTOS架构中，保持简洁的初始化流程。
 *
 * @return ErrorCode::SUCCESS 初始化成功
 * @return ErrorCode::SENSOR_INITIALIZATION_FAILED 初始化失败
 * @note 必须在使用其他功能前调用此方法
 * @note 在RTOS架构中，复杂的健康检查由SensorTask处理
 */
ErrorCode LIS::begin() {
    LOG_TRACE("LIS: 开始初始化光照传感器硬件");

    // 修复：使用与BME280相同的I2C1总线，共享引脚配置
    // 注意：为了确保I2C1总线正确初始化，这里重新初始化（如果已初始化则无影响）
    Wire1.begin(BH1750_SDA_PIN, BH1750_SCL_PIN);
    Wire1.setClock(100000); // 设置I2C时钟频率为100kHz
    delay(50); // 给I2C总线一些初始化时间

    LOG_VERBOSE("LIS: 使用I2C1总线 - SDA:%d, SCL:%d", BH1750_SDA_PIN, BH1750_SCL_PIN);

    // 初始化BH1750传感器（修复：使用Wire1独立总线）
    if (!_bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire1)) {
        return reportError(ErrorCode::SENSOR_INITIALIZATION_FAILED, "BH1750传感器初始化失败");
    }

    // 重置统计数据
    _readCount = 0;
    _lastLightIntensity = 0.0f;
    _lastReadTime = 0;

    // 清除错误状态
    clearLastError();

    // 标记为已初始化
    _isInitialized = true;

    LOG_NOTICE("LIS: 光照传感器硬件初始化完成");
    return ErrorCode::SUCCESS;
}

/**
 * @brief 读取光照传感器数值
 *
 * 执行光照强度测量流程，包括BH1750传感器读取、数据转换和内置验证。
 * 在RTOS架构中，此方法由SensorTask周期性调用。
 * 内置数据验证确保返回值的可靠性。
 *
 * @return 光照强度值(lux)，失败时返回-1.0
 * @note 测量范围：0-65535 lux，精度：±20%
 * @note 在RTOS环境中，调用频率由SensorTask控制
 * @note 内置数据验证，与SMS、WDS类保持一致的设计模式
 */
float LIS::readLightLevel() {
    // 检查传感器是否已初始化
    if (!_isInitialized) {
        reportError(ErrorCode::NOT_INITIALIZED, "传感器未初始化");
        return -1.0f;
    }

    LOG_TRACE("LIS: 开始读取光照强度，BH1750传感器");

    // 读取BH1750传感器原始数据
    const float rawLight = _bh1750.readLightLevel();

    // 使用内置数据验证方法
    if (!isDataValid(rawLight)) {
        String errorMsg = "光照强度读数超出有效范围: " + String(rawLight, 2) + " lux (有效范围: 0-65535 lux)";
        reportError(ErrorCode::SENSOR_OUT_OF_RANGE, errorMsg);
        return _lastLightIntensity; // 返回上次有效值
    }

    // 更新统计数据
    _lastLightIntensity = rawLight;
    _readCount++;
    _lastReadTime = millis();

    // 清除错误状态（成功读取）
    clearLastError();

    LOG_TRACE("LIS: 光照强度读取成功 - 强度: %.2f lux, 采集次数: %lu",
              _lastLightIntensity, _readCount);

    return _lastLightIntensity;
}

/**
 * @brief 获取传感器累计读取次数
 *
 * 返回自传感器初始化以来的累计读取次数，用于统计分析和诊断。
 * 计数器在每次成功调用readLightLevel()时递增。
 *
 * @return 累计读取次数
 *
 * @note 计数器在传感器重新初始化时会重置为0
 * @note 失败的读取操作不会增加计数
 */
unsigned long LIS::getReadCount() const {
    return _readCount;
}

/**
 * @brief 验证光照强度数据有效性
 *
 * 静态方法，用于验证光照强度值是否在合理范围内。
 * 可以独立使用，不依赖于LIS实例。
 * 与SMS、WDS类保持一致的设计模式。
 *
 * @param lightLevel 要验证的光照强度值
 * @return true 数据有效（0-65535 lux范围内）
 * @return false 数据无效（超出范围）
 *
 * @note 有效范围：0-65535 lux（包含边界值）
 * @note 此方法为静态方法，在SensorTask中用于统一验证
 */
bool LIS::isDataValid(float lightLevel) {
    return (lightLevel >= 0.0f && lightLevel <= 65535.0f);
}

/**
 * @brief 白天/夜晚检测
 *
 * 检测当前是否为白天，使用简单的阈值判断。
 *
 * @return true 白天(>10 lux)，false 夜晚
 * @note 在RTOS架构中，复杂的光照分析由业务逻辑层处理
 */
bool LIS::isDaytime() const {
    return _lastLightIntensity > 10.0f;
}

// ==================== 错误处理接口 ====================

/**
 * @brief 获取最后一次错误码
 *
 * @return ErrorCode 最后发生的错误码
 * @note ErrorCode::SUCCESS 表示无错误
 */
ErrorCode LIS::getLastErrorCode() const {
    return _lastErrorCode;
}

/**
 * @brief 获取最后一次错误信息
 *
 * @return 错误信息的详细描述
 * @note 返回空字符串表示无错误
 */
String LIS::getLastErrorMessage() const {
    return _lastErrorMessage;
}

/**
 * @brief 清除错误状态
 *
 * 清除最后一次错误记录，重置错误状态为成功。
 *
 * @note 在RTOS架构中，错误恢复主要由SensorTask处理
 */
void LIS::clearLastError() {
    _lastErrorCode = ErrorCode::SUCCESS;
    _lastErrorMessage = "";
    _lastErrorTime = 0;

    LOG_TRACE("LIS: 错误状态已清除");
}

// ==================== 私有方法实现 ====================

/**
 * @brief 报告错误并记录日志
 *
 * 统一的错误处理方法，记录错误信息。
 *
 * @param code 错误码
 * @param message 错误描述信息
 * @return ErrorCode 传入的错误码（便于链式调用）
 *
 * @note 在RTOS架构中，复杂的错误恢复由SensorTask处理
 */
ErrorCode LIS::reportError(ErrorCode code, const String& message) {
    _lastErrorCode = code;
    _lastErrorMessage = message;
    _lastErrorTime = millis();

    // 根据错误严重程度记录不同级别的日志
    if (code >= ErrorCode::CRITICAL_ERROR) {
        LOG_FATAL("LIS严重错误: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else if (code >= ErrorCode::SYSTEM_ERROR) {
        LOG_ERROR("LIS系统错误: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else if (code >= ErrorCode::HARDWARE_ERROR) {
        LOG_WARNING("LIS硬件警告: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else if (code >= ErrorCode::SENSOR_READ_ERROR) {
        LOG_WARNING("LIS传感器警告: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else {
        LOG_NOTICE("LIS通知: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    }

    return code;
}