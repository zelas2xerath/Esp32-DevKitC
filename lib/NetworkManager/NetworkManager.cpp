#include <NetworkManager.h>
#include <LogManager/LogManager.h>
#include <ConfigManager/ConfigManager.h>

NetworkManager::NetworkManager(ConfigManager* configManager)
    : _configManager(configManager),
      _isWiFiConnected(false),
      _lastErrorCode(ErrorCode::SUCCESS),
      _lastErrorMessage(""),
      _lastErrorTime(0),
      _networkQuality{0, 0.0, 0, 0, 0},
      _lastQualityCheckTime(0),
      _pingSuccessCount(0),
      _pingTotalCount(0),
      _pingStartTime(0),
      _isPinging(false)
{
    LOG_TRACE("NetworkManager构造完成");
}

/**
 * 初始化网络管理器 - 重构后只负责WiFi连接和网络质量监控
 */
ErrorCode NetworkManager::begin() {
    LOG_TRACE("初始化网络管理器...");
    
    if (!_configManager || !_configManager->isNetworkConfigured()) {
        LOG_ERROR("配置管理器未初始化或网络未配置");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    // 连接WiFi网络
    LOG_TRACE("正在连接WiFi...");
    ErrorCode wifiResult = connectToWiFi();
    if (wifiResult != ErrorCode::SUCCESS) {
        return wifiResult;
    }

    // 初始化NTP时间同步
    LOG_TRACE("初始化时间同步...");
    setupTimeSync();
    
    LOG_NOTICE("网络管理器初始化完成！");
    return ErrorCode::SUCCESS;
}

/**
 * 网络管理器循环任务 - 重构后只负责WiFi连接检查和网络质量监控
 */
ErrorCode NetworkManager::loop() {
    // 检查WiFi连接状态
    ErrorCode connectionResult = checkConnection();
    if (connectionResult != ErrorCode::SUCCESS) {
        return connectionResult;
    }

    // 定期检查网络质量（每30秒一次）
    unsigned long currentTime = millis();
    if (currentTime - _lastQualityCheckTime > 30000) {
        checkNetworkQuality();
        _lastQualityCheckTime = currentTime;
    }

    return ErrorCode::SUCCESS;
}

/**
 * 报告错误
 */
ErrorCode NetworkManager::reportError(ErrorCode code, const String& message) {
    _lastErrorCode = code;
    _lastErrorMessage = message;
    _lastErrorTime = millis();
    
    // 根据错误级别记录日志
    if (code == ErrorCode::SUCCESS) {
        LOG_TRACE("%s", message.c_str());
    } else {
        LOG_ERROR("网络错误 [%d]: %s", static_cast<int>(code), message.c_str());
    }
    
    return code;
}

/**
 * 连接WiFi网络（Station模式，失败时重试）
 */
ErrorCode NetworkManager::connectToWiFi() {
    if (!_configManager) {
        return reportError(ErrorCode::NOT_INITIALIZED, "配置管理器未初始化");
    }
    
    const NetworkConfig& config = _configManager->getConfig();
    
    WiFiClass::mode(WIFI_STA);
    WiFi.begin(config.wifiSSID, config.wifiPassword);

    LOG_TRACE("正在连接WiFi网络: %s", config.wifiSSID);
    
    // 等待连接，最多30秒
    int attempts = 0;
    while (WiFiClass::status() != WL_CONNECTED && attempts < 30) {
        delay(1000);
        attempts++;
        LOG_TRACE("WiFi连接尝试 %d/30...", attempts);
    }
    
    if (WiFiClass::status() == WL_CONNECTED) {
        _isWiFiConnected = true;
        LOG_NOTICE("WiFi连接成功！IP地址: %s", WiFi.localIP().toString().c_str());
        return ErrorCode::SUCCESS;
    }
    
    _isWiFiConnected = false;
    return reportError(ErrorCode::WIFI_CONNECTION_ERROR, "WiFi连接失败");
}

/**
 * 检查网络连接状态
 */
ErrorCode NetworkManager::checkConnection() {
    // 检查WiFi连接
    if (WiFiClass::status() != WL_CONNECTED) {
        if (_isWiFiConnected) {
            LOG_WARNING("WiFi连接已断开，尝试重连");
            _isWiFiConnected = false;
            
            // 尝试重连WiFi
            ErrorCode reconnectResult = connectToWiFi();
            if (reconnectResult != ErrorCode::SUCCESS) {
                return reconnectResult;
            }
        } else {
            return reportError(ErrorCode::WIFI_CONNECTION_ERROR, "WiFi未连接");
        }
    } else {
        _isWiFiConnected = true;
    }

    return ErrorCode::SUCCESS;
}

/**
 * 获取WiFi客户端对象（保留接口兼容性）
 * 重构后：主要用于MqttManager的底层WiFi连接
 */
WiFiClient& NetworkManager::getClient() {
    return _client;
}

/**
 * 获取设备ID
 */
String NetworkManager::getDeviceId() {
    return getMACAddress();
}

/**
 * 获取MAC地址
 */
String NetworkManager::getMACAddress() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char macStr[18] = {};
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return {macStr};
}

/**
 * 获取格式化的时间
 */
