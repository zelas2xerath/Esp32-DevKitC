#include <lib.h>
#include <ControlAgent.h>
#include <LogManager/LogManager.h>
#include <SystemManager/RTOSSystemManager.h>

// 常量定义
constexpr unsigned long WARNING_TIMEOUT = 60000;  // 警告状态超时时间（毫秒）
constexpr unsigned long ERROR_TIMEOUT = 300000;   // 错误状态超时时间（毫秒）

/**
 * 构造函数
 */
ControlAgent::ControlAgent(
    const uint8_t pin,
    bool& forceModeRef,
    int& lowThresholdRef,
    int& highThresholdRef,
    Preferences& prefsRef
) : controlPin(pin),
    forceMode(forceModeRef),
    lowThreshold(lowThresholdRef),
    highThreshold(highThresholdRef),
    prefs(prefsRef),
    currentState(SystemState::NORMAL),
    previousState(SystemState::NORMAL),
    lastErrorCode(ErrorCode::SUCCESS),
    lastErrorMessage(""),
    lastErrorTime(0),
    stateChangeTime(0),
    warningTimeout(WARNING_TIMEOUT),
    errorTimeout(ERROR_TIMEOUT),
    irrigationEnabled(true),
    historyIndex(0),
    historyCount(0) {
    // 初始化状态历史记录
    for (auto& rec : stateHistory) {
        rec = {SystemState::NORMAL, SystemState::NORMAL, ErrorCode::SUCCESS, 0};
    }
}

/**
 * 初始化控制代理
 */
ErrorCode ControlAgent::begin() {
    // 初始化控制输出引脚
    pinMode(controlPin, OUTPUT);
    digitalWrite(controlPin, LOW);
    LOG_TRACE("控制引脚初始化完成");

    // 加载历史配置参数
    prefs.begin("soil_cfg", false);
    ErrorCode result = loadThresholds();
    prefs.end();
    
    if (result != ErrorCode::SUCCESS) {
        return result;
    }
    
    LOG_TRACE("湿度阈值：低=%d, 高=%d", lowThreshold, highThreshold);
    
    // 初始化状态机
    currentState = SystemState::NORMAL;
    stateChangeTime = millis();
    irrigationEnabled = true;
    
    return ErrorCode::SUCCESS;
}

/**
 * 根据当前湿度执行自动控制
 */
bool ControlAgent::autoControl(int moisture) const {
    // 只有在非强制模式且灌溉启用时执行自动控制
    if (forceMode || !irrigationEnabled) {
        return false;
    }

    int prevPinState = digitalRead(controlPin);
    
    if (moisture < lowThreshold) {
        digitalWrite(controlPin, HIGH);
        LOG_NOTICE("湿度过低，启动灌溉");
    } else if (moisture > highThreshold) {
        digitalWrite(controlPin, LOW);
        LOG_NOTICE("湿度正常，停止灌溉");
    }
    
    return prevPinState != digitalRead(controlPin);
}

/**
 * 执行强制控制
 */
ErrorCode ControlAgent::forceControl(bool state) {
    // 检查系统状态，如果是ERROR或CRITICAL状态，则不允许强制控制
    if (currentState == SystemState::ERROR || currentState == SystemState::CRITICAL) {
        String errorMsg = "系统当前处于" + String(LogManager::getSystemStateStringStatic(currentState)) + "状态，不允许强制控制";
        LOG_ERROR("ControlAgent: %s", errorMsg.c_str());
        return ErrorCode::FORCE_MODE_ERROR;
    }
    
    forceMode = true;
    int prevPinState = digitalRead(controlPin);
    digitalWrite(controlPin, state ? HIGH : LOW);
    
    // 记录状态变化
    if (prevPinState != digitalRead(controlPin)) {
        LOG_NOTICE("强制控制: %s (状态已改变)", state ? "开启" : "关闭");
    } else {
        LOG_NOTICE("强制控制: %s (状态未变)", state ? "开启" : "关闭");
    }
    
    return ErrorCode::SUCCESS;
}

/**
 * 退出强制控制模式
 */
ErrorCode ControlAgent::exitForceMode() {
    if (!forceMode) {
        LOG_TRACE("已经不在强制控制模式，无需退出");
        return ErrorCode::SUCCESS;
    }
    
    forceMode = false;
    LOG_NOTICE("已退出强制控制模式");
    return ErrorCode::SUCCESS;
}

