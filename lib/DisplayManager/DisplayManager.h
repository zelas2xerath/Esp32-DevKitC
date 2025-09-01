#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <lib.h>
#include <interfaces/IDisplayManager.h>


class DisplayManager final : public IDisplayManager {
    // OLED显示对象
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C& oled;
    
    // 缓存上一次显示的内容（最多4行）
    String lastLines[4];
    
    // I2C引脚
    uint8_t clockPin;
    uint8_t dataPin;
    
    // 错误信息
    ErrorCode _lastErrorCode;
    String _lastErrorMessage;
    unsigned long _lastErrorTime;

public:
    /**
     * 构造函数
     * @param oledRef OLED显示对象引用
     * @param clock I2C时钟引脚
     * @param data I2C数据引脚
     */
    explicit DisplayManager(U8G2_SSD1306_128X64_NONAME_F_HW_I2C& oledRef, uint8_t clock = 21, uint8_t data = 19);
    
    /**
     * 初始化显示屏
     * @return 错误码
     */
    ErrorCode begin() override;

    /**
     * 显示1行文本
     * @param line1 第1行文本
     * @return 错误码
     */
    ErrorCode showText(const String& line1) override;

    /**
     * 显示2行文本
     * @param line1 第1行文本
     * @param line2 第2行文本
     * @return 错误码
     */
    ErrorCode showText(const String& line1, const String& line2) override;

    /**
     * 显示3行文本
     * @param line1 第1行文本
     * @param line2 第2行文本
     * @param line3 第3行文本
     * @return 错误码
     */
    ErrorCode showText(const String& line1, const String& line2, const String& line3) override;

    /**
     * 显示4行文本
     * @param line1 第1行文本
     * @param line2 第2行文本
     * @param line3 第3行文本
     * @param line4 第4行文本
     * @return 错误码
     */
    ErrorCode showText(const String& line1, const String& line2, const String& line3, const String& line4) override;
    
    /**
     * 清空显示屏
     * @return 错误码
     */
    ErrorCode clear() override;

    /**
     * 显示进度条
     * @param progress 进度百分比 (0-100)
     * @param title 进度条标题
     * @return 错误码
     */
    ErrorCode showProgressBar(int progress, const String& title = "") override;

    /**
     * 显示错误信息
     * @param errorCode 错误码
     * @param message 错误信息
     * @return 错误码
     */
    ErrorCode showError(ErrorCode errorCode, const String& message) override;

    /**
     * 显示系统状态
     * @param state 系统状态
     * @return 错误码
     */
    ErrorCode showSystemState(SystemState state) override;

    /**
     * 获取最后一次错误码
     * @return 错误码
     */
    ErrorCode getLastErrorCode() const override { return _lastErrorCode; }

    /**
     * 获取最后一次错误信息
     * @return 错误信息
     */
    String getLastErrorMessage() const override { return _lastErrorMessage; }

    /**
     * 清除最后一次错误信息
     */
    void clearLastError() override;
    
private:
    /**
     * 报告错误
     * @param code 错误码
     * @param message 错误信息
     * @return 错误码
     */
    ErrorCode reportError(ErrorCode code, const String& message);
};

// 注意：DisplayManager对象由SystemManager管理，不在此处声明全局变量

// ==================== 全局显示函数 ====================
// 注意：全局显示函数已移除，因为DisplayManager现在由SystemManager管理
// 如需显示文本，请通过SystemManager的getDisplayManager()方法访问

#endif // DISPLAY_MANAGER_H 