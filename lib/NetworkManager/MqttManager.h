#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <lib.h>

// 前置声明
class ConfigManager;
class CommandProcessor;
class RTOSSystemManager;

/**
 * MQTT管理器类 - 专门负责MQTT通信管理
 * 
 * 职责范围：
 * - MQTT连接管理和自动重连
 * - 消息发布和订阅
 * - MQTT回调处理
 * - 主题管理
 * - 消息转发到CommandProcessor
 * 
 * 从NetworkManager中分离出来，实现单一职责原则
 */
class MqttManager {
public:
    /**
     * 构造函数
     * @param client WiFi客户端引用
     * @param configManager 配置管理器指针
     */
    explicit MqttManager(WiFiClient& client, ConfigManager* configManager);
    
    /**
     * 析构函数
     */
    ~MqttManager();

    /**
     * 初始化MQTT管理器
     * @return 错误码
     */
    ErrorCode begin();

    /**
     * MQTT循环处理
     * @return 错误码
     */
    ErrorCode loop();

    // ===== 连接管理 =====
    
    /**
     * 连接到MQTT Broker
     * @return 错误码
     */
    ErrorCode connect();
    
    /**
     * 断开MQTT连接
     */
    void disconnect();
    
    /**
     * 重连MQTT
     * @return 错误码
     */
    ErrorCode reconnect();
    
    /**
     * 检查MQTT连接状态
     * @return 是否已连接
     */
    bool isConnected() const;

    // ===== 消息发布 =====
    
    /**
     * 发布传感器数据
     * @param payload JSON格式的传感器数据
     * @return 错误码
     */
    ErrorCode publishSensorData(const String& payload);
    
    /**
     * 发布系统状态
     * @param payload JSON格式的系统状态
     * @return 错误码
     */
    ErrorCode publishSystemStatus(const String& payload);
    
    /**
     * 发布告警信息
     * @param payload JSON格式的告警信息
     * @return 错误码
     */
    ErrorCode publishAlert(const String& payload);
    
    /**
     * 发布命令响应
     * @param messageId 消息ID
     * @param command 命令名称
     * @param status 执行状态
     * @param result 执行结果
     * @return 错误码
     */
    ErrorCode publishCommandResponse(const String& messageId, const String& command, 
                                   const String& status, const String& result);

    // ===== 消息订阅 =====
    
    /**
     * 订阅所有命令主题
     * @return 错误码
     */
    ErrorCode subscribeToCommands();

    // ===== 消息转发设置 =====
    
    /**
     * 设置CommandProcessor用于消息转发
     * @param processor CommandProcessor指针
     */
    void setCommandProcessor(CommandProcessor* processor);

    /**
     * 设置RTOSSystemManager用于传感器数据读取
     * @param systemManager RTOSSystemManager指针
     */
    void setSystemManager(RTOSSystemManager* systemManager);

    // ===== 消息创建工具 =====
    
    /**
     * 创建传感器数据消息
     * @return JSON格式的传感器数据
     */
    String createSensorDataMessage();
    
    /**
     * 创建系统状态消息
     * @return JSON格式的系统状态
     */
    String createSystemStatusMessage();
    
    /**
     * 创建告警消息
     * @param level 告警级别
     * @param type 告警类型
     * @param message 告警消息
     * @param sensor 传感器名称
     * @return JSON格式的告警消息
     */
    String createAlertMessage(const String& level, const String& type, 
                            const String& message, const String& sensor);

    // ===== 错误处理 =====
    
    /**
     * 获取最后一次错误码
     * @return 错误码
     */
    ErrorCode getLastErrorCode() const;
    
    /**
     * 获取最后一次错误信息
     * @return 错误信息
     */
    String getLastErrorMessage() const;
    
    /**
     * 清除最后一次错误信息
     */
    void clearLastError();

    // ===== 单个传感器数据格式化方法（用于实时数据发送）=====

    /**
     * 创建单个土壤湿度传感器数据消息
     * @param moisture 湿度值
     * @param readCount 读取次数
     * @return JSON格式的传感器数据字符串
     */
    String createSingleSoilMoistureMessage(int moisture, int readCount);

    /**
     * 创建单个环境传感器数据消息
     * @param temperature 温度值
     * @param humidity 湿度值
     * @param pressure 气压值
     * @return JSON格式的传感器数据字符串
     */
    String createSingleEnvironmentMessage(float temperature, float humidity, float pressure);

    /**
     * 创建单个光照传感器数据消息
     * @param intensity 光照强度
     * @param isDaytime 是否白天
     * @return JSON格式的传感器数据字符串
     */
    String createSingleLightMessage(float intensity, bool isDaytime);

    /**
     * 创建单个水位传感器数据消息
     * @param level 水位值
     * @param isLowLevel 是否低水位
     * @return JSON格式的传感器数据字符串
     */
    String createSingleWaterLevelMessage(float level, bool isLowLevel);

