#include <MqttManager.h>
#include <LogManager/LogManager.h>
#include <ConfigManager/ConfigManager.h>
#include <CommandProcessor/CommandProcessor.h>
#include <SystemManager/RTOSSystemManager.h>
#include <SensorManager/SensorManager.h>
#include <LightManagement/LightManager.h>  // 添加智能光照管理支持

// 全局指针，用于MQTT回调函数访问MqttManager实例
MqttManager* g_mqttManagerInstance = nullptr;

MqttManager::MqttManager(WiFiClient& client, ConfigManager* configManager)
    : _configManager(configManager),
      _client(client),
      _mqttClient(_client),
      _commandProcessor(nullptr),
      _systemManager(nullptr),
      _isConnected(false),
      _lastReconnectAttempt(0),
      _lastErrorCode(ErrorCode::SUCCESS),
      _lastErrorMessage(""),
      _lastErrorTime(0)
{
    // 设置全局实例指针用于MQTT回调
    g_mqttManagerInstance = this;
    
    // 设置MQTT回调函数
    _mqttClient.setCallback(mqttCallback);
    
    LOG_TRACE("MqttManager构造完成");
}

MqttManager::~MqttManager() {
    disconnect();
    g_mqttManagerInstance = nullptr;
    LOG_TRACE("MqttManager析构完成");
}

/**
 * 初始化MQTT管理器
 */
ErrorCode MqttManager::begin() {
    LOG_TRACE("初始化MQTT管理器...");
    
    if (!_configManager || !_configManager->isNetworkConfigured()) {
        return reportError(ErrorCode::NOT_INITIALIZED, "配置管理器未初始化或网络未配置");
    }
    
    // 设置MQTT主题
    setupTopics();
    
    // 连接到MQTT Broker
    ErrorCode connectResult = connect();
    if (connectResult != ErrorCode::SUCCESS) {
        return connectResult;
    }
    
    // 订阅命令主题
    ErrorCode subscribeResult = subscribeToCommands();
    if (subscribeResult != ErrorCode::SUCCESS) {
        return subscribeResult;
    }
    
    LOG_NOTICE("MQTT管理器初始化完成！");
    return ErrorCode::SUCCESS;
}

/**
 * MQTT循环处理
 */
ErrorCode MqttManager::loop() {
    // 检查连接状态并处理消息
    if (_isConnected && _mqttClient.connected()) {
        _mqttClient.loop();
        return ErrorCode::SUCCESS;
    }
    // 连接断开，尝试重连
    _isConnected = false;
    return reconnect();
}

/**
 * 连接到MQTT Broker
 */
ErrorCode MqttManager::connect() {
    if (!_configManager) {
        return reportError(ErrorCode::NOT_INITIALIZED, "配置管理器未初始化");
    }

    const NetworkConfig& config = _configManager->getConfig();
    
    // 设置MQTT服务器
    _mqttClient.setServer(config.mqttBroker, config.mqttPort);

    LOG_TRACE("正在连接MQTT Broker: %s:%d", config.mqttBroker, config.mqttPort);

    // 生成客户端ID
    String clientId = "ESP32_" + String(config.deviceId);
    
    // 尝试连接
    bool connected = false;
    if (strlen(config.mqttUsername) > 0 && strlen(config.mqttPassword) > 0) {
        // 使用用户名和密码连接
        connected = _mqttClient.connect(clientId.c_str(),
                                      config.mqttUsername,
                                      config.mqttPassword);
    } else {
        // 匿名连接
        connected = _mqttClient.connect(clientId.c_str());
    }
    
    if (connected) {
        _isConnected = true;
        LOG_NOTICE("MQTT连接成功，客户端ID: %s", clientId.c_str());
        return ErrorCode::SUCCESS;
    }

    _isConnected = false;
    return reportError(ErrorCode::SERVER_CONNECTION_ERROR, 
                      "MQTT连接失败，状态码: " + String(_mqttClient.state()));
}

/**
 * 断开MQTT连接
 */
void MqttManager::disconnect() {
    if (_mqttClient.connected()) {
        _mqttClient.disconnect();
        LOG_TRACE("MQTT连接已断开");
    }
    _isConnected = false;
}

/**
 * 重连MQTT
 */
ErrorCode MqttManager::reconnect() {
    if (_isConnected && _mqttClient.connected()) {
        return ErrorCode::SUCCESS; // 已连接
    }

    unsigned long currentTime = millis();
    if (currentTime - _lastReconnectAttempt < 5000) {
        return ErrorCode::SUCCESS; // 避免频繁重连
    }
    _lastReconnectAttempt = currentTime;

    LOG_TRACE("尝试MQTT重连...");
    ErrorCode result = connect();
    
    if (result == ErrorCode::SUCCESS) {
        // 重连成功，重新订阅主题
        subscribeToCommands();
    }
    
    return result;
}

/**
 * 检查MQTT连接状态
 */
bool MqttManager::isConnected() const {
    return _isConnected && const_cast<PubSubClient&>(_mqttClient).connected();
}

/**
 * 发布传感器数据
 */
ErrorCode MqttManager::publishSensorData(const String& payload) {
    // 详细的连接状态检查
    bool internalConnected = _isConnected;
    bool clientConnected = _mqttClient.connected();

    LOG_VERBOSE("MQTT发布检查: 内部状态=%s, 客户端状态=%s, 载荷大小=%d",
                internalConnected ? "已连接" : "未连接",
                clientConnected ? "已连接" : "未连接",
                payload.length());

    if (!isConnected()) {
        return reportError(ErrorCode::SERVER_CONNECTION_ERROR,
                          String("MQTT未连接 - 内部:") + (internalConnected ? "true" : "false") +
                          " 客户端:" + (clientConnected ? "true" : "false"));
    }

    // 检查载荷大小
    if (payload.length() == 0) {
        return reportError(ErrorCode::INVALID_PARAMETER, "载荷为空");
    }

    if (payload.length() > 1024) {
        return reportError(ErrorCode::INVALID_PARAMETER,
                          String("载荷过大: ") + payload.length() + " 字节");
    }

    // 检查主题是否有效
    if (_sensorDataTopic.isEmpty()) {
        return reportError(ErrorCode::NOT_INITIALIZED, "传感器数据主题未设置");
    }

    LOG_VERBOSE("准备发布到主题: %s, 载荷前100字符: %.100s",
                _sensorDataTopic.c_str(), payload.c_str());

    // 尝试发布
    bool publishResult = _mqttClient.publish(_sensorDataTopic.c_str(), payload.c_str());

    if (publishResult) {
        LOG_NOTICE("传感器数据发布成功到主题: %s", _sensorDataTopic.c_str());
        return ErrorCode::SUCCESS;
    }

    // 发布失败，获取详细错误信息
    int mqttState = _mqttClient.state();
    String stateDescription;

    switch (mqttState) {
        case MQTT_CONNECTION_TIMEOUT:
            stateDescription = "连接超时(-4)";
            break;
        case MQTT_CONNECTION_LOST:
            stateDescription = "连接丢失(-3)";
            break;
        case MQTT_CONNECT_FAILED:
            stateDescription = "连接失败(-2)";
            break;
        case MQTT_DISCONNECTED:
            stateDescription = "已断开连接(-1)";
            break;
        case MQTT_CONNECTED:
            stateDescription = "已连接(0) - 但发布失败";
            break;
        case MQTT_CONNECT_BAD_PROTOCOL:
            stateDescription = "协议版本错误(1)";
            break;
        case MQTT_CONNECT_BAD_CLIENT_ID:
            stateDescription = "客户端ID错误(2)";
            break;
        case MQTT_CONNECT_UNAVAILABLE:
            stateDescription = "服务器不可用(3)";
            break;
        case MQTT_CONNECT_BAD_CREDENTIALS:
            stateDescription = "认证失败(4)";
            break;
        case MQTT_CONNECT_UNAUTHORIZED:
            stateDescription = "未授权(5)";
            break;
        default:
            stateDescription = String("未知状态(") + mqttState + ")";
            break;
    }

    return reportError(ErrorCode::SERVER_CONNECTION_ERROR,
                      String("传感器数据发布失败 - ") + stateDescription +
                      " 载荷大小:" + payload.length() + "字节");
}