/**
 * 设置湿度阈值
 */
ErrorCode ControlAgent::setThresholds(int low, int high) {
    // 检查阈值有效性
    if (low >= high || low < 0 || low > 100 || high < 0 || high > 100) {
        LOG_ERROR("ControlAgent: 阈值设置无效，低阈值必须小于高阈值且在0-100范围内");
        return ErrorCode::THRESHOLD_INVALID;
    }
    
    // 保存阈值
    lowThreshold = low;
    highThreshold = high;
    
    // 触发事件 - 使用RTOS系统管理器（生命周期管理迁移）
    if (rtosSystemManager) {
        rtosSystemManager->postEvent(Event::ThresholdChanged, low, high, "阈值已更新");
    }
    
    return ErrorCode::SUCCESS;
}

/**
 * 保存阈值到Flash
 */
ErrorCode ControlAgent::saveThresholds(int low, int high) {
    prefs.begin("soil_cfg", false);
    
    if (!prefs.putInt("low", low) || !prefs.putInt("high", high)) {
        prefs.end();
        LOG_ERROR("ControlAgent: 阈值保存到Flash失败");
        return ErrorCode::STORAGE_WRITE_ERROR;
    }
    
    prefs.end();
    LOG_TRACE("阈值已保存到Flash");
    return ErrorCode::SUCCESS;
}

/**
 * 加载阈值从Flash
 */
ErrorCode ControlAgent::loadThresholds() {
    if (prefs.isKey("low") && prefs.isKey("high")) {
        lowThreshold = prefs.getInt("low");
        highThreshold = prefs.getInt("high");
        LOG_TRACE("加载历史阈值: 低=%d, 高=%d", lowThreshold, highThreshold);
        
        // 验证加载的阈值
        if (lowThreshold >= highThreshold || lowThreshold < 0 || lowThreshold > 100 || 
            highThreshold < 0 || highThreshold > 100) {
            LOG_WARNING("加载的阈值无效，使用默认值");
            lowThreshold = 20;
            highThreshold = 30;
            LOG_ERROR("ControlAgent: 加载的阈值无效");
            return ErrorCode::CONFIG_INVALID;
        }
        
        return ErrorCode::SUCCESS;
    }
    LOG_WARNING("未找到历史配置，使用默认阈值: 20/30");
    lowThreshold = 20;
    highThreshold = 30;
    LOG_ERROR("ControlAgent: 未找到历史配置");
    return ErrorCode::CONFIG_LOAD_ERROR;
}

/**
 * 获取当前控制状态
 */
int ControlAgent::getControlState() const {
    return digitalRead(controlPin);
}

/**
 * 获取当前控制模式
 */
bool ControlAgent::isForceMode() const {
    return forceMode;
}

/**
 * 获取控制模式描述字符串
 */
String ControlAgent::getModeString() const {
    if (forceMode) {
        return digitalRead(controlPin) == HIGH ? "强制开" : "强制关";
    }
    return "自动";
}

/**
 * 获取当前系统状态
 */
SystemState ControlAgent::getSystemState() const {
    return currentState;
}

/**
 * 设置系统状态
 */
