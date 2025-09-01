#include <LogManager.h>
#include <cstdarg>

// 全局LogManager实例指针（用于宏定义）
LogManager* globalLogManager = nullptr;

// 初始化静态成员变量（用于向后兼容）
LogManager::Level LogManager::staticCurrentLevel = Level::NOTICE;
bool LogManager::staticInitialized = false;
ErrorInfo LogManager::staticLastError = {ErrorCode::SUCCESS, "", "", 0, SystemState::NORMAL};

/**
 * @brief LogManager构造函数
 */
LogManager::LogManager()
    : currentLevel(Level::NOTICE),
      initialized(false),
      lastError{ErrorCode::SUCCESS, "", "", 0, SystemState::NORMAL} {
}

/**
 * @brief 初始化日志管理器（实例方法）
 */
void LogManager::begin(Level level, bool logToSerial, unsigned long serialBaud) {
    // 避免重复初始化
    if (initialized) {
        return;
    }

    currentLevel = level;

    // 如果需要输出到串口，且串口未初始化，则初始化串口
    if (logToSerial && !Serial) {
        Serial.begin(serialBaud);
        delay(100); // 给串口一些初始化时间
    }

    // 设置日志输出目标和级别
    Log.begin(static_cast<int>(level), &Serial);

    // 配置日志前缀回调函数
    Log.setPrefix(logPrefix);

    // 配置显示日志级别
    Log.setSuffix([](Print* _logOutput, int /*level*/) {
        _logOutput->print("\n");
    });

    initialized = true;

    // 输出初始化完成日志
    Log.notice(F("LogManager初始化完成，当前日志级别: %d" CR), static_cast<int>(level));
}

/**
 * @brief 初始化日志管理器（静态方法，向后兼容）
 */
void LogManager::beginStatic(Level level, bool logToSerial, unsigned long serialBaud) {
    // 避免重复初始化
    if (staticInitialized) {
        return;
    }

    staticCurrentLevel = level;

    // 如果需要输出到串口，且串口未初始化，则初始化串口
    if (logToSerial && !Serial) {
        Serial.begin(serialBaud);
        delay(100); // 给串口一些初始化时间
    }

    // 设置日志输出目标和级别
    Log.begin(static_cast<int>(level), &Serial);

    // 配置日志前缀回调函数
    Log.setPrefix(logPrefix);

    // 配置显示日志级别
    Log.setSuffix([](Print* _logOutput, int /*level*/) {
        _logOutput->print("\n");
    });

    staticInitialized = true;

    // 输出初始化完成日志
    Log.notice(F("LogManager初始化完成，当前日志级别: %d" CR), static_cast<int>(level));
}

/**
 * @brief 设置日志级别（实例方法）
 */
void LogManager::setLevel(Level level) {
    if (!initialized) {
        // 如果实例未初始化，使用静态方法
        setLevelStatic(level);
        begin(level);
        return;
    }

    currentLevel = level;
    Log.setLevel(static_cast<int>(level));
    Log.notice(F("日志级别已更新: %d" CR), static_cast<int>(level));
}

/**
 * @brief 设置日志级别（静态方法）
 */
void LogManager::setLevelStatic(Level level) {
    staticCurrentLevel = level;
    Log.setLevel(static_cast<int>(level));
}

/**
 * @brief 获取当前日志级别（实例方法）
 */
LogManager::Level LogManager::getLevel() {
    return currentLevel;
}

/**
 * @brief 获取当前日志级别（静态方法）
 */
LogManager::Level LogManager::getLevelStatic() {
    return staticCurrentLevel;
}

/**
 * @brief 致命级别日志（实例方法）
 */
void LogManager::fatal(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log.fatal(buffer);
}

/**
 * @brief 错误级别日志（实例方法）
 */
void LogManager::error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log.error(buffer);
}

/**
 * @brief 警告级别日志（实例方法）
 */
void LogManager::warning(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log.warning(buffer);
}

/**
 * @brief 提示级别日志（实例方法）
 */
void LogManager::notice(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log.notice(buffer);
}

/**
 * @brief 信息级别日志（实例方法）
 */
void LogManager::info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log.info(buffer);
}

/**
 * @brief 跟踪级别日志（实例方法）
 */
void LogManager::trace(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log.trace(buffer);
}

/**
 * @brief 详细级别日志（实例方法）
 */
void LogManager::verbose(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log.verbose(buffer);
}

// ==================== IErrorHandler接口实现 ====================

/**
 * @brief 记录错误信息
 */
