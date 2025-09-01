#ifndef ISCS_H
#define ISCS_H

#include <Arduino.h>
#include <HardwareSerial.h>

/**
 * @file ISCS.h
 * @brief 智能土壤综合传感器(Intelligent Soil Comprehensive Sensor)库
 * @author Zelas2Xerath
 * @date 2025-07-20
 * @version 2.0.0
 * 
 * 本库提供了完整的ISCS土壤传感器通信接口，支持：
 * - RS485 Modbus RTU通信协议
 * - 8种土壤参数实时监测
 * - 传感器配置管理
 * - 数据校准和验证
 * - 通信状态监控
 */

// ------------------【调试配置】------------------

// 启用原始数据调试输出以分析数据读取问题
#define ISCS_DEBUG_RAW_DATA

// ------------------【硬件配置常量】------------------

// RS485通信引脚定义
constexpr int RS485_RX_PIN = 16;        // GPIO16(U2RXD) 8合1串口输入（MAX485 RO）
constexpr int RS485_TX_PIN = 17;        // GPIO17(U2TXD) 8合1串口输出（MAX485 DI）
constexpr int RS485_DE_RE_PIN = 4;      // GPIO4 MAX485收发控制

// 通信参数常量
constexpr uint8_t ISCS_DEFAULT_ADDRESS = 1;        // 默认传感器地址
constexpr uint32_t ISCS_DEFAULT_TIMEOUT = 1000;    // 默认通信超时时间(ms)
constexpr uint32_t ISCS_PING_TIMEOUT = 500;        // Ping超时时间(ms)
constexpr size_t ISCS_MAX_FRAME_SIZE = 256;        // 最大帧长度

// ------------------【Modbus协议定义】------------------

/**
 * @brief Modbus功能码枚举
 * 定义支持的Modbus RTU功能码
 */
enum class ModbusFunctionCode : uint8_t {
    READ_HOLDING_REGISTERS = 0x03,    // 读保持寄存器
    WRITE_SINGLE_REGISTER = 0x06,     // 写单个寄存器
    WRITE_MULTIPLE_REGISTERS = 0x10   // 写多个寄存器
};

/**
 * @brief 传感器寄存器地址枚举
 * 定义土壤传感器各参数的寄存器地址
 */
enum class SensorRegister : uint16_t {
    SOIL_TEMPERATURE = 0x0000,    // 土壤温度寄存器
    SOIL_MOISTURE = 0x0001,       // 土壤湿度寄存器
    SOIL_EC = 0x0002,             // 土壤EC寄存器
    SOIL_SALINITY = 0x0003,       // 土壤盐分寄存器
    SOIL_NITROGEN = 0x0004,       // 土壤氮寄存器
    SOIL_PHOSPHORUS = 0x0005,     // 土壤磷寄存器
    SOIL_POTASSIUM = 0x0006,      // 土壤钾寄存器
    SOIL_PH = 0x0007,             // 土壤pH值寄存器
    DEVICE_ADDRESS = 0x0030,      // 设备地址寄存器
    BAUD_RATE = 0x0031,           // 波特率寄存器
    PARITY = 0x0032,              // 校验位寄存器
    AUTO_REPORT = 0x0033          // 自动上报寄存器
};

/**
 * @brief 波特率枚举
 * 定义支持的串口波特率
 */
enum class BaudRate : uint16_t {
    BAUD_1200 = 1200,     // 1200 bps
    BAUD_2400 = 2400,     // 2400 bps
    BAUD_4800 = 4800,     // 4800 bps
    BAUD_9600 = 9600,     // 9600 bps (默认)
    BAUD_19200 = 19200    // 19200 bps
};

/**
 * @brief 校验位枚举
 * 定义串口校验位类型
 */
enum class Parity : uint8_t {
    NONE = 0,      // 无校验
    ODD = 1,       // 奇校验
    EVEN = 2       // 偶校验
};

/**
 * @brief Modbus通信状态枚举
 * 定义通信操作的返回状态
 */
