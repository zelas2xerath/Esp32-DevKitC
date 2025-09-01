/**
 * @file ISCS.cpp
 * @brief 智能土壤综合传感器(Intelligent Soil Comprehensive Sensor)实现文件
 * @author ESP32开发团队
 * @date 2025-07-20
 * @version 2.1.3
 * 
 * 本文件实现了ISCS土壤传感器的完整通信功能，包括：
 * - RS485 Modbus RTU协议实现
 * - 数据读取和配置管理
 * - CRC校验和错误处理
 * - 通信状态监控
 */

#include <ISCS.h>
#include <LogManager.h>  // 包含LOG_ERROR等日志宏定义

// ------------------【CRC16查找表】------------------

/**
 * @brief CRC16-Modbus查找表
 * 用于快速计算Modbus RTU协议的CRC16校验码
 */
static const uint16_t crc16_table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

// ------------------【构造和析构函数】------------------

ISCS::ISCS() : 
    _serial(nullptr),                           // 串口对象初始化为空
    _address(ISCS_DEFAULT_ADDRESS),             // 使用默认地址
    _baudRate(BaudRate::BAUD_9600),            // 使用默认波特率
    _isTransmitting(false),                     // 初始为接收状态
    _lastError(ModbusStatus::SUCCESS),          // 初始无错误
    _lastErrorMessage(""),                      // 初始错误信息为空
    _communicationCount(0),                     // 通信次数计数器清零
    _errorCount(0)                              // 错误次数计数器清零
{
    // 构造函数完成，对象已准备好进行初始化
}

ISCS::~ISCS() {
    // 安全释放串口对象内存
    if (_serial != nullptr) {
        delete _serial;
        _serial = nullptr;
    }
}

// ------------------【初始化和配置方法】------------------

bool ISCS::begin(uint8_t address, BaudRate baudRate) {
    // 验证地址范围 (1-253为有效Modbus地址)
    if (address < 1 || address > 253) {
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "传感器地址超出有效范围(1-253)";
        return false;
    }
    
    // 保存配置参数
    _address = address;
    _baudRate = baudRate;
    
    try {
        // 创建串口对象 - 使用UART2
        _serial = new HardwareSerial(2);
        if (_serial == nullptr) {
            _lastError = ModbusStatus::INVALID_RESPONSE;
            _lastErrorMessage = "串口对象创建失败";
            return false;
        }
        
        // 初始化串口通信
        // 参数：波特率, 数据位8+无校验+停止位1, RX引脚, TX引脚
        _serial->begin(static_cast<uint32_t>(_baudRate), SERIAL_8N1, 
                      RS485_RX_PIN, RS485_TX_PIN);
        
        // 配置RS485收发控制引脚
        pinMode(RS485_DE_RE_PIN, OUTPUT);
        enableReceive(); // 默认设置为接收模式
        
        // 清空接收缓冲区，确保干净的通信环境
        clearReceiveBuffer();
        
        // 重置统计信息
        resetStatistics();
        
        _lastError = ModbusStatus::SUCCESS;
        _lastErrorMessage = "ISCS传感器初始化成功";
        return true;
        
    } catch (...) {
        // 异常处理
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "初始化过程中发生异常";
        return false;
    }
}

void ISCS::setAddress(uint8_t address) {
    // 验证地址范围
    if (address >= 1 && address <= 253) {
        _address = address;
    }
}

uint8_t ISCS::getAddress() const {
    return _address;
}

void ISCS::setBaudRate(BaudRate baudRate) {
    _baudRate = baudRate;
    // 如果串口已初始化，更新波特率
    if (_serial != nullptr) {
        _serial->updateBaudRate(static_cast<uint32_t>(_baudRate));
    }
}

BaudRate ISCS::getBaudRate() const {
    return _baudRate;
}

// ------------------【通信控制方法】------------------

void ISCS::enableTransmit() {
    // 设置RS485为发送模式
    digitalWrite(RS485_DE_RE_PIN, HIGH);
    _isTransmitting = true;
    // 等待信号稳定，确保可靠的模式切换
    delayMicroseconds(50);
}

