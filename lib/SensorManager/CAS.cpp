/**
 * @file CAS.cpp
 * @brief 综合分析传感器硬件抽象类实现 - 专为RTOS架构优化的轻量级实现
 * @version 6.0.0 - RTOS架构适配版本
 */

#include <SensorManager.h>
#include <LogManager/LogManager.h>

/**
 * @brief CAS类构造函数
 *
 * 初始化综合分析传感器硬件抽象实例，设置基本参数。
 * 构造函数只进行基本的参数设置，不执行硬件初始化。
 *
 * @note 硬件初始化需要调用begin()方法完成
 * @note 在RTOS架构中，传感器配置通常是预定义的
 */
CAS::CAS() :
    _isInitialized(false),
    _readCount(0),
    _lastReadTime(0),
    _lastErrorCode(ErrorCode::SUCCESS),
    _lastErrorMessage(""),
    _lastErrorTime(0) {

    LOG_TRACE("CAS: 综合分析传感器硬件抽象实例创建");
}

/**
 * @brief 析构函数
 *
 * 清理资源，释放ISCS通信对象。
 */
CAS::~CAS() {
    LOG_TRACE("CAS: 综合分析传感器硬件抽象实例销毁");
}

/**
 * @brief 初始化综合分析传感器硬件
 *
 * @param address 传感器地址，默认为1
 * @param baudRate 波特率，默认为9600
 * @return ErrorCode::SUCCESS 初始化成功
 * @return ErrorCode::SENSOR_INITIALIZATION_FAILED 初始化失败
 * @note 必须在使用其他功能前调用此方法
 * @note 在RTOS架构中，复杂的健康检查由SensorTask处理
 */
ErrorCode CAS::begin(const uint8_t address, const int baudRate) {
    LOG_TRACE("CAS: 开始初始化综合分析传感器硬件，地址: %d, 波特率: %d", address, baudRate);

    // 验证传感器地址范围
    if (address < 1 || address > 253) {
        return reportError(ErrorCode::SENSOR_INITIALIZATION_FAILED,
                          "传感器地址超出有效范围(1-253)");
    }

    // 将整数波特率转换为BaudRate枚举类型
    BaudRate baudRateEnum;
    switch (baudRate) {
        case 1200:  baudRateEnum = BaudRate::BAUD_1200; break;
        case 2400:  baudRateEnum = BaudRate::BAUD_2400; break;
        case 4800:  baudRateEnum = BaudRate::BAUD_4800; break;
        case 19200: baudRateEnum = BaudRate::BAUD_19200; break;
        case 9600:
        default:    baudRateEnum = BaudRate::BAUD_9600; break;
    }

    // 初始化底层ISCS通信模块
    if (!_iscs.begin(address, baudRateEnum)) {
        return reportError(ErrorCode::SENSOR_INITIALIZATION_FAILED,
                          "ISCS通信模块初始化失败");
    }

    // 重置统计数据
    _readCount = 0;
    _lastReadTime = 0;

    // 清除错误状态
    clearLastError();

    // 标记为已初始化
    _isInitialized = true;

    LOG_NOTICE("CAS: 综合分析传感器硬件初始化完成");
    return ErrorCode::SUCCESS;
}

/**
 * @brief 读取土壤传感器数据
 *
 * 执行土壤数据测量流程，包括ISCS通信、数据解析和内置验证。
 * 内置数据验证确保返回值的可靠性。
 *
 * @param data 土壤传感器数据结构引用，用于存储读取结果
 * @return ErrorCode::SUCCESS 读取成功
 * @return ErrorCode::SENSOR_READ_ERROR 读取失败
 * @note 内置数据验证，与SMS、WDS、LIS、ECS类保持一致的设计模式
 */