ErrorCode LogManager::logError(ErrorCode code, const char* module, const char* message, SystemState state) {
    lastError = {code, module, message, millis(), state};
    LOG_ERROR("[%s] %s (错误码: %d, 状态: %d)", module, message, static_cast<int>(code), static_cast<int>(state));
    return code;
}

/**
 * @brief 获取最后一个错误信息
 */
const ErrorInfo& LogManager::getLastError() {
    return lastError;
}

/**
 * @brief 清除最后一个错误信息
 */
void LogManager::clearLastError() {
    lastError = {ErrorCode::SUCCESS, "", "", 0, SystemState::NORMAL};
}

/**
 * @brief 获取错误码对应的字符串
 */
const char* LogManager::getErrorString(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS: return "成功";
        case ErrorCode::UNKNOWN_ERROR: return "未知错误";
        case ErrorCode::INVALID_PARAMETER: return "无效参数";
        case ErrorCode::TIMEOUT: return "超时";
        case ErrorCode::INITIALIZATION_FAILED: return "初始化失败";
        case ErrorCode::SENSOR_READ_ERROR: return "传感器读取错误";
        case ErrorCode::SENSOR_DATA_VALIDATION_FAILED: return "传感器数据验证失败";
        case ErrorCode::SENSOR_INITIALIZATION_FAILED: return "传感器初始化失败";
        case ErrorCode::NETWORK_CONNECTION_ERROR: return "网络连接错误";
        case ErrorCode::MQTT_CONNECTION_ERROR: return "MQTT连接错误";
        case ErrorCode::WIFI_CONNECTION_ERROR: return "WiFi连接错误";
        case ErrorCode::MEMORY_ALLOCATION_FAILED: return "内存分配失败";
        case ErrorCode::SYSTEM_ERROR: return "系统错误";
        case ErrorCode::HARDWARE_ERROR: return "硬件错误";
        default: return "未定义错误";
    }
}

/**
 * @brief 获取系统状态对应的字符串
 */
const char* LogManager::getSystemStateString(SystemState state) {
    switch (state) {
        case SystemState::NORMAL: return "正常";
        case SystemState::WARNING: return "警告";
        case SystemState::ERROR: return "错误";
        case SystemState::CRITICAL: return "严重";
        default: return "未知";
    }
}

/**
 * @brief 获取系统状态对应的字符串（静态方法）
 */
const char* LogManager::getSystemStateStringStatic(SystemState state) {
    switch (state) {
        case SystemState::NORMAL: return "正常";
        case SystemState::WARNING: return "警告";
        case SystemState::ERROR: return "错误";
        case SystemState::CRITICAL: return "严重";
        default: return "未知";
    }
}

/**
 * @brief 自定义日志前缀回调函数
 *
 * 格式：[时间][级别]
 */
void LogManager::logPrefix(Print* _logOutput, int level) {
    // 获取当前运行时间
    unsigned long runtime = millis();
    unsigned int hours = runtime / 3600000;
    runtime %= 3600000;
    unsigned int minutes = runtime / 60000;
    runtime %= 60000;
    unsigned int seconds = runtime / 1000;
    unsigned int milliseconds = runtime % 1000;

    // 输出时间戳 [HH:MM:SS.mmm]
    _logOutput->print("[");
    if (hours < 10) _logOutput->print("0");
    _logOutput->print(hours);
    _logOutput->print(":");
    if (minutes < 10) _logOutput->print("0");
    _logOutput->print(minutes);
    _logOutput->print(":");
    if (seconds < 10) _logOutput->print("0");
    _logOutput->print(seconds);
    _logOutput->print(".");
    if (milliseconds < 100) _logOutput->print("0");
    if (milliseconds < 10) _logOutput->print("0");
    _logOutput->print(milliseconds);
    _logOutput->print("]");

    // 输出日志级别
    _logOutput->print("[");
    switch (level) {
        case LOG_LEVEL_FATAL:
            _logOutput->print("致命");
            break;
        case LOG_LEVEL_ERROR:
            _logOutput->print("错误");
            break;
        case LOG_LEVEL_WARNING:
            _logOutput->print("警告");
            break;
        case LOG_LEVEL_NOTICE:
            _logOutput->print("提示");
            break;
        case LOG_LEVEL_TRACE:
            _logOutput->print("跟踪");
            break;
        case LOG_LEVEL_VERBOSE:
            _logOutput->print("详细");
            break;
        default:
            _logOutput->print("未知");
            break;
    }
    _logOutput->print("] ");
}
