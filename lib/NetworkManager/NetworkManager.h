#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <lib.h>
#include <interfaces/INetworkManager.h>

class ConfigManager;
/**
 * 网络管理器类 - 重构后专注于WiFi连接和网络质量管理
 *
 * 职责范围：
 * - WiFi连接管理和自动重连
 * - 网络状态监控
 * - 网络质量评估和测试
 * - 时间同步管理
 * - 设备信息获取
 *
 * 实现INetworkManager接口
 */
class NetworkManager final : public INetworkManager {
public:
    explicit NetworkManager(ConfigManager* configManager);
    ErrorCode begin() override;
    ErrorCode loop() override;

    // 网络状态
    bool isWiFiConnected() const override;
    ErrorCode checkConnection() override;

    // WiFi连接管理
    ErrorCode connectToWiFi();

    // 数据通信
    WiFiClient& getClient() override;  // 返回WiFi客户端

    // 设备信息
    String getDeviceId() override;
    String getMACAddress() override;
    String getFormattedTime() override;

    // 错误处理
    ErrorCode getLastErrorCode() const override;
    String getLastErrorMessage() const override;
    void clearLastError() override;

    // 网络质量监测
    NetworkQuality getNetworkQuality() const override;
    ErrorCode checkNetworkQuality() override;
    String getNetworkQualityJson() const override;

    // 时间同步
    static void setupTimeSync();

private:
    ConfigManager* _configManager;
    WiFiClient _client;  // WiFi客户端
    bool _isWiFiConnected;

    // 错误信息
    ErrorCode _lastErrorCode;
    String _lastErrorMessage;
    unsigned long _lastErrorTime;

    // 网络质量信息
    NetworkQuality _networkQuality;
    unsigned long _lastQualityCheckTime;
    unsigned int _pingSuccessCount;
    unsigned int _pingTotalCount;
    unsigned long _pingStartTime;
    bool _isPinging;
    
    // 网络质量测试
    ErrorCode measureSignalStrength();
    ErrorCode measureLatency();
    ErrorCode measurePacketLoss();

    // 错误处理
    ErrorCode reportError(ErrorCode code, const String& message);
};

#endif // NETWORK_MANAGER_H 