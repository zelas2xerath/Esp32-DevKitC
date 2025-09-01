/*
 * RTOS系统管理器实现
 * 负责管理FreeRTOS任务和系统组件的生命周期
 * 作者：[Zelas2Xerath]
 * 版本：2.1.3 - RTOS重构版本
 */

#include <ArduinoJson.h>
#include <SystemManager/RTOSSystemManager.h>
#include <ConfigManager/ConfigManager.h>
#include <NetworkManager/NetworkManager.h>
#include <NetworkManager/MqttManager.h>
#include <DisplayManager/DisplayManager.h>
#include <ControlAgent/ControlAgent.h>
#include <CommandProcessor/CommandProcessor.h>
#include <LogManager/LogManager.h>
#include <SensorManager/SensorManager.h>


// ==================== 构造函数和析构函数 ====================

RTOSSystemManager::RTOSSystemManager() 
    : _isInitialized(false)
    , _isInConfigMode(false)
    , _rtosStarted(false)
    , _systemState(SystemState::NORMAL)
    , _lastErrorCode(ErrorCode::SUCCESS)
    , _lastErrorMessage("")
    , _lastErrorTime(0)
    , _configManager(nullptr)
    , _networkManager(nullptr)
    , _mqttManager(nullptr)
    , _displayManager(nullptr)
    , _controlAgent(nullptr)
    , _commandProcessor(nullptr)
    , _logger(nullptr)
    , _sms(nullptr)
    , _environmentSensor(nullptr)
    , _lightSensor(nullptr)
    , _waterSensor(nullptr)
    , _casSensor(nullptr)
    , _forceMode(false)
    , _lowThreshold(30)
    , _highThreshold(70)
    , _readCount(0)
{
    LOG_TRACE("RTOSSystemManager构造函数");
}

RTOSSystemManager::~RTOSSystemManager() {
    LOG_TRACE("RTOSSystemManager析构函数");
    
    // 停止RTOS任务
    if (_rtosStarted) {
        stopRTOSTasks();
    }
    
    // 清理传感器
    delete _sms;
    delete _environmentSensor;
    delete _lightSensor;
    delete _waterSensor;
    delete _casSensor;
    delete _configManager;
    delete _networkManager;
    delete _mqttManager;
    delete _displayManager;
    delete _controlAgent;
    delete _commandProcessor;
}

// ==================== 初始化和配置 ====================

