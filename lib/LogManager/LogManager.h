#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <Arduino.h>
#include <ArduinoLog.h>
#include <lib.h>
#include <interfaces/ILogger.h>

// 前向声明
class LogManager;
extern LogManager* globalLogManager;

// 日志宏定义，简化日志调用
// 如果有全局LogManager实例，使用实例方法；否则使用静态方法（向后兼容）
#define LOG_FATAL(...)    do { if (globalLogManager) globalLogManager->fatal(__VA_ARGS__); else Log.fatal(__VA_ARGS__); } while(0)
#define LOG_ERROR(...)    do { if (globalLogManager) globalLogManager->error(__VA_ARGS__); else Log.error(__VA_ARGS__); } while(0)
#define LOG_WARNING(...)  do { if (globalLogManager) globalLogManager->warning(__VA_ARGS__); else Log.warning(__VA_ARGS__); } while(0)
#define LOG_NOTICE(...)   do { if (globalLogManager) globalLogManager->notice(__VA_ARGS__); else Log.notice(__VA_ARGS__); } while(0)
#define LOG_INFO(...)     do { if (globalLogManager) globalLogManager->info(__VA_ARGS__); else Log.info(__VA_ARGS__); } while(0)
#define LOG_TRACE(...)    do { if (globalLogManager) globalLogManager->trace(__VA_ARGS__); else Log.trace(__VA_ARGS__); } while(0)
#define LOG_VERBOSE(...)  do { if (globalLogManager) globalLogManager->verbose(__VA_ARGS__); else Log.verbose(__VA_ARGS__); } while(0)

// 错误处理宏定义，简化错误处理调用
#define LOG_ERROR_CODE(code, module, msg) LogManager::logError(code, module, msg)
#define CHECK_ERROR(condition, code, module, msg) do { if(!(condition)) { return LogManager::logError(code, module, msg); } } while(0)
#define RETURN_IF_ERROR(result) do { if((result) != ErrorCode::SUCCESS) { return result; } } while(0)

/**
 * @brief 日志管理器类
 *
 * 封装 ArduinoLog 库，提供统一的日志接口
 * 支持不同级别的日志输出，可配置输出级别
 * 支持格式化日志输出
 * 实现ILogger接口
 */
class LogManager final : public ILogger {
public:
    // 构造函数
    LogManager();

    // 使用ILogger中定义的Level枚举

    /**
     * @brief 初始化日志管理器
     *
     * @param level 日志级别
     * @param logToSerial 是否输出到串口
     * @param serialBaud 串口波特率
     */
    void begin(Level level = Level::NOTICE, bool logToSerial = true, unsigned long serialBaud = 115200) override;

    /**
     * @brief 设置日志级别
     *
     * @param level 日志级别
     */
    void setLevel(Level level) override;

    /**
     * @brief 获取当前日志级别
     *
     * @return Level 当前日志级别
     */
    Level getLevel() override;

    /**
     * @brief 致命级别日志
     *
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    void fatal(const char* format, ...) override;

    /**
     * @brief 错误级别日志
     *
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    void error(const char* format, ...) override;

    /**
     * @brief 警告级别日志
     *
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    void warning(const char* format, ...) override;

    /**
     * @brief 提示级别日志
     *
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    void notice(const char* format, ...) override;

    /**
     * @brief 信息级别日志
     *
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    void info(const char* format, ...) override;

    /**
     * @brief 跟踪级别日志
     *
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    void trace(const char* format, ...) override;

    /**
     * @brief 详细级别日志
     *
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    void verbose(const char* format, ...) override;

    /**
     * @brief 记录结构化错误信息
     *
     * @param code 错误码
     * @param module 错误发生的模块
     * @param message 错误信息
     * @param severity 错误严重程度（默认为ERROR）
     * @return ErrorCode 返回相同的错误码，方便链式调用
     */
    ErrorCode logError(ErrorCode code, const char* module, const char* message, SystemState severity = SystemState::ERROR) override;

    /**
     * @brief 获取最近的错误信息
     *
     * @return const ErrorInfo& 最近的错误信息
     */
    const ErrorInfo& getLastError() override;

    /**
     * @brief 清除最近的错误信息
     */
    void clearLastError() override;

    /**
     * @brief 获取错误码对应的字符串描述
     *
     * @param code 错误码
     * @return const char* 错误码描述
     */
    const char* getErrorString(ErrorCode code) override;

    /**
     * @brief 获取系统状态对应的字符串描述
     *
     * @param state 系统状态
     * @return const char* 系统状态描述
     */
    const char* getSystemStateString(SystemState state) override;

    // 静态方法保持向后兼容性
    static void beginStatic(Level level = Level::NOTICE, bool logToSerial = true, unsigned long serialBaud = 115200);
    static void setLevelStatic(Level level);
    static Level getLevelStatic();
    static ErrorCode logErrorStatic(ErrorCode code, const char* module, const char* message, SystemState severity = SystemState::ERROR);
    static const ErrorInfo& getLastErrorStatic();
    static void clearLastErrorStatic();
    static const char* getErrorStringStatic(ErrorCode code);
    static const char* getSystemStateStringStatic(SystemState state);

private:
    Level currentLevel;
    bool initialized;
    ErrorInfo lastError;

    // 静态成员变量（用于向后兼容）
    static Level staticCurrentLevel;
    static bool staticInitialized;
    static ErrorInfo staticLastError;

    /**
     * @brief 日志前缀回调函数
     */
    static void logPrefix(Print* _logOutput, int level);
};

#endif // LOG_MANAGER_H 