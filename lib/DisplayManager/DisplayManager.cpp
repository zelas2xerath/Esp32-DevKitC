#include <DisplayManager.h>
#include <LogManager/LogManager.h>

DisplayManager* displayManager = nullptr;

/**
 * 构造函数
 */
DisplayManager::DisplayManager(U8G2_SSD1306_128X64_NONAME_F_HW_I2C& oledRef, uint8_t clock, uint8_t data) 
    : oled(oledRef), clockPin(clock), dataPin(data), _lastErrorCode(ErrorCode::SUCCESS), _lastErrorMessage(""), _lastErrorTime(0) {
    // 初始化lastLines数组
    for (auto & lastLine : lastLines) {
        lastLine = "";
    }
}

/**
 * 初始化显示屏
 */
ErrorCode DisplayManager::begin() {
    // 初始化OLED显示屏
    oled.begin();
    oled.setFont(u8g2_font_wqy12_t_gb2312);
    clear();
    LOG_TRACE("DisplayManager: 显示屏初始化完成");
    return ErrorCode::SUCCESS;
}

/**
 * 显示1行文本
 */
ErrorCode DisplayManager::showText(const String& line1) {
    // 检查内容是否变化，避免频繁刷新显示
    if (line1 == lastLines[0] && lastLines[1].isEmpty() && lastLines[2].isEmpty() && lastLines[3].isEmpty()) {
        return ErrorCode::SUCCESS;
    }
    
    // 更新缓存
    lastLines[0] = line1;
    lastLines[1] = "";
    lastLines[2] = "";
    lastLines[3] = "";
    
    // 清空显示缓冲区
    oled.clearBuffer();
    
    // 设置字体
    oled.setFont(u8g2_font_wqy12_t_gb2312);
    
    // 显示文本
    oled.drawUTF8(0, 16, line1.c_str());
    
    // 发送显示缓冲区到屏幕
    oled.sendBuffer();
    LOG_VERBOSE("DisplayManager: 显示文本 [%s]", line1.c_str());
    return ErrorCode::SUCCESS;
}

/**
 * 显示2行文本
 */
ErrorCode DisplayManager::showText(const String& line1, const String& line2) {
    // 检查内容是否变化，避免频繁刷新显示
    if (line1 == lastLines[0] && line2 == lastLines[1] && 
        lastLines[2].isEmpty() && lastLines[3].isEmpty()) {
        return ErrorCode::SUCCESS;
    }
    
    // 更新缓存
    lastLines[0] = line1;
    lastLines[1] = line2;
    lastLines[2] = "";
    lastLines[3] = "";
    
    // 清空显示缓冲区
    oled.clearBuffer();
    
    // 设置字体
    oled.setFont(u8g2_font_wqy12_t_gb2312);
    
    // 显示文本
    oled.drawUTF8(0, 16, line1.c_str());
    oled.drawUTF8(0, 32, line2.c_str());
    
    // 发送显示缓冲区到屏幕
    oled.sendBuffer();
    LOG_VERBOSE("DisplayManager: 显示文本 [%s] [%s]", line1.c_str(), line2.c_str());
    return ErrorCode::SUCCESS;
}

/**
 * 显示3行文本
 */
ErrorCode DisplayManager::showText(const String& line1, const String& line2, const String& line3) {
    // 检查内容是否变化，避免频繁刷新显示
    if (line1 == lastLines[0] && line2 == lastLines[1] && 
        line3 == lastLines[2] && lastLines[3].isEmpty()) {
        return ErrorCode::SUCCESS;
    }
    
    // 更新缓存
    lastLines[0] = line1;
    lastLines[1] = line2;
    lastLines[2] = line3;
    lastLines[3] = "";
    
    // 清空显示缓冲区
    oled.clearBuffer();
    
    // 设置字体
    oled.setFont(u8g2_font_wqy12_t_gb2312);
    
    // 显示文本
    oled.drawUTF8(0, 16, line1.c_str());
    oled.drawUTF8(0, 32, line2.c_str());
    oled.drawUTF8(0, 48, line3.c_str());
    
    // 发送显示缓冲区到屏幕
    oled.sendBuffer();
    LOG_VERBOSE("DisplayManager: 显示文本 [%s] [%s] [%s]", line1.c_str(), line2.c_str(), line3.c_str());
    return ErrorCode::SUCCESS;
}

/**
 * 显示4行文本
 */
ErrorCode DisplayManager::showText(const String& line1, const String& line2, const String& line3, const String& line4) {
    // 检查内容是否变化，避免频繁刷新显示
    if (line1 == lastLines[0] && line2 == lastLines[1] && 
        line3 == lastLines[2] && line4 == lastLines[3]) {
        return ErrorCode::SUCCESS;
    }
    
    // 更新缓存
    lastLines[0] = line1;
    lastLines[1] = line2;
    lastLines[2] = line3;
    lastLines[3] = line4;
    
    // 清空显示缓冲区
    oled.clearBuffer();
    
    // 设置字体
    oled.setFont(u8g2_font_wqy12_t_gb2312);
    
    // 显示文本
    oled.drawUTF8(0, 16, line1.c_str());
    oled.drawUTF8(0, 32, line2.c_str());
    oled.drawUTF8(0, 48, line3.c_str());
    oled.drawUTF8(0, 64, line4.c_str());
    
    // 发送显示缓冲区到屏幕
    oled.sendBuffer();
    LOG_VERBOSE("DisplayManager: 显示文本 [%s] [%s] [%s] [%s]", 
                line1.c_str(), line2.c_str(), line3.c_str(), line4.c_str());
    return ErrorCode::SUCCESS;
}