ErrorCode CAS::readSoilData(SoilSensorData& data) {
    // 检查传感器是否已初始化
    if (!_isInitialized) {
        return reportError(ErrorCode::NOT_INITIALIZED, "传感器未初始化");
    }

    LOG_TRACE("CAS: 开始读取土壤传感器数据，ISCS通信");

    // 调用底层ISCS模块读取数据
    const ModbusStatus result = _iscs.readSoilData(data);

    if (result == ModbusStatus::SUCCESS) {
        // 使用内置数据验证方法
        if (!isDataValid(data)) {
            String errorMsg = "土壤传感器数据超出有效范围 - 温度:" + String(data.temperature, 1) +
                             "°C, 湿度:" + String(data.moisture, 1) + "%, pH:" + String(data.ph, 2) +
                             ", EC:" + String(data.ec) + " μS/cm";
            return reportError(ErrorCode::SENSOR_OUT_OF_RANGE, errorMsg);
        }

        // 更新统计数据
        _readCount++;
        _lastReadTime = millis();

        // 清除错误状态（成功读取）
        clearLastError();

        LOG_TRACE("CAS: 土壤数据读取成功 - 温度:%.1f°C, 湿度:%.1f%%, EC:%u μS/cm, pH:%.2f, 采集次数: %lu",
                  data.temperature, data.moisture, data.ec, data.ph, _readCount);

        return ErrorCode::SUCCESS;
    }

    // 处理读取失败情况，将ModbusStatus错误转换为ErrorCode
    ErrorCode errorCode;
    String errorMessage;

    switch (result) {
        case ModbusStatus::TIMEOUT:
            errorCode = ErrorCode::SENSOR_TIMEOUT;
            errorMessage = "传感器通信超时";
            break;
        case ModbusStatus::CRC_ERROR:
            errorCode = ErrorCode::SENSOR_CONNECTION_ERROR;
            errorMessage = "数据校验错误，可能存在通信干扰";
            break;
        case ModbusStatus::INVALID_RESPONSE:
            errorCode = ErrorCode::SENSOR_READ_ERROR;
            errorMessage = "传感器响应格式无效: " + _iscs.getLastErrorMessage();
            break;
        case ModbusStatus::DATA_VALIDATION_FAILED:
            errorCode = ErrorCode::SENSOR_DATA_VALIDATION_FAILED;
            errorMessage = "传感器数据验证失败: " + _iscs.getLastErrorMessage();
            break;
        case ModbusStatus::NO_RESPONSE:
            errorCode = ErrorCode::SENSOR_CONNECTION_ERROR;
            errorMessage = "传感器无响应，请检查连接";
            break;
        default:
            errorCode = ErrorCode::UNKNOWN_ERROR;
            errorMessage = "未知通信错误";
            break;
    }

    LOG_ERROR("CAS: 土壤数据读取失败: %s", errorMessage.c_str());

    return reportError(errorCode, errorMessage);
}

/**
 * @brief 获取传感器累计读取次数
 *
 * 返回自传感器初始化以来的累计读取次数，用于统计分析和诊断。
 * 计数器在每次成功调用readSoilData()时递增。
 *
 * @return 累计读取次数
 *
 * @note 计数器在传感器重新初始化时会重置为0
 * @note 失败的读取操作不会增加计数
 */
unsigned long CAS::getReadCount() const {
    return _readCount;
}

/**
 * @brief 验证土壤传感器数据有效性
 *
 * 静态方法，用于验证土壤传感器数据是否在合理范围内。
 * 可以独立使用，不依赖于CAS实例。
 * 与SMS、WDS、LIS、ECS类保持一致的设计模式。
 *
 * @param data 要验证的土壤传感器数据
 * @return true 数据有效（所有值都在合理范围内）
 * @return false 数据无效（任一值超出范围或异常）
 *
 * @note 温度有效范围：-40°C至+80°C
 * @note 湿度有效范围：0-100%
 * @note pH有效范围：0-14
 * @note EC有效范围：0-20000 μS/cm
 * @note 此方法为静态方法，在SensorTask中用于统一验证
 */
bool CAS::isDataValid(const SoilSensorData& data) {
    return (data.temperature >= -40.0f && data.temperature <= 80.0f &&
            data.moisture >= 0.0f && data.moisture <= 100.0f &&
            data.ph >= 0.0f && data.ph <= 14.0f &&
            data.ec >= 0 && data.ec <= 20000 &&
            !isnan(data.temperature) && !isnan(data.moisture) && !isnan(data.ph));
}

// ==================== 错误处理接口 ====================

/**
 * @brief 获取最后一次错误码
 *
 * @return ErrorCode 最后发生的错误码
 * @note ErrorCode::SUCCESS 表示无错误
 */
ErrorCode CAS::getLastErrorCode() const {
    return _lastErrorCode;
}

/**
 * @brief 获取最后一次错误信息
 *
 * @return 错误信息的详细描述
 * @note 返回空字符串表示无错误
 */
String CAS::getLastErrorMessage() const {
    return _lastErrorMessage;
}

/**
 * @brief 清除错误状态
 *
 * 清除最后一次错误记录，重置错误状态为成功。
 *
 * @note 在RTOS架构中，错误恢复主要由SensorTask处理
 */
void CAS::clearLastError() {
    _lastErrorCode = ErrorCode::SUCCESS;
    _lastErrorMessage = "";
    _lastErrorTime = 0;

    LOG_TRACE("CAS: 错误状态已清除");
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
ErrorCode CAS::reportError(ErrorCode code, const String& message) {
    _lastErrorCode = code;
    _lastErrorMessage = message;
    _lastErrorTime = millis();

    // 根据错误严重程度记录不同级别的日志
    if (code >= ErrorCode::CRITICAL_ERROR) {
        LOG_FATAL("CAS严重错误: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else if (code >= ErrorCode::SYSTEM_ERROR) {
        LOG_ERROR("CAS系统错误: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else if (code >= ErrorCode::HARDWARE_ERROR) {
        LOG_WARNING("CAS硬件警告: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else if (code >= ErrorCode::SENSOR_READ_ERROR) {
        LOG_WARNING("CAS传感器警告: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    } else {
        LOG_NOTICE("CAS通知: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    }

    return code;
}