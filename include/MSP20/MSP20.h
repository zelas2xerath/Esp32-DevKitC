/**
 * @file MSP20.h
 * @brief MSP20水深压力传感器驱动库
 * @author zelas2xerath
 * @date 2025-08-21
 * @version 2.1.0
 *
 * MSP20水深压力传感器精简驱动库，提供高精度水深测量功能
 * 专注核心功能，简洁高效的实现
 */

#ifndef MSP20_H
#define MSP20_H

#include <Arduino.h>

/**
 * @brief MSP20水深压力传感器驱动类
 *
 * 精简的MSP20传感器驱动，专注核心功能：
 * - 高精度水深测量
 * - 压力计算
 * - 基本校准功能
 *
 * 技术规格：
 * - 测量范围：0-500cm水深
 * - 精度：±0.1%FS
 * - 工作电压：5V
 * - ADC分辨率：12位(0-4095)
 * - 公式：水深 = 电压 * 63.1 + 5.9739
 * - 公式：压力 = 电压 * 6.2672 - 0.44178
 */
class MSP20 {
    // ==================== 硬件常量 ====================
    static constexpr double WORKING_VOLTAGE = 5.0;           // 工作电压(V)
    static constexpr uint16_t ADC_RESOLUTION = 4095;         // ADC分辨率(12位)

    // ==================== 公式常量 ====================
    static constexpr double DEPTH_SLOPE = 63.1;              // 水深公式斜率
    static constexpr double DEPTH_OFFSET = 5.9739;           // 水深公式偏移
    static constexpr double PRESSURE_SLOPE = 6.2672;         // 压力公式斜率
    static constexpr double PRESSURE_OFFSET = -0.44178;      // 压力公式偏移

    // ==================== 验证范围 ====================
    static constexpr double MIN_VALID_VOLTAGE = 0.0;         // 最小有效电压(V)
    static constexpr double MAX_VALID_VOLTAGE = 5.0;         // 最大有效电压(V)
    static constexpr double MIN_VALID_DEPTH = 0.0;           // 最小有效水深(cm)
    static constexpr double MAX_VALID_DEPTH = 500.0;         // 最大有效水深(cm)
    static constexpr uint8_t MIN_SAMPLES = 3;                // 最小采样次数
    static constexpr uint8_t MAX_SAMPLES = 100;              // 最大采样次数

public:
    // ==================== 构造与析构 ====================

    /**
     * @brief 构造函数
     * @param pin ADC引脚编号
     * @param samplesCount 采样次数(3-100)
     */
    explicit MSP20(uint8_t pin, uint8_t samplesCount = 20);

    /**
     * @brief 析构函数
     */
    ~MSP20() = default;

    // ==================== 初始化与配置 ====================

    /**
     * @brief 初始化传感器
     * @return true 初始化成功，false 初始化失败
     */
    bool begin();

    // ==================== 数据读取接口 ====================

    /**
     * @brief 读取电压值
     * @return 电压值(V)，失败返回0.0
     */
    double readVoltage();

    /**
     * @brief 读取压力值
     * @return 压力值(kPa)，失败返回0.0
     */
    double readPressure();

    /**
     * @brief 读取水深值
     * @return 水深值(cm)，失败返回0.0
     */
    double readWaterDepth();

    /**
     * @brief 读取水深值(米)
     * @return 水深值(m)，失败返回0.0
     */
    double readWaterDepthMeters();

    // ==================== 校准功能 ====================

    /**
     * @brief 手动校准传感器
     * @param zeroOffset 零点偏移值(cm)
     * @param scaleFactor 比例因子
     * @return true 校准成功，false 参数无效
     */
    bool calibrate(double zeroOffset, double scaleFactor = 1.0);

private:
    // ==================== 私有成员变量 ====================

    uint8_t _pin;                    // ADC引脚编号
    uint8_t _samplesCount;           // 采样次数
    bool _isInitialized;             // 初始化状态
    double _zeroOffset;              // 零点偏移(cm)
    double _scaleFactor;             // 比例因子

    // ==================== 私有方法 ====================

    /**
     * @brief ADC值转换为电压
     * @param adcValue ADC原始值
     * @return 电压值(V)
     */
    double adcToVoltage(uint16_t adcValue) const;

    /**
     * @brief 电压转换为压力
     * @param voltage 电压值(V)
     * @return 压力值(kPa)
     */
    double voltageToPressure(double voltage) const;

    /**
     * @brief 电压转换为水深
     * @param voltage 电压值(V)
     * @return 水深值(cm)
     */
    double voltageToDepth(double voltage) const;

    /**
     * @brief 读取稳定的ADC平均值
     * @return 稳定的ADC平均值
     */
    double readStableADC() const;

    /**
     * @brief 验证读数有效性
     * @param value 待验证的数值
     * @param minValue 最小有效值
     * @param maxValue 最大有效值
     * @return true 数值有效，false 数值无效
     */
    bool validateReading(double value, double minValue, double maxValue) const;

    /**
     * @brief 应用校准参数
     * @param rawValue 原始测量值
     * @return 校准后的值
     */
    double applyCalibration(double rawValue) const;
};

#endif // MSP20_H