void ISCS::enableReceive() {
    // 设置RS485为接收模式
    digitalWrite(RS485_DE_RE_PIN, LOW);
    _isTransmitting = false;
    // 短暂延时确保模式切换完成
    delayMicroseconds(10);
}

bool ISCS::isTransmitting() const {
    return _isTransmitting;
}

// ------------------【数据读取功能】------------------

ModbusStatus ISCS::readSoilData(SoilSensorData& data, uint8_t targetAddress) {
    // 使用当前地址如果未指定目标地址
    if (targetAddress == 0) {
        targetAddress = _address;
    }
    
    uint8_t buffer[ISCS_MAX_FRAME_SIZE];  // 通信缓冲区
    size_t length;                        // 数据长度
    
    // 增加通信次数统计
    _communicationCount++;
    
    // 构建读取土壤数据的Modbus请求帧
    // 读取8个连续寄存器(0x0000-0x0007)，包含所有土壤参数
    if (!buildReadRequest(buffer, length, targetAddress, 
                         static_cast<uint16_t>(SensorRegister::SOIL_TEMPERATURE), 8)) {
        _errorCount++;
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "构建读取请求失败";
        return _lastError;
    }
    
    // 发送请求帧
    if (!sendModbusFrame(buffer, length)) {
        _errorCount++;
        _lastError = ModbusStatus::NO_RESPONSE;
        _lastErrorMessage = "发送请求失败";
        return _lastError;
    }
    
    // 接收响应帧
    if (!receiveModbusFrame(buffer, length)) {
        _errorCount++;
        _lastError = ModbusStatus::TIMEOUT;
        _lastErrorMessage = "接收响应超时";
        return _lastError;
    }
    
    // 验证响应帧格式
    if (!parseReadResponse(buffer, length, targetAddress, 8)) {
        _errorCount++;
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "响应帧格式错误";
        return _lastError;
    }
    
    // 解析土壤数据 (跳过地址、功能码、字节数，从第4个字节开始)
    const uint8_t* dataPtr = buffer + 3;

    // 解析各项土壤参数 (每个参数占2个字节，高字节在前)
    uint16_t rawTemp = dataPtr[0] << 8 | dataPtr[1];
    uint16_t rawMoisture = dataPtr[2] << 8 | dataPtr[3];
    uint16_t rawEC = dataPtr[4] << 8 | dataPtr[5];
    uint16_t rawSalinity = dataPtr[6] << 8 | dataPtr[7];
    uint16_t rawNitrogen = dataPtr[8] << 8 | dataPtr[9];
    uint16_t rawPhosphorus = dataPtr[10] << 8 | dataPtr[11];
    uint16_t rawPotassium = dataPtr[12] << 8 | dataPtr[13];
    uint16_t rawPH = dataPtr[14] << 8 | dataPtr[15];

    // 记录原始数据用于诊断（始终启用以便问题排查）
    LOG_TRACE("ISCS原始数据: 温度=0x%04X(%d), 湿度=0x%04X(%d), EC=0x%04X(%d), pH=0x%04X(%d), 盐分=0x%04X(%d), 氮=0x%04X(%d), 磷=0x%04X(%d), 钾=0x%04X(%d)",
              rawTemp, rawTemp, rawMoisture, rawMoisture, rawEC, rawEC, rawPH, rawPH,
              rawSalinity, rawSalinity, rawNitrogen, rawNitrogen, rawPhosphorus, rawPhosphorus, rawPotassium, rawPotassium);

    // 转换原始数据为实际值
    data.temperature = convertTemperature(rawTemp);
    data.moisture = convertMoisture(rawMoisture);
    data.ec = rawEC;
    data.salinity = rawSalinity;
    data.nitrogen = rawNitrogen;
    data.phosphorus = rawPhosphorus;
    data.potassium = rawPotassium;
    data.ph = convertPH(rawPH);

    // 验证转换后的数据是否在合理范围内
    if (!data.isValid()) {
        _errorCount++;
        _lastError = ModbusStatus::DATA_VALIDATION_FAILED;

        // 提供详细的数据验证失败信息
        String detailedMsg = "转换后的传感器数据超出有效范围 - ";
        detailedMsg += "温度:" + String(data.temperature, 1) + "°C";
        detailedMsg += ", 湿度:" + String(data.moisture, 1) + "%";
        detailedMsg += ", pH:" + String(data.ph, 2);
        detailedMsg += ", EC:" + String(data.ec) + " μS/cm";
        detailedMsg += ", 盐分:" + String(data.salinity) + " mg/L";
        detailedMsg += ", 氮:" + String(data.nitrogen) + " mg/kg";
        detailedMsg += ", 磷:" + String(data.phosphorus) + " mg/kg";
        detailedMsg += ", 钾:" + String(data.potassium) + " mg/kg";

        // 分析具体的验证失败原因
        String failureReason = "";
        if (data.temperature < -40.0 || data.temperature > 80.0) {
            failureReason += "温度超范围;";
        }
        if (data.moisture < 0.0 || data.moisture > 100.0) {
            failureReason += "湿度超范围;";
        }
        if (data.ph < 0.0 || data.ph > 14.0) {
            failureReason += "pH超范围;";
        }
        if (data.ec > 20000) {
            failureReason += "EC超范围;";
        }
        if (isnan(data.temperature) || isnan(data.moisture) || isnan(data.ph)) {
            failureReason += "数据包含NaN;";
        }
        // 检查异常的零值模式（可能表示传感器硬件问题）
        if (data.moisture == 0.0 && data.ph == 0.0 && data.ec == 0) {
            failureReason += "多个参数为零值(可能硬件故障);";
        }

        _lastErrorMessage = detailedMsg + " [失败原因:" + failureReason + "]";
        LOG_ERROR("ISCS: %s", _lastErrorMessage.c_str());
        return _lastError;
    }
    
    _lastError = ModbusStatus::SUCCESS;
    _lastErrorMessage = "土壤数据读取成功";
    return _lastError;
}

