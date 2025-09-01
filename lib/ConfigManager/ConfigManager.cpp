#include <portal_page.h>
#include <ConfigManager.h>
#include <LogManager/LogManager.h>
#include <DisplayManager/DisplayManager.h>

class RTOSSystemManager {
public:
    IDisplayManager* getDisplayManager() const;
};

extern RTOSSystemManager* rtosSystemManager;

// 静态成员变量定义
const char* ConfigManager::DEFAULT_WIFI_SSID = "";
const char* ConfigManager::DEFAULT_WIFI_PASSWORD = "";
const char* ConfigManager::DEFAULT_SERVER_IP = "192.168.1.100";
const char* ConfigManager::DEFAULT_NTP_SERVER = "ntp1.aliyun.com";
const char* ConfigManager::AP_SSID = "ESP32_SoilMonitor";
const char* ConfigManager::AP_PASSWORD = "12345678";

// MQTT默认配置
const char* ConfigManager::DEFAULT_MQTT_BROKER = "192.168.31.100";
const uint16_t ConfigManager::DEFAULT_MQTT_PORT = 1883;
const char* ConfigManager::DEFAULT_MQTT_USERNAME = "";
const char* ConfigManager::DEFAULT_MQTT_PASSWORD = "";

/**
 * 构造函数
 */
ConfigManager::ConfigManager() : config(), isConfigured(false), _lastErrorCode(ErrorCode::SUCCESS), _lastErrorMessage(""), _lastErrorTime(0) {
    // 初始化配置结构体
    memset(&config, 0, sizeof(NetworkConfig));
}

/**
 * 初始化配置管理器
 */
ErrorCode ConfigManager::begin() {
    LOG_TRACE("初始化配置管理器...");
    
    // 初始化Preferences
    prefs.begin("network_cfg", false);
    
    // 尝试加载已保存的配置
    loadConfigFromNVS();
    
    // 检查配置是否有效
    if (validateConfig(config)) {
        isConfigured = true;
        LOG_NOTICE("加载已保存的网络配置成功");
        LOG_VERBOSE("WiFi: %s", config.wifiSSID);
        LOG_VERBOSE("服务器: %s:%d", config.serverIP, config.serverPort);
        return ErrorCode::SUCCESS;
    }
    LOG_WARNING("未找到有效配置或配置无效，需要配网");
    setDefaultConfig();
    isConfigured = false;
    return ErrorCode::CONFIG_LOAD_ERROR;
}

/**
 * 启动配网模式
 */
ErrorCode ConfigManager::startConfigMode() {
    LOG_NOTICE("启动配网模式...");
    
    // 使用RTOS系统管理器显示配网信息
    if (rtosSystemManager && rtosSystemManager->getDisplayManager()) {
        rtosSystemManager->getDisplayManager()->showText("配网模式", "连接WiFi: " + String(AP_SSID));
    }
    
    // 启动AP模式
    WiFiClass::mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    LOG_NOTICE("配网AP已启动: %s", AP_SSID);
    // 使用RTOS系统管理器显示AP启动信息
    if (rtosSystemManager && rtosSystemManager->getDisplayManager()) {
        rtosSystemManager->getDisplayManager()->showText("配网AP已启动", "IP: 192.168.4.1");
    }
    
    // 启动Web服务器
    webServer.begin();
    
    // 设置Web服务器路由
    webServer.on("/", HTTP_GET, [this]
    {
        webServer.send(200, "text/html", PORTAL_HTML);
    });
    
    webServer.on("/save", HTTP_POST, [this]
    {
        handleSaveConfig();
    });
    
    webServer.onNotFound([this]
    {
        webServer.sendHeader("Location", "/", true);
        webServer.send(302, "text/plain", "");
    });
    
    LOG_NOTICE("配网Web服务器启动成功");
    return ErrorCode::SUCCESS;
}

/**
 * 处理配网Web服务器
 */
ErrorCode ConfigManager::handleWebServer() {
    webServer.handleClient();
    return ErrorCode::SUCCESS;
}

/**
 * 处理保存配置请求
 */