enum class ModbusStatus {
    SUCCESS,                // 操作成功
    TIMEOUT,                // 通信超时
    CRC_ERROR,              // CRC校验错误
    INVALID_RESPONSE,       // 无效响应格式
    NO_RESPONSE,            // 无响应
    DATA_VALIDATION_FAILED  // 数据验证失败
};

// ------------------【数据结构定义】------------------

/**
 * @brief 土壤质量等级枚举
 * 根据土壤参数评估土壤质量
 */
enum class SoilQualityLevel {
    EXCELLENT = 0,  // 优秀
    GOOD = 1,       // 良好
    FAIR = 2,       // 一般
    POOR = 3,       // 较差
    VERY_POOR = 4   // 很差
};

/**
 * @brief 土壤类型枚举
 * 根据土壤成分确定土壤类型
 */
enum class SoilType {
    SANDY = 0,      // 砂质土
    LOAMY = 1,      // 壤土
    CLAY = 2,       // 粘土
    SILTY = 3,      // 粉质土
    ORGANIC = 4     // 有机土
};

/**
 * @brief 土壤健康状态结构体
 * 综合评估土壤健康状况和种植建议
 */
struct SoilHealthStatus {
    SoilQualityLevel quality;        // 土壤质量等级
    SoilType type;                   // 土壤类型
    bool isSuitableForCrops;         // 是否适合作物种植
    String recommendations;          // 土壤改良建议
    float healthScore;               // 土壤健康评分(0-100)

    /**
     * @brief 默认构造函数
     * 初始化为中等质量壤土
     */
    SoilHealthStatus()
        : quality(SoilQualityLevel::FAIR),
          type(SoilType::LOAMY),
          isSuitableForCrops(true),
          recommendations(""),
          healthScore(50.0f) {}
};

/**
 * @brief 土壤传感器数据结构
 * 存储从传感器读取的所有土壤参数
 */
struct SoilSensorData {
    float temperature;     // 土壤温度 (°C)
    float moisture;        // 土壤湿度 (%)
    uint16_t ec;          // 土壤EC (μS/cm)
    uint16_t salinity;    // 土壤盐分 (mg/L)
    uint16_t nitrogen;    // 土壤氮 (mg/kg)
    uint16_t phosphorus;  // 土壤磷 (mg/kg)
    uint16_t potassium;   // 土壤钾 (mg/kg)
    float ph;             // 土壤pH值
    
    /**
     * @brief 默认构造函数
     * 初始化所有参数为0
     */
    SoilSensorData() : temperature(0.0), moisture(0.0), ec(0), salinity(0),
                      nitrogen(0), phosphorus(0), potassium(0), ph(0.0) {}
    
    /**
     * @brief 重置所有数据为0
     */
    void reset() {
        temperature = 0.0;
        moisture = 0.0;
        ec = 0;
        salinity = 0;
        nitrogen = 0;
        phosphorus = 0;
        potassium = 0;
        ph = 0.0;
    }
    
    /**
     * @brief 检查数据有效性
     * @return true 数据在有效范围内
     * @return false 数据超出有效范围
     *
     * @note pH值范围修改为0.0-14.0，与CAS验证逻辑保持一致
     * @note 增加NaN检查，确保数据完整性
     */
    bool isValid() const {
        return temperature >= -40.0 && temperature <= 80.0 &&
               moisture >= 0.0 && moisture <= 100.0 &&
               ec <= 20000 &&
               salinity <= 20000 &&
               nitrogen <= 1999 &&
               phosphorus <= 1999 &&
               potassium <= 1999 &&
               ph >= 0.0 && ph <= 14.0 &&  // 修复：统一pH范围为0.0-14.0
               !isnan(temperature) && !isnan(moisture) && !isnan(ph);  // 增加NaN检查
    }
};

/**
 * @brief 传感器配置结构
 * 存储传感器的配置参数
 */
struct SensorConfig {
    uint8_t address;      // 设备地址 (1-253)
    BaudRate baudRate;    // 波特率
    Parity parity;        // 校验位
    uint16_t autoReport;  // 自动上报间隔 (秒)
    