/**
 * 发布系统状态
 */
ErrorCode MqttManager::publishSystemStatus(const String& payload) {
    if (!isConnected()) {
        return reportError(ErrorCode::SERVER_CONNECTION_ERROR, "MQTT未连接");
    }

    if (_mqttClient.publish(_systemStatusTopic.c_str(), payload.c_str())) {
        LOG_NOTICE("系统状态发布成功到主题: %s", _systemStatusTopic.c_str());
        return ErrorCode::SUCCESS;
    }

    return reportError(ErrorCode::SERVER_CONNECTION_ERROR, "系统状态发布失败");
}

/**
 * 发布告警信息
 */
ErrorCode MqttManager::publishAlert(const String& payload) {
    if (!isConnected()) {
        return reportError(ErrorCode::SERVER_CONNECTION_ERROR, "MQTT未连接");
    }

    if (_mqttClient.publish(_alertsTopic.c_str(), payload.c_str())) {
        LOG_NOTICE("告警信息发布成功到主题: %s", _alertsTopic.c_str());
        return ErrorCode::SUCCESS;
    }

    return reportError(ErrorCode::SERVER_CONNECTION_ERROR, "告警信息发布失败");
}

/**
 * 发布命令响应
 */
ErrorCode MqttManager::publishCommandResponse(const String& messageId, const String& command, 
                                            const String& status, const String& result) {
    if (!isConnected()) {
        return reportError(ErrorCode::SERVER_CONNECTION_ERROR, "MQTT未连接");
    }

    // 构建命令响应JSON消息
    String response = "{";
    response += R"("message_id":")" + messageId + "\",";
    response += R"("command":")" + command + "\",";
    response += R"("status":")" + status + "\",";
    response += R"("timestamp":")" + String(millis()) + "\",";
    response += "\"result\":" + result;
    response += "}";

    if (_mqttClient.publish(_commandResponseTopic.c_str(), response.c_str())) {
        LOG_TRACE("命令响应发布成功: %s", response.c_str());
        return ErrorCode::SUCCESS;
    }

    return reportError(ErrorCode::SERVER_CONNECTION_ERROR, "命令响应发布失败");
}

/**
 * 订阅所有命令主题
 */
ErrorCode MqttManager::subscribeToCommands() {
    if (!isConnected()) {
        return reportError(ErrorCode::SERVER_CONNECTION_ERROR, "MQTT未连接");
    }

    // 订阅控制命令主题
    if (!_mqttClient.subscribe(_controlCommandTopic.c_str())) {
        return reportError(ErrorCode::SERVER_CONNECTION_ERROR, "订阅控制命令主题失败");
    }
    LOG_NOTICE("成功订阅控制命令主题: %s", _controlCommandTopic.c_str());

    // 订阅系统命令主题
    if (!_mqttClient.subscribe(_systemCommandTopic.c_str())) {
        return reportError(ErrorCode::SERVER_CONNECTION_ERROR, "订阅系统命令主题失败");
    }
    LOG_NOTICE("成功订阅系统命令主题: %s", _systemCommandTopic.c_str());

    // 订阅查询命令主题
    if (!_mqttClient.subscribe(_queryCommandTopic.c_str())) {
        return reportError(ErrorCode::SERVER_CONNECTION_ERROR, "订阅查询命令主题失败");
    }
    LOG_NOTICE("成功订阅查询命令主题: %s", _queryCommandTopic.c_str());

    return ErrorCode::SUCCESS;
}

/**
 * 设置CommandProcessor用于消息转发
 */
void MqttManager::setCommandProcessor(CommandProcessor* processor) {
    _commandProcessor = processor;
    LOG_TRACE("CommandProcessor已设置，消息转发功能已启用");
}

/**
 * 设置RTOSSystemManager用于传感器数据读取
 */
void MqttManager::setSystemManager(RTOSSystemManager* systemManager) {
    _systemManager = systemManager;
    LOG_TRACE("RTOSSystemManager已设置，传感器数据读取功能已启用");
}

/**
 * 设置MQTT主题
 */
void MqttManager::setupTopics() {
    if (!_configManager) {
        return;
    }

    const NetworkConfig& config = _configManager->getConfig();
    auto deviceId = String(config.deviceId);

    // 构建符合agriculture/{device_id}/{message_type}/{sub_type}规范的主题
    _sensorDataTopic = "agriculture/" + deviceId + "/data/sensors";
    _systemStatusTopic = "agriculture/" + deviceId + "/status/system";
    _alertsTopic = "agriculture/" + deviceId + "/data/alerts";
    _controlCommandTopic = "agriculture/" + deviceId + "/command/control";
    _systemCommandTopic = "agriculture/" + deviceId + "/command/system";
    _queryCommandTopic = "agriculture/" + deviceId + "/command/query";
    _commandResponseTopic = "agriculture/" + deviceId + "/response/command";

    LOG_TRACE("MQTT主题设置完成:");
    LOG_TRACE("  传感器数据: %s", _sensorDataTopic.c_str());
    LOG_TRACE("  系统状态: %s", _systemStatusTopic.c_str());
    LOG_TRACE("  告警信息: %s", _alertsTopic.c_str());
    LOG_TRACE("  控制命令: %s", _controlCommandTopic.c_str());
    LOG_TRACE("  系统命令: %s", _systemCommandTopic.c_str());
    LOG_TRACE("  查询命令: %s", _queryCommandTopic.c_str());
    LOG_TRACE("  命令响应: %s", _commandResponseTopic.c_str());
}

/**
 * MQTT消息回调函数（静态）
 */
void MqttManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    // 将payload转换为字符串
    String message;
    message.reserve(length + 1);
    for (unsigned int i = 0; i < length; i++) {
        message += static_cast<char>(payload[i]);
    }

    auto topicStr = String(topic);
    LOG_TRACE("收到MQTT消息 - 主题: %s, 内容: %s", topicStr.c_str(), message.c_str());

    // 通过全局实例指针调用实例方法
    if (g_mqttManagerInstance) {
        if (topicStr.endsWith("/command/control")) {
            g_mqttManagerInstance->handleControlCommand(topicStr, message);
        } else if (topicStr.endsWith("/command/system")) {
            g_mqttManagerInstance->handleSystemCommand(topicStr, message);
        } else if (topicStr.endsWith("/command/query")) {
            g_mqttManagerInstance->handleQueryCommand(topicStr, message);
        } else {
            LOG_WARNING("收到未知主题的MQTT消息: %s", topicStr.c_str());
        }
    }
}

/**
 * 处理控制命令
 */
void MqttManager::handleControlCommand(const String& topic, const String& payload) {
    LOG_TRACE("处理控制命令: %s", payload.c_str());

    // 提取消息ID用于响应
    String messageId = extractMessageId(payload);

    // 发送确认响应，表示消息已接收
    publishCommandResponse(messageId, "CONTROL_RECEIVED", "SUCCESS",
                          "{\"message\":\"控制命令已接收，正在处理\"}");

    // 转发到CommandProcessor处理
    if (forwardToCommandProcessor("CONTROL", payload)) {
        LOG_TRACE("控制命令已转发给CommandProcessor，消息ID: %s", messageId.c_str());
    } else {
        LOG_ERROR("控制命令转发失败，消息ID: %s", messageId.c_str());
        publishCommandResponse(messageId, "CONTROL_ERROR", "ERROR",
                              "{\"error\":\"命令处理器不可用\"}");
    }
}

/**
 * 处理系统命令
 */
void MqttManager::handleSystemCommand(const String& topic, const String& payload) {
    LOG_TRACE("处理系统命令: %s", payload.c_str());

    // 提取消息ID用于响应
    String messageId = extractMessageId(payload);

    // 发送确认响应，表示消息已接收
    publishCommandResponse(messageId, "SYSTEM_RECEIVED", "SUCCESS",
                          "{\"message\":\"系统命令已接收，正在处理\"}");

    // 转发到CommandProcessor处理
    if (forwardToCommandProcessor("SYSTEM", payload)) {
        LOG_TRACE("系统命令已转发给CommandProcessor，消息ID: %s", messageId.c_str());
    } else {
        LOG_ERROR("系统命令转发失败，消息ID: %s", messageId.c_str());
        publishCommandResponse(messageId, "SYSTEM_ERROR", "ERROR",
                              "{\"error\":\"命令处理器不可用\"}");
    }
}

/**
 * 处理查询命令
 */
void MqttManager::handleQueryCommand(const String& topic, const String& payload) {
    LOG_TRACE("处理查询命令: %s", payload.c_str());

    // 提取消息ID用于响应
    String messageId = extractMessageId(payload);

    // 发送确认响应，表示消息已接收
    publishCommandResponse(messageId, "QUERY_RECEIVED", "SUCCESS",
                          "{\"message\":\"查询命令已接收，正在处理\"}");

    // 转发到CommandProcessor处理
    if (forwardToCommandProcessor("QUERY", payload)) {
        LOG_TRACE("查询命令已转发给CommandProcessor，消息ID: %s", messageId.c_str());
    } else {
        LOG_ERROR("查询命令转发失败，消息ID: %s", messageId.c_str());
        publishCommandResponse(messageId, "QUERY_ERROR", "ERROR",
                              "{\"error\":\"命令处理器不可用\"}");
    }
}

/**
 * 提取消息ID
 */
String MqttManager::extractMessageId(const String& payload) {
    String messageId = "unknown";
    int idStart = payload.indexOf(R"("message_id":")");
    if (idStart > 0) {
        idStart += 14; // 跳过"message_id":"
        int idEnd = payload.indexOf("\"", idStart);
        if (idEnd > idStart) {
            messageId = payload.substring(idStart, idEnd);
        }
    }
    return messageId;
}

/**
 * 转发消息到CommandProcessor
 */
bool MqttManager::forwardToCommandProcessor(const String& command, const String& payload) {
    if (!_commandProcessor) {
        LOG_WARNING("CommandProcessor未设置，无法转发消息");
        return false;
    }

    // 调用CommandProcessor的processCommand方法处理JSON命令
    ErrorCode result = _commandProcessor->processCommand(payload);

    if (result == ErrorCode::SUCCESS) {
        LOG_TRACE("消息转发成功: %s", command.c_str());
        return true;
    }
    LOG_ERROR("消息转发失败: %s, 错误码: %d", command.c_str(), static_cast<int>(result));
    return false;
}

/**
 * 报告错误
 */
ErrorCode MqttManager::reportError(ErrorCode code, const String& message) {
    _lastErrorCode = code;
    _lastErrorMessage = message;
    _lastErrorTime = millis();

    // 根据错误级别记录日志
    if (code == ErrorCode::SUCCESS) {
        LOG_TRACE("%s", message.c_str());
    } else {
        LOG_ERROR("MQTT错误 [%d]: %s", static_cast<int>(code), message.c_str());
    }

    return code;
}

/**
 * 获取最后一次错误码
 */
ErrorCode MqttManager::getLastErrorCode() const {
    return _lastErrorCode;
}

/**
 * 获取最后一次错误信息
 */
String MqttManager::getLastErrorMessage() const {
    return _lastErrorMessage;
}

/**
 * 清除最后一次错误信息
 */
void MqttManager::clearLastError() {
    _lastErrorCode = ErrorCode::SUCCESS;
    _lastErrorMessage = "";
    _lastErrorTime = 0;
}

/**
 * 创建传感器数据消息 - 优化后通过RTOSSystemManager获取真实传感器数据
 */
String MqttManager::createSensorDataMessage() {
    if (!_configManager) {
        LOG_ERROR("配置管理器未初始化，无法创建传感器数据消息");
        return "{}";
    }

    if (!_systemManager) {
        LOG_WARNING("系统管理器未设置，使用默认传感器数据");
        return createDefaultSensorDataMessage();
    }

    const NetworkConfig& config = _configManager->getConfig();

    // 创建JSON文档（优化缓冲区大小）
    DynamicJsonDocument doc(2048);

    // 设备信息
    doc["device_id"] = String(config.deviceId);
    doc["timestamp"] = millis();

    // 传感器数据容器
    JsonObject sensors = doc.createNestedObject("sensors");

    // 通过RTOSSystemManager的公共接口获取传感器数据
    // 这种方法符合系统架构设计原则，避免了直接访问传感器类

    // 1. 获取土壤湿度传感器数据（SMS）
    addSoilMoistureData(sensors);

    // 2. 获取环境传感器数据（ECS - BME280）
    addEnvironmentSensorData(sensors);

    // 3. 获取光照传感器数据（LIS - BH1750）
    addLightSensorData(sensors);

    // 4. 获取水深传感器数据（WDS - MSP20）
    addWaterSensorData(sensors);

    // 5. 获取CAS土壤综合传感器数据
    addCASSensorData(sensors);

    // 序列化JSON
    String result;
    serializeJson(doc, result);

    LOG_TRACE("传感器数据消息创建成功，大小: %d字节", result.length());
    return result;
}

