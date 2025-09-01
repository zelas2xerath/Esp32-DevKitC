#ifndef ICONTROL_AGENT_H
#define ICONTROL_AGENT_H

#include <Arduino.h>
#include <lib.h>

// 前向声明，使用ControlAgent.h中定义的StateChangeRecord
struct StateChangeRecord;

/**
 * @brief 控制代理接口
 * 
 * 定义控制代理的抽象接口，负责自动控制和强制控制功能
 */
class IControlAgent {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~IControlAgent() = default;

    /**
     * @brief 初始化控制代理
     * 
     * @return ErrorCode 错误码
     */
    virtual ErrorCode begin() = 0;
    
    /**
     * @brief 根据当前湿度执行自动控制
     * 
     * @param moisture 当前湿度值
     * @return bool 控制状态是否发生变化
     */
    virtual bool autoControl(int moisture) const = 0;
    
    /**
     * @brief 执行强制控制
     * 
     * @param state 强制状态（true开启，false关闭）
     * @return ErrorCode 错误码
     */
    virtual ErrorCode forceControl(bool state) = 0;
    
    /**
     * @brief 退出强制控制模式
     * 
     * @return ErrorCode 错误码
     */
    virtual ErrorCode exitForceMode() = 0;
    
    /**
     * @brief 获取当前系统状态
     * 
     * @return SystemState 当前系统状态
     */
    virtual SystemState getCurrentState() const = 0;
    
    /**
     * @brief 设置系统状态
     *
     * @param state 新的系统状态
     * @param errorCode 错误码（可选）
     * @return ErrorCode 错误码
     */
    virtual ErrorCode setSystemState(SystemState state, ErrorCode errorCode = ErrorCode::SUCCESS) = 0;

    /**
     * @brief 更新状态机
     *
     * 执行状态机的周期性更新，处理状态转换和定时任务
     */
    virtual void updateStateMachine() = 0;
    
    /**
     * @brief 获取状态变化历史记录
     *
     * @param index 历史记录索引（0为最新记录）
     * @return StateChangeRecord 状态变化记录
     */
    virtual StateChangeRecord getStateHistory(uint8_t index) const = 0;

    /**
     * @brief 获取状态历史记录数量
     *
     * @return uint8_t 历史记录数量
     */
    virtual uint8_t getStateHistoryCount() const = 0;
    
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
     * @brief 设置灌溉状态
     * 
     * @param enabled 是否启用灌溉
     * @return ErrorCode 错误码
     */
    virtual ErrorCode setIrrigationEnabled(bool enabled) = 0;
    
    /**
     * @brief 获取灌溉状态
     * 
     * @return bool 灌溉是否启用
     */
    virtual bool isIrrigationEnabled() const = 0;
    
    /**
     * @brief 获取控制输出状态
     * 
     * @return bool 控制输出状态
     */
    virtual bool getControlOutputState() const = 0;
    
    /**
     * @brief 检查是否处于强制模式
     * 
     * @return bool 是否处于强制模式
     */
    virtual bool isInForceMode() const = 0;
    
    /**
     * @brief 获取湿度阈值
     * 
     * @param lowThreshold 低湿度阈值输出参数
     * @param highThreshold 高湿度阈值输出参数
     */
    virtual void getThresholds(int& lowThreshold, int& highThreshold) const = 0;
    
    /**
     * @brief 设置湿度阈值
     *
     * @param lowThreshold 低湿度阈值
     * @param highThreshold 高湿度阈值
     * @return ErrorCode 错误码
     */
    virtual ErrorCode setThresholds(int lowThreshold, int highThreshold) = 0;

    /**
     * @brief 获取系统状态（兼容性方法）
     *
     * @return SystemState 系统状态
     */
    virtual SystemState getSystemState() const = 0;

    /**
     * @brief 获取前一个状态
     *
     * @return SystemState 前一个系统状态
     */
    virtual SystemState getPreviousState() const = 0;

    /**
     * @brief 获取状态持续时间
     *
     * @return unsigned long 状态持续时间（毫秒）
     */
    virtual unsigned long getStateDuration() const = 0;

    /**
     * @brief 获取状态变化历史的JSON格式
     *
     * @param maxEntries 最大条目数
     * @return String JSON格式的状态变化历史
     */
    virtual String getStateHistoryJson(uint8_t maxEntries) const = 0;

    /**
     * @brief 保存阈值到Flash
     *
     * @param low 低湿度阈值
     * @param high 高湿度阈值
     * @return ErrorCode 错误码
     */
    virtual ErrorCode saveThresholds(int low, int high) = 0;

    /**
     * @brief 获取控制模式描述字符串
     *
     * @return String 模式描述字符串
     */
    virtual String getModeString() const = 0;
};

#endif // ICONTROL_AGENT_H