    /**
     * @brief 默认构造函数
     * 使用默认配置参数
     */
    SensorConfig() : address(ISCS_DEFAULT_ADDRESS), baudRate(BaudRate::BAUD_9600), 
                    parity(Parity::NONE), autoReport(0) {}
};

// ------------------【ISCS主类声明】------------------

/**
 * @brief ISCS (Intelligent Soil Comprehensive Sensor) 智能土壤综合传感器类
 * 
 * 本类提供完整的ISCS土壤传感器通信接口，支持：
 * - RS485 Modbus RTU协议通信
 * - 土壤多参数实时监测
 * - 传感器配置管理
 * - 数据校准和验证
 * - 通信状态和统计信息
 * 
 * 使用示例：
 * @code
 * ISCS sensor;
 * if (sensor.begin(1, BaudRate::BAUD_9600)) {
 *     SoilSensorData data;
 *     if (sensor.readSoilData(data) == ModbusStatus::SUCCESS) {
 *         Serial.println("温度: " + String(data.temperature) + "°C");
 *     }
 * }
 * @endcode
 */
class ISCS {
public:
    // ------------------【构造和析构】------------------
    
    /**
     * @brief 构造函数
     * 初始化ISCS传感器对象
     */
    ISCS();
    
    /**
     * @brief 析构函数
     * 清理资源
     */
    ~ISCS();
    
    // ------------------【初始化和配置】------------------
    
    /**
     * @brief 初始化传感器通信
     * @param address 传感器地址 (1-253)
     * @param baudRate 通信波特率
     * @return true 初始化成功
     * @return false 初始化失败
     */
    bool begin(uint8_t address = ISCS_DEFAULT_ADDRESS, BaudRate baudRate = BaudRate::BAUD_9600);
    
    /**
     * @brief 设置传感器地址
     * @param address 新的传感器地址
     */
    void setAddress(uint8_t address);
    
    /**
     * @brief 获取当前传感器地址
     * @return uint8_t 当前地址
     */
    uint8_t getAddress() const;
    
    /**
     * @brief 设置通信波特率
     * @param baudRate 新的波特率
     */
    void setBaudRate(BaudRate baudRate);
    
    /**
     * @brief 获取当前波特率
     * @return BaudRate 当前波特率
     */
    BaudRate getBaudRate() const;
    
    // ------------------【通信控制】------------------
    
    /**
     * @brief 启用发送模式
     * 设置RS485为发送状态
     */
    void enableTransmit();
    
    /**
     * @brief 启用接收模式
     * 设置RS485为接收状态
     */
    void enableReceive();
    
    /**
     * @brief 检查是否处于发送状态
     * @return true 正在发送
     * @return false 处于接收状态
     */
    bool isTransmitting() const;
    
    // ------------------【数据读取功能】------------------
    
    /**
     * @brief 读取土壤传感器数据
     * @param data 存储读取数据的结构体引用
     * @param targetAddress 目标传感器地址，0表示使用当前地址
     * @return ModbusStatus 操作状态
     */
    ModbusStatus readSoilData(SoilSensorData& data, uint8_t targetAddress = 0);
    
    /**
     * @brief 读取传感器配置
     * @param config 存储配置的结构体引用
     * @param targetAddress 目标传感器地址，0表示使用当前地址
     * @return ModbusStatus 操作状态
     */
    ModbusStatus readSensorConfig(SensorConfig& config, uint8_t targetAddress = 0);
    
    // ------------------【配置写入功能】------------------
    
    /**
     * @brief 写入传感器地址
     * @param newAddress 新地址
     * @param targetAddress 目标传感器地址，0表示使用当前地址
     * @return ModbusStatus 操作状态
     */
    ModbusStatus writeSensorAddress(uint8_t newAddress, uint8_t targetAddress = 0);
    
    /**
     * @brief 写入传感器配置
     * @param config 配置参数
     * @param targetAddress 目标传感器地址，0表示使用当前地址
     * @return ModbusStatus 操作状态
     */
    ModbusStatus writeSensorConfig(const SensorConfig& config, uint8_t targetAddress = 0);
    
