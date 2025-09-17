#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <BH1750.h>
#include <Adafruit_BME280.h>
#include <MSP20/MSP20.h>
#include <ISCS/ISCS.h>
#include <lib.h>


// ------------------【硬件配置】------------------
// BME280环境传感器I2C引脚 (使用I2C1)
constexpr int BME280_SCL_PIN = 25;
constexpr int BME280_SDA_PIN = 26;
// BH1750光照传感器I2C引脚 (使用I2C1，与BME280共享总线)
constexpr int BH1750_SCL_PIN = 25;
constexpr int BH1750_SDA_PIN = 26;
// 土壤湿度传感器ADC引脚
constexpr int soilMoistureSensorAdcPin = 34;
// 水深传感器ADC引脚
constexpr int waterDepthSensorAdcPin = 35;

/**
 * @brief 土壤湿度传感器硬件抽象类 (Soil Moisture Sensor)
 *
 * SMS类是ESP32土壤湿度传感器的轻量级硬件抽象层，专为RTOS架构优化。
 *
 * 主要功能：
 * - 高精度土壤湿度测量（0-100%）
 * - 数据质量验证和基本错误处理
 * - 读取统计信息管理
 * - 简洁的硬件初始化和配置
 *
 * 使用示例：
 * @code
 * SMS soilSensor(34);  // 使用GPIO34作为ADC输入
 * if (soilSensor.begin() == ErrorCode::SUCCESS) {
 *     int moisture = soilSensor.readSoilMoisture();  // 读取湿度值
 *     if (SMS::isDataValid(moisture)) {
 *         // 处理有效数据
 *     }
 * }
 * @endcode
 *
 * @note 在RTOS架构中，传感器的周期性读取由SensorTask统一管理
 * @note 本类不包含loop函数，避免与RTOS任务调度冲突
 */
class SMS {
public:
    // ==================== 构造与初始化 ====================

    /**
     * @brief 构造函数
     *
     * 创建土壤湿度传感器硬件抽象实例，初始化基本参数。
     *
     * @param sensorPin ADC引脚编号，默认使用soilMoistureSensorAdcPin(34)
     * @note 构造函数不会进行硬件初始化，需要调用begin()方法
     * @note 在RTOS架构中，引脚配置通常在编译时确定
     */
    explicit SMS(int sensorPin = soilMoistureSensorAdcPin);

    /**
     * @brief 初始化传感器硬件
     *
     * 执行传感器硬件初始化，包括ADC引脚配置。
     * 初始化成功后传感器即可正常使用。
     *
     * @return ErrorCode::SUCCESS 初始化成功
     * @return ErrorCode::SENSOR_INITIALIZATION_FAILED 初始化失败
     * @note 必须在使用其他功能前调用此方法
     * @note 重复调用是安全的，会重新初始化传感器
     */
    ErrorCode begin();

    // ==================== 数据读取接口 ====================

    /**
     * @brief 读取土壤湿度值
     *
     * 执行完整的湿度测量流程，包括ADC采样、数据转换和统计更新。
     * 在RTOS架构中，此方法由SensorTask周期性调用。
     *
     * @return 土壤湿度值（0-100%），0表示完全干燥，100表示完全湿润
     * @return -1 表示读取失败，可通过getLastErrorCode()获取错误信息
     * @note 测量精度：±1%，响应时间：<100ms
     * @note 在RTOS环境中，调用频率由SensorTask控制
     */
    int readSoilMoisture();

    /**
     * @brief 获取传感器读取总次数
     *
     * 返回自传感器初始化以来的累计读取次数，用于统计和诊断。
     *
     * @return 累计读取次数
     * @note 计数器在传感器重新初始化时会重置
     */
    unsigned long getReadCount() const;

    // ==================== 数据验证接口 ====================

    /**
     * @brief 验证湿度数据是否有效
     *
     * 检查湿度值是否在合理范围内，用于数据质量控制。
     *
     * @param moistureValue 要验证的湿度值
     * @return true 数据有效（0-100范围内）
     * @return false 数据无效（超出范围或异常值）
     * @note 此方法为静态方法，可以独立使用
     * @note 在SensorTask中用于统一的数据验证
     */
    static bool isDataValid(int moistureValue);