ErrorCode ControlAgent::setSystemState(SystemState state, ErrorCode errorCode, const String& errorMessage) {
    // 如果状态没有变化，直接返回
    if (state == currentState) {
        LOG_TRACE("系统状态未变化，维持在: %s", LogManager::getSystemStateStringStatic(currentState));
        return ErrorCode::SUCCESS;
    }
    
    // 保存旧状态
    SystemState oldState = currentState;
    
    // 记录状态变化时间
    stateChangeTime = millis();
    
    // 根据新状态执行相应操作
    switch (state) {
        case SystemState::NORMAL:
            LOG_NOTICE("系统状态变更为: 正常");
            // 清除错误信息
            lastErrorCode = ErrorCode::SUCCESS;
            lastErrorMessage = "";
            lastErrorTime = 0;
            // 恢复灌溉功能
            irrigationEnabled = true;
            break;
            
        case SystemState::WARNING:
            LOG_WARNING("系统状态变更为: 警告");
            // 记录警告信息
            if (errorCode != ErrorCode::SUCCESS) {
                lastErrorCode = errorCode;
                lastErrorMessage = errorMessage;
                lastErrorTime = millis();
            }
            break;
            
        case SystemState::ERROR:
            LOG_ERROR("系统状态变更为: 错误");
            // 记录错误信息
            if (errorCode != ErrorCode::SUCCESS) {
                lastErrorCode = errorCode;
                lastErrorMessage = errorMessage;
                lastErrorTime = millis();
            }
            // 停止灌溉功能
            irrigationEnabled = false;
            digitalWrite(controlPin, LOW);
            break;
            
        case SystemState::CRITICAL:
            LOG_FATAL("系统状态变更为: 严重错误");
            // 记录错误信息
            if (errorCode != ErrorCode::SUCCESS) {
                lastErrorCode = errorCode;
                lastErrorMessage = errorMessage;
                lastErrorTime = millis();
            }
            // 停止灌溉功能
            irrigationEnabled = false;
            digitalWrite(controlPin, LOW);
            // 强制退出强制模式
            forceMode = false;
            break;
    }
    
    // 更新历史状态
    previousState = oldState;
    
    // 更新当前状态
    currentState = state;
    
    // 记录状态变化历史
    recordStateChange(oldState, state, errorCode);
    
    // 通知RTOS系统管理器状态变更（生命周期管理迁移）
    if (rtosSystemManager) {
        rtosSystemManager->setSystemState(state, errorCode, errorMessage.c_str());
    }
    
    return ErrorCode::SUCCESS;
}

/**
 * 报告错误
 */
ErrorCode ControlAgent::reportError(ErrorCode errorCode, const String& errorMessage, SystemState severity) {
    // 记录错误信息
    lastErrorCode = errorCode;
    lastErrorMessage = errorMessage;
    lastErrorTime = millis();
    
    // 根据严重程度设置系统状态
    setSystemState(severity, errorCode, errorMessage);
    
    // 记录错误日志
    switch (severity) {
        case SystemState::WARNING:
            LOG_WARNING("ControlAgent: %s (错误码: %d)", errorMessage.c_str(), static_cast<int>(errorCode));
            break;
        case SystemState::ERROR:
            LOG_ERROR("ControlAgent: %s (错误码: %d)", errorMessage.c_str(), static_cast<int>(errorCode));
            break;
        case SystemState::CRITICAL:
            LOG_FATAL("ControlAgent: %s (错误码: %d)", errorMessage.c_str(), static_cast<int>(errorCode));
            break;
        default:
            LOG_NOTICE("ControlAgent: %s (错误码: %d)", errorMessage.c_str(), static_cast<int>(errorCode));
            break;
    }
    
    return errorCode;
}

/**
 * 清除错误状态
 */
ErrorCode ControlAgent::clearError() {
    // 清除错误信息
    lastErrorCode = ErrorCode::SUCCESS;
    lastErrorMessage = "";
    lastErrorTime = 0;
    
    // 恢复正常状态
    setSystemState(SystemState::NORMAL);
    
    return ErrorCode::SUCCESS;
}

/**
 * 获取最后一次错误码
 */
ErrorCode ControlAgent::getLastErrorCode() const {
    return lastErrorCode;
}

/**
 * 获取最后一次错误信息
 */
String ControlAgent::getLastErrorMessage() const {
    return lastErrorMessage;
}

/**
 * 设置灌溉状态
 */
ErrorCode ControlAgent::setIrrigationEnabled(bool enabled) {
    // 如果系统处于错误或严重错误状态，不允许启用灌溉
    if (enabled && (currentState == SystemState::ERROR || currentState == SystemState::CRITICAL)) {
        String errorMsg = "系统当前处于" + String(LogManager::getSystemStateStringStatic(currentState)) + "状态，不允许启用灌溉";
        LOG_ERROR("ControlAgent: %s", errorMsg.c_str());
        return ErrorCode::OPERATION_FAILED;
    }
    
    irrigationEnabled = enabled;
    
    // 如果禁用灌溉，关闭控制输出
    if (!enabled) {
        digitalWrite(controlPin, LOW);
    }
    
    LOG_NOTICE("灌溉功能: %s", enabled ? "启用" : "禁用");
    
    return ErrorCode::SUCCESS;
}

/**
 * 获取灌溉状态
 */
bool ControlAgent::isIrrigationEnabled() const {
    return irrigationEnabled;
}

