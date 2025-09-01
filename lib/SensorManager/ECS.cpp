/**
 * @file ECS.cpp
 * @brief 环境传感器硬件抽象类实现 - 专为RTOS架构优化的轻量级实现
 * @version 5.0.0 - RTOS架构适配版本
 */

#include <Wire.h>
#include <SensorManager.h>
#include <LogManager/LogManager.h>

/**
 * @brief ECS类构造函数
 *
 * 初始化环境传感器硬件抽象实例，设置基本参数。
 * 构造函数只进行基本的参数设置，不执行硬件初始化。
 *
 * @note 硬件初始化需要调用begin()方法完成
 * @note 在RTOS架构中，传感器配置通常是预定义的
 */
ECS::ECS() :
    _isInitialized(false),
    _readCount(0),
    _lastReadTime(0),
    _lastErrorCode(ErrorCode::SUCCESS),
    _lastErrorMessage(""),
    _lastErrorTime(0) {

    LOG_TRACE("ECS: 环境传感器硬件抽象实例创建");
}

/**
 * @brief 初始化环境传感器硬件
 *
 * 执行传感器硬件初始化，包括BME280传感器配置。
 * 在RTOS架构中，保持简洁的初始化流程。
 *
 * @return ErrorCode::SUCCESS 初始化成功
 * @return ErrorCode::SENSOR_INITIALIZATION_FAILED 初始化失败
 * @note 必须在使用其他功能前调用此方法
 * @note 在RTOS架构中，复杂的健康检查由SensorTask处理
 */
ErrorCode ECS::begin() {
    LOG_TRACE("ECS: 开始初始化环境传感器硬件");

    // 修复：初始化独立的I2C1总线，使用专用引脚避免与OLED显示屏冲突
    Wire1.begin(BME280_SDA_PIN, BME280_SCL_PIN);
    Wire1.setClock(100000); // 设置I2C时钟频率为100kHz，提高稳定性
    delay(100); // 给I2C总线一些初始化时间

    LOG_VERBOSE("ECS: I2C1总线初始化完成 - SDA:%d, SCL:%d", BME280_SDA_PIN, BME280_SCL_PIN);

    // 初始化BME280传感器（修复：使用Wire1独立总线）
    if (!_bme280.begin(0x76, &Wire1)) {
        return reportError(ErrorCode::SENSOR_INITIALIZATION_FAILED, "BME280传感器初始化失败");
    }

    // 重置统计数据
    _readCount = 0;
    _lastReadTime = 0;

    // 清除错误状态
    clearLastError();

    // 标记为已初始化
    _isInitialized = true;

    LOG_NOTICE("ECS: 环境传感器硬件初始化完成 - 使用I2C1总线");
    return ErrorCode::SUCCESS;
}

/**
     * @brief 获取传感器读取总次数
     *
     * 返回自传感器初始化以来的累计读取次数，用于统计和诊断。
     *
     * @return 累计读取次数
     * @note 计数器在传感器重新初始化时会重置
     */
unsigned long ECS::getReadCount() const{
    return _readCount;
}

/**
 * @brief 读取温度值
 *
 * 执行温度测量流程，包括BME280传感器读取、数据转换和内置验证。
 * 在RTOS架构中，此方法由SensorTask周期性调用。
 * 内置数据验证确保返回值的可靠性。
 *
 * @return 温度值(°C)，失败时返回-999.0
 * @note 测量范围：-40°C至+85°C，精度：±1°C
 * @note 内置数据验证，与SMS、WDS、LIS类保持一致的设计模式
 */
float ECS::readTemperature() {
    // 检查传感器是否已初始化
    if (!_isInitialized) {
        reportError(ErrorCode::NOT_INITIALIZED, "传感器未初始化");
        return -999.0f;
    }

    LOG_TRACE("ECS: 开始读取温度，BME280传感器");

    // 读取BME280传感器原始数据
    const float rawTemp = _bme280.readTemperature();

    // 检查读数是否合理（使用内置验证逻辑）
    if (isnan(rawTemp) || rawTemp < -40.0f || rawTemp > 85.0f) {
        const String errorMsg = "温度读数超出有效范围: " + String(rawTemp, 2) + "°C (有效范围: -40°C至+85°C)";
        reportError(ErrorCode::SENSOR_OUT_OF_RANGE, errorMsg);
        return -999.0f;
    }

    // 更新统计数据
    _readCount++;
    _lastReadTime = millis();

    // 清除错误状态（成功读取）
    clearLastError();

    LOG_TRACE("ECS: 温度读取成功 - 温度: %.2f°C, 采集次数: %lu", rawTemp, _readCount);

    return rawTemp;
}

/**
 * @brief 读取湿度值
 *
 * 执行湿度测量流程，包括BME280传感器读取、数据转换和内置验证。
 * 在RTOS架构中，此方法由SensorTask周期性调用。
 * 内置数据验证确保返回值的可靠性。
 *
 * @return 湿度值(%)，失败时返回-1.0
 * @note 测量范围：0-100%，精度：±3%
 * @note 内置数据验证，与SMS、WDS、LIS类保持一致的设计模式
 */
