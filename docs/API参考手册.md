# ESP32传感器管理系统API参考手册

## 📋 文档信息

- **项目名称**: ESP32智能传感器管理系统
- **API版本**: v9.0.0
- **文档版本**: v1.0.0
- **创建日期**: 2025-07-21
- **适用范围**: 开发者、系统集成商、高级用户

## 🎯 API概述

本文档提供ESP32传感器管理系统所有公共接口的详细说明，包括传感器类、数据滤波器、智能光照管理器等核心组件的API接口。

### API设计原则

- **一致性**: 所有传感器类遵循统一的接口设计模式
- **易用性**: 简洁明了的方法命名和参数设计
- **可靠性**: 完整的错误处理和状态反馈机制
- **扩展性**: 支持新功能和新传感器类型的扩展

### 错误处理机制

所有API都使用统一的错误处理机制：

```cpp
enum class ErrorCode {
    // 通用错误码 (0-99)
    SUCCESS = 0,                // 成功
    UNKNOWN_ERROR = 1,          // 未知错误
    INVALID_PARAMETER = 2,      // 无效参数
    TIMEOUT = 3,                // 操作超时
    INITIALIZATION_FAILED = 4,  // 初始化失败
    NOT_INITIALIZED = 5,        // 未初始化
    RESOURCE_BUSY = 6,          // 资源忙
    RESOURCE_UNAVAILABLE = 7,   // 资源不可用
    OPERATION_FAILED = 8,       // 操作失败
    
    // 传感器错误码 (100-199)
    SENSOR_READ_ERROR = 100,    // 传感器读取错误
    SENSOR_TIMEOUT = 101,       // 传感器超时
    SENSOR_CALIBRATION_ERROR = 102, // 传感器校准错误
    SENSOR_OUT_OF_RANGE = 103,  // 传感器读数超出范围
    SENSOR_CONNECTION_ERROR = 104, // 传感器连接错误
    SENSOR_INITIALIZATION_FAILED = 105, // 传感器初始化失败
    SENSOR_CONFIGURATION_ERROR = 106, // 传感器配置错误
    SENSOR_DATA_VALIDATION_FAILED = 107, // 传感器数据验证失败
    
    // 网络错误码 (200-299)
    NETWORK_CONNECTION_ERROR = 200, // 网络连接错误
    NETWORK_TIMEOUT = 201,      // 网络超时
    SERVER_CONNECTION_ERROR = 202, // 服务器连接错误
    SERVER_RESPONSE_ERROR = 203, // 服务器响应错误
    WIFI_CONNECTION_ERROR = 204, // WiFi连接错误
    MQTT_CONNECTION_ERROR = 205, // MQTT连接错误
    NETWORK_ERROR = 206,        // 通用网络错误
    
    // 存储错误码 (300-399)
    STORAGE_WRITE_ERROR = 300,  // 存储写入错误
    STORAGE_READ_ERROR = 301,   // 存储读取错误
    STORAGE_FULL = 302,         // 存储空间已满
    
    // 控制系统错误码 (400-499)
    CONTROL_SYSTEM_ERROR = 400, // 控制系统错误
    THRESHOLD_INVALID = 401,    // 阈值无效
    FORCE_MODE_ERROR = 402,     // 强制模式错误
    
    // 配置错误码 (500-599)
    CONFIG_INVALID = 500,       // 配置无效
    CONFIG_SAVE_ERROR = 501,    // 配置保存错误
    CONFIG_LOAD_ERROR = 502,    // 配置加载错误
    
    // 硬件错误码 (600-699)
    HARDWARE_ERROR = 600,       // 硬件错误
    PIN_ERROR = 601,            // 引脚错误
    I2C_ERROR = 602,            // I2C错误
    SPI_ERROR = 603,            // SPI错误
    UART_ERROR = 604,           // UART错误
    
    // 系统错误码 (700-799)
    SYSTEM_ERROR = 700,         // 系统错误
    MEMORY_ERROR = 701,         // 内存错误
    TASK_ERROR = 702,           // 任务错误
    WATCHDOG_TIMEOUT = 703,     // 看门狗超时
    STACK_OVERFLOW = 704,       // 堆栈溢出
    MEMORY_ALLOCATION_FAILED = 705, // 内存分配失败
    WATER_LEVEL_LOW = 706,      // 水位过低
    
    // 其他错误码 (800-899)
    COMMAND_ERROR = 800,        // 命令错误
    PARAMETER_ERROR = 801,      // 参数错误
    PERMISSION_ERROR = 802,     // 权限错误
    
    // 严重错误码 (900-999)
    CRITICAL_ERROR = 900,       // 严重错误
    SYSTEM_HALT = 999           // 系统停止
};
```