    // ==================== 错误处理接口 ====================

    /**
     * @brief 获取最后一次错误码
     *
     * @return ErrorCode 最后发生的错误码
     * @note ErrorCode::SUCCESS 表示无错误
     */
    ErrorCode getLastErrorCode() const { return _lastErrorCode; }

    /**
     * @brief 获取最后一次错误信息
     *
     * @return 错误信息的详细描述
     * @note 返回空字符串表示无错误
     */
    String getLastErrorMessage() const { return _lastErrorMessage; }

    /**
     * @brief 清除错误状态
     *
     * 清除最后一次错误记录，重置错误状态为成功。
     *
     * @note 在RTOS架构中，错误恢复主要由SensorTask处理
     */
    void clearLastError();

private:
    // ==================== 硬件配置 ====================
    int _sensorPin;                     // 土壤湿度传感器ADC引脚

    // ==================== 状态管理 ====================
    bool _isInitialized;                // 是否已初始化

    // ==================== 数据统计 ====================
    unsigned long _moistureReadCount;   // 湿度采集总次数
    int _lastMoistureValue;             // 最后一次读取的湿度值
    unsigned long _lastReadTime;        // 上次读取时间戳

    // ==================== 错误管理 ====================
    ErrorCode _lastErrorCode;           // 最后一次错误码
    String _lastErrorMessage;           // 最后一次错误信息
    unsigned long _lastErrorTime;       // 最后一次错误时间

    // ==================== 私有方法 ====================

    /**
     * @brief 报告错误并记录日志
     *
     * 统一的错误处理方法，记录错误信息。
     *
     * @param code 错误码
     * @param message 错误描述信息
     * @return ErrorCode 传入的错误码（便于链式调用）
     */
    ErrorCode reportError(ErrorCode code, const String& message);
};

/**
 * @brief 环境传感器硬件抽象类 (Environment Control System)
 *
 * ECS类是ESP32环境传感器的轻量级硬件抽象层，专为RTOS架构优化。
 * 基于BME280传感器提供核心的温度、湿度、气压测量功能。
 *
 * 主要功能：
 * - 高精度温度测量（-40°C至+85°C）
 * - 高精度湿度测量（0-100%）
 * - 高精度气压测量（300-1100 hPa）
 * - 基本的数据验证和错误处理
 * - 读取统计信息管理
 * - 简洁的硬件初始化和配置
 *
 * 技术规格：
 * - 温度精度：±1°C
 * - 湿度精度：±3%
 * - 气压精度：±1 hPa
 * - 响应时间：<1s
 *
 * 使用示例：
 * @code
 * ECS envSensor;
 * if (envSensor.begin() == ErrorCode::SUCCESS) {
 *     float temp = envSensor.readTemperature();
 *     float humidity = envSensor.readHumidity();
 *     float pressure = envSensor.readPressure();
 *     if (ECS::isDataValid(temp, humidity, pressure)) {
 *         // 处理有效数据
 *     }
 * }
 * @endcode
 *
 * @note 在RTOS架构中，传感器的周期性读取由SensorTask统一管理
 * @note 本类不包含复杂的数据分析功能，专注于硬件抽象
 */
class ECS {
public:
    // ==================== 构造与初始化 ====================

    /**
     * @brief 构造函数
     *
     * 创建环境传感器硬件抽象实例，初始化基本参数。
     *
     * @note 构造函数不会进行硬件初始化，需要调用begin()方法
     * @note 在RTOS架构中，传感器配置通常是预定义的
     */
    ECS();

    /**
     * @brief 初始化环境传感器硬件
     *
     * 执行传感器硬件初始化，包括BME280传感器配置。
     * 初始化成功后传感器即可正常使用。
     *
     * @return ErrorCode::SUCCESS 初始化成功
     * @return ErrorCode::SENSOR_INITIALIZATION_FAILED 初始化失败
     * @note 必须在使用其他功能前调用此方法
     * @note 在RTOS架构中，复杂的健康检查由SensorTask处理
     */
    ErrorCode begin();

