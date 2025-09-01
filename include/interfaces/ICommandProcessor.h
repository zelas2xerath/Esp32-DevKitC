#ifndef ICOMMAND_PROCESSOR_H
#define ICOMMAND_PROCESSOR_H

#include <Arduino.h>
#include <lib.h>

/**
 * @brief 命令处理器接口
 * 
 * 定义命令处理器的抽象接口，负责处理来自服务器的各种指令
 */
class ICommandProcessor {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~ICommandProcessor() = default;

    /**
     * @brief 处理收到的指令
     * 
     * @param cmd 指令字符串
     * @return ErrorCode 错误码
     */
    virtual ErrorCode processCommand(const String& cmd) = 0;
    
    /**
     * @brief 获取支持的指令列表
     * 
     * @return String 支持的指令列表（JSON格式）
     */
    virtual String getSupportedCommands() const = 0;
    
    /**
     * @brief 获取指令处理统计信息
     * 
     * @return String 统计信息（JSON格式）
     */
    virtual String getCommandStatistics() const = 0;
    
    /**
     * @brief 重置指令处理统计信息
     */
    virtual void resetStatistics() = 0;
    
    /**
     * @brief 获取最后处理的指令
     * 
     * @return String 最后处理的指令
     */
    virtual String getLastCommand() const = 0;
    
    /**
     * @brief 获取最后处理指令的结果
     * 
     * @return ErrorCode 最后处理指令的错误码
     */
    virtual ErrorCode getLastCommandResult() const = 0;
    
    /**
     * @brief 获取最后处理指令的时间戳
     * 
     * @return unsigned long 时间戳
     */
    virtual unsigned long getLastCommandTimestamp() const = 0;
};

#endif // ICOMMAND_PROCESSOR_H