ErrorCode ConfigManager::handleSaveConfig() {
    if (webServer.hasArg("plain")) {
        String jsonData = webServer.arg("plain");
        LOG_VERBOSE("收到配置数据: %s", jsonData.c_str());
        
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, jsonData);
        
        if (error) {
            LOG_ERROR("JSON解析失败: %s", error.c_str());
            sendErrorResponse("JSON解析失败");
            return ErrorCode::PARAMETER_ERROR;
        }
        
        // 解析配置数据
        NetworkConfig newConfig = {};
        
        strcpy(newConfig.wifiSSID, doc["ssid"] | "");
        strcpy(newConfig.wifiPassword, doc["password"] | "");
        strcpy(newConfig.serverIP, doc["server_ip"] | "192.168.1.100");
        newConfig.serverPort = doc["server_port"] | 7289;
        strcpy(newConfig.ntpServer, doc["ntp_server"] | "ntp1.aliyun.com");
        newConfig.gmtOffset = 8 * 3600;  // 默认GMT+8
        newConfig.daylightOffset = 0;

        // MQTT配置解析
        strcpy(newConfig.mqttBroker, doc["mqtt_broker"] | DEFAULT_MQTT_BROKER);
        newConfig.mqttPort = doc["mqtt_port"] | DEFAULT_MQTT_PORT;
        strcpy(newConfig.mqttClientId, doc["mqtt_client_id"] | "");
        strcpy(newConfig.mqttUsername, doc["mqtt_username"] | DEFAULT_MQTT_USERNAME);
        strcpy(newConfig.mqttPassword, doc["mqtt_password"] | DEFAULT_MQTT_PASSWORD);
        strcpy(newConfig.deviceId, doc["device_id"] | "esp32-01");
        
        // 验证配置
        if (!validateConfig(newConfig)) {
            sendErrorResponse("配置验证失败");
            return ErrorCode::CONFIG_INVALID;
        }
        
        // 保存配置
        ErrorCode saveResult = saveConfig(newConfig);
        if (saveResult == ErrorCode::SUCCESS) {
            isConfigured = true;
            LOG_NOTICE("配置保存成功");
            sendSuccessResponse();
            
            // 延迟重启 - 使用RTOS系统管理器显示重启信息
            if (rtosSystemManager && rtosSystemManager->getDisplayManager()) {
                rtosSystemManager->getDisplayManager()->showText("配置已保存", "系统重启中...");
            }
            delay(2000);
            ESP.restart();
            return ErrorCode::SUCCESS;
        }
        sendErrorResponse("配置保存失败");
        return saveResult;
    }
    sendErrorResponse("未收到配置数据");
    return ErrorCode::PARAMETER_ERROR;
}

/**
 * 发送成功响应
 */
ErrorCode ConfigManager::sendSuccessResponse() {
    String response = "{\"success\":true,\"message\":\"配置保存成功\"}";
    webServer.send(200, "application/json", response);
    return ErrorCode::SUCCESS;
}

/**
 * 发送错误响应
 */
ErrorCode ConfigManager::sendErrorResponse(const String& message) {
    String response = R"({"success":false,"message":")" + message + "\"}";
    webServer.send(400, "application/json", response);
    return ErrorCode::SUCCESS;
}

/**
 * 保存网络配置
 */
ErrorCode ConfigManager::saveConfig(const NetworkConfig& config) {
    if (!validateConfig(config)) {
        LOG_ERROR("配置验证失败");
        return ErrorCode::CONFIG_INVALID;
    }
    
    // 复制配置
    memcpy(&this->config, &config, sizeof(NetworkConfig));
    
    // 保存到NVS
    return saveConfigToNVS();
}

/**
 * 加载网络配置
 */
NetworkConfig ConfigManager::loadConfig() const
{
    return config;
}

/**
 * 重置配置
 */
ErrorCode ConfigManager::resetConfig() {
    LOG_WARNING("重置网络配置");
    
    // 清除NVS中的配置
    prefs.clear();
    
    // 重置配置结构体
    setDefaultConfig();
    isConfigured = false;
    
    LOG_NOTICE("配置已重置");
    return ErrorCode::SUCCESS;
}

/**
 * 保存配置到NVS
 */
ErrorCode ConfigManager::saveConfigToNVS() {
    prefs.putString("wifi_ssid", config.wifiSSID);
    prefs.putString("wifi_pass", config.wifiPassword);
    prefs.putString("server_ip", config.serverIP);
    prefs.putUShort("server_port", config.serverPort);
    prefs.putString("ntp_server", config.ntpServer);
    prefs.putLong("gmt_offset", config.gmtOffset);
    prefs.putInt("daylight_offset", config.daylightOffset);

    // 保存MQTT配置
    prefs.putString("mqtt_broker", config.mqttBroker);
    prefs.putUShort("mqtt_port", config.mqttPort);
    prefs.putString("mqtt_client_id", config.mqttClientId);
    prefs.putString("mqtt_username", config.mqttUsername);
    prefs.putString("mqtt_password", config.mqttPassword);
    prefs.putString("device_id", config.deviceId);

    prefs.putBool("configured", true);
    
    LOG_VERBOSE("配置已保存到NVS");
    return ErrorCode::SUCCESS;
}

/**
 * 从NVS加载配置
 */