    // ==================== 数据读取接口 ====================

    /**
     * @brief 读取温度值
     *
     * 执行温度测量流程，包括BME280传感器读取、数据转换和内置验证。
     * 在RTOS架构中，此方法由SensorTask周期性调用。
     *
     * @return 温度值(°C)，失败时返回-999.0
     * @note 测量范围：-40°C至+85°C，精度：±1°C
     * @note 内置数据验证，与SMS、WDS、LIS类保持一致的设计模式
     */
    float readTemperature();

    /**
     * @brief 读取湿度值
     *
     * 执行湿度测量流程，包括BME280传感器读取、数据转换和内置验证。
     * 在RTOS架构中，此方法由SensorTask周期性调用。
     *
     * @return 湿度值(%)，失败时返回-1.0
     * @note 测量范围：0-100%，精度：±3%
     * @note 内置数据验证，与SMS、WDS、LIS类保持一致的设计模式
     */
    float readHumidity();

    /**
     * @brief 读取气压值
     *
     * 执行气压测量流程，包括BME280传感器读取、数据转换和内置验证。
     * 在RTOS架构中，此方法由SensorTask周期性调用。
     *
     * @return 气压值(hPa)，失败时返回-1.0
     * @note 测量范围：300-1100 hPa，精度：±1 hPa
     * @note 内置数据验证，与SMS、WDS、LIS类保持一致的设计模式
     */
    float readPressure();

    /**
     * @brief 获取传感器读取总次数
     *
     * 返回自传感器初始化以来的累计读取次数，用于统计和诊断。
     *
     * @return 累计读取次数
     * @note 计数器在传感器重新初始化时会重置
     */
    unsigned long getReadCount() const;

    // ==================== 数据验证接口 ====================

    /**
     * @brief 验证环境传感器数据是否有效
     *
     * 检查温度、湿度、气压值是否在合理范围内，用于数据质量控制。
     * 与SMS、WDS、LIS类保持一致的设计模式。
     *
     * @param temperature 要验证的温度值
     * @param humidity 要验证的湿度值
     * @param pressure 要验证的气压值
     * @return true 数据有效（所有值都在合理范围内）
     * @return false 数据无效（值超出范围或异常）
     * @note 此方法为静态方法，可以独立使用
     * @note 在SensorTask中用于统一的数据验证
     */
    static bool isDataValid(float temperature, float humidity, float pressure);

    // ==================== 错误处理接口 ====================

    /**
     * @brief 获取最后一次错误码
     *
     * @return ErrorCode 最后发生的错误码
     * @note ErrorCode::SUCCESS 表示无错误
     */
    ErrorCode getLastErrorCode() const;

    /**
     * @brief 获取最后一次错误信息
     *
     * @return 错误信息的详细描述
     * @note 返回空字符串表示无错误
     */
    String getLastErrorMessage() const;

    /**
     * @brief 清除错误状态
     *
     * 清除最后一次错误记录，重置错误状态为成功。
     *
     * @note 在RTOS架构中，错误恢复主要由SensorTask处理
     */
    void clearLastError();

private:
    // ==================== 硬件配置 ====================
    Adafruit_BME280 _bme280;           // BME280传感器对象

    // ==================== 状态管理 ====================
    bool _isInitialized;               // 是否已初始化

    // ==================== 数据统计 ====================
    unsigned long _readCount;          // 读取次数
    unsigned long _lastReadTime;       // 上次读取时间戳

    // ==================== 错误管理 ====================
    ErrorCode _lastErrorCode;          // 最后一次错误码
    String _lastErrorMessage;          // 最后一次错误信息
    unsigned long _lastErrorTime;      // 最后一次错误时间

    // ==================== 私有方法 ====================

    /**
     * @brief 报告错误并记录日志
     *
     * 统一的错误处理方法，记录错误信息。
     *
     * @param code 错误码
     * @param message 错误描述信息
     * @return ErrorCode 传入的错误码（便于链式调用）
     */
    ErrorCode reportError(ErrorCode code, const String& message);
};

