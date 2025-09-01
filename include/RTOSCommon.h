#ifndef RTOS_COMMON_H
#define RTOS_COMMON_H

#include <Arduino.h>
#include <lib.h>
#include <ISCS/ISCS.h>  // 包含SoilSensorData定义

// ==================== 消息类型定义 ====================

/**
 * @brief 传感器数据消息类型
 */
typedef enum {
    SENSOR_MSG_SOIL_MOISTURE,    // 土壤湿度数据
    SENSOR_MSG_ENVIRONMENT,      // 环境数据（温湿度、气压）
    SENSOR_MSG_LIGHT,           // 光照数据
    SENSOR_MSG_WATER_LEVEL,     // 水位数据
    SENSOR_MSG_CAS_SOIL,        // CAS土壤综合数据
    SENSOR_MSG_ERROR            // 传感器错误
} SensorMessageType;

/**
 * @brief 网络消息类型
 */
typedef enum {
    NETWORK_MSG_SENSOR_DATA,    // 传感器数据上传
    NETWORK_MSG_STATUS_UPDATE,  // 状态更新
    NETWORK_MSG_HEARTBEAT,      // 心跳包
    NETWORK_MSG_COMMAND_RESP,   // 指令响应
    NETWORK_MSG_ERROR_REPORT    // 错误报告
} NetworkMessageType;

/**
 * @brief 控制指令类型
 */
typedef enum {
    CONTROL_CMD_AUTO_IRRIGATION,    // 自动灌溉控制
    CONTROL_CMD_FORCE_ON,          // 强制开启
    CONTROL_CMD_FORCE_OFF,         // 强制关闭
    CONTROL_CMD_EXIT_FORCE,        // 退出强制模式
    CONTROL_CMD_SET_THRESHOLD,     // 设置阈值
    CONTROL_CMD_SYSTEM_STATE       // 系统状态变更
} ControlCommandType;

/**
 * @brief 显示消息类型
 */
typedef enum {
    DISPLAY_MSG_SENSOR_DATA,    // 显示传感器数据
    DISPLAY_MSG_SYSTEM_STATUS,  // 显示系统状态
    DISPLAY_MSG_ERROR_INFO,     // 显示错误信息
    DISPLAY_MSG_NETWORK_INFO,   // 显示网络信息
    DISPLAY_MSG_CONTROL_INFO    // 显示控制信息
} DisplayMessageType;

// ==================== 数据结构定义 ====================

// 具名结构体定义（用于类型转换）
struct SoilMoistureData {
    int moisture;           // 土壤湿度值
    int readCount;          // 读取次数
};

struct EnvironmentData {
    float temperature;      // 温度
    float humidity;         // 湿度
    float pressure;         // 气压
};

struct LightData {
    float intensity;        // 光照强度
    bool isDaytime;         // 是否白天
};

struct WaterLevelData {
    float level;            // 水位高度
    bool isLowLevel;        // 是否低水位
};

struct SensorErrorData {
    ErrorCode errorCode;    // 错误码
    char errorMsg[64];      // 错误信息
};

struct ThresholdData {
    int lowThreshold;       // 低阈值
    int highThreshold;      // 高阈值
};

struct SystemStateData {
    SystemState state;      // 系统状态
    ErrorCode errorCode;    // 错误码
    char errorMsg[64];      // 错误信息
};

/**
 * @brief 传感器数据消息结构
 */
struct SensorDataMessage {
    SensorMessageType type;         // 消息类型
    unsigned long timestamp;        // 时间戳
    union {
        SoilMoistureData soilData;  // 土壤湿度数据
        EnvironmentData envData;    // 环境数据
        LightData lightData;        // 光照数据
        WaterLevelData waterData;   // 水位数据
        SoilSensorData casData;     // CAS传感器数据
        SensorErrorData errorData;  // 错误数据
    } data;

    // 构造函数
    SensorDataMessage() : type(SENSOR_MSG_SOIL_MOISTURE), timestamp(0), data{} {
        // union成员通过{}初始化为零
    }
};

/**
 * @brief 网络发送消息结构
 */
struct NetworkSendMessage {
    NetworkMessageType type;        // 消息类型
    unsigned long timestamp;        // 时间戳
    char payload[1024];             // 消息载荷（增加到1024字节以支持复杂传感器数据）
    uint8_t priority;               // 优先级（0-255，数值越大优先级越高）
    uint8_t retryCount;             // 重试次数

    // 构造函数
    NetworkSendMessage() : type(NETWORK_MSG_SENSOR_DATA), timestamp(0), priority(0), retryCount(0) {
        memset(payload, 0, sizeof(payload));
    }
};

/**
 * @brief 控制指令消息结构
 */
struct ControlCommandMessage {
    ControlCommandType type;        // 指令类型
    unsigned long timestamp;        // 时间戳
    union {
        struct {
            int moisture;           // 当前湿度值
        } autoIrrigation;

        struct {
            bool state;             // 强制状态
        } forceControl;

        ThresholdData threshold;    // 阈值数据
        SystemStateData systemState; // 系统状态数据
    } data;