## 🌱 SMS土壤湿度传感器API

### 类概述

SMS类提供土壤湿度传感器的完整功能接口，支持ADC读取、数据校准和状态监控。

```cpp
class SMS {
public:
    // 构造函数和析构函数
    SMS(int adcPin, int powerPin = -1);
    ~SMS();
    
    // 核心功能方法
    bool begin();
    int readSoilMoisture();
    
    // 状态和错误处理
    ErrorCode getLastErrorCode() const;
    String getLastErrorMessage() const;
    unsigned long getReadCount() const;
    
    // 静态工具方法
    static bool isDataValid(int moisture);
    static String getMoistureLevel(int moisture);
    static int calibrateMoisture(int rawValue);
};
```

### 方法详细说明

#### 构造函数

```cpp
SMS(int adcPin, int powerPin = -1)
```

**功能**: 创建SMS传感器实例

**参数**:
- `adcPin`: ADC引脚号（必需）
- `powerPin`: 电源控制引脚号（可选，-1表示不使用）

**示例**:
```cpp
// 使用GPIO36作为ADC引脚，GPIO4作为电源控制引脚
SMS soilSensor(36, 4);

// 只使用ADC引脚，不使用电源控制
SMS soilSensor(36);
```

#### begin()

```cpp
bool begin()
```

**功能**: 初始化传感器

**返回值**: 
- `true`: 初始化成功
- `false`: 初始化失败

**示例**:
```cpp
if (soilSensor.begin()) {
    Serial.println("SMS传感器初始化成功");
} else {
    Serial.println("SMS传感器初始化失败");
    Serial.println(soilSensor.getLastErrorMessage());
}
```

#### readSoilMoisture()

```cpp
int readSoilMoisture()
```

**功能**: 读取土壤湿度值

**返回值**: 
- `0-4095`: 有效的湿度值（12位ADC）
- `-1`: 读取失败

**数据含义**:
- `0-1000`: 干燥土壤
- `1000-2500`: 适中湿度
- `2500-4095`: 湿润土壤

**示例**:
```cpp
int moisture = soilSensor.readSoilMoisture();
if (moisture >= 0) {
    Serial.printf("土壤湿度: %d (%s)\n", 
                  moisture, 
                  SMS::getMoistureLevel(moisture).c_str());
} else {
    Serial.println("读取失败: " + soilSensor.getLastErrorMessage());
}
```

#### 静态工具方法

```cpp
static bool isDataValid(int moisture)
```

**功能**: 验证湿度数据的有效性

**参数**: `moisture` - 待验证的湿度值

**返回值**: 
- `true`: 数据有效
- `false`: 数据无效

```cpp
static String getMoistureLevel(int moisture)
```

**功能**: 获取湿度等级的中文描述

**参数**: `moisture` - 湿度值

**返回值**: 湿度等级字符串（"干燥"、"适中"、"湿润"）

**示例**:
```cpp
int moisture = 1500;
if (SMS::isDataValid(moisture)) {
    String level = SMS::getMoistureLevel(moisture);
    Serial.println("湿度等级: " + level);
}
```

## 💧 WDS水位传感器API

### 类概述

WDS类提供超声波水位传感器的功能接口，支持距离测量、水位计算和低水位检测。

```cpp
class WDS {
public:
    // 构造函数和析构函数
    WDS(int trigPin, int echoPin);
    ~WDS();
    
    // 核心功能方法
    bool begin();
    double readWaterDepth();
    bool isLowWaterLevel();
    
    // 配置方法
    void setTankHeight(double height);
    void setLowWaterThreshold(double threshold);
    
    // 状态和错误处理
    ErrorCode getLastErrorCode() const;
    String getLastErrorMessage() const;
    unsigned long getReadCount() const;
    
    // 静态工具方法
    static bool isDataValid(double depth);
    static String getWaterLevelStatus(double depth, double threshold);
};
```

### 方法详细说明

#### 构造函数

```cpp
WDS(int trigPin, int echoPin)
```

**功能**: 创建WDS传感器实例

**参数**:
- `trigPin`: 超声波触发引脚
- `echoPin`: 超声波回声引脚

**示例**:
```cpp
// 使用GPIO5作为触发引脚，GPIO18作为回声引脚
WDS waterSensor(5, 18);
```

#### readWaterDepth()

```cpp
double readWaterDepth()
```