ModbusStatus ISCS::readSensorConfig(SensorConfig& config, uint8_t targetAddress) {
    // 使用当前地址如果未指定目标地址
    if (targetAddress == 0) {
        targetAddress = _address;
    }
    
    uint8_t buffer[ISCS_MAX_FRAME_SIZE];  // 通信缓冲区
    size_t length;                        // 数据长度
    
    // 增加通信次数统计
    _communicationCount++;
    
    // 构建读取配置的Modbus请求帧
    // 读取4个连续寄存器(0x0030-0x0033)，包含所有配置参数
    if (!buildReadRequest(buffer, length, targetAddress, 
                         static_cast<uint16_t>(SensorRegister::DEVICE_ADDRESS), 4)) {
        _errorCount++;
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "构建配置读取请求失败";
        return _lastError;
    }
    
    // 发送请求帧
    if (!sendModbusFrame(buffer, length)) {
        _errorCount++;
        _lastError = ModbusStatus::NO_RESPONSE;
        _lastErrorMessage = "发送配置请求失败";
        return _lastError;
    }
    
    // 接收响应帧
    if (!receiveModbusFrame(buffer, length)) {
        _errorCount++;
        _lastError = ModbusStatus::TIMEOUT;
        _lastErrorMessage = "接收配置响应超时";
        return _lastError;
    }
    
    // 验证响应帧格式
    if (!parseReadResponse(buffer, length, targetAddress, 4)) {
        _errorCount++;
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "配置响应帧格式错误";
        return _lastError;
    }
    
    // 解析配置数据 (跳过地址、功能码、字节数，从第4个字节开始)
    const uint8_t* dataPtr = buffer + 3;
    
    // 解析各项配置参数
    config.address = static_cast<uint8_t>(dataPtr[0] << 8 | dataPtr[1]);
    config.baudRate = static_cast<BaudRate>(dataPtr[2] << 8 | dataPtr[3]);
    config.parity = static_cast<Parity>(dataPtr[4] << 8 | dataPtr[5]);
    config.autoReport = dataPtr[6] << 8 | dataPtr[7];
    
    _lastError = ModbusStatus::SUCCESS;
    _lastErrorMessage = "传感器配置读取成功";
    return _lastError;
}

// ------------------【配置写入功能】------------------