    /**
     * 创建单个CAS传感器数据消息
     * @param temperature 土壤温度
     * @param moisture 土壤湿度
     * @param ph pH值
     * @param ec 电导率
     * @param nitrogen 氮含量
     * @param phosphorus 磷含量
     * @param potassium 钾含量
     * @return JSON格式的传感器数据字符串
     */
    String createSingleCASMessage(float temperature, float moisture, float ph,
                                 uint16_t ec, uint16_t nitrogen, uint16_t phosphorus, uint16_t potassium);

    /**
     * 创建传感器错误消息
     * @param errorCode 错误码
     * @param errorMessage 错误信息
     * @return JSON格式的错误消息字符串
     */
    String createSensorErrorMessage(ErrorCode errorCode, const String& errorMessage);

private:
    // ===== 成员变量 =====
    
    ConfigManager* _configManager;          // 配置管理器
    WiFiClient& _client;                    // WiFi客户端引用
    PubSubClient _mqttClient;               // MQTT客户端
    CommandProcessor* _commandProcessor;    // 命令处理器（用于消息转发）
    RTOSSystemManager* _systemManager;      // 系统管理器（用于传感器数据读取）
    
    // 连接状态
    bool _isConnected;                      // MQTT连接状态
    unsigned long _lastReconnectAttempt;    // 上次重连尝试时间
    
    // MQTT主题 - 符合agriculture/{device_id}/{message_type}/{sub_type}规范
    String _sensorDataTopic;                // agriculture/ESP32_001/data/sensors
    String _systemStatusTopic;              // agriculture/ESP32_001/status/system
    String _alertsTopic;                    // agriculture/ESP32_001/data/alerts
    String _controlCommandTopic;            // agriculture/ESP32_001/command/control
    String _systemCommandTopic;             // agriculture/ESP32_001/command/system
    String _queryCommandTopic;              // agriculture/ESP32_001/command/query
    String _commandResponseTopic;           // agriculture/ESP32_001/response/command
    
    // 错误信息
    ErrorCode _lastErrorCode;               // 最后一次错误码
    String _lastErrorMessage;               // 最后一次错误信息
    unsigned long _lastErrorTime;           // 最后一次错误时间

    // ===== 私有方法 =====
    
    /**
     * 设置MQTT主题
     */
    void setupTopics();
    
    /**
     * MQTT消息回调函数（静态）
     * @param topic 主题
     * @param payload 消息内容
     * @param length 消息长度
     */
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
    
    /**
     * 处理控制命令
     * @param topic 主题
     * @param payload 消息内容
     */
    void handleControlCommand(const String& topic, const String& payload);
    
    /**
     * 处理系统命令
     * @param topic 主题
     * @param payload 消息内容
     */
    void handleSystemCommand(const String& topic, const String& payload);
    
    /**
     * 处理查询命令
     * @param topic 主题
     * @param payload 消息内容
     */
    void handleQueryCommand(const String& topic, const String& payload);
    
    /**
     * 提取消息ID
     * @param payload JSON消息内容
     * @return 消息ID
     */
    String extractMessageId(const String& payload);
    
    /**
     * 转发消息到CommandProcessor
     * @param command 命令类型
     * @param payload 消息内容
     * @return 是否转发成功
     */
    bool forwardToCommandProcessor(const String& command, const String& payload);
    
    /**
     * 报告错误
     * @param code 错误码
     * @param message 错误信息
     * @return 错误码
     */
    ErrorCode reportError(ErrorCode code, const String& message);
    
    /**
     * 创建默认传感器数据消息（当系统管理器不可用时）
     * @return JSON格式的默认传感器数据
     */
    String createDefaultSensorDataMessage() const;

    /**
     * 数据质量评估
     * @param value 数值
     * @param minValue 最小值
     * @param maxValue 最大值
     * @return 质量等级字符串
     */
    static String evaluateDataQuality(float value, float minValue, float maxValue);

    // ===== 传感器数据获取辅助方法 =====

    /**
     * 添加土壤湿度传感器数据到JSON对象
     * @param sensors JSON传感器对象
     */
    void addSoilMoistureData(JsonObject& sensors);

    /**
     * 添加环境传感器数据到JSON对象
     * @param sensors JSON传感器对象
     */
    void addEnvironmentSensorData(JsonObject& sensors);

    /**
     * 添加光照传感器数据到JSON对象
     * @param sensors JSON传感器对象
     */
    void addLightSensorData(JsonObject& sensors);

    /**
     * 添加水深传感器数据到JSON对象
     * @param sensors JSON传感器对象
     */
    void addWaterSensorData(JsonObject& sensors);

    /**
     * 添加CAS土壤综合传感器数据到JSON对象
     * @param sensors JSON传感器对象
     */
    void addCASSensorData(JsonObject& sensors);

    /**
     * 添加CAS传感器错误数据到JSON对象
     * @param sensors JSON传感器对象
     * @param errorMessage 错误信息
     */
    static void addCASErrorData(JsonObject& sensors, const String& errorMessage);
};

// 全局实例指针，用于静态回调函数访问
extern MqttManager* g_mqttManagerInstance;

#endif // MQTT_MANAGER_H