/**
 * 创建默认传感器数据消息（当系统管理器不可用时）
 */
String MqttManager::createDefaultSensorDataMessage() const
{
    if (!_configManager) {
        return "{}";
    }

    const NetworkConfig& config = _configManager->getConfig();

    // 创建JSON文档
    DynamicJsonDocument doc(1024);

    // 设备信息
    doc["device_id"] = String(config.deviceId);
    doc["timestamp"] = millis();

    // 默认传感器数据
    JsonObject sensors = doc.createNestedObject("sensors");

    // 土壤湿度传感器（默认值）
    JsonObject soilMoisture = sensors.createNestedObject("soil_moisture");
    soilMoisture["value"] = 0.0;
    soilMoisture["unit"] = "%";
    soilMoisture["quality"] = "error";

    // 空气温度（默认值）
    JsonObject airTemperature = sensors.createNestedObject("air_temperature");
    airTemperature["value"] = 0.0;
    airTemperature["unit"] = "°C";
    airTemperature["quality"] = "error";

    // 空气湿度（默认值）
    JsonObject airHumidity = sensors.createNestedObject("air_humidity");
    airHumidity["value"] = 0.0;
    airHumidity["unit"] = "%";
    airHumidity["quality"] = "error";

    // 序列化JSON
    String result;
    serializeJson(doc, result);

    LOG_WARNING("使用默认传感器数据，系统管理器不可用");
    return result;
}

/**
 * 创建系统状态消息
 */
String MqttManager::createSystemStatusMessage() {
    if (!_configManager) {
        return "{}";
    }

    const NetworkConfig& config = _configManager->getConfig();

    // 简化的系统状态消息
    String message = "{";
    message += R"("device_id":")" + String(config.deviceId) + "\",";
    message += "\"timestamp\":" + String(millis()) + ",";
    message += "\"status\":{";
    message += "\"mqtt_connected\":" + String(_isConnected ? "true" : "false") + ",";
    message += "\"uptime\":" + String(millis()) + ",";
    message += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    message += "\"wifi_rssi\":" + String(WiFi.RSSI());
    message += "}}";

    return message;
}

/**
 * 创建告警消息
 */
String MqttManager::createAlertMessage(const String& level, const String& type,
                                     const String& message, const String& sensor) {
    if (!_configManager) {
        return "{}";
    }

    const NetworkConfig& config = _configManager->getConfig();

    String alertMessage = "{";
    alertMessage += R"("device_id":")" + String(config.deviceId) + "\",";
    alertMessage += "\"timestamp\":" + String(millis()) + ",";
    alertMessage += "\"alert\":{";
    alertMessage += R"("level":")" + level + "\",";
    alertMessage += R"("type":")" + type + "\",";
    alertMessage += R"("message":")" + message + "\",";
    alertMessage += R"("sensor":")" + sensor + "\"";
    alertMessage += "}}";

    return alertMessage;
}

/**
 * 数据质量评估
 */
String MqttManager::evaluateDataQuality(float value, float minValue, float maxValue) {
    // 检查数值是否为NaN或无穷大
    if (isnan(value) || isinf(value)) {
        return "error";
    }

    // 检查数值是否在有效范围内
    if (value < minValue || value > maxValue) {
        return "out_of_range";
    }

    // 根据数值在范围内的位置评估质量
    const float range = maxValue - minValue;
    const float normalizedValue = (value - minValue) / range;

    if (normalizedValue >= 0.2 && normalizedValue <= 0.8) {
        return "good";
    }
    if (normalizedValue >= 0.1 && normalizedValue <= 0.9) {
        return "fair";
    }
    return "poor";
}

// ===== 传感器数据获取辅助方法实现 =====

/**
 * @brief 添加土壤湿度传感器数据到JSON对象
 *
 * @param sensors JSON对象引用，用于添加传感器数据
 */
void MqttManager::addSoilMoistureData(JsonObject& sensors) {
    JsonObject soilMoisture = sensors.createNestedObject("soil_moisture");

    // 通过RTOSSystemManager获取SMS传感器实例
    SMS* sms = _systemManager->getSMS();
    if (sms) {
        // 读取土壤湿度数据（使用SMS类优化后的接口）
        const int moistureValue = sms->readSoilMoisture();

        // 检查传感器是否有错误并验证数据有效性
        if (sms->getLastErrorCode() == ErrorCode::SUCCESS && SMS::isDataValid(moistureValue)) {
            soilMoisture["value"] = moistureValue;
            soilMoisture["unit"] = "%";
            soilMoisture["quality"] = evaluateDataQuality(static_cast<float>(moistureValue), 0.0, 100.0);
            soilMoisture["read_count"] = sms->getReadCount();

            LOG_TRACE("SMS传感器数据获取成功: %d%%", moistureValue);
        } else {
            // 处理传感器读取错误或数据无效的情况
            soilMoisture["value"] = 0;
            soilMoisture["unit"] = "%";
            soilMoisture["quality"] = "error";

            // 根据错误类型提供详细的错误信息
            if (sms->getLastErrorCode() != ErrorCode::SUCCESS) {
                // 传感器读取错误
                soilMoisture["error"] = sms->getLastErrorMessage();
                LOG_WARNING("SMS传感器读取失败: %s", sms->getLastErrorMessage().c_str());
            } else {
                // 数据验证失败
                const String errorMsg = "土壤湿度数据超出有效范围: " + String(moistureValue) + "%";
                soilMoisture["error"] = errorMsg;
                LOG_WARNING("SMS传感器数据无效: %s", errorMsg.c_str());
            }
        }
    } else {
        // SMS传感器未初始化，使用默认值
        soilMoisture["value"] = 0;
        soilMoisture["unit"] = "%";
        soilMoisture["quality"] = "not_available";
        soilMoisture["error"] = "传感器未初始化";
        LOG_WARNING("SMS传感器未初始化");
    }
}

/**
 * @brief 添加环境传感器数据到JSON对象
 *
 * @param sensors JSON对象引用，用于添加传感器数据
 */