ModbusStatus ISCS::writeSensorAddress(uint8_t newAddress, uint8_t targetAddress) {
    // 验证新地址范围
    if (newAddress < 1 || newAddress > 253) {
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "新地址超出有效范围(1-253)";
        return _lastError;
    }

    // 使用当前地址如果未指定目标地址
    if (targetAddress == 0) {
        targetAddress = _address;
    }

    uint8_t buffer[ISCS_MAX_FRAME_SIZE];  // 通信缓冲区
    size_t length;                        // 数据长度

    // 增加通信次数统计
    _communicationCount++;

    // 构建写入地址的Modbus请求帧
    if (!buildWriteRequest(buffer, length, targetAddress,
                          static_cast<uint16_t>(SensorRegister::DEVICE_ADDRESS), newAddress)) {
        _errorCount++;
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "构建地址写入请求失败";
        return _lastError;
    }

    // 发送请求帧
    if (!sendModbusFrame(buffer, length)) {
        _errorCount++;
        _lastError = ModbusStatus::NO_RESPONSE;
        _lastErrorMessage = "发送地址写入请求失败";
        return _lastError;
    }

    // 接收响应帧
    if (!receiveModbusFrame(buffer, length)) {
        _errorCount++;
        _lastError = ModbusStatus::TIMEOUT;
        _lastErrorMessage = "接收地址写入响应超时";
        return _lastError;
    }

    // 验证响应帧格式
    if (!parseWriteResponse(buffer, length, targetAddress,
                           static_cast<uint16_t>(SensorRegister::DEVICE_ADDRESS))) {
        _errorCount++;
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "地址写入响应帧格式错误";
        return _lastError;
    }

    // 更新本地地址
    _address = newAddress;

    _lastError = ModbusStatus::SUCCESS;
    _lastErrorMessage = "传感器地址写入成功";
    return _lastError;
}

ModbusStatus ISCS::writeSensorConfig(const SensorConfig& config, uint8_t targetAddress) {
    // 验证配置参数
    if (config.address < 1 || config.address > 253) {
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "配置中的地址超出有效范围";
        return _lastError;
    }

    // 使用当前地址如果未指定目标地址
    if (targetAddress == 0) {
        targetAddress = _address;
    }

    // 分别写入各个配置项

    // 写入设备地址
    ModbusStatus status = writeSensorAddress(config.address, targetAddress);
    if (status != ModbusStatus::SUCCESS) {
        return status;
    }

    // 写入波特率
    status = writeSensorBaudRate(config.baudRate, targetAddress);
    if (status != ModbusStatus::SUCCESS) {
        return status;
    }

    // 写入校验位
    status = writeSensorParity(config.parity, targetAddress);
    if (status != ModbusStatus::SUCCESS) {
        return status;
    }

    // 写入自动上报间隔
    status = writeSensorAutoReport(config.autoReport, targetAddress);
    if (status != ModbusStatus::SUCCESS) {
        return status;
    }

    _lastError = ModbusStatus::SUCCESS;
    _lastErrorMessage = "传感器配置写入成功";
    return _lastError;
}

ModbusStatus ISCS::querySensorAddress(uint8_t& address) {
    // 广播查询地址 - 使用地址0进行广播
    SensorConfig config;
    ModbusStatus status = readSensorConfig(config, 0);

    if (status == ModbusStatus::SUCCESS) {
        address = config.address;
    }

    return status;
}

ModbusStatus ISCS::writeSensorParity(Parity parity, uint8_t targetAddress) {
    // 使用当前地址如果未指定目标地址
    if (targetAddress == 0) {
        targetAddress = _address;
    }

    uint8_t buffer[ISCS_MAX_FRAME_SIZE];  // 通信缓冲区
    size_t length;                        // 数据长度

    // 增加通信次数统计
    _communicationCount++;

    // 构建写入校验位的Modbus请求帧
    if (!buildWriteRequest(buffer, length, targetAddress,
                          static_cast<uint16_t>(SensorRegister::PARITY),
                          static_cast<uint16_t>(parity))) {
        _errorCount++;
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "构建校验位写入请求失败";
        return _lastError;
    }

    // 发送请求帧
    if (!sendModbusFrame(buffer, length)) {
        _errorCount++;
        _lastError = ModbusStatus::NO_RESPONSE;
        _lastErrorMessage = "发送校验位写入请求失败";
        return _lastError;
    }

    // 接收响应帧
    if (!receiveModbusFrame(buffer, length)) {
        _errorCount++;
        _lastError = ModbusStatus::TIMEOUT;
        _lastErrorMessage = "接收校验位写入响应超时";
        return _lastError;
    }

    // 验证响应帧格式
    if (!parseWriteResponse(buffer, length, targetAddress,
                           static_cast<uint16_t>(SensorRegister::PARITY))) {
        _errorCount++;
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "校验位写入响应帧格式错误";
        return _lastError;
    }

    _lastError = ModbusStatus::SUCCESS;
    _lastErrorMessage = "传感器校验位写入成功";
    return _lastError;
}