    /**
     * @brief 查询传感器地址
     * @param address 存储查询到的地址
     * @return ModbusStatus 操作状态
     */
    ModbusStatus querySensorAddress(uint8_t& address);
    
    /**
     * @brief 写入校验位配置
     * @param parity 校验位类型
     * @param targetAddress 目标传感器地址，0表示使用当前地址
     * @return ModbusStatus 操作状态
     */
    ModbusStatus writeSensorParity(Parity parity, uint8_t targetAddress = 0);
    
    /**
     * @brief 写入自动上报间隔
     * @param interval 上报间隔(秒)
     * @param targetAddress 目标传感器地址，0表示使用当前地址
     * @return ModbusStatus 操作状态
     */
    ModbusStatus writeSensorAutoReport(uint16_t interval, uint8_t targetAddress = 0);
    
    /**
     * @brief 写入波特率配置
     * @param baudRate 新波特率
     * @param targetAddress 目标传感器地址，0表示使用当前地址
     * @return ModbusStatus 操作状态
     */
    ModbusStatus writeSensorBaudRate(BaudRate baudRate, uint8_t targetAddress = 0);

    // ------------------【高级功能】------------------

    /**
     * @brief 测试传感器连接
     * @param targetAddress 目标传感器地址，0表示使用当前地址
     * @return true 传感器响应正常
     * @return false 传感器无响应或错误
     */
    bool pingSensor(uint8_t targetAddress = 0);

    /**
     * @brief 检查传感器是否连接
     * @param targetAddress 目标传感器地址，0表示使用当前地址
     * @return true 传感器已连接
     * @return false 传感器未连接
     */
    bool isSensorConnected(uint8_t targetAddress = 0);

    /**
     * @brief 获取最后一次错误状态
     * @return ModbusStatus 错误状态
     */
    ModbusStatus getLastError() const;

    /**
     * @brief 获取最后一次错误信息
     * @return String 错误信息描述
     */
    String getLastErrorMessage() const;

    // ------------------【数据转换工具】------------------

    /**
     * @brief 转换原始温度值为实际温度
     * @param rawValue 原始16位值
     * @return float 实际温度值(°C)
     */
    static float convertTemperature(uint16_t rawValue);

    /**
     * @brief 转换原始湿度值为实际湿度
     * @param rawValue 原始16位值
     * @return float 实际湿度值(%)
     */
    static float convertMoisture(uint16_t rawValue);

    /**
     * @brief 转换原始pH值为实际pH值
     * @param rawValue 原始16位值
     * @return float 实际pH值
     */
    static float convertPH(uint16_t rawValue);

    /**
     * @brief 转换实际温度为原始值
     * @param temperature 实际温度(°C)
     * @return uint16_t 原始16位值
     */
    static uint16_t convertToRawTemperature(float temperature);

    /**
     * @brief 转换实际湿度为原始值
     * @param moisture 实际湿度(%)
     * @return uint16_t 原始16位值
     */
    static uint16_t convertToRawMoisture(float moisture);

    /**
     * @brief 转换实际pH值为原始值
     * @param ph 实际pH值
     * @return uint16_t 原始16位值
     */
    static uint16_t convertToRawPH(float ph);

    // ------------------【统计信息】------------------

    /**
     * @brief 获取通信次数
     * @return unsigned long 总通信次数
     */
    unsigned long getCommunicationCount() const;

    /**
     * @brief 获取错误次数
     * @return unsigned long 总错误次数
     */
    unsigned long getErrorCount() const;

    /**
     * @brief 获取通信成功率
     * @return float 成功率(0.0-1.0)
     */
    float getSuccessRate() const;

    /**
     * @brief 重置统计信息
     */
    void resetStatistics();

private:
    // ------------------【私有成员变量】------------------

