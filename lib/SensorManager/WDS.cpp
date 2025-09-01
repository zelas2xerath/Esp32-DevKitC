/**
 * @file WDS.cpp
 * @brief 水位传感器硬件抽象类实现 - 专为RTOS架构优化的轻量级实现
 * @version 3.0.0 - RTOS架构适配版本
 */

#include <SensorManager.h>
#include <LogManager/LogManager.h>

/**
 * @brief WDS类构造函数
 *
 * 初始化水位传感器硬件抽象实例，设置基本参数。
 * 构造函数只进行基本的参数设置，不执行硬件初始化。
 *
 * @param sensorPin ADC引脚编号，通常在编译时通过常量确定
 *
 * @note 硬件初始化需要调用begin()方法完成
 * @note 在RTOS架构中，引脚配置通常是预定义的
 */
WDS::WDS(const int sensorPin) :
    _msp20(sensorPin),
    _isInitialized(false),
    _readCount(0),
    _lastWaterDepth(0.0),
    _lastReadTime(0),
    _lastErrorCode(ErrorCode::SUCCESS),
    _lastErrorMessage(""),
    _lastErrorTime(0) {

    LOG_TRACE("WDS: 水位传感器硬件抽象实例创建，传感器引脚: %d", sensorPin);
}

/**
 * @brief 初始化水位传感器硬件
 *
 * 执行传感器硬件初始化，包括MSP20传感器配置。
 * 在RTOS架构中，保持简洁地初始化流程。
 *
 * @return ErrorCode::SUCCESS 初始化成功
 * @return ErrorCode::SENSOR_INITIALIZATION_FAILED 初始化失败
 * @note 必须在使用其他功能前调用此方法
 * @note 在RTOS架构中，复杂的健康检查由SensorTask处理
 */
ErrorCode WDS::begin() {
    LOG_TRACE("WDS: 开始初始化水位传感器硬件");

    // 初始化MSP20传感器
    if (!_msp20.begin()) {
        return reportError(ErrorCode::SENSOR_INITIALIZATION_FAILED,
                          "MSP20传感器初始化失败，请检查硬件连接");
    }

    // 重置统计数据
    _readCount = 0;
    _lastWaterDepth = 0.0;
    _lastReadTime = 0;

    // 清除错误状态
    clearLastError();

    // 标记为已初始化
    _isInitialized = true;

    LOG_NOTICE("WDS: 水位传感器硬件初始化完成");
    return ErrorCode::SUCCESS;
}

/**
 * @brief 读取水位传感器数值
 *
 * 执行水深测量流程，包括MSP20传感器读取、数据转换和内置验证。
 * 在RTOS架构中，此方法由SensorTask周期性调用。
 * 内置数据验证确保返回值的可靠性。
 *
 * @return 水深值(cm)，失败时返回-1.0
 * @note 测量范围：0-200cm，精度：±1cm
 * @note 在RTOS环境中，调用频率由SensorTask控制
 * @note 内置数据验证，与SMS类保持一致的设计模式
 */
double WDS::readWaterDepth() {
    // 检查传感器是否已初始化
    if (!_isInitialized) {
        reportError(ErrorCode::NOT_INITIALIZED, "传感器未初始化");
        return -1.0;
    }

    LOG_TRACE("WDS: 开始读取水深，MSP20传感器");

    // 读取MSP20传感器原始数据
    const double rawDepth = _msp20.readWaterDepth();

    // 使用内置数据验证方法
    if (!isDataValid(rawDepth)) {
        String errorMsg = "水深读数超出有效范围: " + String(rawDepth, 2) + "cm (有效范围: 0-200cm)";
        reportError(ErrorCode::SENSOR_OUT_OF_RANGE, errorMsg);
        return _lastWaterDepth; // 返回上次有效值
    }

    // 更新统计数据
    _lastWaterDepth = rawDepth;
    _readCount++;
    _lastReadTime = millis();

    // 清除错误状态（成功读取）
    clearLastError();

    LOG_TRACE("WDS: 水深读取成功 - 水深: %.2fcm, 采集次数: %lu",
              _lastWaterDepth, _readCount);

    return _lastWaterDepth;
}

/**
 * @brief 获取传感器累计读取次数
 *
 * 返回自传感器初始化以来的累计读取次数，用于统计分析和诊断。
 * 计数器在每次成功调用readWaterDepth()时递增。
 *
 * @return 累计读取次数
 *
 * @note 计数器在传感器重新初始化时会重置为0
 * @note 失败的读取操作不会增加计数
 */
unsigned long WDS::getReadCount() const {
    return _readCount;
}

/**
 * @brief 验证水深数据有效性
 *
 * 静态方法，用于验证水深值是否在合理范围内。
 * 可以独立使用，不依赖于WDS实例。
 * 与SMS类保持一致的设计模式。
 *
 * @param waterDepth 要验证的水深值
 * @return true 数据有效（0-200cm范围内）
 * @return false 数据无效（超出范围）
 *
 * @note 有效范围：0-200cm（包含边界值）
 * @note 此方法为静态方法，在SensorTask中用于统一验证
 */
bool WDS::isDataValid(double waterDepth) {
    return (waterDepth >= 0.0 && waterDepth <= 200.0);
}

/**
 * @brief 低水位检测
 *
 * 检测当前是否为低水位状态，使用简单的阈值判断。
 *
 * @return true 低水位(<1cm)，false 正常水位
 * @note 在RTOS架构中，复杂的趋势分析由业务逻辑层处理
 */
bool WDS::isLowWaterLevel() const {
    return _lastWaterDepth < 1.0;
}

// ==================== 错误处理接口 ====================

/**
 * @brief 获取最后一次错误码
 *
 * @return ErrorCode 最后发生的错误码
 * @note ErrorCode::SUCCESS 表示无错误
 */
ErrorCode WDS::getLastErrorCode() const {
    return _lastErrorCode;
}

/**
 * @brief 获取最后一次错误信息
 *
 * @return 错误信息的详细描述
 * @note 返回空字符串表示无错误
 */
String WDS::getLastErrorMessage() const {
    return _lastErrorMessage;
}

/**
 * @brief 清除错误状态
 *
 * 清除最后一次错误记录，重置错误状态为成功。
 *
 * @note 在RTOS架构中，错误恢复主要由SensorTask处理
 */
void WDS::clearLastError() {
    _lastErrorCode = ErrorCode::SUCCESS;
    _lastErrorMessage = "";
    _lastErrorTime = 0;

    LOG_TRACE("WDS: 错误状态已清除");
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
ErrorCode WDS::reportError(ErrorCode code, const String& message) {
    _lastErrorCode = code;
    _lastErrorMessage = message;
    _lastErrorTime = millis();

    // 根据错误严重程度记录不同级别的日志
    if (code >= ErrorCode::CRITICAL_ERROR) {
        LOG_FATAL("WDS严重错误: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else if (code >= ErrorCode::SYSTEM_ERROR) {
        LOG_ERROR("WDS系统错误: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else if (code >= ErrorCode::HARDWARE_ERROR) {
        LOG_WARNING("WDS硬件警告: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else if (code >= ErrorCode::SENSOR_READ_ERROR) {
        LOG_WARNING("WDS传感器警告: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else {
        LOG_NOTICE("WDS通知: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    }

    return code;
}