ModbusStatus ISCS::writeSensorAutoReport(uint16_t interval, uint8_t targetAddress) {
    // 使用当前地址如果未指定目标地址
    if (targetAddress == 0) {
        targetAddress = _address;
    }

    uint8_t buffer[ISCS_MAX_FRAME_SIZE];  // 通信缓冲区
    size_t length;                        // 数据长度

    // 增加通信次数统计
    _communicationCount++;

    // 构建写入自动上报间隔的Modbus请求帧
    if (!buildWriteRequest(buffer, length, targetAddress,
                          static_cast<uint16_t>(SensorRegister::AUTO_REPORT), interval)) {
        _errorCount++;
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "构建自动上报写入请求失败";
        return _lastError;
    }

    // 发送请求帧
    if (!sendModbusFrame(buffer, length)) {
        _errorCount++;
        _lastError = ModbusStatus::NO_RESPONSE;
        _lastErrorMessage = "发送自动上报写入请求失败";
        return _lastError;
    }

    // 接收响应帧
    if (!receiveModbusFrame(buffer, length)) {
        _errorCount++;
        _lastError = ModbusStatus::TIMEOUT;
        _lastErrorMessage = "接收自动上报写入响应超时";
        return _lastError;
    }

    // 验证响应帧格式
    if (!parseWriteResponse(buffer, length, targetAddress,
                           static_cast<uint16_t>(SensorRegister::AUTO_REPORT))) {
        _errorCount++;
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "自动上报写入响应帧格式错误";
        return _lastError;
    }

    _lastError = ModbusStatus::SUCCESS;
    _lastErrorMessage = "传感器自动上报间隔写入成功";
    return _lastError;
}

ModbusStatus ISCS::writeSensorBaudRate(BaudRate baudRate, uint8_t targetAddress) {
    // 使用当前地址如果未指定目标地址
    if (targetAddress == 0) {
        targetAddress = _address;
    }

    uint8_t buffer[ISCS_MAX_FRAME_SIZE];  // 通信缓冲区
    size_t length;                        // 数据长度

    // 增加通信次数统计
    _communicationCount++;

    // 构建写入波特率的Modbus请求帧
    if (!buildWriteRequest(buffer, length, targetAddress,
                          static_cast<uint16_t>(SensorRegister::BAUD_RATE),
                          static_cast<uint16_t>(baudRate))) {
        _errorCount++;
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "构建波特率写入请求失败";
        return _lastError;
    }

    // 发送请求帧
    if (!sendModbusFrame(buffer, length)) {
        _errorCount++;
        _lastError = ModbusStatus::NO_RESPONSE;
        _lastErrorMessage = "发送波特率写入请求失败";
        return _lastError;
    }

    // 接收响应帧
    if (!receiveModbusFrame(buffer, length)) {
        _errorCount++;
        _lastError = ModbusStatus::TIMEOUT;
        _lastErrorMessage = "接收波特率写入响应超时";
        return _lastError;
    }

    // 验证响应帧格式
    if (!parseWriteResponse(buffer, length, targetAddress,
                           static_cast<uint16_t>(SensorRegister::BAUD_RATE))) {
        _errorCount++;
        _lastError = ModbusStatus::INVALID_RESPONSE;
        _lastErrorMessage = "波特率写入响应帧格式错误";
        return _lastError;
    }

    // 更新本地波特率配置
    _baudRate = baudRate;
    if (_serial != nullptr) {
        _serial->updateBaudRate(static_cast<uint32_t>(_baudRate));
    }

    _lastError = ModbusStatus::SUCCESS;
    _lastErrorMessage = "传感器波特率写入成功";
    return _lastError;
}