ErrorCode ConfigManager::loadConfigFromNVS() {
    // 检查是否已配置
    if (!prefs.getBool("configured", false)) {
        LOG_VERBOSE("NVS中未找到配置");
        return ErrorCode::CONFIG_LOAD_ERROR;
    }
    
    // 加载配置
    strcpy(config.wifiSSID, prefs.getString("wifi_ssid", "").c_str());
    strcpy(config.wifiPassword, prefs.getString("wifi_pass", "").c_str());
    strcpy(config.serverIP, prefs.getString("server_ip", DEFAULT_SERVER_IP).c_str());
    config.serverPort = prefs.getUShort("server_port", DEFAULT_SERVER_PORT);
    strcpy(config.ntpServer, prefs.getString("ntp_server", DEFAULT_NTP_SERVER).c_str());
    config.gmtOffset = prefs.getLong("gmt_offset", DEFAULT_GMT_OFFSET);
    config.daylightOffset = prefs.getInt("daylight_offset", DEFAULT_DAYLIGHT_OFFSET);

    // 加载MQTT配置
    strcpy(config.mqttBroker, prefs.getString("mqtt_broker", DEFAULT_MQTT_BROKER).c_str());
    config.mqttPort = prefs.getUShort("mqtt_port", DEFAULT_MQTT_PORT);
    strcpy(config.mqttClientId, prefs.getString("mqtt_client_id", "").c_str());
    strcpy(config.mqttUsername, prefs.getString("mqtt_username", DEFAULT_MQTT_USERNAME).c_str());
    strcpy(config.mqttPassword, prefs.getString("mqtt_password", DEFAULT_MQTT_PASSWORD).c_str());

    // 加载设备ID，如果不存在则生成标准格式的设备ID
    String savedDeviceId = prefs.getString("device_id", "");
    if (savedDeviceId.length() == 0) {
        // 生成基于MAC地址的标准设备ID
        String standardDeviceId = generateStandardDeviceId();
        strcpy(config.deviceId, standardDeviceId.c_str());
        // 保存生成的设备ID到NVS
        prefs.putString("device_id", standardDeviceId);
        LOG_NOTICE("生成新的标准设备ID: %s", standardDeviceId.c_str());
    } else {
        // 标准化现有的设备ID
        String normalizedDeviceId = normalizeDeviceId(savedDeviceId);
        strcpy(config.deviceId, normalizedDeviceId.c_str());
        // 如果标准化后的ID与原ID不同，更新保存
        if (normalizedDeviceId != savedDeviceId) {
            prefs.putString("device_id", normalizedDeviceId);
            LOG_NOTICE("设备ID已标准化: %s -> %s", savedDeviceId.c_str(), normalizedDeviceId.c_str());
        }
    }
    
    LOG_VERBOSE("从NVS加载配置完成");
    return ErrorCode::SUCCESS;
}

/**
 * 设置默认配置
 */
void ConfigManager::setDefaultConfig() {
    strcpy(config.wifiSSID, DEFAULT_WIFI_SSID);
    strcpy(config.wifiPassword, DEFAULT_WIFI_PASSWORD);
    strcpy(config.serverIP, DEFAULT_SERVER_IP);
    config.serverPort = DEFAULT_SERVER_PORT;
    strcpy(config.ntpServer, DEFAULT_NTP_SERVER);
    config.gmtOffset = DEFAULT_GMT_OFFSET;
    config.daylightOffset = DEFAULT_DAYLIGHT_OFFSET;
}

/**
 * 验证配置有效性
 */
bool ConfigManager::validateConfig(const NetworkConfig& cfg) {
    // 检查WiFi配置
    if (strlen(cfg.wifiSSID) == 0) {
        LOG_VERBOSE("WiFi SSID为空，配置无效");
        return false;
    }
    
    // 检查服务器配置
    if (strlen(cfg.serverIP) == 0) {
        LOG_VERBOSE("服务器IP为空，配置无效");
        return false;
    }
    
    // 检查端口配置
    if (cfg.serverPort == 0) {
        LOG_VERBOSE("服务器端口为0，配置无效");
        return false;
    }
    
    return true;
}

/**
 * 生成基于MAC地址的标准设备ID
 * 格式: ESP32_XXXXXX (使用MAC地址后6位)
 */
String ConfigManager::generateStandardDeviceId() {
    String macAddr = WiFi.macAddress();
    // 移除冒号并转换为大写
    macAddr.replace(":", "");
    macAddr.toUpperCase();

    // 使用MAC地址后6位生成设备ID
    String deviceId = "ESP32_" + macAddr.substring(6); // 取后6位

    LOG_TRACE("基于MAC地址生成设备ID: %s -> %s", WiFi.macAddress().c_str(), deviceId.c_str());
    return deviceId;
}

/**
 * 标准化设备ID格式
 * 支持多种输入格式，统一转换为ESP32_XXX格式
 */