/**
 * @brief 光照传感器硬件抽象类 (Light Intensity Sensor)
 *
 * LIS类是ESP32光照传感器的轻量级硬件抽象层，专为RTOS架构优化。
 * 基于BH1750传感器提供核心的光照强度测量功能。
 *
 * 主要功能：
 * - 高精度光照强度测量（0-65535 lux）
 * - 基本的数据验证和错误处理
 * - 读取统计信息管理
 * - 简洁的硬件初始化和配置
 *
 * 技术规格：
 * - 测量范围：0-65535 lux
 * - 精度：±20%
 * - 响应时间：<200ms
 *
 * 使用示例：
 * @code
 * LIS lightSensor;
 * if (lightSensor.begin() == ErrorCode::SUCCESS) {
 *     float intensity = lightSensor.readLightLevel();
 *     if (LIS::isDataValid(intensity)) {
 *         // 处理有效数据
 *     }
 * }
 * @endcode
 *
 * @note 在RTOS架构中，传感器的周期性读取由SensorTask统一管理
 * @note 本类不包含复杂的数据分析功能，专注于硬件抽象
 */
class LIS {
public:
    // ==================== 构造与初始化 ====================

    /**
     * @brief 构造函数
     *
     * 创建光照传感器硬件抽象实例，初始化基本参数。
     *
     * @note 构造函数不会进行硬件初始化，需要调用begin()方法
     * @note 在RTOS架构中，传感器配置通常是预定义的
     */
    LIS();

    /**
     * @brief 初始化光照传感器硬件
     *
     * 执行传感器硬件初始化，包括BH1750传感器配置。
     * 初始化成功后传感器即可正常使用。
     *
     * @return ErrorCode::SUCCESS 初始化成功
     * @return ErrorCode::SENSOR_INITIALIZATION_FAILED 初始化失败
     * @note 必须在使用其他功能前调用此方法
     * @note 在RTOS架构中，复杂的健康检查由SensorTask处理
     */
    ErrorCode begin();

    // ==================== 数据读取接口 ====================

    /**
     * @brief 读取光照强度值
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
    float readLightLevel();

    /**
     * @brief 获取传感器读取总次数
     *
     * 返回自传感器初始化以来的累计读取次数，用于统计和诊断。
     *
     * @return 累计读取次数
     * @note 计数器在传感器重新初始化时会重置
     */
    unsigned long getReadCount() const;
    
    // ==================== 数据验证接口 ====================

    /**
     * @brief 验证光照强度数据是否有效
     *
     * 检查光照强度值是否在合理范围内，用于数据质量控制。
     * 与SMS、WDS类保持一致的设计模式。
     *
     * @param lightLevel 要验证的光照强度值
     * @return true 数据有效（0-65535 lux范围内）
     * @return false 数据无效（超出范围或异常值）
     * @note 此方法为静态方法，可以独立使用
     * @note 在SensorTask中用于统一的数据验证
     */
    static bool isDataValid(float lightLevel);

    /**
     * @brief 白天/夜晚检测接口
     *
     * 检测当前是否为白天，使用简单的阈值判断。
     *
     * @return true 白天(>10 lux)，false 夜晚
     * @note 在RTOS架构中，复杂的光照分析由业务逻辑层处理
     */
    bool isDaytime() const;

    // ==================== 错误处理接口 ====================

    /**
     * @brief 获取最后一次错误码
     *
     * @return ErrorCode 最后发生的错误码
     * @note ErrorCode::SUCCESS 表示无错误
     */
    ErrorCode getLastErrorCode() const;

    /**
     * @brief 获取最后一次错误信息
     *
     * @return 错误信息的详细描述
     * @note 返回空字符串表示无错误
     */
    String getLastErrorMessage() const;

    /**
     * @brief 清除错误状态
     *
     * 清除最后一次错误记录，重置错误状态为成功。
     *
     * @note 在RTOS架构中，错误恢复主要由SensorTask处理
     */
    void clearLastError();

private:
    // ==================== 硬件配置 ====================
    BH1750 _bh1750;                    // BH1750传感器对象

    // ==================== 状态管理 ====================
    bool _isInitialized;               // 是否已初始化