// ------------------【高级功能】------------------

bool ISCS::pingSensor(uint8_t targetAddress) {
    // 使用当前地址如果未指定目标地址
    if (targetAddress == 0) {
        targetAddress = _address;
    }

    uint8_t buffer[ISCS_MAX_FRAME_SIZE];  // 通信缓冲区
    size_t length;                        // 数据长度

    // 构建ping请求 - 读取1个寄存器进行连接测试
    if (!buildReadRequest(buffer, length, targetAddress, 0x0000, 1)) {
        return false;
    }

    // 发送ping请求
    if (!sendModbusFrame(buffer, length)) {
        return false;
    }

    // 接收ping响应 - 使用较短的超时时间
    if (!receiveModbusFrame(buffer, length, ISCS_PING_TIMEOUT)) {
        return false;
    }

    // 如果能接收到响应，说明传感器连接正常
    return true;
}

bool ISCS::isSensorConnected(uint8_t targetAddress) {
    return pingSensor(targetAddress);
}

ModbusStatus ISCS::getLastError() const {
    return _lastError;
}

String ISCS::getLastErrorMessage() const {
    return _lastErrorMessage;
}

// ------------------【数据转换工具】------------------

float ISCS::convertTemperature(const uint16_t rawValue) {
    // 土壤温度转换公式：实际温度 = 有符号原始值 / 10.0
    // 原始数据使用补码表示，支持范围：-40°C 到 +80°C
    // 示例：0xFFDD = -35 (补码) → -3.5°C
    const auto signedValue = static_cast<int16_t>(rawValue);
    return static_cast<float>(signedValue) / 10.0f;
}

float ISCS::convertMoisture(const uint16_t rawValue) {
    // 土壤湿度转换公式：实际湿度 = 原始值 / 10.0
    // 支持范围：0% 到 100%
    return static_cast<float>(rawValue) / 10.0f;
}

float ISCS::convertPH(const uint16_t rawValue) {
    // 土壤pH值转换公式：实际pH = 原始值 / 100.0
    // 支持范围：3.0 到 10.0
    return static_cast<float>(rawValue) / 100.0f;
}

uint16_t ISCS::convertToRawTemperature(float temperature) {
    // 温度反向转换公式：原始值 = 实际温度 * 10.0 (使用补码表示)
    // 限制在有效范围内
    if (temperature < -40.0f) temperature = -40.0f;
    if (temperature > 80.0f) temperature = 80.0f;

    // 转换为有符号整数，然后转换为无符号（保持补码表示）
    const auto signedRaw = static_cast<int16_t>(temperature * 10.0f);
    return static_cast<uint16_t>(signedRaw);
}

uint16_t ISCS::convertToRawMoisture(float moisture) {
    // 湿度反向转换公式：原始值 = 实际湿度 * 10.0
    // 限制在有效范围内
    if (moisture < 0.0f) moisture = 0.0f;
    if (moisture > 100.0f) moisture = 100.0f;
    return static_cast<uint16_t>(moisture * 10.0f);
}

uint16_t ISCS::convertToRawPH(float ph) {
    // pH值反向转换公式：原始值 = 实际pH * 100.0
    // 限制在有效范围内
    if (ph < 3.0f) ph = 3.0f;
    if (ph > 10.0f) ph = 10.0f;
    return static_cast<uint16_t>(ph * 100.0f);
}

// ------------------【统计信息】------------------

unsigned long ISCS::getCommunicationCount() const {
    return _communicationCount;
}

unsigned long ISCS::getErrorCount() const {
    return _errorCount;
}

float ISCS::getSuccessRate() const {
    // 计算通信成功率
    if (_communicationCount == 0) {
        return 0.0f;
    }
    return static_cast<float>(_communicationCount - _errorCount) / static_cast<float>(_communicationCount);
}

void ISCS::resetStatistics() {
    // 重置所有统计信息
    _communicationCount = 0;
    _errorCount = 0;
}