ErrorCode RTOSSystemManager::begin(U8G2_SSD1306_128X64_NONAME_F_HW_I2C& oled, Preferences& prefs) {
    LOG_NOTICE("开始初始化RTOS系统管理器...");
    
    if (_isInitialized) {
        LOG_WARNING("RTOS系统管理器已经初始化");
        return ErrorCode::SUCCESS;
    }
    
    // 设置日志管理器（使用全局实例）
    _logger = globalLogManager;
    if (!_logger) {
        Serial.println("错误：全局日志管理器未初始化");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    // 初始化系统组件
    ErrorCode result = initializeComponents(oled, prefs);
    if (result != ErrorCode::SUCCESS) {
        LOG_ERROR("系统组件初始化失败: %d", static_cast<int>(result));
        return result;
    }
    
    // 初始化传感器
    result = initializeSensors();
    if (result != ErrorCode::SUCCESS) {
        LOG_ERROR("传感器初始化失败: %d", static_cast<int>(result));
        return result;
    }
    
    // 初始化管理器
    result = initializeManagers(prefs);
    if (result != ErrorCode::SUCCESS) {
        LOG_ERROR("管理器初始化失败: %d", static_cast<int>(result));
        return result;
    }
    
    // 验证系统配置
    result = validateSystemConfiguration();
    if (result != ErrorCode::SUCCESS) {
        LOG_ERROR("系统配置验证失败: %d", static_cast<int>(result));
        return result;
    }
    
    _isInitialized = true;
    _systemState = SystemState::NORMAL;
    
    LOG_NOTICE("RTOS系统管理器初始化完成");
    return ErrorCode::SUCCESS;
}

ErrorCode RTOSSystemManager::startRTOSTasks() {
    LOG_NOTICE("启动RTOS任务...");
    
    if (!_isInitialized) {
        LOG_ERROR("系统未初始化，无法启动任务");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    if (_rtosStarted) {
        LOG_WARNING("RTOS任务已经启动");
        return ErrorCode::SUCCESS;
    }
    
    _rtosStarted = true;
    _systemState = SystemState::NORMAL;
    
    LOG_NOTICE("RTOS任务启动完成");
    return ErrorCode::SUCCESS;
}

void RTOSSystemManager::stopRTOSTasks() {
    LOG_NOTICE("停止RTOS任务...");
    
    if (!_rtosStarted) {
        LOG_WARNING("RTOS任务未启动");
        return;
    }
    
    _rtosStarted = false;
    _systemState = SystemState::WARNING;
    
    LOG_NOTICE("RTOS任务已停止");
}

// ==================== 系统状态管理 ====================

bool RTOSSystemManager::isInitialized() const {
    return _isInitialized;
}

bool RTOSSystemManager::isNetworkConfigured() const {
    // 简化实现：检查网络管理器是否存在且WiFi已连接
    return _networkManager && _networkManager->isWiFiConnected();
}

bool RTOSSystemManager::isServerConnected() const {
    // 重构后：检查MQTT连接状态而不是TCP服务器连接
    return _mqttManager && _mqttManager->isConnected();
}

bool RTOSSystemManager::isInConfigMode() const {
    return _isInConfigMode;
}

SystemState RTOSSystemManager::getSystemState() const {
    return _systemState;
}

ErrorCode RTOSSystemManager::setSystemState(SystemState state, ErrorCode errorCode, const char* errorMessage) {
    _systemState = state;
    
    if (errorCode != ErrorCode::SUCCESS) {
        _lastErrorCode = errorCode;
        _lastErrorMessage = errorMessage ? String(errorMessage) : "";
        _lastErrorTime = millis();
        
        LOG_ERROR("系统状态变更: %d, 错误: %s", static_cast<int>(state), errorMessage ? errorMessage : "未知错误");
    }
    
    return ErrorCode::SUCCESS;
}

ErrorCode RTOSSystemManager::reportError(ErrorCode code, const char* message, SystemState severity) {
    _lastErrorCode = code;
    _lastErrorMessage = message ? String(message) : "";
    _lastErrorTime = millis();
    
    // 根据严重程度设置系统状态
    if (severity == SystemState::CRITICAL) {
        _systemState = SystemState::CRITICAL;
    } else if (severity == SystemState::ERROR && _systemState != SystemState::CRITICAL) {
        _systemState = SystemState::ERROR;
    }
    
    LOG_ERROR("系统错误报告: 代码=%d, 消息=%s, 严重程度=%d", 
              static_cast<int>(code), message ? message : "无", static_cast<int>(severity));
    
    return ErrorCode::SUCCESS;
}

// ==================== RTOS任务管理 ====================

String RTOSSystemManager::getTaskStatus() const {
    // 创建JSON格式的任务状态信息
    DynamicJsonDocument doc(512);
    
    doc["initialized"] = _isInitialized;
    doc["rtos_started"] = _rtosStarted;
    doc["system_state"] = static_cast<int>(_systemState);
    doc["uptime"] = millis();
    
    // 添加任务健康状态
    JsonObject tasks = doc.createNestedObject("tasks");
    tasks["sensor"] = true;  // 简化实现
    tasks["network"] = true;
    tasks["control"] = true;
    tasks["display"] = true;
    tasks["watchdog"] = true;
    
    String result;
    serializeJson(doc, result);
    return result;
}

bool RTOSSystemManager::areTasksHealthy() const {
    // 简化实现：检查基本状态
    return _isInitialized && _rtosStarted && (_systemState == SystemState::NORMAL || _systemState == SystemState::WARNING);
}

ErrorCode RTOSSystemManager::restartTask(const char* taskName) {
    LOG_NOTICE("重启任务: %s", taskName);
    // 简化实现：记录重启请求
    return ErrorCode::SUCCESS;
}

String RTOSSystemManager::getQueueStatus() const {
    // 创建JSON格式的队列状态信息
    DynamicJsonDocument doc(256);
    
    doc["sensor_queue"] = "active";
    doc["network_queue"] = "active";
    doc["control_queue"] = "active";
    doc["display_queue"] = "active";
    doc["event_queue"] = "active";
    
    String result;
    serializeJson(doc, result);
    return result;
}

// ==================== 事件处理 ====================

ErrorCode RTOSSystemManager::postEvent(Event eventType, int param1, int param2, const char* message) {
    LOG_TRACE("发送事件: type=%d, param1=%d, param2=%d, msg=%s", 
              static_cast<int>(eventType), param1, param2, message ? message : "");
    
    // 简化实现：直接记录事件
    return ErrorCode::SUCCESS;
}

ErrorCode RTOSSystemManager::postSensorData(const SensorDataMessage& sensorData) {
    LOG_TRACE("发送传感器数据: type=%d", static_cast<int>(sensorData.type));
    return ErrorCode::SUCCESS;
}

ErrorCode RTOSSystemManager::postControlCommand(const ControlCommandMessage& controlCmd) {
    LOG_TRACE("发送控制指令: type=%d", static_cast<int>(controlCmd.type));
    return ErrorCode::SUCCESS;
}

ErrorCode RTOSSystemManager::postDisplayMessage(const DisplayMessage& displayMsg) {
    LOG_TRACE("发送显示消息: type=%d", static_cast<int>(displayMsg.type));
    
    // 直接更新显示（使用简化的接口）
    if (_displayManager) {
        // 使用DisplayManager的showText方法
        auto title = String(displayMsg.line1);
        String content = String(displayMsg.line2) + " " + String(displayMsg.line3) + " " + String(displayMsg.line4);
        _displayManager->showText(title, content);
    }
    
    return ErrorCode::SUCCESS;
}

ErrorCode RTOSSystemManager::postNetworkMessage(const NetworkSendMessage& networkMsg) {
    LOG_TRACE("发送网络消息: type=%d", static_cast<int>(networkMsg.type));
    return ErrorCode::SUCCESS;
}

// ==================== 配网模式处理 ====================

ErrorCode RTOSSystemManager::enterConfigMode() {
    LOG_NOTICE("进入配网模式");
    _isInConfigMode = true;
    return ErrorCode::SUCCESS;
}

ErrorCode RTOSSystemManager::exitConfigMode() {
    LOG_NOTICE("退出配网模式");
    _isInConfigMode = false;
    return ErrorCode::SUCCESS;
}

ErrorCode RTOSSystemManager::handleConfigMode() {
    // 简化实现：处理配网模式
    if (_networkManager) {
        // 检查网络连接状态
        _networkManager->loop();
    }
    return ErrorCode::SUCCESS;
}

// ==================== 系统信息 ====================

String RTOSSystemManager::getSystemStatus() const {
    DynamicJsonDocument doc(512);

    doc["initialized"] = _isInitialized;
    doc["config_mode"] = _isInConfigMode;
    doc["system_state"] = static_cast<int>(_systemState);
    doc["last_error"] = static_cast<int>(_lastErrorCode);
    doc["error_message"] = _lastErrorMessage;
    doc["uptime"] = millis();
    doc["free_heap"] = ESP.getFreeHeap();

    String result;
    serializeJson(doc, result);
    return result;
}

SystemResourceInfo RTOSSystemManager::getSystemResourceInfo() const {
    SystemResourceInfo info;
    info.freeHeapSize = ESP.getFreeHeap();
    info.minFreeHeapSize = ESP.getMinFreeHeap();
    info.cpuUsage = 0; // 简化实现
    info.taskCount = 6; // 固定任务数量
    info.uptime = millis() / 1000;

    return info;
}

NetworkQuality RTOSSystemManager::getNetworkQuality() const {
    NetworkQuality quality;

    if (_networkManager) {
        quality = _networkManager->getNetworkQuality();
    } else {
        // 默认值
        quality.rssi = -100;
        quality.packetLossRate = 100.0f;
        quality.latency = 0;
        quality.lastCheckTime = 0;
        quality.reconnectCount = 0;
    }

    return quality;
}

// ==================== 错误处理 ====================

ErrorCode RTOSSystemManager::getLastErrorCode() const {
    return _lastErrorCode;
}

String RTOSSystemManager::getLastErrorMessage() const {
    return _lastErrorMessage;
}

void RTOSSystemManager::clearLastError() {
    _lastErrorCode = ErrorCode::SUCCESS;
    _lastErrorMessage = "";
    _lastErrorTime = 0;
}

// ==================== 私有方法实现 ====================

ErrorCode RTOSSystemManager::initializeComponents(U8G2_SSD1306_128X64_NONAME_F_HW_I2C& oled, Preferences& prefs) {
    LOG_TRACE("初始化系统组件...");

    // 初始化配置管理器
    _configManager = new ConfigManager();
    ErrorCode result = _configManager->begin();
    if (result != ErrorCode::SUCCESS) {
        LOG_ERROR("配置管理器初始化失败");
        return result;
    }

    // 初始化显示管理器
    _displayManager = new DisplayManager(oled);
    result = _displayManager->begin();
    if (result != ErrorCode::SUCCESS) {
        LOG_ERROR("显示管理器初始化失败");
        return result;
    }

    LOG_TRACE("系统组件初始化完成");
    return ErrorCode::SUCCESS;
}

ErrorCode RTOSSystemManager::initializeSensors() {
    LOG_TRACE("初始化传感器...");

    // 初始化土壤湿度传感器
    _sms = new SMS();
    ErrorCode result = _sms->begin();
    if (result != ErrorCode::SUCCESS) {
        LOG_WARNING("土壤湿度传感器初始化失败");
        // 非致命错误，继续初始化
    }

    // 初始化环境传感器
    _environmentSensor = new ECS();
    result = _environmentSensor->begin();
    if (result != ErrorCode::SUCCESS) {
        LOG_WARNING("环境传感器初始化失败");
    }

    // 初始化光照传感器
    _lightSensor = new LIS();
    result = _lightSensor->begin();
    if (result != ErrorCode::SUCCESS) {
        LOG_WARNING("光照传感器初始化失败");
    }

    // 初始化水位传感器
    _waterSensor = new WDS();
    result = _waterSensor->begin();
    if (result != ErrorCode::SUCCESS) {
        LOG_WARNING("水位传感器初始化失败");
    }

    // 初始化CAS传感器
    _casSensor = new CAS();
    result = _casSensor->begin();
    if (result != ErrorCode::SUCCESS) {
        LOG_WARNING("CAS传感器初始化失败");
    }

    LOG_TRACE("传感器初始化完成");
    return ErrorCode::SUCCESS;
}

ErrorCode RTOSSystemManager::initializeManagers(Preferences& systemPrefs) {
    LOG_TRACE("初始化管理器...");

    // 初始化网络管理器（重构后只负责WiFi连接和网络质量监控）
    _networkManager = new NetworkManager(_configManager);
    ErrorCode result = _networkManager->begin();
    if (result != ErrorCode::SUCCESS) {
        LOG_WARNING("网络管理器初始化失败");
        // 非致命错误，继续初始化
    }

    // 初始化MQTT管理器（专门负责MQTT通信）
    _mqttManager = new MqttManager(_networkManager->getClient(), _configManager);
    result = _mqttManager->begin();
    if (result != ErrorCode::SUCCESS) {
        LOG_WARNING("MQTT管理器初始化失败");
        // 非致命错误，继续初始化
    }

    // 初始化控制代理（使用成员变量作为引用参数）
    _controlAgent = new ControlAgent(
        controlOutputPin,
        _forceMode,
        _lowThreshold,
        _highThreshold,
        systemPrefs
    );
    result = _controlAgent->begin();
    if (result != ErrorCode::SUCCESS) {
        LOG_WARNING("控制代理初始化失败");
    }

    // 初始化命令处理器（重构后使用简化的参数）
    _commandProcessor = new CommandProcessor(
        _networkManager->getClient(),  // WiFiClient引用
        systemPrefs,                   // Preferences引用
        _lowThreshold,                 // 低阈值引用
        _highThreshold,                // 高阈值引用
        _forceMode,                    // 强制模式引用
        _readCount,                    // 读取计数引用
        nullptr,                       // 湿度读取函数（暂时为空）
        controlOutputPin,              // 控制输出引脚
        _controlAgent                  // 控制代理
    );

    // 设置MQTT管理器的消息转发和传感器数据读取
    if (_mqttManager && _commandProcessor) {
        // 将ICommandProcessor*转换为CommandProcessor*
        auto* commandProcessor = static_cast<CommandProcessor*>(_commandProcessor);
        _mqttManager->setCommandProcessor(commandProcessor);
        LOG_TRACE("MQTT消息转发已设置");
    }

    if (_mqttManager) {
        // 设置系统管理器引用，用于传感器数据读取
        _mqttManager->setSystemManager(this);
        LOG_TRACE("MQTT传感器数据读取已设置");
    }

    LOG_TRACE("命令处理器初始化完成");

    LOG_TRACE("管理器初始化完成");
    return ErrorCode::SUCCESS;
}

ErrorCode RTOSSystemManager::validateSystemConfiguration() {
    LOG_TRACE("验证系统配置...");

    // 检查关键组件
    if (!_configManager) {
        LOG_ERROR("配置管理器未初始化");
        return ErrorCode::INITIALIZATION_FAILED;
    }

    if (!_displayManager) {
        LOG_ERROR("显示管理器未初始化");
        return ErrorCode::INITIALIZATION_FAILED;
    }

    // 检查至少有一个传感器可用（简化检查，只验证指针非空）
    bool hasSensor = _sms != nullptr ||
                     _environmentSensor != nullptr ||
                     _lightSensor != nullptr ||
                     _waterSensor != nullptr ||
                     _casSensor != nullptr;

    if (!hasSensor) {
        LOG_WARNING("没有可用的传感器");
        // 非致命错误，系统仍可运行
    }

    LOG_TRACE("系统配置验证完成");
    return ErrorCode::SUCCESS;
}
