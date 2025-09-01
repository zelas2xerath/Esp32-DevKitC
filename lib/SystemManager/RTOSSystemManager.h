#ifndef RTOS_SYSTEM_MANAGER_H
#define RTOS_SYSTEM_MANAGER_H

#include <lib.h>
#include <Preferences.h>
#include <RTOSCommon.h>
#include <interfaces/ILogger.h>
#include <interfaces/INetworkManager.h>
#include <interfaces/IDisplayManager.h>
#include <interfaces/IControlAgent.h>
#include <interfaces/ICommandProcessor.h>

// 前置声明
class ConfigManager;
class MqttManager;
class U8G2_SSD1306_128X64_NONAME_F_HW_I2C;

// 传感器类前置声明
class SMS;
class ECS;
class LIS;
class WDS;
class CAS;

/**
 * RTOS系统管理器类
 * 负责管理FreeRTOS任务和系统组件
 */
class RTOSSystemManager {
public:
    RTOSSystemManager();
    ~RTOSSystemManager();
    
    // ==================== 初始化和配置 ====================
    
    /**
     * @brief 初始化RTOS系统管理器
     * @param oled OLED显示屏对象
     * @param prefs 配置存储对象
     * @return ErrorCode 错误码
     */
    ErrorCode begin(U8G2_SSD1306_128X64_NONAME_F_HW_I2C& oled, Preferences& prefs);
    
    /**
     * @brief 启动RTOS任务
     * @return ErrorCode 错误码
     */
    ErrorCode startRTOSTasks();
    
    /**
     * @brief 停止RTOS任务
     */
    void stopRTOSTasks();
    
    // ==================== 系统状态管理 ====================
    
    /**
     * @brief 检查系统是否已初始化
     * @return bool 是否已初始化
     */
    bool isInitialized() const;
    
    /**
     * @brief 检查网络是否已配置
     * @return bool 是否已配置
     */
    bool isNetworkConfigured() const;
    
    /**
     * @brief 检查服务器是否已连接
     * @return bool 是否已连接
     */
    bool isServerConnected() const;
    
    /**
     * @brief 检查是否处于配网模式
     * @return bool 是否处于配网模式
     */
    bool isInConfigMode() const;
    
    /**
     * @brief 获取当前系统状态
     * @return SystemState 系统状态
     */
    SystemState getSystemState() const;
    
    /**
     * @brief 设置系统状态
     * @param state 新的系统状态
     * @param errorCode 错误码
     * @param errorMessage 错误信息
     * @return ErrorCode 错误码
     */
    ErrorCode setSystemState(SystemState state, ErrorCode errorCode = ErrorCode::SUCCESS, 
                            const char* errorMessage = nullptr);
    
    /**
     * @brief 报告系统错误
     * @param code 错误码
     * @param message 错误信息
     * @param severity 严重程度
     * @return ErrorCode 错误码
     */
    ErrorCode reportError(ErrorCode code, const char* message, 
                         SystemState severity = SystemState::ERROR);
    
    // ==================== 组件访问接口 ====================
    
    // 传感器管理
    SMS* getSMS() const { return _sms; }
    ECS* getEnvironmentSensor() const { return _environmentSensor; }
    LIS* getLightSensor() const { return _lightSensor; }
    WDS* getWaterSensor() const { return _waterSensor; }
    CAS* getCASSensor() const { return _casSensor; }
    
    // 管理器访问
    ConfigManager* getConfigManager() const { return _configManager; }
    INetworkManager* getNetworkManager() const { return _networkManager; }
    MqttManager* getMqttManager() const { return _mqttManager; }
    IDisplayManager* getDisplayManager() const { return _displayManager; }
    IControlAgent* getControlAgent() const { return _controlAgent; }
    ICommandProcessor* getCommandProcessor() const { return _commandProcessor; }
    ILogger* getLogger() const { return _logger; }
    
    // ==================== RTOS任务管理 ====================
    
    /**
     * @brief 获取任务运行状态
     * @return String 任务状态JSON字符串
     */
    String getTaskStatus() const;
    
    /**
     * @brief 检查所有任务是否健康
     * @return bool 是否健康
     */
    bool areTasksHealthy() const;
    
    /**
     * @brief 重启指定任务
     * @param taskName 任务名称
     * @return ErrorCode 错误码
     */
    ErrorCode restartTask(const char* taskName);
    
    /**
     * @brief 获取队列使用情况
     * @return String 队列使用情况JSON字符串
     */
    String getQueueStatus() const;
    
    // ==================== 事件处理 ====================
    