String NetworkManager::getFormattedTime() {
    tm timeinfo = {};  // 初始化为零
    char timeStr[20];
    
    if (!getLocalTime(&timeinfo)) {
        return "Time not set";
    }
    
    sprintf(timeStr, "%04d-%02d-%02d %02d:%02d:%02d",
            timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    return {timeStr};
}

/**
 * 设置时间同步
 */
void NetworkManager::setupTimeSync() {
    auto ntpServer = "ntp1.aliyun.com";
    constexpr long gmtOffset = 8 * 3600; // GMT+8
    constexpr int daylightOffset = 0;
    
    configTime(gmtOffset, daylightOffset, ntpServer);
    
    LOG_TRACE("正在同步时间...");
    
    // 等待时间同步，最多10秒
    int attempts = 0;
    tm timeinfo = {};
    while (!getLocalTime(&timeinfo) && attempts < 10) {
        delay(1000);
        attempts++;
        LOG_TRACE("时间同步尝试 %d/10...", attempts);
    }
    
    if (getLocalTime(&timeinfo)) {
        LOG_NOTICE("时间同步成功: %04d-%02d-%02d %02d:%02d:%02d",
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        LOG_WARNING("时间同步超时，将在后台继续尝试");
    }
}

/**
 * 检查WiFi连接状态
 */
bool NetworkManager::isWiFiConnected() const {
    return _isWiFiConnected;
}

/**
 * 获取最后一次错误码
 */
ErrorCode NetworkManager::getLastErrorCode() const {
    return _lastErrorCode;
}

/**
 * 获取最后一次错误信息
 */
String NetworkManager::getLastErrorMessage() const {
    return _lastErrorMessage;
}

/**
 * 清除最后一次错误信息
 */
void NetworkManager::clearLastError() {
    _lastErrorCode = ErrorCode::SUCCESS;
    _lastErrorMessage = "";
    _lastErrorTime = 0;
}

/**
 * 获取网络质量信息
 */
NetworkQuality NetworkManager::getNetworkQuality() const {
    return _networkQuality;
}

/**
 * 检查网络质量
 */
ErrorCode NetworkManager::checkNetworkQuality() {
    if (!_isWiFiConnected) {
        return reportError(ErrorCode::WIFI_CONNECTION_ERROR, "WiFi未连接，无法检查网络质量");
    }

    LOG_TRACE("正在检查网络质量...");

    // 测量信号强度
    measureSignalStrength();

    // 测量网络延迟（简化版本）
    measureLatency();

    // 测量丢包率
    measurePacketLoss();

    // 更新最后检查时间
    _lastQualityCheckTime = millis();

    LOG_TRACE("网络质量检查完成 - RSSI: %d dBm, 延迟: %d ms, 丢包率: %.1f%%, 质量评分: %d",
              _networkQuality.rssi, _networkQuality.latency,
              _networkQuality.packetLossRate, _networkQuality.getQualityScore());

    return ErrorCode::SUCCESS;
}

/**
 * 测量信号强度
 */
ErrorCode NetworkManager::measureSignalStrength() {
    _networkQuality.rssi = static_cast<int>(static_cast<unsigned char>(WiFi.RSSI()));
    LOG_TRACE("信号强度: %d dBm (%s)",
             _networkQuality.rssi,
             _networkQuality.getSignalLevelString());
    return ErrorCode::SUCCESS;
}

/**
 * 测量网络延迟（简化版本）
 */
ErrorCode NetworkManager::measureLatency() {
    // 简化的延迟测试，实际项目中可以ping网关或DNS服务器
    unsigned long startTime = millis();

    // 模拟网络延迟测试
    delay(10);  // 简化的延迟模拟

    unsigned long endTime = millis();
    _networkQuality.latency = static_cast<int>(endTime - startTime);

    LOG_TRACE("网络延迟: %d ms", _networkQuality.latency);
    return ErrorCode::SUCCESS;
}

/**
 * 测量丢包率
 */
ErrorCode NetworkManager::measurePacketLoss() {
    // 使用历史数据计算丢包率
    if (_pingTotalCount > 0) {
        _networkQuality.packetLossRate = 100.0f * (1.0f - static_cast<float>(_pingSuccessCount) / static_cast<float>(_pingTotalCount));
    } else {
        _networkQuality.packetLossRate = 0.0f;
    }

    // 更新ping统计（简化版本）
    _pingTotalCount++;
    if (_networkQuality.rssi > -70) {  // 信号强度好时认为ping成功
        _pingSuccessCount++;
    }

    LOG_TRACE("丢包率: %.1f%% (成功: %d, 总计: %d)",
             _networkQuality.packetLossRate,
             _pingSuccessCount,
             _pingTotalCount);

    return ErrorCode::SUCCESS;
}

/**
 * 获取网络质量JSON格式数据
 */
String NetworkManager::getNetworkQualityJson() const {
    String json = "{";
    json += "\"rssi\":" + String(_networkQuality.rssi) + ",";
    json += R"("signal_level":")" + String(_networkQuality.getSignalLevelString()) + "\",";
    json += "\"latency\":" + String(_networkQuality.latency) + ",";
    json += "\"packet_loss\":" + String(_networkQuality.packetLossRate, 1) + ",";
    json += "\"quality_score\":" + String(_networkQuality.getQualityScore()) + ",";

    // 添加时间戳
    char timeStr[20];
    sprintf(timeStr, "%lu", _lastQualityCheckTime);
    json += R"("last_check":")" + String(timeStr) + "\"";
    json += "}";
    return json;
}
