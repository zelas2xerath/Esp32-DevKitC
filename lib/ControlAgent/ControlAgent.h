#ifndef CONTROL_AGENT_H
#define CONTROL_AGENT_H

#include <Arduino.h>
#include <Preferences.h>
#include <lib.h>
#include <interfaces/IControlAgent.h>

// 状态历史记录结构体
struct StateChangeRecord {
    SystemState previousState;   // 之前的状态
    SystemState newState;        // 新状态
    ErrorCode errorCode;         // 相关的错误码
    unsigned long timestamp;     // 状态变化时间戳
};

class ControlAgent final : public IControlAgent {
    // 控制引脚
    uint8_t controlPin;
    
    // 控制模式
    bool& forceMode;
    
    // 湿度阈值
    int& lowThreshold;
    int& highThreshold;
    
    // 偏好设置
    Preferences& prefs;
    
    // 系统状态
    SystemState currentState;
    SystemState previousState;   // 添加前一个状态的记录
    
    // 错误信息
    ErrorCode lastErrorCode;
    String lastErrorMessage;
    unsigned long lastErrorTime;
    
    // 状态机相关
    unsigned long stateChangeTime;
    unsigned long warningTimeout;
    unsigned long errorTimeout;
    bool irrigationEnabled;
    
    // 状态历史记录
    static constexpr uint8_t MAX_STATE_HISTORY = 5;  // 最多保存5条历史记录
    StateChangeRecord stateHistory[MAX_STATE_HISTORY]{};
    uint8_t historyIndex;
    uint8_t historyCount;
    
    // 记录状态变化
    void recordStateChange(SystemState oldState, SystemState newState, ErrorCode code);

public:
    /**
     * 构造函数
     * @param pin 控制输出引脚
     * @param forceModeRef 强制模式引用
     * @param lowThresholdRef 低湿度阈值引用
     * @param highThresholdRef 高湿度阈值引用
     * @param prefsRef Preferences引用
     */
    ControlAgent(
        uint8_t pin,
        bool& forceModeRef,
        int& lowThresholdRef,
        int& highThresholdRef,
        Preferences& prefsRef
    );
    
    /**
     * 初始化控制代理
     * @return 错误码
     */
    ErrorCode begin() override;

    /**
     * 根据当前湿度执行自动控制
     * @param moisture 当前湿度值
     * @return 控制状态是否发生变化
     */
    bool autoControl(int moisture) const override;

    /**
     * 执行强制控制
     * @param state 强制状态（true开启，false关闭）
     * @return 错误码
     */
    ErrorCode forceControl(bool state) override;

    /**
     * 退出强制控制模式
     * @return 错误码
     *
     * 注意：虽然此函数目前总是返回SUCCESS，但保留ErrorCode返回类型
     * 是为了与其他控制函数保持API一致性，并支持未来可能的错误条件。
     * 例如，将来可能会添加某些条件下无法退出强制模式的情况。
     */
    ErrorCode exitForceMode() override;
    
    /**
     * 设置湿度阈值
     * @param low 低湿度阈值
     * @param high 高湿度阈值
     * @return 错误码
     */
    ErrorCode setThresholds(int low, int high) override;
    
    /**
     * 保存阈值到Flash
     * @param low 低湿度阈值
     * @param high 高湿度阈值
     * @return 错误码
     */
    ErrorCode saveThresholds(int low, int high) override;

    /**
     * 加载阈值从Flash
     * @return 错误码
     */
    ErrorCode loadThresholds();

    /**
     * 获取当前控制状态
     * @return 控制状态（HIGH/LOW）
     */
    int getControlState() const;

    /**
     * 获取当前控制模式
     * @return 是否为强制模式
     */
    bool isForceMode() const;

    /**
     * 获取控制模式描述字符串
     * @return 模式描述字符串
     */
    String getModeString() const override;
    
    /**
     * 获取当前系统状态
     * @return 系统状态
     */
    SystemState getCurrentState() const override;