    // ==================== 数据统计 ====================
    unsigned long _readCount;          // 读取次数
    float _lastLightIntensity;         // 最后一次读取的光照强度
    unsigned long _lastReadTime;       // 上次读取时间戳

    // ==================== 错误管理 ====================
    ErrorCode _lastErrorCode;          // 最后一次错误码
    String _lastErrorMessage;          // 最后一次错误信息
    unsigned long _lastErrorTime;      // 最后一次错误时间

    // ==================== 私有方法 ====================

    /**
     * @brief 报告错误并记录日志
     *
     * 统一的错误处理方法，记录错误信息。
     *
     * @param code 错误码
     * @param message 错误描述信息
     * @return ErrorCode 传入的错误码（便于链式调用）
     */
    ErrorCode reportError(ErrorCode code, const String& message);
};

/**
 * @brief 水位传感器硬件抽象类 (Water Depth Sensor)
 *
 * WDS类是ESP32水位传感器的轻量级硬件抽象层，专为RTOS架构优化。
 * 基于MSP20传感器提供核心的水位测量功能。
 *
 * 主要功能：
 * - 高精度水深测量(±1cm精度)
 * - 基本的数据验证和错误处理
 * - 读取统计信息管理
 * - 简洁的硬件初始化和配置
 *
 * 技术规格：
 * - 测量范围：0-200cm
 * - 精度：±1cm
 * - 响应时间：<500ms
 *
 * 使用示例：
 * @code
 * WDS waterSensor(35);  // 使用GPIO35作为ADC输入
 * if (waterSensor.begin() == ErrorCode::SUCCESS) {
 *     double depth = waterSensor.readWaterDepth();  // 读取水深(cm)
 *     if (depth > 0 && depth < 200) {
 *         // 处理有效数据
 *     }
 * }
 * @endcode
 *
 * @note 在RTOS架构中，传感器的周期性读取由SensorTask统一管理
 * @note 本类不包含复杂的数据分析功能，专注于硬件抽象
 */
class WDS {
public:
    // ==================== 构造与初始化 ====================

    /**
     * @brief 构造函数
     *
     * 创建水位传感器硬件抽象实例，初始化基本参数。
     *
     * @param sensorPin ADC引脚编号，默认使用waterDepthSensorAdcPin(35)
     * @note 构造函数不会进行硬件初始化，需要调用begin()方法
     * @note 在RTOS架构中，引脚配置通常在编译时确定
     */
    explicit WDS(int sensorPin = waterDepthSensorAdcPin);

    /**
     * @brief 初始化水位传感器硬件
     *
     * 执行传感器硬件初始化，包括MSP20传感器配置。
     * 初始化成功后传感器即可正常使用。
     *
     * @return ErrorCode::SUCCESS 初始化成功
     * @return ErrorCode::SENSOR_INITIALIZATION_FAILED 初始化失败
     * @note 必须在使用其他功能前调用此方法
     * @note 在RTOS架构中，复杂的健康检查由SensorTask处理
     */
    ErrorCode begin();

    // ==================== 数据读取接口 ====================

    /**
     * @brief 读取水深值（厘米）
     *
     * 执行水深测量流程，包括MSP20传感器读取、数据转换和基本验证。
     * 在RTOS架构中，此方法由SensorTask周期性调用。
     *
     * @return 水深值(cm)，失败时返回-1.0
     * @note 测量范围：0-200cm，精度：±1cm
     * @note 在RTOS环境中，调用频率由SensorTask控制
     */
    double readWaterDepth();

    /**
     * @brief 获取传感器读取总次数
     *
     * 返回自传感器初始化以来的累计读取次数，用于统计和诊断。
     *
     * @return 累计读取次数
     * @note 计数器在传感器重新初始化时会重置
     */
    unsigned long getReadCount() const;

    // ==================== 数据验证接口 ====================

    /**
     * @brief 验证水深数据是否有效
     *
     * 检查水深值是否在合理范围内，用于数据质量控制。
     * 与SMS类保持一致的设计模式。
     *
     * @param waterDepth 要验证的水深值
     * @return true 数据有效（0-200cm范围内）
     * @return false 数据无效（超出范围或异常值）
     * @note 此方法为静态方法，可以独立使用
     * @note 在SensorTask中用于统一的数据验证
     */
    static bool isDataValid(double waterDepth);