**功能**: 读取水位深度

**返回值**: 
- `0.0-200.0`: 有效的水位深度（厘米）
- `-1.0`: 读取失败

**示例**:
```cpp
double depth = waterSensor.readWaterDepth();
if (depth >= 0) {
    Serial.printf("水位深度: %.2f cm\n", depth);
    if (waterSensor.isLowWaterLevel()) {
        Serial.println("警告: 低水位！");
    }
} else {
    Serial.println("读取失败: " + waterSensor.getLastErrorMessage());
}
```

#### 配置方法

```cpp
void setTankHeight(double height)
```

**功能**: 设置水箱总高度

**参数**: `height` - 水箱高度（厘米）

```cpp
void setLowWaterThreshold(double threshold)
```

**功能**: 设置低水位阈值

**参数**: `threshold` - 低水位阈值（厘米）

**示例**:
```cpp
waterSensor.setTankHeight(100.0);      // 设置水箱高度为100cm
waterSensor.setLowWaterThreshold(20.0); // 设置低水位阈值为20cm
```

## 💡 LIS光照传感器API

### 类概述

LIS类提供光照传感器的功能接口，支持光照强度测量、昼夜判断和光照等级分析。

```cpp
class LIS {
public:
    // 构造函数和析构函数
    LIS(uint8_t i2cAddress = 0x23);
    ~LIS();
    
    // 核心功能方法
    bool begin();
    float readLightLevel();
    bool isDaytime();
    
    // 配置方法
    void setDaytimeThreshold(float threshold);
    void setMeasurementMode(uint8_t mode);
    
    // 状态和错误处理
    ErrorCode getLastErrorCode() const;
    String getLastErrorMessage() const;
    unsigned long getReadCount() const;
    
    // 静态工具方法
    static bool isDataValid(float lightLevel);
    static String getLightLevelDescription(float lightLevel);
};
```

### 方法详细说明

#### readLightLevel()

```cpp
float readLightLevel()
```

**功能**: 读取光照强度

**返回值**: 
- `0.0-65535.0`: 有效的光照强度（lux）
- `-1.0`: 读取失败

**示例**:
```cpp
float lightLevel = lightSensor.readLightLevel();
if (lightLevel >= 0) {
    Serial.printf("光照强度: %.1f lux (%s)\n", 
                  lightLevel,
                  LIS::getLightLevelDescription(lightLevel).c_str());
    
    if (lightSensor.isDaytime()) {
        Serial.println("当前是白天");
    } else {
        Serial.println("当前是夜晚");
    }
} else {
    Serial.println("读取失败: " + lightSensor.getLastErrorMessage());
}
```

#### 静态工具方法

```cpp
static String getLightLevelDescription(float lightLevel)
```

**功能**: 获取光照强度的描述

**返回值**: 光照强度描述字符串

**光照等级对照表**:
- `0-10 lux`: "极暗"
- `10-100 lux`: "暗"
- `100-1000 lux`: "室内光"
- `1000-10000 lux`: "明亮"
- `10000+ lux`: "强光"

## 🌡️ ECS环境传感器API

### 类概述

ECS类提供环境传感器的功能接口，支持温度、湿度、气压的同时测量。

```cpp
class ECS {
public:
    // 构造函数和析构函数
    ECS(uint8_t i2cAddress = 0x76);
    ~ECS();
    
    // 核心功能方法
    bool begin();
    float readTemperature();
    float readHumidity();
    float readPressure();
    bool readAllData(float& temp, float& humidity, float& pressure);
    
    // 状态和错误处理
    ErrorCode getLastErrorCode() const;
    String getLastErrorMessage() const;
    unsigned long getReadCount() const;
    
    // 静态工具方法
    static bool isDataValid(float temperature, float humidity, float pressure);
    static String getComfortLevel(float temperature, float humidity);
};
```

### 方法详细说明

#### readAllData()

```cpp
bool readAllData(float& temp, float& humidity, float& pressure)
```

**功能**: 一次性读取所有环境数据

**参数**: 
- `temp`: 温度输出参数（°C）
- `humidity`: 湿度输出参数（%）
- `pressure`: 气压输出参数（hPa）

**返回值**: 
- `true`: 读取成功
- `false`: 读取失败