void MqttManager::addEnvironmentSensorData(JsonObject& sensors) {
    // 通过RTOSSystemManager获取ECS传感器实例
    ECS* ecs = _systemManager->getEnvironmentSensor();
    if (ecs) {
        // 读取环境传感器数据（使用ECS类优化后的接口）
        const float temperature = ecs->readTemperature();
        const float humidity = ecs->readHumidity();
        const float pressure = ecs->readPressure();

        const bool dataValid = ecs->getLastErrorCode() == ErrorCode::SUCCESS &&
            ECS::isDataValid(temperature, humidity, pressure);

        const unsigned long readCount = ecs->getReadCount();

        // 读取空气温度
        JsonObject airTemperature = sensors.createNestedObject("air_temperature");
        if (dataValid) {
            airTemperature["value"] = temperature;
            airTemperature["unit"] = "°C";
            airTemperature["quality"] = evaluateDataQuality(temperature, -40.0, 85.0);
            airTemperature["read_count"] = readCount;
            LOG_TRACE("ECS温度数据获取成功: %.2f°C", temperature);
        } else {
            airTemperature["value"] = 0.0;
            airTemperature["unit"] = "°C";
            airTemperature["quality"] = "error";
            airTemperature["read_count"] = readCount;

            // 根据错误类型提供详细的错误信息
            if (ecs->getLastErrorCode() != ErrorCode::SUCCESS) {
                airTemperature["error"] = ecs->getLastErrorMessage();
                LOG_WARNING("ECS温度读取失败: %s", ecs->getLastErrorMessage().c_str());
            } else {
                String errorMsg = "温度数据超出有效范围: " + String(temperature, 2) + "°C";
                airTemperature["error"] = errorMsg;
                LOG_WARNING("ECS温度数据无效: %s", errorMsg.c_str());
            }
        }

        // 读取空气湿度
        JsonObject airHumidity = sensors.createNestedObject("air_humidity");
        if (dataValid) {
            airHumidity["value"] = humidity;
            airHumidity["unit"] = "%";
            airHumidity["quality"] = evaluateDataQuality(humidity, 0.0, 100.0);
            LOG_TRACE("ECS湿度数据获取成功: %.2f%%", humidity);
        } else {
            airHumidity["value"] = 0.0;
            airHumidity["unit"] = "%";
            airHumidity["quality"] = "error";

            // 根据错误类型提供详细的错误信息
            if (ecs->getLastErrorCode() != ErrorCode::SUCCESS) {
                airHumidity["error"] = ecs->getLastErrorMessage();
                LOG_WARNING("ECS湿度读取失败: %s", ecs->getLastErrorMessage().c_str());
            } else {
                String errorMsg = "湿度数据超出有效范围: " + String(humidity, 2) + "%";
                airHumidity["error"] = errorMsg;
                LOG_WARNING("ECS湿度数据无效: %s", errorMsg.c_str());
            }
        }

        // 读取大气压力
        JsonObject atmosphericPressure = sensors.createNestedObject("atmospheric_pressure");
        if (dataValid) {
            atmosphericPressure["value"] = pressure;
            atmosphericPressure["unit"] = "hPa";
            atmosphericPressure["quality"] = evaluateDataQuality(pressure, 300.0, 1100.0);
            LOG_TRACE("ECS气压数据获取成功: %.2f hPa", pressure);
        } else {
            atmosphericPressure["value"] = 0.0;
            atmosphericPressure["unit"] = "hPa";
            atmosphericPressure["quality"] = "error";

            // 根据错误类型提供详细的错误信息
            if (ecs->getLastErrorCode() != ErrorCode::SUCCESS) {
                atmosphericPressure["error"] = ecs->getLastErrorMessage();
                LOG_WARNING("ECS气压读取失败: %s", ecs->getLastErrorMessage().c_str());
            } else {
                String errorMsg = "气压数据超出有效范围: " + String(pressure, 2) + " hPa";
                atmosphericPressure["error"] = errorMsg;
                LOG_WARNING("ECS气压数据无效: %s", errorMsg.c_str());
            }
        }
    } else {
        // ECS传感器未初始化，使用默认值
        JsonObject airTemperature = sensors.createNestedObject("air_temperature");
        airTemperature["value"] = 0.0;
        airTemperature["unit"] = "°C";
        airTemperature["quality"] = "not_available";
        airTemperature["error"] = "传感器未初始化";

        JsonObject airHumidity = sensors.createNestedObject("air_humidity");
        airHumidity["value"] = 0.0;
        airHumidity["unit"] = "%";
        airHumidity["quality"] = "not_available";
        airHumidity["error"] = "传感器未初始化";

        JsonObject atmosphericPressure = sensors.createNestedObject("atmospheric_pressure");
        atmosphericPressure["value"] = 0.0;
        atmosphericPressure["unit"] = "hPa";
        atmosphericPressure["quality"] = "not_available";
        atmosphericPressure["error"] = "传感器未初始化";

        LOG_WARNING("ECS传感器未初始化");
    }
}

/**
 * @brief 添加光照传感器数据到JSON对象
 *
 * 第九阶段优化后的光照传感器数据获取方法，集成智能光照管理功能：
 * 1. 获取LIS传感器的原始光照数据
 * 2. 添加智能光照管理器的分析结果
 * 3. 提供光照等级、趋势、昼夜模式等智能分析信息
 * 4. 包含设备工作模式和自动切换状态
 *
 * JSON输出格式：
 * {
 *   "light_intensity": {
 *     "value": 1250.5, "unit": "lux", "quality": "good",
 *     "is_daytime": true, "read_count": 1234
 *   },
 *   "light_analysis": {
 *     "level": "明亮", "trend": "上升", "daynight_mode": "白天",
 *     "trend_rate": 25.3, "average_intensity": 1180.2,
 *     "device_mode": "正常模式", "is_reliable": true
 *   },
 *   "light_statistics": {
 *     "total_samples": 5678, "valid_samples": 5650,
 *     "mode_switches": 12, "events": 45
 *   }
 * }
 *
 * @param sensors JSON对象引用，用于添加传感器数据
 */
