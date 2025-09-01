#ifndef IDISPLAY_MANAGER_H
#define IDISPLAY_MANAGER_H

#include <Arduino.h>
#include <lib.h>

/**
 * @brief 显示管理器接口
 * 
 * 定义显示管理器的抽象接口，负责OLED显示屏的控制和内容显示
 */
class IDisplayManager {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~IDisplayManager() = default;

    /**
     * @brief 初始化显示屏
     * 
     * @return ErrorCode 错误码
     */
    virtual ErrorCode begin() = 0;
    
    /**
     * @brief 显示1行文本
     * 
     * @param line1 第1行文本
     * @return ErrorCode 错误码
     */
    virtual ErrorCode showText(const String& line1) = 0;
    
    /**
     * @brief 显示2行文本
     * 
     * @param line1 第1行文本
     * @param line2 第2行文本
     * @return ErrorCode 错误码
     */
    virtual ErrorCode showText(const String& line1, const String& line2) = 0;
    
    /**
     * @brief 显示3行文本
     * 
     * @param line1 第1行文本
     * @param line2 第2行文本
     * @param line3 第3行文本
     * @return ErrorCode 错误码
     */
    virtual ErrorCode showText(const String& line1, const String& line2, const String& line3) = 0;
    
    /**
     * @brief 显示4行文本
     * 
     * @param line1 第1行文本
     * @param line2 第2行文本
     * @param line3 第3行文本
     * @param line4 第4行文本
     * @return ErrorCode 错误码
     */
    virtual ErrorCode showText(const String& line1, const String& line2, const String& line3, const String& line4) = 0;
    
    /**
     * @brief 清空显示屏
     * 
     * @return ErrorCode 错误码
     */
    virtual ErrorCode clear() = 0;
    
    /**
     * @brief 显示进度条
     * 
     * @param progress 进度百分比 (0-100)
     * @param title 进度条标题
     * @return ErrorCode 错误码
     */
    virtual ErrorCode showProgressBar(int progress, const String& title = "") = 0;
    
    /**
     * @brief 显示错误信息
     * 
     * @param errorCode 错误码
     * @param message 错误信息
     * @return ErrorCode 错误码
     */
    virtual ErrorCode showError(ErrorCode errorCode, const String& message) = 0;
    
    /**
     * @brief 显示系统状态
     * 
     * @param state 系统状态
     * @return ErrorCode 错误码
     */
    virtual ErrorCode showSystemState(SystemState state) = 0;
    
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
     * @brief 清除最后一次错误信息
     */
    virtual void clearLastError() = 0;
};

#endif // IDISPLAY_MANAGER_H