**示例**:
```cpp
float temp, humidity, pressure;
if (envSensor.readAllData(temp, humidity, pressure)) {
    if (ECS::isDataValid(temp, humidity, pressure)) {
        Serial.printf("环境数据 - 温度: %.1f°C, 湿度: %.1f%%, 气压: %.1f hPa\n", 
                      temp, humidity, pressure);
        Serial.println("舒适度: " + ECS::getComfortLevel(temp, humidity));
    } else {
        Serial.println("数据验证失败");
    }
} else {
    Serial.println("读取失败: " + envSensor.getLastErrorMessage());
}
```

## 🌿 CAS土壤综合传感器API

### 类概述

CAS类提供土壤综合传感器的功能接口，支持土壤温度、湿度、pH、EC、营养元素等多参数测量。

```cpp
struct SoilSensorData {
    float temperature;      // 土壤温度 (°C)
    float moisture;         // 土壤湿度 (%)
    float ph;              // pH值
    uint16_t ec;           // 电导率 (μS/cm)
    uint16_t nitrogen;     // 氮含量 (mg/L)
    uint16_t phosphorus;   // 磷含量 (mg/L)
    uint16_t potassium;    // 钾含量 (mg/L)
    uint16_t salinity;     // 盐分 (ppm)
};

class CAS {
public:
    // 构造函数和析构函数
    CAS(HardwareSerial* serial, int dePin = -1);
    ~CAS();
    
    // 核心功能方法
    bool begin();
    ErrorCode readSoilData(SoilSensorData& data);
    
    // 状态和错误处理
    ErrorCode getLastErrorCode() const;
    String getLastErrorMessage() const;
    unsigned long getReadCount() const;
    
    // 静态工具方法
    static bool isDataValid(const SoilSensorData& data);
    static String getSoilQualityAssessment(const SoilSensorData& data);
};
```

### 方法详细说明

#### readSoilData()

```cpp
ErrorCode readSoilData(SoilSensorData& data)
```

**功能**: 读取完整的土壤数据

**参数**: `data` - 土壤数据输出结构体

**返回值**: 错误码（ErrorCode::SUCCESS表示成功）

**示例**:
```cpp
SoilSensorData soilData;
ErrorCode result = soilSensor.readSoilData(soilData);

if (result == ErrorCode::SUCCESS && CAS::isDataValid(soilData)) {
    Serial.println("=== 土壤综合数据 ===");
    Serial.printf("土壤温度: %.1f°C\n", soilData.temperature);
    Serial.printf("土壤湿度: %.1f%%\n", soilData.moisture);
    Serial.printf("pH值: %.2f\n", soilData.ph);
    Serial.printf("电导率: %u μS/cm\n", soilData.ec);
    Serial.printf("氮含量: %u mg/L\n", soilData.nitrogen);
    Serial.printf("磷含量: %u mg/L\n", soilData.phosphorus);
    Serial.printf("钾含量: %u mg/L\n", soilData.potassium);
    Serial.printf("盐分: %u ppm\n", soilData.salinity);
    
    String assessment = CAS::getSoilQualityAssessment(soilData);
    Serial.println("土壤质量评估: " + assessment);
} else {
    Serial.println("读取失败: " + soilSensor.getLastErrorMessage());
}
```

## 🔧 数据滤波器API

### DataFilterManager类

```cpp
class DataFilterManager {
public:
    // 构造函数和析构函数
    DataFilterManager();
    ~DataFilterManager();
    
    // 初始化和控制
    bool begin();
    void maintenance();
    bool isInitialized() const;
    
    // 滤波方法
    int filterSMSData(int rawMoisture);
    double filterWDSData(double rawWaterLevel);
    float filterLISData(float rawLightLevel);
    bool filterECSData(float rawTemp, float rawHumidity, float rawPressure,
                       float& filteredTemp, float& filteredHumidity, float& filteredPressure);
    bool filterCASData(const SoilSensorData& rawData, SoilSensorData& filteredData);
    
    // 状态和统计
    void resetAllFilters();
    String getStatusSummary() const;
};

// 全局滤波器管理器实例
extern DataFilterManager* globalDataFilterManager;
```

### 使用示例

```cpp
// 初始化滤波器管理器
globalDataFilterManager = new DataFilterManager();
if (globalDataFilterManager->begin()) {
    Serial.println("滤波器管理器初始化成功");
}

// 使用滤波器处理传感器数据
int rawMoisture = soilSensor.readSoilMoisture();
if (rawMoisture >= 0) {
    int filteredMoisture = globalDataFilterManager->filterSMSData(rawMoisture);
    Serial.printf("原始值: %d, 滤波值: %d\n", rawMoisture, filteredMoisture);
}

// 定期维护
globalDataFilterManager->maintenance();
```

## 💡 智能光照管理器API

### LightManager类