void MqttManager::addLightSensorData(JsonObject& sensors) {
    // 1. 添加基础光照传感器数据
    JsonObject light = sensors.createNestedObject("light_intensity");

    // 通过RTOSSystemManager获取LIS传感器实例
    LIS* lis = _systemManager->getLightSensor();
    if (lis) {
        // 读取光照强度数据（使用LIS类优化后的统一接口）
        const float lightIntensity = lis->readLightLevel();

        // 检查传感器是否有错误并验证数据有效性
        if (lis->getLastErrorCode() == ErrorCode::SUCCESS && LIS::isDataValid(lightIntensity)) {
            light["value"] = lightIntensity;
            light["unit"] = "lux";
            light["quality"] = evaluateDataQuality(lightIntensity, 0.0, 65535.0);
            light["read_count"] = lis->getReadCount();
            light["is_daytime"] = lis->isDaytime();

            LOG_TRACE("LIS传感器数据获取成功: %.2f lux", lightIntensity);
        } else {
            // 处理传感器读取错误或数据无效的情况
            light["value"] = 0.0;
            light["unit"] = "lux";
            light["quality"] = "error";
            light["is_daytime"] = false;

            // 根据错误类型提供详细的错误信息
            if (lis->getLastErrorCode() != ErrorCode::SUCCESS) {
                // 传感器读取错误
                light["error"] = lis->getLastErrorMessage();
                LOG_WARNING("LIS传感器读取失败: %s", lis->getLastErrorMessage().c_str());
            } else {
                // 数据验证失败
                String errorMsg = "光照强度数据超出有效范围: " + String(lightIntensity, 2) + " lux";
                light["error"] = errorMsg;
                LOG_WARNING("LIS传感器数据无效: %s", errorMsg.c_str());
            }
        }
    } else {
        // LIS传感器未初始化，使用默认值
        light["value"] = 0.0;
        light["unit"] = "lux";
        light["quality"] = "not_available";
        light["error"] = "传感器未初始化";
        light["is_daytime"] = false;
        LOG_WARNING("LIS传感器未初始化");
    }

    // 2. 添加智能光照管理分析数据
    if (globalLightManager && globalLightManager->isInitialized()) {
        const LightAnalysisResult& analysis = globalLightManager->getCurrentAnalysis();

        if (analysis.isReliable) {
            JsonObject lightAnalysis = sensors.createNestedObject("light_analysis");

            // 光照等级分析（简化版本）
            lightAnalysis["level"] = LightManager::getLightLevelString(analysis.currentLevel);
            lightAnalysis["daynight_mode"] = LightManager::getDayNightModeString(analysis.dayNightMode);
            lightAnalysis["average_intensity"] = analysis.averageIntensity;
            lightAnalysis["is_reliable"] = analysis.isReliable;

            // 设备工作模式信息
            DeviceMode currentMode = globalLightManager->getCurrentMode();
            lightAnalysis["device_mode"] = LightManager::getDeviceModeString(currentMode);
            lightAnalysis["auto_mode_enabled"] = (currentMode == DeviceMode::AUTO);

            LOG_TRACE("光照分析数据: 等级=%s, 模式=%s",
                     LightManager::getLightLevelString(analysis.currentLevel).c_str(),
                     LightManager::getDeviceModeString(currentMode).c_str());
        }

        // 3. 添加简化的光照管理状态信息
        JsonObject lightStats = sensors.createNestedObject("light_statistics");
        lightStats["status"] = "simplified_version";
        lightStats["manager_initialized"] = true;
    } else {
        // 光照管理器未初始化或不可用
        JsonObject lightAnalysis = sensors.createNestedObject("light_analysis");
        lightAnalysis["status"] = "not_available";
        lightAnalysis["error"] = "智能光照管理器未初始化";

        if (globalLightManager) {
            LOG_WARNING("光照管理器未初始化，智能分析功能不可用");
        } else {
            LOG_WARNING("光照管理器实例不存在");
        }
    }
}

/**
 * 添加水深传感器数据到JSON对象
 * 通过RTOSSystemManager的公共接口获取WDS传感器数据（MSP20）
 */
void MqttManager::addWaterSensorData(JsonObject& sensors) {
    JsonObject water = sensors.createNestedObject("water_level");

    // 通过RTOSSystemManager获取WDS传感器实例
    WDS* wds = _systemManager->getWaterSensor();
    if (wds) {
        const double waterDepth = wds->readWaterDepth();
        // 检查传感器是否有错误并验证数据有效性
        if (wds->getLastErrorCode() == ErrorCode::SUCCESS && WDS::isDataValid(waterDepth)) {
            water["value"] = waterDepth;
            water["unit"] = "cm";
            water["quality"] = evaluateDataQuality(static_cast<float>(waterDepth), 0.0, 200.0);
            water["read_count"] = wds->getReadCount();
            water["is_dry"] = wds->isLowWaterLevel();

            LOG_TRACE("WDS传感器数据获取成功: %.2f cm", waterDepth);
        } else {
            // 传感器读取错误，使用错误状态
            water["value"] = 0.0;
            water["unit"] = "cm";
            water["quality"] = "error";
            water["error"] = wds->getLastErrorMessage();
            water["is_dry"] = true;
            LOG_WARNING("WDS传感器读取失败: %s", wds->getLastErrorMessage().c_str());
        }
    } else {
        // WDS传感器未初始化，使用默认值
        water["value"] = 0.0;
        water["unit"] = "cm";
        water["quality"] = "not_available";
        water["error"] = "传感器未初始化";
        water["is_dry"] = true;
        LOG_WARNING("WDS传感器未初始化");
    }
}

/**
 * @brief 添加CAS土壤综合传感器数据到JSON对象
 *
 * @param sensors JSON对象引用，用于添加传感器数据
 * @note 与SMS、WDS、LIS、ECS类的处理方法保持一致的设计模式
 */