float ECS::readHumidity() {
    // 检查传感器是否已初始化
    if (!_isInitialized) {
        reportError(ErrorCode::NOT_INITIALIZED, "传感器未初始化");
        return -1.0f;
    }

    LOG_TRACE("ECS: 开始读取湿度，BME280传感器");

    // 读取BME280传感器原始数据
    const float rawHumidity = _bme280.readHumidity();

    // 检查读数是否合理（使用内置验证逻辑）
    if (isnan(rawHumidity) || rawHumidity < 0.0f || rawHumidity > 100.0f) {
        const String errorMsg = "湿度读数超出有效范围: " + String(rawHumidity, 2) + "% (有效范围: 0-100%)";
        reportError(ErrorCode::SENSOR_OUT_OF_RANGE, errorMsg);
        return -1.0f;
    }

    // 更新统计数据
    _readCount++;
    _lastReadTime = millis();

    // 清除错误状态（成功读取）
    clearLastError();

    LOG_TRACE("ECS: 湿度读取成功 - 湿度: %.2f%%, 采集次数: %lu", rawHumidity, _readCount);

    return rawHumidity;
}

/**
 * @brief 读取气压值
 *
 * 执行气压测量流程，包括BME280传感器读取、数据转换和内置验证。
 * 内置数据验证确保返回值的可靠性。
 *
 * @return 气压值(hPa)，失败时返回-1.0
 * @note 测量范围：300-1100 hPa，精度：±1 hPa
 * @note 内置数据验证，与SMS、WDS、LIS类保持一致的设计模式
 */
float ECS::readPressure() {
    // 检查传感器是否已初始化
    if (!_isInitialized) {
        reportError(ErrorCode::NOT_INITIALIZED, "传感器未初始化");
        return -1.0f;
    }

    LOG_TRACE("ECS: 开始读取气压，BME280传感器");

    // 读取BME280传感器原始数据并转换为hPa
    const float rawPressure = _bme280.readPressure() / 100.0f;

    // 检查读数是否合理（使用内置验证逻辑）
    if (isnan(rawPressure) || rawPressure < 300.0f || rawPressure > 1100.0f) {
        const String errorMsg = "气压读数超出有效范围: " + String(rawPressure, 2) + " hPa (有效范围: 300-1100 hPa)";
        reportError(ErrorCode::SENSOR_OUT_OF_RANGE, errorMsg);
        return -1.0f;
    }

    // 更新统计数据
    _readCount++;
    _lastReadTime = millis();

    // 清除错误状态（成功读取）
    clearLastError();

    LOG_TRACE("ECS: 气压读取成功 - 气压: %.2f hPa, 采集次数: %lu", rawPressure, _readCount);

    return rawPressure;
}

/**
 * @brief 验证环境传感器数据有效性
 *
 * 静态方法，用于验证温度、湿度、气压值是否在合理范围内。
 * 可以独立使用，不依赖于ECS实例。
 * 与SMS、WDS、LIS类保持一致的设计模式。
 *
 * @param temperature 要验证的温度值
 * @param humidity 要验证的湿度值
 * @param pressure 要验证的气压值
 * @return true 数据有效（所有值都在合理范围内）
 * @return false 数据无效（超出范围或异常）
 *
 * @note 温度有效范围：-40°C至+85°C
 * @note 湿度有效范围：0-100%
 * @note 气压有效范围：300-1100 hPa
 * @note 此方法为静态方法，在SensorTask中用于统一验证
 */
bool ECS::isDataValid(const float temperature, const float humidity, const float pressure) {
    return temperature >= -40.0f && temperature <= 85.0f &&
        humidity >= 0.0f && humidity <= 100.0f &&
        pressure >= 300.0f && pressure <= 1100.0f &&
        !isnan(temperature) && !isnan(humidity) && !isnan(pressure);
}

// ==================== 错误处理接口 ====================

/**
 * @brief 获取最后一次错误码
 *
 * @return ErrorCode 最后发生的错误码
 * @note ErrorCode::SUCCESS 表示无错误
 */
ErrorCode ECS::getLastErrorCode() const {
    return _lastErrorCode;
}

/**
 * @brief 获取最后一次错误信息
 *
 * @return 错误信息的详细描述
 * @note 返回空字符串表示无错误
 */
String ECS::getLastErrorMessage() const {
    return _lastErrorMessage;
}

/**
 * @brief 清除错误状态
 *
 * 清除最后一次错误记录，重置错误状态为成功。
 *
 * @note 在RTOS架构中，错误恢复主要由SensorTask处理
 */
void ECS::clearLastError() {
    _lastErrorCode = ErrorCode::SUCCESS;
    _lastErrorMessage = "";
    _lastErrorTime = 0;

    LOG_TRACE("ECS: 错误状态已清除");
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
ErrorCode ECS::reportError(ErrorCode code, const String& message) {
    _lastErrorCode = code;
    _lastErrorMessage = message;
    _lastErrorTime = millis();

    // 根据错误严重程度记录不同级别的日志
    if (code >= ErrorCode::CRITICAL_ERROR) {
        LOG_FATAL("ECS严重错误: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else if (code >= ErrorCode::SYSTEM_ERROR) {
        LOG_ERROR("ECS系统错误: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else if (code >= ErrorCode::HARDWARE_ERROR) {
        LOG_WARNING("ECS硬件警告: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else if (code >= ErrorCode::SENSOR_READ_ERROR) {
        LOG_WARNING("ECS传感器警告: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else {
        LOG_NOTICE("ECS通知: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    }

    return code;
}