// ------------------【私有静态方法 - CRC计算】------------------

uint16_t ISCS::calculateCRC16(const uint8_t* data, size_t length) {
    // 使用Modbus标准CRC16算法
    uint16_t crc = 0xFFFF;  // 初始值

    for (size_t i = 0; i < length; i++) {
        // 计算表索引
        uint8_t tableIndex = (crc ^ data[i]) & 0xFF;
        // 更新CRC值
        crc = crc >> 8 ^ crc16_table[tableIndex];
    }

    return crc;
}

bool ISCS::verifyCRC16(const uint8_t* data, size_t length) {
    // 验证CRC16校验码
    if (length < 3) {
        return false;  // 数据长度不足
    }

    // 计算除CRC外的数据部分的CRC
    uint16_t calculatedCRC = calculateCRC16(data, length - 2);

    // 提取接收到的CRC (低字节在前，高字节在后)
    uint16_t receivedCRC = data[length - 2] | data[length - 1] << 8;

    // 比较计算的CRC和接收到的CRC
    return calculatedCRC == receivedCRC;
}

// ------------------【私有方法 - Modbus帧处理】------------------

bool ISCS::buildReadRequest(uint8_t* buffer, size_t& length,
                           uint8_t address, uint16_t startRegister, uint16_t registerCount) {
    // 验证参数
    if (buffer == nullptr || address == 0 || registerCount == 0 || registerCount > 125) {
        return false;
    }

    // 构建Modbus RTU读取请求帧
    buffer[0] = address;                                    // 设备地址
    buffer[1] = static_cast<uint8_t>(ModbusFunctionCode::READ_HOLDING_REGISTERS);  // 功能码
    buffer[2] = static_cast<uint8_t>(startRegister >> 8);   // 起始寄存器高字节
    buffer[3] = static_cast<uint8_t>(startRegister & 0xFF); // 起始寄存器低字节
    buffer[4] = static_cast<uint8_t>(registerCount >> 8);   // 寄存器数量高字节
    buffer[5] = static_cast<uint8_t>(registerCount & 0xFF); // 寄存器数量低字节

    // 计算并添加CRC16校验码
    uint16_t crc = calculateCRC16(buffer, 6);
    buffer[6] = static_cast<uint8_t>(crc & 0xFF);           // CRC低字节
    buffer[7] = static_cast<uint8_t>(crc >> 8);             // CRC高字节

    length = 8;  // 总帧长度
    return true;
}

bool ISCS::buildWriteRequest(uint8_t* buffer, size_t& length,
                            uint8_t address, uint16_t reg, uint16_t value) {
    // 验证参数
    if (buffer == nullptr || address == 0) {
        return false;
    }

    // 构建Modbus RTU写入单个寄存器请求帧
    buffer[0] = address;                                    // 设备地址
    buffer[1] = static_cast<uint8_t>(ModbusFunctionCode::WRITE_SINGLE_REGISTER);  // 功能码
    buffer[2] = static_cast<uint8_t>(reg >> 8);             // 寄存器地址高字节
    buffer[3] = static_cast<uint8_t>(reg & 0xFF);           // 寄存器地址低字节
    buffer[4] = static_cast<uint8_t>(value >> 8);           // 寄存器值高字节
    buffer[5] = static_cast<uint8_t>(value & 0xFF);         // 寄存器值低字节

    // 计算并添加CRC16校验码
    uint16_t crc = calculateCRC16(buffer, 6);
    buffer[6] = static_cast<uint8_t>(crc & 0xFF);           // CRC低字节
    buffer[7] = static_cast<uint8_t>(crc >> 8);             // CRC高字节

    length = 8;  // 总帧长度
    return true;
}