    HardwareSerial* _serial;           // 串口对象指针
    uint8_t _address;                  // 当前设备地址
    BaudRate _baudRate;                // 当前波特率
    bool _isTransmitting;              // 发送状态标志
    ModbusStatus _lastError;           // 最后一次错误状态
    String _lastErrorMessage;          // 最后一次错误信息

    // 统计信息
    unsigned long _communicationCount;  // 通信次数计数器
    unsigned long _errorCount;          // 错误次数计数器

    // ------------------【私有静态方法 - CRC计算】------------------

    /**
     * @brief 计算CRC16校验码
     * @param data 数据缓冲区指针
     * @param length 数据长度
     * @return uint16_t CRC16校验码
     */
    static uint16_t calculateCRC16(const uint8_t* data, size_t length);

    /**
     * @brief 验证CRC16校验码
     * @param data 包含CRC的数据缓冲区指针
     * @param length 数据总长度(包含CRC)
     * @return true CRC校验正确
     * @return false CRC校验错误
     */
    static bool verifyCRC16(const uint8_t* data, size_t length);

    // ------------------【私有方法 - Modbus帧处理】------------------

    /**
     * @brief 构建Modbus读取请求帧
     * @param buffer 输出缓冲区
     * @param length 输出帧长度
     * @param address 设备地址
     * @param startRegister 起始寄存器地址
     * @param registerCount 寄存器数量
     * @return true 构建成功
     * @return false 构建失败
     */
    static bool buildReadRequest(uint8_t* buffer, size_t& length,
                         uint8_t address, uint16_t startRegister, uint16_t registerCount);

    /**
     * @brief 构建Modbus写入请求帧
     * @param buffer 输出缓冲区
     * @param length 输出帧长度
     * @param address 设备地址
     * @param reg 寄存器地址
     * @param value 写入值
     * @return true 构建成功
     * @return false 构建失败
     */
    static bool buildWriteRequest(uint8_t* buffer, size_t& length,
                          uint8_t address, uint16_t reg, uint16_t value);

    /**
     * @brief 解析Modbus读取响应帧
     * @param data 响应数据缓冲区
     * @param length 数据长度
     * @param expectedAddress 期望的设备地址
     * @param expectedRegisters 期望的寄存器数量
     * @return true 解析成功
     * @return false 解析失败
     */
    static bool parseReadResponse(const uint8_t* data, size_t length,
                          uint8_t expectedAddress, uint16_t expectedRegisters);

    /**
     * @brief 解析Modbus写入响应帧
     * @param data 响应数据缓冲区
     * @param length 数据长度
     * @param expectedAddress 期望的设备地址
     * @param expectedRegister 期望的寄存器地址
     * @return true 解析成功
     * @return false 解析失败
     */
    static bool parseWriteResponse(const uint8_t* data, size_t length,
                           uint8_t expectedAddress, uint16_t expectedRegister);

    // ------------------【私有方法 - 通信辅助】------------------

    /**
     * @brief 发送Modbus帧
     * @param data 发送数据缓冲区
     * @param length 数据长度
     * @return true 发送成功
     * @return false 发送失败
     */
    bool sendModbusFrame(const uint8_t* data, size_t length);

    /**
     * @brief 接收Modbus帧
     * @param buffer 接收缓冲区
     * @param length 接收到的数据长度
     * @param timeout 超时时间(毫秒)
     * @return true 接收成功
     * @return false 接收失败或超时
     */
    bool receiveModbusFrame(uint8_t* buffer, size_t& length, unsigned long timeout = ISCS_DEFAULT_TIMEOUT);

    /**
     * @brief 清空接收缓冲区
     */
    void clearReceiveBuffer() const;

    // ------------------【私有方法 - 数据转换】------------------

    /**
     * @brief 转换无符号16位值为有符号16位值
     * @param value 无符号值
     * @return int16_t 有符号值
     */
    static int16_t convertToSigned16(uint16_t value);

    /**
     * @brief 转换有符号16位值为无符号16位值
     * @param value 有符号值
     * @return uint16_t 无符号值
     */
    static uint16_t convertFromSigned16(int16_t value);
};

#endif // ISCS_H
