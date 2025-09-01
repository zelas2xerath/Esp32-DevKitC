#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <lib.h>
#include <interfaces/ICommandProcessor.h>

// 前置声明
class ControlAgent;
class IControlAgent;
class LogManager;
class SystemManager;

// ==================== JSON命令处理器 ====================
class CommandProcessor final : public ICommandProcessor {
public:
    /**
     * 构造函数 - 重构后只处理JSON命令
     *
     * @param client WiFiClient对象，用于与服务器通信
     * @param prefs Preferences对象，用于保存配置
     * @param low 湿度低阈值引用
     * @param high 湿度高阈值引用
     * @param mode 强制模式标志引用
     * @param count 湿度读取计数引用
     * @param readFunc 湿度读取函数指针
     * @param pin 控制输出引脚
     * @param agent 控制代理对象（可选）
     */
    CommandProcessor(WiFiClient& client, Preferences& prefs,
                    int& low, int& high, bool& mode,
                    unsigned long& count, int (*readFunc)(), uint8_t pin,
                    IControlAgent* agent = nullptr);

    ~CommandProcessor() override;
    
    /**
     * 处理收到的JSON指令 - 重构后只处理JSON格式
     *
     * @param cmd JSON格式的指令字符串
     * @return 错误码
     */
    ErrorCode processCommand(const String& cmd) override;

    /**
     * 获取支持的指令列表
     *
     * @return String 支持的指令列表（JSON格式）
     */
    String getSupportedCommands() const override;

    /**
     * 获取指令处理统计信息
     *
     * @return String 统计信息（JSON格式）
     */
    String getCommandStatistics() const override;

    /**
     * 重置指令处理统计信息
     */
    void resetStatistics() override;

    /**
     * 获取最后处理的指令
     *
     * @return String 最后处理的指令
     */
    String getLastCommand() const override;

    /**
     * 获取最后处理指令的结果
     *
     * @return ErrorCode 最后处理指令的错误码
     */
    ErrorCode getLastCommandResult() const override;

    /**
     * 获取最后处理指令的时间戳
     *
     * @return unsigned long 时间戳
     */
    unsigned long getLastCommandTimestamp() const override;

private:
    /**
     * 处理JSON格式的命令 - 重构后的核心方法
     * 直接解析并执行JSON命令，无需文本命令转换
     *
     * @param jsonCmd JSON格式的命令字符串
     * @return ErrorCode 处理结果
     */
    ErrorCode executeJsonCommand(const String& jsonCmd);

    /**
     * 执行强制开启灌溉命令
     */
    ErrorCode executeForceOn();

    /**
     * 执行强制关闭灌溉命令
     */
    ErrorCode executeForceOff();

    /**
     * 执行退出强制模式命令
     */
    ErrorCode executeExitForce();

    /**
     * 执行设置阈值命令
     */
    ErrorCode executeSetThreshold(int low, int high);

    /**
     * 执行状态查询命令
     */
    ErrorCode executeGetStatus();

    /**
     * 执行网络质量查询命令
     */
    ErrorCode executeGetNetworkQuality();

    /**
     * 执行设置日志级别命令
     */
    ErrorCode executeSetLogLevel(int level);

    /**
     * 执行状态历史查询命令
     */
    ErrorCode executeGetStateHistory();

    // 通信和配置相关
    WiFiClient& client;
    Preferences& prefs;

    // 各模块参数的引用
    int& lowThreshold;
    int& highThreshold;
    bool& forceMode;
    unsigned long& readCount;
    int (*readMoistureFunc)();
    uint8_t controlPin;

    // 控制代理
    IControlAgent* controlAgent;

    // 统计信息
    String lastCommand;
    ErrorCode lastCommandResult;
    unsigned long lastCommandTimestamp;
    unsigned long totalCommands;
    unsigned long successfulCommands;
    unsigned long failedCommands;
};

#endif // COMMAND_PROCESSOR_H 