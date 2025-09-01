# MSP20水深压力传感器模块 v2.0

## 📋 模块概述

MSP20是一个专为ESP32设计的高精度水深压力传感器驱动模块。该模块基于MSP20压力传感器，提供完整的水深测量解决方案，支持多种测量模式、自动校准和健康状态监测。

### 🌟 主要特性

- **🎯 高精度测量**: ±0.1%FS精度，支持0-500cm水深测量
- **⚡ 多种测量模式**: 单次、连续、低功耗、高精度四种模式
- **🔧 智能校准**: 支持手动校准和自动校准功能
- **🛡️ 健康监测**: 实时传感器状态监测和异常检测
- **📊 数据滤波**: 内置异常值检测和数据稳定性处理
- **💾 状态缓存**: 保存最后有效读数和统计信息
- **🔄 RTOS兼容**: 完全兼容FreeRTOS多任务环境

## 🏗️ 技术规格

| 参数     | 规格             |
|--------|----------------|
| 测量范围   | 0-500cm (0-5m) |
| 测量精度   | ±0.1%FS        |
| 工作电压   | 5V             |
| 工作温度   | -40°C ~ +85°C  |
| ADC分辨率 | 12位 (0-4095)   |
| 输出信号   | 模拟电压 (0-5V)    |
| 响应时间   | <100ms         |
| 采样频率   | 可配置 (3-100次采样) |

## 📐 计算公式

MSP20传感器使用以下经过校准的计算公式：

### 电压转换
```
电压(V) = (ADC值 / 4095) × 5.0
```

### 压力计算
```
压力(kPa) = 电压(V) × 6.2672 - 0.44178
```

### 水深计算
```
水深(cm) = 电压(V) × 63.1 + 5.9739
```

### 示例计算
当ADC值为2048时：
- 电压 = (2048 / 4095) × 5.0 ≈ 2.5V
- 压力 = 2.5 × 6.2672 - 0.44178 ≈ 15.23 kPa
- 水深 = 2.5 × 63.1 + 5.9739 ≈ 163.72 cm

## 📦 安装和配置

### 1. 硬件连接

```
ESP32 Pin  →  MSP20传感器
GPIO35     →  模拟输出 (推荐使用ADC1通道)
3.3V       →  VCC
GND        →  GND
```

### 2. 软件集成

在你的项目中包含MSP20模块：

```cpp
#include <MSP20/MSP20.h>

// 创建MSP20实例（精简版本）
MSP20 waterSensor(35, 20);  // 使用GPIO35，20次采样
```

### 3. 构建配置

确保在`platformio.ini`中包含MSP20模块路径：

```ini
build_flags =
    -I "include/MSP20"
build_src_filter =
    +<../include/MSP20/>
```

## 🔄 v2.1.0 重构说明

**重构目标：** 精简代码，消除过度设计，专注核心功能

**删除的组件：**
- `MSP20Config.h` - 过度设计的配置系统
- `MSP20Calculator.h/cpp` - 不必要的计算逻辑分离
- 复杂的状态管理和健康检查系统
- 多种测量模式（保留基本功能）

**保留的核心功能：**
- 正确的公式实现（水深 = 电压 * 63.1 + 5.9739）
- 基本的数据验证和校准
- 完全兼容WDS.cpp的调用接口
- 简洁高效的实现（代码行数减少50%+）

## 🚀 快速开始

### 基本使用示例

```cpp
#include <Arduino.h>
#include <MSP20/MSP20.h>

MSP20 waterSensor(35, 20);  // 使用GPIO35，20次采样

void setup() {
    Serial.begin(115200);

    // 初始化传感器
    if (waterSensor.begin()) {
        Serial.println("MSP20传感器初始化成功");

        // 可选：设置校准参数
        waterSensor.calibrate(0.0, 1.0);  // 零点偏移，比例因子
    } else {
        Serial.println("MSP20传感器初始化失败");
    }
}

void loop() {
    // 读取水深值
    double depthCm = waterSensor.readWaterDepth();
    double depthM = waterSensor.readWaterDepthMeters();

    if (depthCm > 0) {
        Serial.printf("水深: %.2f cm (%.3f m)\n", depthCm, depthM);

        // 读取其他数据
        double voltage = waterSensor.readVoltage();
        double pressure = waterSensor.readPressure();

        Serial.printf("电压: %.3f V, 压力: %.2f kPa\n", voltage, pressure);
    } else {
        Serial.println("传感器读取失败");
    }

    delay(1000);
}
```

### 高级功能示例