void MqttManager::addCASSensorData(JsonObject& sensors) {
    // 通过RTOSSystemManager获取CAS传感器实例
    CAS* cas = _systemManager->getCASSensor();
    if (cas) {
        SoilSensorData soilData;
        const ErrorCode result = cas->readSoilData(soilData);

        // 检查传感器是否有错误并验证数据有效性（与SMS、WDS、LIS、ECS类保持一致的处理方式）
        const bool dataValid = (result == ErrorCode::SUCCESS && CAS::isDataValid(soilData));

        if (dataValid) {
            // 土壤温度
            JsonObject soilTemp = sensors.createNestedObject("soil_temperature");
            soilTemp["value"] = soilData.temperature;
            soilTemp["unit"] = "°C";
            soilTemp["quality"] = evaluateDataQuality(soilData.temperature, -40.0, 80.0);
            soilTemp["read_count"] = cas->getReadCount();

            // 土壤湿度（来自CAS传感器）
            JsonObject soilHum = sensors.createNestedObject("soil_humidity");
            soilHum["value"] = soilData.moisture;
            soilHum["unit"] = "%";
            soilHum["quality"] = evaluateDataQuality(soilData.moisture, 0.0, 100.0);

            // 电导率
            JsonObject ec = sensors.createNestedObject("electrical_conductivity");
            ec["value"] = soilData.ec;
            ec["unit"] = "μS/cm";
            ec["quality"] = evaluateDataQuality(soilData.ec, 0.0, 20000.0);

            // 盐分
            JsonObject salinity = sensors.createNestedObject("salinity");
            salinity["value"] = soilData.salinity;
            salinity["unit"] = "ppm";
            salinity["quality"] = evaluateDataQuality(soilData.salinity, 0.0, 20000.0);

            // pH值
            JsonObject ph = sensors.createNestedObject("ph");
            ph["value"] = soilData.ph;
            ph["unit"] = "pH";
            ph["quality"] = evaluateDataQuality(soilData.ph, 0.0, 14.0);

            // 氮含量
            JsonObject nitrogen = sensors.createNestedObject("nitrogen");
            nitrogen["value"] = soilData.nitrogen;
            nitrogen["unit"] = "mg/L";
            nitrogen["quality"] = evaluateDataQuality(soilData.nitrogen, 0.0, 1999.0);

            // 磷含量
            JsonObject phosphorus = sensors.createNestedObject("phosphorus");
            phosphorus["value"] = soilData.phosphorus;
            phosphorus["unit"] = "mg/L";
            phosphorus["quality"] = evaluateDataQuality(soilData.phosphorus, 0.0, 1999.0);

            // 钾含量
            JsonObject potassium = sensors.createNestedObject("potassium");
            potassium["value"] = soilData.potassium;
            potassium["unit"] = "mg/L";
            potassium["quality"] = evaluateDataQuality(soilData.potassium, 0.0, 1999.0);

            // 添加传感器状态信息（简化版本）
            JsonObject casStatus = sensors.createNestedObject("cas_sensor_status");
            casStatus["read_count"] = cas->getReadCount();

            LOG_TRACE("CAS传感器数据获取成功: T=%.1f°C, M=%.1f%%, pH=%.2f, EC=%u μS/cm",
                     soilData.temperature, soilData.moisture, soilData.ph, soilData.ec);
        } else {
            // 处理传感器读取错误或数据无效的情况
            String errorMessage;

            // 根据错误类型提供详细的错误信息
            if (result != ErrorCode::SUCCESS) {
                // 传感器读取错误
                errorMessage = cas->getLastErrorMessage();
                LOG_WARNING("CAS传感器读取失败: %s", errorMessage.c_str());
            } else {
                // 数据验证失败
                errorMessage = "CAS传感器数据超出有效范围 - 温度:" + String(soilData.temperature, 1) +
                              "°C, 湿度:" + String(soilData.moisture, 1) + "%, pH:" + String(soilData.ph, 2) +
                              ", EC:" + String(soilData.ec) + " μS/cm";
                LOG_WARNING("CAS传感器数据无效: %s", errorMessage.c_str());
            }

            // 使用统一的错误数据格式
            addCASErrorData(sensors, errorMessage);
        }
    } else {
        // CAS传感器未初始化，使用默认值
        addCASErrorData(sensors, "传感器未初始化");
        LOG_WARNING("CAS传感器未初始化");
    }
}

/**
 * 添加CAS传感器错误数据到JSON对象
 * 当CAS传感器读取失败或未初始化时使用
 */
void MqttManager::addCASErrorData(JsonObject& sensors, const String& errorMessage) {
    // 土壤温度（错误状态）
    JsonObject soilTemp = sensors.createNestedObject("soil_temperature");
    soilTemp["value"] = 0.0;
    soilTemp["unit"] = "°C";
    soilTemp["quality"] = "error";
    soilTemp["error"] = errorMessage;
    soilTemp["read_count"] = 0;

    // 土壤湿度（错误状态）
    JsonObject soilHum = sensors.createNestedObject("soil_humidity");
    soilHum["value"] = 0.0;
    soilHum["unit"] = "%";
    soilHum["quality"] = "error";
    soilHum["error"] = errorMessage;

    // 电导率（错误状态）
    JsonObject ec = sensors.createNestedObject("electrical_conductivity");
    ec["value"] = 0.0;
    ec["unit"] = "μS/cm";
    ec["quality"] = "error";
    ec["error"] = errorMessage;

    // 盐分（错误状态）
    JsonObject salinity = sensors.createNestedObject("salinity");
    salinity["value"] = 0.0;
    salinity["unit"] = "ppm";
    salinity["quality"] = "error";
    salinity["error"] = errorMessage;

    // pH值（错误状态）
    JsonObject ph = sensors.createNestedObject("ph");
    ph["value"] = 0.0;
    ph["unit"] = "pH";
    ph["quality"] = "error";
    ph["error"] = errorMessage;

    // 氮含量（错误状态）
    JsonObject nitrogen = sensors.createNestedObject("nitrogen");
    nitrogen["value"] = 0.0;
    nitrogen["unit"] = "mg/L";
    nitrogen["quality"] = "error";
    nitrogen["error"] = errorMessage;

    // 磷含量（错误状态）
    JsonObject phosphorus = sensors.createNestedObject("phosphorus");
    phosphorus["value"] = 0.0;
    phosphorus["unit"] = "mg/L";
    phosphorus["quality"] = "error";
    phosphorus["error"] = errorMessage;

    // 钾含量（错误状态）
    JsonObject potassium = sensors.createNestedObject("potassium");
    potassium["value"] = 0.0;
    potassium["unit"] = "mg/L";
    potassium["quality"] = "error";
    potassium["error"] = errorMessage;

    // CAS传感器状态（错误状态）
    JsonObject casStatus = sensors.createNestedObject("cas_sensor_status");
    casStatus["read_count"] = 0;
    casStatus["temperature_alarm"] = false;
    casStatus["moisture_alarm"] = false;
    casStatus["ph_alarm"] = false;
    casStatus["ec_alarm"] = false;
    casStatus["error"] = errorMessage;
}

// ==================== 单个传感器数据格式化方法 ====================

/**
 * 创建单个土壤湿度传感器数据消息
 */
String MqttManager::createSingleSoilMoistureMessage(int moisture, int readCount) {
    if (!_configManager) {
        return "{}";
    }

    const NetworkConfig& config = _configManager->getConfig();

    // 创建JSON文档
    DynamicJsonDocument doc(512);

    // 设备信息
    doc["device_id"] = String(config.deviceId);
    doc["timestamp"] = millis();

    // 传感器数据容器
    JsonObject sensors = doc.createNestedObject("sensors");

    // 土壤湿度传感器数据
    JsonObject soilMoisture = sensors.createNestedObject("soil_moisture");
    soilMoisture["value"] = moisture;
    soilMoisture["unit"] = "%";
    soilMoisture["quality"] = evaluateDataQuality(static_cast<float>(moisture), 0.0, 100.0);
    soilMoisture["read_count"] = readCount;

    // 序列化JSON
    String result;
    serializeJson(doc, result);

    LOG_TRACE("单个土壤湿度数据消息创建成功，大小: %d字节", result.length());
    return result;
}

/**
 * 创建单个环境传感器数据消息
 */