```cpp
enum class LightLevel {
    EXTREME_DARK = 0,   // 极暗
    DARK = 1,           // 暗
    INDOOR = 2,         // 室内光
    BRIGHT = 3,         // 明亮
    INTENSE = 4         // 强光
};

enum class DeviceMode {
    ENERGY_SAVING = 0,  // 节能模式
    NORMAL = 1,         // 正常模式
    HIGH_BRIGHTNESS = 2,// 高亮模式
    AUTO = 3            // 自动模式
};

class LightManager {
public:
    // 构造函数和析构函数
    LightManager();
    ~LightManager();
    
    // 初始化和控制
    bool begin();
    void maintenance();
    bool isInitialized() const;
    
    // 核心功能
    bool updateLightData(float lightIntensity);
    const LightAnalysisResult& getCurrentAnalysis() const;
    DeviceMode getCurrentMode() const;
    
    // 模式控制
    bool setManualMode(DeviceMode mode);
    bool enableAutoMode();
    
    // 状态和统计
    const LightManagementStats& getStatistics() const;
    void resetStatistics();
    String getStatusSummary() const;
    bool isHealthy() const;
    
    // 静态工具方法
    static String getLightLevelString(LightLevel level);
    static String getDeviceModeString(DeviceMode mode);
};

// 全局光照管理器实例
extern LightManager* globalLightManager;
```

### 使用示例

```cpp
// 初始化光照管理器
globalLightManager = new LightManager();
if (globalLightManager->begin()) {
    Serial.println("光照管理器初始化成功");
}

// 更新光照数据并获取分析结果
float lightLevel = lightSensor.readLightLevel();
if (lightLevel >= 0) {
    if (globalLightManager->updateLightData(lightLevel)) {
        const LightAnalysisResult& analysis = globalLightManager->getCurrentAnalysis();
        
        if (analysis.isReliable) {
            Serial.printf("光照等级: %s\n", 
                         LightManager::getLightLevelString(analysis.currentLevel).c_str());
            Serial.printf("当前模式: %s\n", 
                         LightManager::getDeviceModeString(globalLightManager->getCurrentMode()).c_str());
            Serial.printf("趋势变化率: %.1f lux/分钟\n", analysis.trendRate);
        }
    }
}

// 手动设置设备模式
globalLightManager->setManualMode(DeviceMode::HIGH_BRIGHTNESS);

// 启用自动模式
globalLightManager->enableAutoMode();

// 定期维护
globalLightManager->maintenance();
```

## 📊 最佳实践

### 1. 错误处理最佳实践

```cpp
// 推荐的错误处理模式
int moisture = soilSensor.readSoilMoisture();
if (moisture < 0) {
    ErrorCode error = soilSensor.getLastErrorCode();
    String errorMsg = soilSensor.getLastErrorMessage();
    
    // 记录错误日志
    Serial.printf("SMS传感器错误 [%d]: %s\n", static_cast<int>(error), errorMsg.c_str());
    
    // 根据错误类型采取相应措施
    switch (error) {
        case ErrorCode::SENSOR_NOT_FOUND:
            // 检查硬件连接
            break;
        case ErrorCode::SENSOR_READ_FAILED:
            // 重试读取
            break;
        case ErrorCode::TIMEOUT:
            // 增加超时时间或检查通信
            break;
        default:
            // 其他错误处理
            break;
    }
}
```

### 2. 性能优化建议

```cpp
// 批量读取环境数据，提高效率
float temp, humidity, pressure;
if (envSensor.readAllData(temp, humidity, pressure)) {
    // 一次性处理所有数据
    processEnvironmentData(temp, humidity, pressure);
}

// 使用滤波器提高数据质量
if (globalDataFilterManager && globalDataFilterManager->isInitialized()) {
    float filteredTemp, filteredHumidity, filteredPressure;
    globalDataFilterManager->filterECSData(temp, humidity, pressure,
                                          filteredTemp, filteredHumidity, filteredPressure);
    // 使用滤波后的数据
}
```

### 3. 内存管理建议

```cpp
// 避免频繁的动态内存分配
static char buffer[256];  // 使用静态缓冲区

// 及时释放不需要的对象
if (sensor != nullptr) {
    delete sensor;
    sensor = nullptr;
}

// 定期执行维护操作
if (globalDataFilterManager) {
    globalDataFilterManager->maintenance();  // 清理过期数据
}
```

---

**API文档维护说明**: 本文档随系统版本更新，新增API或修改现有API时请及时更新文档。如有疑问请联系开发团队。