    /**
     * @brief 发送事件到事件队列
     * @param eventType 事件类型
     * @param param1 参数1
     * @param param2 参数2
     * @param message 事件消息
     * @return ErrorCode 错误码
     */
    ErrorCode postEvent(Event eventType, int param1, int param2, const char* message);
    
    /**
     * @brief 发送传感器数据到队列
     * @param sensorData 传感器数据
     * @return ErrorCode 错误码
     */
    ErrorCode postSensorData(const SensorDataMessage& sensorData);
    
    /**
     * @brief 发送控制指令到队列
     * @param controlCmd 控制指令
     * @return ErrorCode 错误码
     */
    ErrorCode postControlCommand(const ControlCommandMessage& controlCmd);
    
    /**
     * @brief 发送显示消息到队列
     * @param displayMsg 显示消息
     * @return ErrorCode 错误码
     */
    ErrorCode postDisplayMessage(const DisplayMessage& displayMsg);
    
    /**
     * @brief 发送网络消息到队列
     * @param networkMsg 网络消息
     * @return ErrorCode 错误码
     */
    ErrorCode postNetworkMessage(const NetworkSendMessage& networkMsg);
    
    // ==================== 配网模式处理 ====================
    
    /**
     * @brief 进入配网模式
     * @return ErrorCode 错误码
     */
    ErrorCode enterConfigMode();
    
    /**
     * @brief 退出配网模式
     * @return ErrorCode 错误码
     */
    ErrorCode exitConfigMode();
    
    /**
     * @brief 处理配网模式
     * @return ErrorCode 错误码
     */
    ErrorCode handleConfigMode();
    
    // ==================== 系统信息 ====================
    
    /**
     * @brief 获取系统状态信息
     * @return String 系统状态JSON字符串
     */
    String getSystemStatus() const;
    
    /**
     * @brief 获取系统资源使用情况
     * @return SystemResourceInfo 系统资源信息
     */
    SystemResourceInfo getSystemResourceInfo() const;
    
    /**
     * @brief 获取网络质量信息
     * @return NetworkQuality 网络质量信息
     */
    NetworkQuality getNetworkQuality() const;
    
    // ==================== 错误处理 ====================
    
    /**
     * @brief 获取最后一次错误码
     * @return ErrorCode 错误码
     */
    ErrorCode getLastErrorCode() const;
    
    /**
     * @brief 获取最后一次错误信息
     * @return String 错误信息
     */
    String getLastErrorMessage() const;
    
    /**
     * @brief 清除最后一次错误
     */
    void clearLastError();
    
private:
    // ==================== 私有成员变量 ====================
    
    // 初始化状态
    bool _isInitialized;
    bool _isInConfigMode;
    bool _rtosStarted;
    
    // 系统状态
    SystemState _systemState;
    ErrorCode _lastErrorCode;
    String _lastErrorMessage;
    unsigned long _lastErrorTime;
    
    // 组件指针
    ConfigManager* _configManager;
    INetworkManager* _networkManager;
    MqttManager* _mqttManager;
    IDisplayManager* _displayManager;
    IControlAgent* _controlAgent;
    ICommandProcessor* _commandProcessor;
    ILogger* _logger;
    
    // 传感器指针
    SMS* _sms;
    ECS* _environmentSensor;
    LIS* _lightSensor;
    WDS* _waterSensor;
    CAS* _casSensor;

    // 控制代理配置参数（用作引用参数）
    bool _forceMode;
    int _lowThreshold;
    int _highThreshold;
    unsigned long _readCount;  // 读取计数
    
    // ==================== 私有方法 ====================
    
    /**
     * @brief 初始化系统组件
     * @param oled OLED显示屏对象
     * @param prefs 配置存储对象
     * @return ErrorCode 错误码
     */
    ErrorCode initializeComponents(U8G2_SSD1306_128X64_NONAME_F_HW_I2C& oled, Preferences& prefs);
    
    /**
     * @brief 初始化传感器
     * @return ErrorCode 错误码
     */
    ErrorCode initializeSensors();
    
    /**
     * @brief 初始化管理器
     * @param systemPrefs 系统配置
     * @return ErrorCode 错误码
     */
    ErrorCode initializeManagers(Preferences& systemPrefs);
    
    /**
     * @brief 验证系统配置
     * @return ErrorCode 错误码
     */
    ErrorCode validateSystemConfiguration();
};

// 全局系统管理器实例声明
extern RTOSSystemManager* rtosSystemManager;

#endif // RTOS_SYSTEM_MANAGER_H
