#ifndef LIB_H
#define LIB_H

// ------------------【前向声明】------------------
struct SoilSensorData;

// 湿度控制输出引脚（低湿度输出高电平，高湿度输出低电平）
constexpr int controlOutputPin = 27;

// ------------------【错误码定义】------------------
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

// ------------------【系统状态定义】------------------
enum class SystemState {
    NORMAL,         // 正常状态
    WARNING,        // 警告状态
    ERROR,          // 错误状态
    CRITICAL        // 严重错误状态
};

// ------------------【事件定义】------------------
enum class Event {
    None,                    // 无事件
    ThresholdChanged,        // 阈值变更
    FloodingAlert,          // 洪水警报
    NetworkError,           // 网络错误
    SensorError,            // 传感器错误
    WaterLevelAlert,        // 水位警报
    SoilParameterAlert,     // 土壤参数警报
    EnvironmentWarning,     // 环境警告
    SystemStartup,          // 系统启动
    SystemShutdown,         // 系统关闭
    SystemError,            // 系统错误
    ConfigurationChanged,   // 配置变更
    CalibrationRequired,    // 需要校准
    MaintenanceRequired     // 需要维护
};

// ------------------【错误信息结构】------------------
struct ErrorInfo {
    ErrorCode code;              // 错误码
    const char* module;          // 错误发生的模块
    const char* message;         // 错误信息
    unsigned long timestamp;     // 错误时间戳
    SystemState severity;        // 错误严重程度
};

// ------------------【辅助函数】------------------
/**
 * @brief 检查ErrorCode是否表示成功
 * @param code 错误码
 * @return bool 是否成功
 */
inline bool isSuccess(ErrorCode code) {
    return code == ErrorCode::SUCCESS;
}

/**
 * @brief 检查ErrorCode是否表示失败
 * @param code 错误码
 * @return bool 是否失败
 */
inline bool isError(ErrorCode code) {
    return code != ErrorCode::SUCCESS;
}

#endif // LIB_H