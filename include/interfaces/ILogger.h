#ifndef ILOGGER_H
#define ILOGGER_H

#include <lib.h>

/**
 * @brief 日志管理器接口
 * 
 * 定义日志管理器的抽象接口，支持不同级别的日志输出
 * 提供统一的日志记录和错误处理接口
 */
class ILogger {
public:
    /**
     * @brief 日志级别枚举
     */
    enum class Level {
        SILENT  = 0,  // 无日志输出
        FATAL   = 1,  // 致命错误
        ERROR   = 2,  // 错误
        WARNING = 3,  // 警告
        NOTICE  = 4,  // 提示
        TRACE   = 5,  // 跟踪
        VERBOSE = 6   // 详细
    };

    /**
     * @brief 虚析构函数
     */
    virtual ~ILogger() = default;

    /**
     * @brief 初始化日志管理器
     * 
     * @param level 日志级别
     * @param logToSerial 是否输出到串口
     * @param serialBaud 串口波特率
     */
    virtual void begin(Level level = Level::NOTICE, bool logToSerial = true, unsigned long serialBaud = 115200) = 0;

    /**
     * @brief 设置日志级别
     * 
     * @param level 日志级别
     */
    virtual void setLevel(Level level) = 0;

    /**
     * @brief 获取当前日志级别
     * 
     * @return Level 当前日志级别
     */
    virtual Level getLevel() = 0;

    /**
     * @brief 致命级别日志
     * 
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    virtual void fatal(const char* format, ...) = 0;

    /**
     * @brief 错误级别日志
     * 
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    virtual void error(const char* format, ...) = 0;

    /**
     * @brief 警告级别日志
     * 
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    virtual void warning(const char* format, ...) = 0;

    /**
     * @brief 提示级别日志
     * 
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    virtual void notice(const char* format, ...) = 0;

    /**
     * @brief 信息级别日志
     * 
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    virtual void info(const char* format, ...) = 0;

    /**
     * @brief 跟踪级别日志
     * 
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    virtual void trace(const char* format, ...) = 0;

    /**
     * @brief 详细级别日志
     * 
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    virtual void verbose(const char* format, ...) = 0;

    /**
     * @brief 记录结构化错误信息
     * 
     * @param code 错误码
     * @param module 错误发生的模块
     * @param message 错误信息
     * @param severity 错误严重程度（默认为ERROR）
     * @return ErrorCode 返回相同的错误码，方便链式调用
     */
    virtual ErrorCode logError(ErrorCode code, const char* module, const char* message, SystemState severity = SystemState::ERROR) = 0;
    
    /**
     * @brief 获取最近的错误信息
     * 
     * @return const ErrorInfo& 最近的错误信息
     */
    virtual const ErrorInfo& getLastError() = 0;
    
    /**
     * @brief 清除最近的错误信息
     */
    virtual void clearLastError() = 0;
    
    /**
     * @brief 获取错误码对应的字符串描述
     * 
     * @param code 错误码
     * @return const char* 错误码描述
     */
    virtual const char* getErrorString(ErrorCode code) = 0;
    
    /**
     * @brief 获取系统状态对应的字符串描述
     * 
     * @param state 系统状态
     * @return const char* 系统状态描述
     */
    virtual const char* getSystemStateString(SystemState state) = 0;
};

#endif // ILOGGER_H