    /**
     * @brief 低水位检测接口
     *
     * 检测是否为低水位状态，使用简单的阈值判断。
     *
     * @return true 低水位(<1cm)，false 正常水位
     * @note 在RTOS架构中，复杂的趋势分析由业务逻辑层处理
     */
    bool isLowWaterLevel() const;

    // ==================== 错误处理接口 ====================

    /**
     * @brief 获取最后一次错误码
     *
     * @return ErrorCode 最后发生的错误码
     * @note ErrorCode::SUCCESS 表示无错误
     */
    ErrorCode getLastErrorCode() const;

    /**
     * @brief 获取最后一次错误信息
     *
     * @return 错误信息的详细描述
     * @note 返回空字符串表示无错误
     */
    String getLastErrorMessage() const;

    /**
     * @brief 清除错误状态
     *
     * 清除最后一次错误记录，重置错误状态为成功。
     *
     * @note 在RTOS架构中，错误恢复主要由SensorTask处理
     */
    void clearLastError();

private:
    // ==================== 硬件配置 ====================
    MSP20 _msp20;                      // MSP20传感器对象

    // ==================== 状态管理 ====================
    bool _isInitialized;               // 是否已初始化

    // ==================== 数据统计 ====================
    unsigned long _readCount;          // 读取次数
    double _lastWaterDepth;            // 最后一次读取的水深值
    unsigned long _lastReadTime;       // 上次读取时间戳

    // ==================== 错误管理 ====================
    ErrorCode _lastErrorCode;          // 最后一次错误码
    String _lastErrorMessage;          // 最后一次错误信息
    unsigned long _lastErrorTime;      // 最后一次错误时间

    // ==================== 私有方法 ====================

    /**
     * @brief 报告错误并记录日志
     *
     * 统一的错误处理方法，记录错误信息。
     *
     * @param code 错误码
     * @param message 错误描述信息
     * @return ErrorCode 传入的错误码（便于链式调用）
     */
    ErrorCode reportError(ErrorCode code, const String& message);
};

// ================== 综合土壤分析传感器相关声明（CAS模块） ==================

/**
 * @brief 综合分析传感器硬件抽象类 (Comprehensive Analysis Sensor)
 *
 * CAS类是ESP32综合分析传感器的轻量级硬件抽象层，专为RTOS架构优化。
 * 基于ISCS通信协议提供核心的土壤传感器数据读取功能。
 *
 * 主要功能：
 * - 土壤温度测量（-40°C至+80°C）
 * - 土壤湿度测量（0-100%）
 * - 土壤pH值测量（0-14）
 * - 土壤EC值测量（0-20000 μS/cm）
 * - 基本的数据验证和错误处理
 * - 读取统计信息管理
 * - 简洁的硬件初始化和配置
 *
 * 技术规格：
 * - 温度精度：±0.5°C
 * - 湿度精度：±2%
 * - pH精度：±0.1
 * - EC精度：±2%
 * - 响应时间：<2s
 *
 * 使用示例：
 * @code
 * CAS soilSensor;
 * if (soilSensor.begin() == ErrorCode::SUCCESS) {
 *     SoilSensorData data;
 *     if (soilSensor.readSoilData(data) == ErrorCode::SUCCESS) {
 *         if (CAS::isDataValid(data)) {
 *             // 处理有效数据
 *         }
 *     }
 * }
 * @endcode
 *
 * @note 在RTOS架构中，传感器的周期性读取由SensorTask统一管理
 * @note 本类不包含复杂的数据分析功能，专注于硬件抽象
 */
class CAS {
public:
    // ==================== 构造与初始化 ====================

    /**
     * @brief 构造函数
     *
     * 创建综合分析传感器硬件抽象实例，初始化基本参数。
     *
     * @note 构造函数不会进行硬件初始化，需要调用begin()方法
     * @note 在RTOS架构中，传感器配置通常是预定义的
     */
    CAS();