/**
 * 处理状态机逻辑
 */
void ControlAgent::updateStateMachine() {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - stateChangeTime;
    
    // 根据当前状态执行相应逻辑
    switch (currentState) {
        case SystemState::WARNING:
            // 警告状态超时后自动恢复到正常状态
            if (elapsedTime > warningTimeout) {
                LOG_NOTICE("警告状态超时，自动恢复到正常状态");
                setSystemState(SystemState::NORMAL);
            }
            break;
            
        case SystemState::ERROR:
            // 错误状态超时后自动降级到警告状态
            if (elapsedTime > errorTimeout) {
                LOG_NOTICE("错误状态超时，自动降级到警告状态");
                setSystemState(SystemState::WARNING);
            }
            break;
            
        case SystemState::CRITICAL:
            // 严重错误状态不会自动恢复，需要手动清除
            // break;
        default:
            // 正常状态不需要特殊处理
            break;
    }
} 

/**
 * 记录状态变化
 */
void ControlAgent::recordStateChange(SystemState oldState, SystemState newState, ErrorCode code) {
    // 更新历史记录
    stateHistory[historyIndex] = {
        oldState,
        newState,
        code,
        millis()
    };
    
    // 更新索引和计数
    historyIndex = (historyIndex + 1) % MAX_STATE_HISTORY;
    if (historyCount < MAX_STATE_HISTORY) {
        historyCount++;
    }
}

/**
 * 获取状态持续时间
 */
unsigned long ControlAgent::getStateDuration() const {
    return millis() - stateChangeTime;
}

/**
 * 获取前一个状态
 */
SystemState ControlAgent::getPreviousState() const {
    return previousState;
}

/**
 * 获取状态变化历史
 */
StateChangeRecord ControlAgent::getStateHistory(uint8_t index) const {
    // 确保索引有效
    if (index >= historyCount) {
        // 返回空记录
        return {SystemState::NORMAL, SystemState::NORMAL, ErrorCode::SUCCESS, 0};
    }
    
    // 计算实际索引（最新记录在数组中的位置）
    uint8_t actualIndex = (historyIndex + MAX_STATE_HISTORY - 1 - index) % MAX_STATE_HISTORY;
    return stateHistory[actualIndex];
}

/**
 * 获取状态历史记录数量
 */
uint8_t ControlAgent::getStateHistoryCount() const {
    return historyCount;
}

/**
 * 获取状态变化历史的JSON格式
 */
String ControlAgent::getStateHistoryJson(uint8_t maxEntries) const {
    String json = "[";
    
    // 限制条目数量
    uint8_t entries = min(historyCount, maxEntries);
    
    for (uint8_t i = 0; i < entries; i++) {
        if (i > 0) {
            json += ",";
        }
        
        StateChangeRecord record = getStateHistory(i);
        json += "{";
        json += R"("from":")" + String(LogManager::getSystemStateStringStatic(record.previousState)) + "\",";
        json += R"("to":")" + String(LogManager::getSystemStateStringStatic(record.newState)) + "\",";
        json += "\"code\":" + String(static_cast<int>(record.errorCode)) + ",";
        json += "\"time\":" + String(record.timestamp);
        json += "}";
    }
    
    json += "]";
    return json;
}

// ==================== 接口方法实现 ====================

/**
 * 获取当前系统状态（接口方法）
 */
SystemState ControlAgent::getCurrentState() const {
    return currentState;
}

/**
 * 设置系统状态（接口方法）
 */
ErrorCode ControlAgent::setSystemState(SystemState state, ErrorCode errorCode) {
    return setSystemState(state, errorCode, "");
}

/**
 * 获取控制输出状态
 */
bool ControlAgent::getControlOutputState() const {
    return digitalRead(controlPin) == HIGH;
}

/**
 * 检查是否处于强制模式
 */
bool ControlAgent::isInForceMode() const {
    return forceMode;
}

/**
 * 获取湿度阈值
 */
void ControlAgent::getThresholds(int& lowThreshold, int& highThreshold) const {
    lowThreshold = this->lowThreshold;
    highThreshold = this->highThreshold;
}

/**
 * 获取状态变化历史的JSON格式（带默认参数的便利方法）
 */
String ControlAgent::getStateHistoryJsonDefault() const {
    return getStateHistoryJson(MAX_STATE_HISTORY);
}

