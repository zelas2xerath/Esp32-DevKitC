# LogManager 模块

## 概述

LogManager 是一个轻量级的日志管理模块，封装了 ArduinoLog 库，提供统一的、分级的日志管理功能。使用 LogManager 可以实现：

1. 根据日志级别过滤日志输出，减少不必要的串口通信和存储开销
2. 提供统一的日志格式，包含时间戳和日志级别
3. 通过简单的宏定义简化日志调用

## 日志级别

LogManager 支持以下日志级别（从低到高）：

- `SILENT`: 关闭所有日志输出
- `FATAL`: 致命错误，通常导致程序终止的错误
- `ERROR`: 错误，影响功能但不会导致程序终止
- `WARNING`: 警告，可能存在的问题
- `NOTICE`: 提示，重要的系统状态变化
- `TRACE`: 跟踪，记录程序流程和状态变化
- `VERBOSE`: 详细，记录所有细节信息

日志级别设置为某一级别后，该级别及更低级别的日志都会被输出。例如，设置为 `WARNING` 级别时，`WARNING`、`ERROR` 和 `FATAL` 级别的日志都会被输出，而 `NOTICE`、`TRACE` 和 `VERBOSE` 级别的日志会被忽略。

## 使用方法

### 初始化

```cpp
#include "LogManager.h"

void setup() {
  // 初始化日志管理器，设置日志级别为 NOTICE
  LogManager::begin(LogManager::Level::NOTICE);
  
  // 或者指定串口和波特率
  // LogManager::begin(LogManager::Level::NOTICE, true, 115200);
}
```

### 输出日志

```cpp
// 使用宏定义输出日志
LOG_FATAL("致命错误: %s", errorMsg);
LOG_ERROR("错误: %d", errorCode);
LOG_WARNING("警告: %s 不可用", deviceName);
LOG_NOTICE("系统状态: %s", statusStr);
LOG_TRACE("函数调用: %s()", funcName);
LOG_VERBOSE("变量值: %d", value);

// 或者使用类方法
LogManager::fatal("致命错误: %s", errorMsg);
LogManager::error("错误: %d", errorCode);
// ...
```

### 动态调整日志级别

```cpp
// 在运行时调整日志级别
LogManager::setLevel(LogManager::Level::VERBOSE); // 设置为最详细级别
LogManager::setLevel(LogManager::Level::WARNING); // 只显示警告、错误和致命错误
LogManager::setLevel(LogManager::Level::SILENT);  // 关闭所有日志输出
```

## 最佳实践

1. 在开发和调试阶段使用 `VERBOSE` 或 `TRACE` 级别，以获取详细的系统信息
2. 在生产环境使用 `WARNING` 或 `NOTICE` 级别，减少资源占用
3. 对性能敏感的代码段，临时降低日志级别或使用 `SILENT` 级别
4. 使用格式化字符串提高日志可读性
5. 在循环中避免过度日志输出，考虑使用计数器或时间间隔

## 日志格式

默认情况下，日志输出格式为：
```
[HH:MM:SS.mmm][级别] 日志内容
```

例如：
```
[00:01:23.456][提示] 系统初始化完成
``` 