```cpp
#include <Arduino.h>
#include <MSP20/MSP20.h>

MSP20 waterSensor(35, 30);  // 使用GPIO35，30次采样提高精度

void setup() {
    Serial.begin(115200);

    if (waterSensor.begin()) {
        Serial.println("MSP20传感器初始化成功");

        // 手动设置校准参数
        if (waterSensor.calibrate(2.5, 1.05)) {  // 零点偏移2.5cm，比例因子1.05
            Serial.println("校准参数设置成功");
        }
    }
}

void loop() {
    // 读取各种数据
    double voltage = waterSensor.readVoltage();
    double pressure = waterSensor.readPressure();
    double depthCm = waterSensor.readWaterDepth();
    double depthM = waterSensor.readWaterDepthMeters();

    if (depthCm > 0) {
        Serial.printf("电压: %.3fV | 压力: %.2fkPa | 水深: %.2fcm (%.3fm)\n",
                      voltage, pressure, depthCm, depthM);
    } else {
        Serial.println("传感器读取失败");
    }

    delay(2000);
}
```

## 📚 API参考

### 构造函数

```cpp
MSP20(uint8_t pin, uint8_t samplesCount = 20)
```

### 初始化和配置

| 方法        | 描述     | 返回值               |
|-----------|--------|-------------------|
| `begin()` | 初始化传感器 | `bool` - 成功返回true |

### 数据读取

| 方法                       | 描述       | 返回值                |
|--------------------------|----------|-------------------|
| `readVoltage()`          | 读取电压值    | `double` - 电压值(V)   |
| `readPressure()`         | 读取压力值    | `double` - 压力值(kPa) |
| `readWaterDepth()`       | 读取水深值(厘米) | `double` - 水深值(cm)  |
| `readWaterDepthMeters()` | 读取水深值(米)  | `double` - 水深值(m)   |

### 校准功能

| 方法                                      | 描述   | 返回值               |
|-----------------------------------------|------|-------------------|
| `calibrate(double offset, double scale)` | 手动校准 | `bool` - 成功返回true |

## 🔧 采样配置

精简版本通过构造函数参数配置采样次数：

| 采样次数 | 适用场景         | 精度 | 速度 |
|------|--------------|----|----|
| 3-10 | 快速读取，低功耗     | 低  | 快  |
| 20   | 常规监测（默认）     | 中  | 中  |
| 30-50| 高精度测量        | 高  | 慢  |
| 50+  | 最高精度，实验室环境   | 最高 | 最慢 |

## 🛡️ 错误处理

MSP20模块提供完善的错误检测和处理机制：

### 常见错误类型

- **初始化失败**: 硬件连接问题或引脚配置错误
- **读数超出范围**: 传感器故障或环境异常
- **参数无效**: 配置参数超出有效范围
- **传感器异常**: 硬件故障或信号干扰

### 错误处理示例

```cpp
if (!waterSensor.begin()) {
    Serial.println("传感器初始化失败，请检查硬件连接");
    return;
}

double depth = waterSensor.readWaterDepth();
if (depth == 0.0 && !waterSensor.isHealthy()) {
    Serial.println("传感器读取失败，请检查传感器状态");
    
    // 尝试重新初始化
    if (waterSensor.begin()) {
        Serial.println("传感器重新初始化成功");
    }
}
```

## 🧪 测试

运行MSP20模块的单元测试：

```bash
# 编译并运行测试
pio test -f test_MSP20

# 或者手动运行测试
cp test/test_MSP20.cpp src/main.cpp
pio run --target upload
pio device monitor
```

测试覆盖范围：
- ✅ 构造函数和初始化
- ✅ 配置功能测试
- ✅ 数据读取功能
- ✅ 校准功能
- ✅ 错误处理机制
- ✅ 状态管理

## 🔍 故障排除

### 常见问题

**Q: 传感器初始化失败**
A: 检查硬件连接，确认ADC引脚配置正确，验证电源供应

**Q: 读数不稳定**
A: 增加采样次数，使用高精度模式，检查环境干扰

**Q: 读数偏差较大**
A: 执行校准操作，检查传感器安装位置，验证参考测量

**Q: 传感器状态异常**
A: 检查硬件连接，重新初始化传感器，查看错误日志

### 调试技巧

1. **启用详细日志**: 在LogManager中设置TRACE级别
2. **检查ADC原始值**: 使用`readRawADC()`验证硬件连接
3. **监控传感器状态**: 定期检查`getStatus()`和`isHealthy()`
4. **验证校准参数**: 使用`getCalibrationParameters()`检查校准设置

## 📄 许可证

本模块遵循MIT许可证，详见项目根目录的LICENSE文件。

## 🤝 贡献

欢迎提交Issue和Pull Request来改进MSP20模块。请确保：

1. 代码符合项目编码规范
2. 添加适当的中文注释
3. 包含相应的测试用例
4. 更新相关文档

---

**版本**: v2.0.0  
**作者**: zelas2xerath  
**更新日期**: 2025-07-20