/**
 * 清空显示屏
 */
ErrorCode DisplayManager::clear() {
    // 清空缓存
    for (auto & lastLine : lastLines) {
        lastLine = "";
    }
    
    // 清空显示
    oled.clearBuffer();
    oled.sendBuffer();
    LOG_VERBOSE("DisplayManager: 清空显示");
    return ErrorCode::SUCCESS;
}

/**
 * 显示进度条
 */
ErrorCode DisplayManager::showProgressBar(int progress, const String& title) {
    // 确保进度值在0-100%范围内
    progress = constrain(progress, 0, 100);
    
    // 清空显示缓冲区
    oled.clearBuffer();
    
    // 显示标题（如果有）
    if (title.length() > 0) {
        oled.setFont(u8g2_font_wqy12_t_gb2312);
        oled.drawUTF8(0, 16, title.c_str());
    }
    
    // 绘制进度条外框
    int startY = title.length() > 0 ? 24 : 16;
    oled.drawFrame(0, startY, 128, 12);
    
    // 绘制进度条填充
    int width = map(progress, 0, 100, 0, 124);
    oled.drawBox(2, startY + 2, width, 8);
    
    // 绘制进度百分比
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(55, startY + 30, (String(progress) + "%").c_str());
    
    // 发送显示缓冲区到屏幕
    oled.sendBuffer();
    LOG_VERBOSE("DisplayManager: 显示进度条 [%d%%] [%s]", progress, title.c_str());
    return ErrorCode::SUCCESS;
}

/**
 * 显示错误信息
 */
ErrorCode DisplayManager::showError(ErrorCode errorCode, const String& message) {
    // 记录错误信息
    reportError(errorCode, message);
    
    // 清空显示缓冲区
    oled.clearBuffer();
    
    // 设置字体
    oled.setFont(u8g2_font_wqy12_t_gb2312);
    
    // 显示错误标题
    oled.drawUTF8(0, 16, "错误");
    
    // 显示错误码
    String errorCodeStr = "错误码: " + String(static_cast<int>(errorCode));
    oled.drawUTF8(0, 32, errorCodeStr.c_str());
    
    // 显示错误信息（可能需要截断）
    String shortMessage = message;
    if (shortMessage.length() > 20) {
        shortMessage = shortMessage.substring(0, 17) + "...";
    }
    oled.drawUTF8(0, 48, shortMessage.c_str());
    
    // 发送显示缓冲区到屏幕
    oled.sendBuffer();
    
    LOG_ERROR("DisplayManager: 显示错误 [%d] %s", static_cast<int>(errorCode), message.c_str());
    return ErrorCode::SUCCESS;
}

/**
 * 显示系统状态
 */
ErrorCode DisplayManager::showSystemState(SystemState state) {
    // 清空显示缓冲区
    oled.clearBuffer();
    
    // 设置字体
    oled.setFont(u8g2_font_wqy12_t_gb2312);
    
    // 显示状态标题
    oled.drawUTF8(0, 16, "系统状态");
    
    // 获取状态文本
    const char* stateStr = LogManager::getSystemStateStringStatic(state);
    oled.drawUTF8(0, 32, stateStr);
    
    // 根据状态显示不同的图标或额外信息
    String stateInfo;
    switch (state) {
        case SystemState::NORMAL:
            stateInfo = "系统运行正常";
            break;
        case SystemState::WARNING:
            stateInfo = "请注意系统警告";
            break;
        case SystemState::ERROR:
            stateInfo = "系统出现错误";
            break;
        case SystemState::CRITICAL:
            stateInfo = "系统严重错误!";
            break;
    }
    oled.drawUTF8(0, 48, stateInfo.c_str());
    
    // 发送显示缓冲区到屏幕
    oled.sendBuffer();
    
    LOG_TRACE("DisplayManager: 显示系统状态 [%s]", stateStr);
    return ErrorCode::SUCCESS;
}

/**
 * 清除最后一次错误信息
 */
void DisplayManager::clearLastError() {
    _lastErrorCode = ErrorCode::SUCCESS;
    _lastErrorMessage = "";
    _lastErrorTime = 0;
    LOG_TRACE("DisplayManager: 清除错误信息");
}

/**
 * 报告错误
 */
ErrorCode DisplayManager::reportError(ErrorCode code, const String& message) {
    _lastErrorCode = code;
    _lastErrorMessage = message;
    _lastErrorTime = millis();
    
    LOG_ERROR("DisplayManager: 错误 [%d] %s", static_cast<int>(code), message.c_str());
    return code;
} 