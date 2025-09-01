#include <SensorManager.h>
#include <LogManager/LogManager.h>

/**
 * @brief SMS类构造函数
 *
 * 初始化土壤湿度传感器硬件抽象实例，设置基本参数。
 * 构造函数只进行基本的参数设置，不执行硬件初始化。
 *
 * @param sensorPin ADC引脚编号，通常在编译时通过常量确定
 *
 * @note 硬件初始化需要调用begin()方法完成
 * @note 在RTOS架构中，引脚配置通常是预定义的
 */
SMS::SMS(const int sensorPin) :
    _sensorPin(sensorPin),
    _isInitialized(false),
    _moistureReadCount(0),
    _lastMoistureValue(0),
    _lastReadTime(0),
    _lastErrorCode(ErrorCode::SUCCESS),
    _lastErrorMessage(""),
    _lastErrorTime(0) {

    LOG_TRACE("SMS: 传感器硬件抽象实例创建，ADC引脚: %d", _sensorPin);
}

/**
 * @brief 初始化土壤湿度传感器硬件
 *
 * 执行传感器硬件初始化，包括ADC引脚配置。
 * 在RTOS架构中，保持简洁的初始化流程。
 *
 * @return ErrorCode::SUCCESS 初始化成功
 * @return ErrorCode::SENSOR_INITIALIZATION_FAILED 初始化失败
 *
 * @note 重复调用是安全的，会重新初始化传感器
 * @note 在RTOS架构中，复杂的健康检查由SensorTask处理
 */
ErrorCode SMS::begin() {
    LOG_TRACE("SMS: 开始初始化土壤湿度传感器硬件，引脚: %d", _sensorPin);

    // 配置ADC引脚为输入模式
    pinMode(_sensorPin, INPUT);

    // 重置统计数据
    _moistureReadCount = 0;
    _lastMoistureValue = 0;
    _lastReadTime = 0;

    // 清除错误状态
    clearLastError();

    // 标记为已初始化
    _isInitialized = true;

    LOG_NOTICE("SMS: 土壤湿度传感器硬件初始化完成，引脚: %d", _sensorPin);
    return ErrorCode::SUCCESS;
}

/**
 * @brief 读取土壤湿度传感器数值
 *
 * 执行湿度测量流程，包括ADC采样、数据转换和统计更新。
 * 采用12位ADC精度，测量范围0-4095，映射到0-100%湿度值。
 *
 * 测量原理：
 * - ADC值越低，土壤越湿润（导电性好）
 * - ADC值越高，土壤越干燥（导电性差）
 * - 映射关系：ADC=0→湿度100%，ADC=4095→湿度0%
 *
 * @return 土壤湿度值（0-100%），0表示完全干燥，100表示完全湿润
 * @return -1 表示读取失败，错误信息可通过getLastErrorCode()获取
 *
 * @note 在RTOS架构中，此方法由SensorTask周期性调用
 * @note 异常值处理简化，复杂的错误恢复由任务层处理
 */
int SMS::readSoilMoisture() {
    // 检查传感器是否已初始化
    if (!_isInitialized) {
        reportError(ErrorCode::NOT_INITIALIZED, "传感器未初始化");
        return -1;
    }

    LOG_TRACE("SMS: 开始读取土壤湿度，引脚: %d", _sensorPin);

    // 读取ADC原始值
    const int adcVal = analogRead(_sensorPin);

    // 验证ADC读数有效性
    if (adcVal < 0 || adcVal > 4095) {
        String errorMsg = "ADC读数超出范围: " + String(adcVal) + " (有效范围: 0-4095)";
        reportError(ErrorCode::SENSOR_READ_ERROR, errorMsg);
        return _lastMoistureValue; // 返回上次有效值
    }

    // ADC值反向映射到湿度百分比
    // 映射公式：湿度% = map(ADC值, 0, 4095, 100, 0)
    int moistureValue = map(adcVal, 0, 4095, 100, 0);

    // 数据范围验证
    if (!isDataValid(moistureValue)) {
        String errorMsg = "湿度值超出有效范围: " + String(moistureValue) + "%";
        reportError(ErrorCode::SENSOR_OUT_OF_RANGE, errorMsg);
        return _lastMoistureValue;
    }

    // 更新统计数据
    _lastMoistureValue = moistureValue;
    _moistureReadCount++;
    _lastReadTime = millis();

    // 清除错误状态（成功读取）
    clearLastError();

    LOG_TRACE("SMS: 湿度读取成功 - ADC: %d, 湿度: %d%%, 采集次数: %lu",
              adcVal, _lastMoistureValue, _moistureReadCount);

    return _lastMoistureValue;
}

/**
 * @brief 获取传感器累计读取次数
 *
 * 返回自传感器初始化以来的累计读取次数，用于统计分析和诊断。
 * 计数器在每次成功调用readSoilMoisture()时递增。
 *
 * @return 累计读取次数
 *
 * @note 计数器在传感器重新初始化时会重置为0
 * @note 失败的读取操作不会增加计数
 */
unsigned long SMS::getReadCount() const {
    return _moistureReadCount;
}

/**
 * @brief 验证湿度数据有效性
 *
 * 静态方法，用于验证湿度值是否在合理范围内。
 * 可以独立使用，不依赖于SMS实例。
 *
 * @param moistureValue 要验证的湿度值
 * @return true 数据有效（0-100范围内）
 * @return false 数据无效（超出范围）
 *
 * @note 有效范围：0-100（包含边界值）
 * @note 此方法为静态方法，在SensorTask中用于统一验证
 */
bool SMS::isDataValid(int moistureValue) {
    return (moistureValue >= 0 && moistureValue <= 100);
}

/**
 * @brief 清除错误状态
 *
 * 重置错误相关的状态信息，包括：
 * - 错误码重置为SUCCESS
 * - 清空错误消息
 * - 重置错误时间戳
 *
 * @note 在RTOS架构中，错误恢复主要由SensorTask处理
 */
void SMS::clearLastError() {
    _lastErrorCode = ErrorCode::SUCCESS;
    _lastErrorMessage = "";
    _lastErrorTime = 0;

    LOG_TRACE("SMS: 错误状态已清除");
}

// ==================== 私有方法实现 ====================

/**
 * @brief 报告错误并记录日志
 *
 * 统一的错误处理方法，负责：
 * 1. 记录错误信息和时间戳
 * 2. 根据错误严重程度选择日志级别
 *
 * @param code 错误码，定义错误类型和严重程度
 * @param message 详细的错误描述信息
 * @return ErrorCode 传入的错误码（便于链式调用）
 *
 * @note 在RTOS架构中，复杂的错误恢复由SensorTask处理
 */
ErrorCode SMS::reportError(ErrorCode code, const String& message) {
    _lastErrorCode = code;
    _lastErrorMessage = message;
    _lastErrorTime = millis();

    // 根据错误严重程度记录不同级别的日志
    if (code >= ErrorCode::CRITICAL_ERROR) {
        LOG_FATAL("SMS严重错误: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else if (code >= ErrorCode::SYSTEM_ERROR) {
        LOG_ERROR("SMS系统错误: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else if (code >= ErrorCode::HARDWARE_ERROR) {
        LOG_WARNING("SMS硬件警告: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else if (code >= ErrorCode::SENSOR_READ_ERROR) {
        LOG_WARNING("SMS传感器警告: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else {
        LOG_NOTICE("SMS通知: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    }

    return code;
}