    // 构造函数
    ControlCommandMessage() : type(CONTROL_CMD_AUTO_IRRIGATION), timestamp(0), data{} {
        // union成员通过{}初始化为零
    }
};

/**
 * @brief 显示消息结构
 */
struct DisplayMessage {
    DisplayMessageType type;        // 消息类型
    unsigned long timestamp;        // 时间戳
    uint8_t priority;               // 显示优先级（0-255）
    char line1[32];                 // 显示行1
    char line2[32];                 // 显示行2
    char line3[32];                 // 显示行3
    char line4[32];                 // 显示行4

    // 构造函数
    DisplayMessage() : type(DISPLAY_MSG_SENSOR_DATA), timestamp(0), priority(0) {
        memset(line1, 0, sizeof(line1));
        memset(line2, 0, sizeof(line2));
        memset(line3, 0, sizeof(line3));
        memset(line4, 0, sizeof(line4));
    }
};

/**
 * @brief 系统事件消息结构
 */
struct SystemEventMessage {
    Event eventType;                // 事件类型
    unsigned long timestamp;        // 时间戳
    int param1;                     // 参数1
    int param2;                     // 参数2
    char message[64];               // 事件消息

    // 构造函数
    SystemEventMessage() : eventType(Event::None), timestamp(0), param1(0), param2(0) {
        memset(message, 0, sizeof(message));
    }
};

// ==================== 任务状态结构 ====================

/**
 * @brief 任务健康状态
 */
typedef struct {
    unsigned long lastHeartbeat;    // 最后心跳时间
    uint32_t cycleCount;            // 循环计数
    uint32_t errorCount;            // 错误计数
    bool isHealthy;                 // 是否健康
    char lastError[64];             // 最后错误信息
} TaskHealthStatus;

/**
 * @brief 系统资源使用情况
 */
typedef struct {
    uint32_t freeHeapSize;          // 空闲堆内存
    uint32_t minFreeHeapSize;       // 最小空闲堆内存
    uint8_t cpuUsage;               // CPU使用率
    uint16_t taskCount;             // 任务数量
    uint32_t uptime;                // 系统运行时间
} SystemResourceInfo;

// ==================== 辅助宏定义 ====================

// 消息队列发送超时时间（毫秒）
#define QUEUE_SEND_TIMEOUT_MS       100
#define QUEUE_RECEIVE_TIMEOUT_MS    1000

// 信号量获取超时时间（毫秒）
#define MUTEX_TIMEOUT_MS            5000

// 事件等待超时时间（毫秒）
#define EVENT_WAIT_TIMEOUT_MS       10000

// 任务心跳超时时间（毫秒）
#define TASK_HEARTBEAT_TIMEOUT_MS   30000

// ==================== 辅助函数声明 ====================

/**
 * @brief 创建传感器数据消息
 * @param type 消息类型
 * @param data 数据指针
 * @return SensorDataMessage 传感器数据消息
 */
SensorDataMessage createSensorMessage(SensorMessageType type, const void* data);

/**
 * @brief 创建网络发送消息
 * @param type 消息类型
 * @param payload 载荷字符串
 * @param priority 优先级
 * @return NetworkSendMessage 网络发送消息
 */
NetworkSendMessage createNetworkMessage(NetworkMessageType type, const char* payload, uint8_t priority);

/**
 * @brief 创建控制指令消息
 * @param type 指令类型
 * @param data 数据指针
 * @return ControlCommandMessage 控制指令消息
 */
ControlCommandMessage createControlMessage(ControlCommandType type, const void* data);

/**
 * @brief 创建显示消息
 * @param type 消息类型
 * @param priority 优先级
 * @param line1 显示行1
 * @param line2 显示行2
 * @param line3 显示行3
 * @param line4 显示行4
 * @return DisplayMessage 显示消息
 */
DisplayMessage createDisplayMessage(DisplayMessageType type, uint8_t priority,
                                    const char* line1, const char* line2 = nullptr,
                                    const char* line3 = nullptr, const char* line4 = nullptr);

/**
 * @brief 创建系统事件消息
 * @param eventType 事件类型
 * @param param1 参数1
 * @param param2 参数2
 * @param message 事件消息
 * @return SystemEventMessage 系统事件消息
 */
SystemEventMessage createSystemEventMessage(Event eventType, int param1, int param2, const char* message);

/**
 * @brief 安全发送消息到队列
 * @param queue 队列句柄
 * @param message 消息指针
 * @param timeout 超时时间
 * @return bool 发送是否成功
 */
bool safeSendToQueue(QueueHandle_t queue, void* message, TickType_t timeout);

/**
 * @brief 安全从队列接收消息
 * @param queue 队列句柄
 * @param message 消息指针
 * @param timeout 超时时间
 * @return bool 接收是否成功
 */
bool safeReceiveFromQueue(QueueHandle_t queue, void* message, TickType_t timeout);

/**
 * @brief 验证传感器消息完整性
 * @param message 传感器消息
 * @return bool 消息是否有效
 */
bool validateSensorMessage(const SensorDataMessage& message);

/**
 * @brief 获取队列使用情况
 * @return String 队列使用情况JSON字符串
 */
String getQueueUsageInfo();

#endif // RTOS_COMMON_H