    /**
     * 获取当前系统状态（兼容性方法）
     * @return 系统状态
     */
    SystemState getSystemState() const override;
    
    /**
     * 设置系统状态（接口方法）
     * @param state 系统状态
     * @param errorCode 错误码（如果状态为ERROR或CRITICAL）
     * @return 错误码
     */
    ErrorCode setSystemState(SystemState state, ErrorCode errorCode = ErrorCode::SUCCESS) override;

    /**
     * 设置系统状态（兼容性方法）
     * @param state 系统状态
     * @param errorCode 错误码（如果状态为ERROR或CRITICAL）
     * @param errorMessage 错误信息（如果状态为ERROR或CRITICAL）
     * @return 错误码
     *
     * 注意：此函数目前总是返回SUCCESS，但保留ErrorCode返回类型
     * 是为了与系统中其他状态设置函数保持一致性，并支持未来可能
     * 的错误条件扩展。当前实现会通知systemManager状态变更，
     * 未来可能会增加验证逻辑返回不同错误码。
     */
    ErrorCode setSystemState(SystemState state, ErrorCode errorCode, const String& errorMessage);
    
    /**
     * 报告错误
     * @param errorCode 错误码
     * @param errorMessage 错误信息
     * @param severity 错误严重程度
     * @return 错误码
     */
    ErrorCode reportError(ErrorCode errorCode, const String& errorMessage, SystemState severity = SystemState::ERROR);
    
    /**
     * 清除错误状态
     * @return 错误码
     */
    ErrorCode clearError();
    
    /**
     * 获取最后一次错误码
     * @return 错误码
     */
    ErrorCode getLastErrorCode() const override;

    /**
     * 获取最后一次错误信息
     * @return 错误信息
     */
    String getLastErrorMessage() const override;
    
    /**
     * 设置灌溉状态
     * @param enabled 是否启用灌溉
     * @return 错误码
     */
    ErrorCode setIrrigationEnabled(bool enabled) override;

    /**
     * 获取灌溉状态
     * @return 是否启用灌溉
     */
    bool isIrrigationEnabled() const override;
    
    /**
     * 处理状态机逻辑
     * 定期调用此方法以更新状态机
     */
    void updateStateMachine() override;
    
    /**
     * 获取状态持续时间（当前状态已持续的时间，毫秒）
     * @return 状态持续时间（毫秒）
     */
    unsigned long getStateDuration() const override;

    /**
     * 获取前一个状态
     * @return 前一个系统状态
     */
    SystemState getPreviousState() const override;
    
    /**
     * 获取状态变化历史（接口方法）
     * @param index 历史记录索引（0为最新记录）
     * @return 状态变化记录
     */
    StateChangeRecord getStateHistory(uint8_t index) const override;

    /**
     * 获取状态历史记录数量（接口方法）
     * @return 历史记录数量
     */
    uint8_t getStateHistoryCount() const override;
    
    /**
     * 获取状态变化历史的JSON格式
     * @param maxEntries 最大条目数
     * @return JSON格式的状态变化历史
     */
    String getStateHistoryJson(uint8_t maxEntries) const override;

    /**
     * 获取状态变化历史的JSON格式（带默认参数的便利方法）
     * @return JSON格式的状态变化历史（使用默认最大条目数）
     */
    String getStateHistoryJsonDefault() const;

    // 接口要求的其他方法
    /**
     * 获取控制输出状态
     * @return 控制输出状态
     */
    bool getControlOutputState() const override;

    /**
     * 检查是否处于强制模式
     * @return 是否处于强制模式
     */
    bool isInForceMode() const override;

    /**
     * 获取湿度阈值
     * @param lowThreshold 低湿度阈值输出参数
     * @param highThreshold 高湿度阈值输出参数
     */
    void getThresholds(int& lowThreshold, int& highThreshold) const override;


};

#endif // CONTROL_AGENT_H 