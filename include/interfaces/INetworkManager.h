#ifndef INETWORK_MANAGER_H
#define INETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <lib.h>

// Forward declaration
class ConfigManager;

/**
 * 网络连接质量结构体
 */
struct NetworkQuality {
    int rssi;                    // 信号强度 (dBm)
    float packetLossRate;        // 丢包率 (%)
    unsigned long latency;       // 延迟 (ms)
    unsigned long lastCheckTime; // 最后检查时间
    unsigned int reconnectCount; // 重连次数
    
    // 信号质量等级
    enum class SignalLevel {
        EXCELLENT,  // 优秀 (> -50 dBm)
        GOOD,       // 良好 (-50 到 -60 dBm)
        FAIR,       // 一般 (-60 到 -70 dBm)
        WEAK,       // 较弱 (-70 到 -80 dBm)
        POOR        // 很差 (< -80 dBm)
    };
    
    // 获取信号质量等级
    SignalLevel getSignalLevel() const {
        if (rssi > -50) return SignalLevel::EXCELLENT;
        if (rssi > -60) return SignalLevel::GOOD;
        if (rssi > -70) return SignalLevel::FAIR;
        if (rssi > -80) return SignalLevel::WEAK;
        return SignalLevel::POOR;
    }
    
    // 获取信号质量等级字符串
    const char* getSignalLevelString() const {
        switch (getSignalLevel()) {
            case SignalLevel::EXCELLENT: return "优秀";
            case SignalLevel::GOOD: return "良好";
            case SignalLevel::FAIR: return "一般";
            case SignalLevel::WEAK: return "较弱";
            case SignalLevel::POOR: return "很差";
            default: return "未知";
        }
    }
    
    // 获取连接质量百分比 (0-100)
    int getQualityPercentage() const {
        // 基于RSSI计算质量百分比
        if (rssi >= -50) return 100;
        if (rssi <= -100) return 0;
        return (rssi + 100) * 2;  // 线性映射 -100dBm=0%, -50dBm=100%
    }

    // 获取网络质量综合评分 (0-100)
    int getQualityScore() const {
        // 信号强度评分 (0-40分)
        int rssiScore = 0;
        if (rssi > -50) rssiScore = 40;
        else if (rssi > -60) rssiScore = 32;
        else if (rssi > -70) rssiScore = 24;
        else if (rssi > -80) rssiScore = 16;
        else if (rssi > -90) rssiScore = 8;
        // 丢包率评分 (0-30分)
        int lossScore = static_cast<int>(30 * (1.0 - packetLossRate / 100.0));
        // 延迟评分 (0-30分)
        int latencyScore = 0;
        if (latency < 50) latencyScore = 30;
        else if (latency < 100) latencyScore = 24;
        else if (latency < 200) latencyScore = 18;
        else if (latency < 300) latencyScore = 12;
        else if (latency < 500) latencyScore = 6;
        return rssiScore + lossScore + latencyScore;
    }
};

/**
 * @brief 网络管理器接口
 * 
 * 定义网络管理器的抽象接口，负责WiFi连接和服务器通信
 */
class INetworkManager {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~INetworkManager() = default;

    /**
     * @brief 初始化网络管理器
     * 
     * @return ErrorCode 错误码
     */
    virtual ErrorCode begin() = 0;

    /**
     * @brief 网络管理器主循环
     * 
     * @return ErrorCode 错误码
     */
    virtual ErrorCode loop() = 0;
    
    /**
     * @brief 检查WiFi连接状态
     * 
     * @return bool WiFi是否已连接
     */
    virtual bool isWiFiConnected() const = 0;

    /**
     * @brief 获取WiFi客户端对象
     * 
     * @return WiFiClient& WiFi客户端引用
     */
    virtual WiFiClient& getClient() = 0;
    
    /**
     * @brief 获取设备ID
     *
     * @return String 设备ID
     */
    virtual String getDeviceId() = 0;

    /**
     * @brief 获取MAC地址
     *
     * @return String MAC地址
     */
    virtual String getMACAddress() = 0;

    /**
     * @brief 获取格式化的时间字符串
     *
     * @return String 格式化的时间
     */
    virtual String getFormattedTime() = 0;
    
    /**
     * @brief 获取最后一次错误码
     * 
     * @return ErrorCode 错误码
     */
    virtual ErrorCode getLastErrorCode() const = 0;

    /**
     * @brief 获取最后一次错误信息
     * 
     * @return String 错误信息
     */
    virtual String getLastErrorMessage() const = 0;

    /**
     * @brief 清除最后一次错误信息
     */
    virtual void clearLastError() = 0;
    
    /**
     * @brief 获取网络质量信息
     * 
     * @return NetworkQuality 网络质量结构体
     */
    virtual NetworkQuality getNetworkQuality() const = 0;

    /**
     * @brief 检查网络连接状态
     *
     * @return ErrorCode 错误码
     */
    virtual ErrorCode checkConnection() = 0;

    /**
     * @brief 检查网络质量
     *
     * @return ErrorCode 错误码
     */
    virtual ErrorCode checkNetworkQuality() = 0;

    /**
     * @brief 获取网络质量JSON字符串
     * 
     * @return String 网络质量JSON
     */
    virtual String getNetworkQualityJson() const = 0;
};

#endif // INETWORK_MANAGER_H
