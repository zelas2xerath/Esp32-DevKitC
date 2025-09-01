#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <lib.h>
#include <WiFi.h>
#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFiManager.h>


// 前置声明
class SystemManager;
class NetworkManager;
class DisplayManager;
class LogManager;

// 配置结构体
struct NetworkConfig {
    char wifiSSID[32];
    char wifiPassword[64];
    char serverIP[16];          // 保留用于兼容性，现在用作MQTT Broker地址
    uint16_t serverPort;        // 保留用于兼容性，现在用作MQTT Broker端口
    char ntpServer[32];
    long gmtOffset;
    int daylightOffset;

    // MQTT相关配置
    char mqttBroker[64];        // MQTT Broker地址
    uint16_t mqttPort;          // MQTT Broker端口
    char mqttClientId[32];      // MQTT客户端ID
    char mqttUsername[32];      // MQTT用户名
    char mqttPassword[64];      // MQTT密码
    char deviceId[16];          // 设备ID，用于构建主题
};

class ConfigManager {
    Preferences prefs;
    WiFiManager wifiManager;
    WebServer webServer;
    NetworkConfig config;
    bool isConfigured;
    
    // 错误信息
    ErrorCode _lastErrorCode;
    String _lastErrorMessage;
    unsigned long _lastErrorTime;
    
    // 默认配置
    static const char* DEFAULT_WIFI_SSID;
    static const char* DEFAULT_WIFI_PASSWORD;
    static const char* DEFAULT_SERVER_IP;
    static constexpr uint16_t DEFAULT_SERVER_PORT = 7289;
    static const char* DEFAULT_NTP_SERVER;
    static constexpr long DEFAULT_GMT_OFFSET = 8 * 3600;
    static constexpr int DEFAULT_DAYLIGHT_OFFSET = 0;

    // MQTT默认配置常量
    static const char* DEFAULT_MQTT_BROKER;
    static const uint16_t DEFAULT_MQTT_PORT;
    static const char* DEFAULT_MQTT_USERNAME;
    static const char* DEFAULT_MQTT_PASSWORD;

    // 配网相关
    static const char* AP_SSID;
    static const char* AP_PASSWORD;
    static constexpr int AP_TIMEOUT = 180; // 3分钟超时
    
public:
    ConfigManager();
    
    /**
     * 初始化配置管理器
     * @return 错误码
     */
    ErrorCode begin();
    
    /**
     * 启动配网模式
     * @return 错误码
     */
    ErrorCode startConfigMode();
    
    /**
     * 处理配网Web服务器
     * @return 错误码
     */
    ErrorCode handleWebServer();
    
    /**
     * 处理保存配置请求
     * @return 错误码
     */
    ErrorCode handleSaveConfig();
    
    /**
     * 发送成功响应
     * @return 错误码
     */
    ErrorCode sendSuccessResponse();
    
    /**
     * 发送错误响应
     * @param message 错误信息
     * @return 错误码
     */
    ErrorCode sendErrorResponse(const String& message);
    
    /**
     * 保存网络配置
     * @param config 配置结构体
     * @return 错误码
     */
    ErrorCode saveConfig(const NetworkConfig& config);
    
    /**
     * 加载网络配置
     * @return 配置结构体
     */
    NetworkConfig loadConfig() const;
    
    /**
     * 检查是否已配置
     * @return 是否已配置
     */
    bool isNetworkConfigured() const { return isConfigured; }
    
    /**
     * 重置配置
     * @return 错误码
     */
    ErrorCode resetConfig();
    
    /**
     * 获取WiFi管理器引用
     * @return WiFiManager引用
     */
    WiFiManager& getWiFiManager() { return wifiManager; }
    
    /**
     * 获取当前配置
     * @return 配置结构体引用
     */
    const NetworkConfig& getConfig() const { return config; }
    
    /**
     * 获取最后一次错误码
     * @return 错误码
     */
    ErrorCode getLastErrorCode() const { return _lastErrorCode; }
    
    /**
     * 获取最后一次错误信息
     * @return 错误信息
     */
    String getLastErrorMessage() const { return _lastErrorMessage; }
    
    /**
     * 清除最后一次错误信息
     */
    void clearLastError();

    // ------------------【设备ID管理方法】------------------

    /**
     * 生成基于MAC地址的标准设备ID
     * @return 标准格式的设备ID (ESP32_XXXXXX)
     */
    static String generateStandardDeviceId();

    /**
     * 标准化设备ID格式
     * @param deviceId 原始设备ID
     * @return 标准化后的设备ID
     */
    static String normalizeDeviceId(const String& deviceId);

    /**
     * 检查是否为标准设备ID格式
     * @param deviceId 设备ID
     * @return 是否为标准格式
     */
    static bool isStandardDeviceId(const String& deviceId);

private:
    /**
     * 保存配置到NVS
     * @return 错误码
     */
    ErrorCode saveConfigToNVS();
    
    /**
     * 从NVS加载配置
     * @return 错误码
     */
    ErrorCode loadConfigFromNVS();
    
    /**
     * 设置默认配置
     */
    void setDefaultConfig();
    
    /**
     * 验证配置有效性
     * @param cfg 配置结构体
     * @return 配置是否有效
     */
    static bool validateConfig(const NetworkConfig& cfg);
    
    /**
     * 报告错误
     * @param code 错误码
     * @param message 错误信息
     * @return 错误码
     */
    ErrorCode reportError(ErrorCode code, const String& message);
};

#endif // CONFIG_MANAGER_H 