String ConfigManager::normalizeDeviceId(const String& deviceId) {
    String input = deviceId;
    input.trim(); // 去除首尾空格

    // 如果已经是标准格式，直接返回
    if (isStandardDeviceId(input)) {
        return input;
    }

    LOG_TRACE("标准化设备ID: %s", input.c_str());

    // 格式1: esp32-01, esp32-02 -> ESP32_001, ESP32_002
    if (input.startsWith("esp32-") && input.length() >= 7) {
        String suffix = input.substring(6);
        int num = suffix.toInt();
        if (num > 0 && num <= 999) {
            String result = "ESP32_" + String(num, DEC);
            // 补零到3位
            while (result.length() < 9) {
                result = result.substring(0, 6) + "0" + result.substring(6);
            }
            LOG_TRACE("格式转换: %s -> %s", input.c_str(), result.c_str());
            return result;
        }
    }

    // 格式2: ESP32-01, ESP32-02 -> ESP32_001, ESP32_002
    if (input.startsWith("ESP32-") && input.length() >= 7) {
        String suffix = input.substring(6);
        int num = suffix.toInt();
        if (num > 0 && num <= 999) {
            String result = "ESP32_" + String(num, DEC);
            // 补零到3位
            while (result.length() < 9) {
                result = result.substring(0, 6) + "0" + result.substring(6);
            }
            LOG_TRACE("格式转换: %s -> %s", input.c_str(), result.c_str());
            return result;
        }
    }

    // 格式3: esp32_01, esp32_02 -> ESP32_001, ESP32_002
    if (input.startsWith("esp32_") && input.length() >= 7) {
        String suffix = input.substring(6);
        int num = suffix.toInt();
        if (num > 0 && num <= 999) {
            String result = "ESP32_" + String(num, DEC);
            // 补零到3位
            while (result.length() < 9) {
                result = result.substring(0, 6) + "0" + result.substring(6);
            }
            LOG_TRACE("格式转换: %s -> %s", input.c_str(), result.c_str());
            return result;
        }
    }

    // 格式4: 纯数字 -> ESP32_XXX
    int num = input.toInt();
    if (num > 0 && num <= 999 && String(num) == input) {
        String result = "ESP32_" + String(num, DEC);
        // 补零到3位
        while (result.length() < 9) {
            result = result.substring(0, 6) + "0" + result.substring(6);
        }
        LOG_TRACE("格式转换: %s -> %s", input.c_str(), result.c_str());
        return result;
    }

    // 格式5: MAC地址格式 AA:BB:CC:DD:EE:FF -> ESP32_DDEEFF
    if (input.length() == 17 && input.indexOf(':') > 0) {
        String macAddr = input;
        macAddr.replace(":", "");
        macAddr.toUpperCase();
        if (macAddr.length() == 12) {
            String result = "ESP32_" + macAddr.substring(6); // 使用后6位
            LOG_TRACE("MAC地址格式转换: %s -> %s", input.c_str(), result.c_str());
            return result;
        }
    }

    // 如果无法识别格式，生成基于当前MAC地址的设备ID
    LOG_WARNING("无法识别设备ID格式: %s，使用MAC地址生成", input.c_str());
    return generateStandardDeviceId();
}

/**
 * 检查是否为标准设备ID格式 (ESP32_XXX)
 */
bool ConfigManager::isStandardDeviceId(const String& deviceId) {
    // 标准格式：ESP32_001, ESP32_002等，至少9个字符
    if (!deviceId.startsWith("ESP32_") || deviceId.length() < 9) {
        return false;
    }

    String suffix = deviceId.substring(6);

    // 检查后缀是否为有效格式（数字或字母数字组合）
    if (suffix.length() >= 3) {
        // 纯数字格式（如001, 123）
        bool isNumeric = true;
        for (int i = 0; i < suffix.length(); i++) {
            if (!isdigit(suffix.charAt(i))) {
                isNumeric = false;
                break;
            }
        }
        if (isNumeric) {
            return true;
        }

        // 字母数字组合格式（如ABC, 001_DEV）
        bool isAlphaNumeric = true;
        for (int i = 0; i < suffix.length(); i++) {
            char c = suffix.charAt(i);
            if (!isalnum(c) && c != '_') {
                isAlphaNumeric = false;
                break;
            }
        }
        if (isAlphaNumeric) {
            return true;
        }
    }

    return false;
}

/**
 * 报告错误
 */
ErrorCode ConfigManager::reportError(ErrorCode code, const String& message) {
    _lastErrorCode = code;
    _lastErrorMessage = message;
    _lastErrorTime = millis();
    LOG_ERROR("ConfigManager: %s (错误码: %d)", message.c_str(), static_cast<int>(code));
    return code;
}

/**
 * 清除最后一次错误信息
 */
void ConfigManager::clearLastError() {
    _lastErrorCode = ErrorCode::SUCCESS;
    _lastErrorMessage = "";
    _lastErrorTime = 0;
} 