String MqttManager::createSingleEnvironmentMessage(float temperature, float humidity, float pressure) {
    if (!_configManager) {
        return "{}";
    }

    const NetworkConfig& config = _configManager->getConfig();

    // 创建JSON文档
    DynamicJsonDocument doc(512);

    // 设备信息
    doc["device_id"] = String(config.deviceId);
    doc["timestamp"] = millis();

    // 传感器数据容器
    JsonObject sensors = doc.createNestedObject("sensors");

    // 空气温度
    JsonObject airTemperature = sensors.createNestedObject("air_temperature");
    airTemperature["value"] = temperature;
    airTemperature["unit"] = "°C";
    airTemperature["quality"] = evaluateDataQuality(temperature, -40.0, 80.0);

    // 空气湿度
    JsonObject airHumidity = sensors.createNestedObject("air_humidity");
    airHumidity["value"] = humidity;
    airHumidity["unit"] = "%";
    airHumidity["quality"] = evaluateDataQuality(humidity, 0.0, 100.0);

    // 大气压力
    JsonObject atmosphericPressure = sensors.createNestedObject("atmospheric_pressure");
    atmosphericPressure["value"] = pressure;
    atmosphericPressure["unit"] = "hPa";
    atmosphericPressure["quality"] = evaluateDataQuality(pressure, 300.0, 1100.0);

    // 序列化JSON
    String result;
    serializeJson(doc, result);

    LOG_TRACE("单个环境数据消息创建成功，大小: %d字节", result.length());
    return result;
}

/**
 * 创建单个光照传感器数据消息
 */
String MqttManager::createSingleLightMessage(float intensity, bool isDaytime) {
    if (!_configManager) {
        return "{}";
    }

    const NetworkConfig& config = _configManager->getConfig();

    // 创建JSON文档
    DynamicJsonDocument doc(512);

    // 设备信息
    doc["device_id"] = String(config.deviceId);
    doc["timestamp"] = millis();

    // 传感器数据容器
    JsonObject sensors = doc.createNestedObject("sensors");

    // 光照强度传感器数据
    JsonObject lightIntensity = sensors.createNestedObject("light_intensity");
    lightIntensity["value"] = intensity;
    lightIntensity["unit"] = "lux";
    lightIntensity["quality"] = evaluateDataQuality(intensity, 0.0, 65535.0);
    lightIntensity["is_daytime"] = isDaytime;

    // 序列化JSON
    String result;
    serializeJson(doc, result);

    LOG_TRACE("单个光照数据消息创建成功，大小: %d字节", result.length());
    return result;
}

/**
 * 创建单个水位传感器数据消息
 */
String MqttManager::createSingleWaterLevelMessage(float level, bool isLowLevel) {
    if (!_configManager) {
        return "{}";
    }

    const NetworkConfig& config = _configManager->getConfig();

    // 创建JSON文档
    DynamicJsonDocument doc(512);

    // 设备信息
    doc["device_id"] = String(config.deviceId);
    doc["timestamp"] = millis();

    // 传感器数据容器
    JsonObject sensors = doc.createNestedObject("sensors");

    // 水位传感器数据
    JsonObject waterLevel = sensors.createNestedObject("water_level");
    waterLevel["value"] = level;
    waterLevel["unit"] = "cm";
    waterLevel["quality"] = evaluateDataQuality(level, 0.0, 200.0);
    waterLevel["is_dry"] = isLowLevel;

    // 序列化JSON
    String result;
    serializeJson(doc, result);

    LOG_TRACE("单个水位数据消息创建成功，大小: %d字节", result.length());
    return result;
}

/**
 * 创建单个CAS传感器数据消息
 */
String MqttManager::createSingleCASMessage(float temperature, float moisture, float ph,
                                         uint16_t ec, uint16_t nitrogen, uint16_t phosphorus, uint16_t potassium) {
    if (!_configManager) {
        return "{}";
    }

    const NetworkConfig& config = _configManager->getConfig();

    // 创建JSON文档（优化缓冲区大小以适应复杂传感器数据）
    DynamicJsonDocument doc(800);

    // 设备信息
    doc["device_id"] = String(config.deviceId);
    doc["timestamp"] = millis();

    // 传感器数据容器
    JsonObject sensors = doc.createNestedObject("sensors");

    // 土壤温度
    JsonObject soilTemp = sensors.createNestedObject("soil_temperature");
    soilTemp["value"] = temperature;
    soilTemp["unit"] = "°C";
    soilTemp["quality"] = evaluateDataQuality(temperature, -40.0, 80.0);

    // 土壤湿度（来自CAS传感器）
    JsonObject soilHum = sensors.createNestedObject("soil_humidity");
    soilHum["value"] = moisture;
    soilHum["unit"] = "%";
    soilHum["quality"] = evaluateDataQuality(moisture, 0.0, 100.0);

    // 电导率
    JsonObject ecObj = sensors.createNestedObject("electrical_conductivity");
    ecObj["value"] = ec;
    ecObj["unit"] = "μS/cm";
    ecObj["quality"] = evaluateDataQuality(static_cast<float>(ec), 0.0, 20000.0);

    // 盐分（根据电导率计算）
    JsonObject salinity = sensors.createNestedObject("salinity");
    salinity["value"] = static_cast<float>(ec) * 0.64; // 简化的盐分计算
    salinity["unit"] = "ppm";
    salinity["quality"] = evaluateDataQuality(static_cast<float>(ec) * 0.64, 0.0, 12800.0);

    // pH值
    JsonObject phObj = sensors.createNestedObject("ph");
    phObj["value"] = ph;
    phObj["unit"] = "pH";
    phObj["quality"] = evaluateDataQuality(ph, 0.0, 14.0);

    // 氮含量
    JsonObject nitrogenObj = sensors.createNestedObject("nitrogen");
    nitrogenObj["value"] = nitrogen;
    nitrogenObj["unit"] = "mg/L";
    nitrogenObj["quality"] = evaluateDataQuality(static_cast<float>(nitrogen), 0.0, 1999.0);

    // 磷含量
    JsonObject phosphorusObj = sensors.createNestedObject("phosphorus");
    phosphorusObj["value"] = phosphorus;
    phosphorusObj["unit"] = "mg/L";
    phosphorusObj["quality"] = evaluateDataQuality(static_cast<float>(phosphorus), 0.0, 1999.0);

    // 钾含量
    JsonObject potassiumObj = sensors.createNestedObject("potassium");
    potassiumObj["value"] = potassium;
    potassiumObj["unit"] = "mg/L";
    potassiumObj["quality"] = evaluateDataQuality(static_cast<float>(potassium), 0.0, 1999.0);

    // 序列化JSON
    String result;
    serializeJson(doc, result);

    LOG_TRACE("单个CAS数据消息创建成功，大小: %d字节", result.length());
    return result;
}

/**
 * 创建传感器错误消息
 */
String MqttManager::createSensorErrorMessage(ErrorCode errorCode, const String& errorMessage) {
    if (!_configManager) {
        return "{}";
    }

    const NetworkConfig& config = _configManager->getConfig();

    // 创建JSON文档
    DynamicJsonDocument doc(512);

    // 设备信息
    doc["device_id"] = String(config.deviceId);
    doc["timestamp"] = millis();

    // 错误信息
    JsonObject error = doc.createNestedObject("error");
    error["code"] = static_cast<int>(errorCode);
    error["message"] = errorMessage;
    error["type"] = "sensor_error";

    // 序列化JSON
    String result;
    serializeJson(doc, result);

    LOG_TRACE("传感器错误消息创建成功，大小: %d字节", result.length());
    return result;
}