bool ISCS::parseReadResponse(const uint8_t* data, size_t length,
                            uint8_t expectedAddress, uint16_t expectedRegisters) {
    // 验证基本参数
    if (data == nullptr || length < 5) {
        return false;
    }

    // 验证CRC16校验码
    if (!verifyCRC16(data, length)) {
        return false;
    }

    // 验证设备地址
    if (data[0] != expectedAddress) {
        return false;
    }

    // 验证功能码
    if (data[1] != static_cast<uint8_t>(ModbusFunctionCode::READ_HOLDING_REGISTERS)) {
        return false;
    }

    // 验证数据字节数
    uint8_t expectedBytes = expectedRegisters * 2;
    if (data[2] != expectedBytes) {
        return false;
    }

    // 验证总帧长度 (地址 + 功能码 + 字节数 + 数据 + CRC)
    if (length != static_cast<size_t>(3 + expectedBytes + 2)) {
        return false;
    }

    return true;
}

bool ISCS::parseWriteResponse(const uint8_t* data, size_t length,
                             uint8_t expectedAddress, uint16_t expectedRegister) {
    // 验证基本参数
    if (data == nullptr || length != 8) {
        return false;
    }

    // 验证CRC16校验码
    if (!verifyCRC16(data, length)) {
        return false;
    }

    // 验证设备地址
    if (data[0] != expectedAddress) {
        return false;
    }

    // 验证功能码
    if (data[1] != static_cast<uint8_t>(ModbusFunctionCode::WRITE_SINGLE_REGISTER)) {
        return false;
    }

    // 验证寄存器地址
    uint16_t responseRegister = data[2] << 8 | data[3];
    if (responseRegister != expectedRegister) {
        return false;
    }

    return true;
}

// ------------------【私有方法 - 通信辅助】------------------

bool ISCS::sendModbusFrame(const uint8_t* data, size_t length) {
    // 验证参数和串口状态
    if (data == nullptr || length == 0 || _serial == nullptr) {
        return false;
    }

    // 清空接收缓冲区
    clearReceiveBuffer();

    // 切换到发送模式
    enableTransmit();

    // 发送数据帧
    size_t bytesWritten = _serial->write(data, length);

    // 等待发送完成
    _serial->flush();

    // 切换回接收模式
    enableReceive();

    // 检查是否发送完整
    return bytesWritten == length;
}

bool ISCS::receiveModbusFrame(uint8_t* buffer, size_t& length, unsigned long timeout) {
    // 验证参数
    if (buffer == nullptr || _serial == nullptr) {
        length = 0;
        return false;
    }

    // 确保处于接收模式
    enableReceive();

    unsigned long startTime = millis();
    size_t bytesReceived = 0;
    bool frameComplete = false;

    // 接收数据帧
    while (!frameComplete && millis() - startTime < timeout) {
        // 检查是否有数据可读
        if (_serial->available() > 0) {
            // 读取一个字节
            int receivedByte = _serial->read();
            if (receivedByte >= 0 && bytesReceived < ISCS_MAX_FRAME_SIZE - 1) {
                buffer[bytesReceived++] = static_cast<uint8_t>(receivedByte);

                // 检查是否接收到完整帧
                if (bytesReceived >= 5) {  // 最小帧长度
                    // 简单的帧完整性检查：如果连续10ms没有新数据，认为帧结束
                    unsigned long lastByteTime = millis();
                    while (millis() - lastByteTime < 10) {
                        if (_serial->available() > 0) {
                            receivedByte = _serial->read();
                            if (receivedByte >= 0 && bytesReceived < ISCS_MAX_FRAME_SIZE - 1) {
                                buffer[bytesReceived++] = static_cast<uint8_t>(receivedByte);
                                lastByteTime = millis();
                            }
                        }
                        delay(1);
                    }
                    frameComplete = true;
                }
            }
        }
        delay(1);  // 短暂延时避免过度占用CPU
    }

    length = bytesReceived;
    return frameComplete && bytesReceived > 0;
}

void ISCS::clearReceiveBuffer() const
{
    // 清空串口接收缓冲区
    if (_serial != nullptr) {
        while (_serial->available() > 0) {
            _serial->read();
        }
    }
}

// ------------------【私有方法 - 数据转换】------------------

int16_t ISCS::convertToSigned16(const uint16_t value) {
    // 将无符号16位值转换为有符号16位值
    return static_cast<int16_t>(value);
}

uint16_t ISCS::convertFromSigned16(const int16_t value) {
    // 将有符号16位值转换为无符号16位值
    return static_cast<uint16_t>(value);
}