    /**
     * @brief 析构函数
     *
     * 清理资源，释放ISCS通信对象。
     */
    ~CAS();

    /**
     * @brief 初始化综合分析传感器硬件
     *
     * 执行传感器硬件初始化，包括ISCS通信配置。
     * 初始化成功后传感器即可正常使用。
     *
     * @param address 传感器地址，默认为1
     * @param baudRate 波特率，默认为9600
     * @return ErrorCode::SUCCESS 初始化成功
     * @return ErrorCode::SENSOR_INITIALIZATION_FAILED 初始化失败
     * @note 必须在使用其他功能前调用此方法
     * @note 在RTOS架构中，复杂的健康检查由SensorTask处理
     */
    ErrorCode begin(uint8_t address = 1, int baudRate = 9600);

    // ==================== 数据读取接口 ====================

    /**
     * @brief 读取土壤传感器数据
     *
     * 执行土壤数据测量流程，包括ISCS通信、数据解析和内置验证。
     * 在RTOS架构中，此方法由SensorTask周期性调用。
     * 内置数据验证确保返回值的可靠性。
     *
     * @param data 土壤传感器数据结构引用，用于存储读取结果
     * @return ErrorCode::SUCCESS 读取成功
     * @return ErrorCode::SENSOR_READ_ERROR 读取失败
     * @note 内置数据验证，与SMS、WDS、LIS、ECS类保持一致的设计模式
     */
    ErrorCode readSoilData(SoilSensorData& data);

    /**
     * @brief 获取传感器读取总次数
     *
     * 返回自传感器初始化以来的累计读取次数，用于统计和诊断。
     *
     * @return 累计读取次数
     * @note 计数器在传感器重新初始化时会重置
     */
    unsigned long getReadCount() const;

    // ==================== 数据验证接口 ====================

    /**
     * @brief 验证土壤传感器数据是否有效
     *
     * 检查土壤传感器数据是否在合理范围内，用于数据质量控制。
     * 与SMS、WDS、LIS、ECS类保持一致的设计模式。
     *
     * @param data 要验证的土壤传感器数据
     * @return true 数据有效（所有值都在合理范围内）
     * @return false 数据无效（值超出范围或异常）
     * @note 此方法为静态方法，可以独立使用
     * @note 在SensorTask中用于统一的数据验证
     */
    static bool isDataValid(const SoilSensorData& data);

    // ==================== 错误处理接口 ====================

    /**
     * @brief 获取最后一次错误码
     *
     * @return ErrorCode 最后发生的错误码
     * @note ErrorCode::SUCCESS 表示无错误
     */
    ErrorCode getLastErrorCode() const;

    /**
     * @brief 获取最后一次错误信息
     *
     * @return 错误信息的详细描述
     * @note 返回空字符串表示无错误
     */
    String getLastErrorMessage() const;

    /**
     * @brief 清除错误状态
     *
     * 清除最后一次错误记录，重置错误状态为成功。
     *
     * @note 在RTOS架构中，错误恢复主要由SensorTask处理
     */
    void clearLastError();

private:
    // ==================== 硬件配置 ====================
    ISCS _iscs;                         // ISCS通信对象

    // ==================== 状态管理 ====================
    bool _isInitialized;                // 是否已初始化

    // ==================== 数据统计 ====================
    unsigned long _readCount;           // 读取次数
    unsigned long _lastReadTime;        // 上次读取时间戳

    // ==================== 错误管理 ====================
    ErrorCode _lastErrorCode;           // 最后一次错误码
    String _lastErrorMessage;           // 最后一次错误信息
    unsigned long _lastErrorTime;       // 最后一次错误时间

    // ==================== 私有方法 ====================

    /**
     * @brief 报告错误并记录日志
     *
     * 统一的错误处理方法，记录错误信息。
     *
     * @param code 错误码
     * @param message 错误描述信息
     * @return ErrorCode 传入的错误码（便于链式调用）
     */
    ErrorCode reportError(ErrorCode code, const String& message);
};

// ================== END ==================

#endif // SENSOR_MANAGER_H 