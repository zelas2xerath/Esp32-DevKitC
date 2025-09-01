/*
 * RTOS显示任务实现
 * 负责更新OLED显示屏内容
 * 作者：[Zelas2Xerath]
 * 版本：2.1.3 - RTOS重构版本
 */

#include <RTOSTasks.h>
#include <RTOSCommon.h>
#include <SystemManager/RTOSSystemManager.h>
#include <SensorManager/SensorManager.h>
#include <LogManager/LogManager.h>
#include <cstring>  // for memset, strncpy

// 外部函数声明
extern void updateTaskHeartbeat(int taskIndex);
extern void reportTaskError(int taskIndex, const char* errorMsg);
extern RTOSSystemManager* rtosSystemManager;

// 任务索引定义
#define DISPLAY_TASK_INDEX 4

// 函数声明
static void processDisplayMessages();
static void updateDisplay(const DisplayMessage& msg);
static void showDefaultDisplay();
static void showSystemStatusDisplay(DisplayMessage& msg);
static void showSensorDataDisplay(DisplayMessage& msg);
static void showNetworkStatusDisplay(DisplayMessage& msg);
static void showControlStatusDisplay(DisplayMessage& msg);

// 显示状态
static DisplayMessage currentDisplay;
static unsigned long lastDisplayUpdate = 0;
static uint8_t displayRotationIndex = 0;

/**
 * @brief 显示更新任务
 * 处理显示消息队列，更新OLED显示内容
 */
void vDisplayTask(void *pvParameters) {
    LOG_NOTICE("显示任务启动");
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    constexpr TickType_t xFrequency = pdMS_TO_TICKS(TASK_PERIOD_DISPLAY);
    
    // 等待系统初始化完成
    vTaskDelay(pdMS_TO_TICKS(6000));
    
    while (true) {
        // 更新任务心跳
        updateTaskHeartbeat(DISPLAY_TASK_INDEX);
        
        // 获取显示互斥锁
        if (xSemaphoreTake(xDisplayMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
            try {
                // 处理显示消息队列
                processDisplayMessages();
                
                // 如果没有新消息，执行默认显示轮换
                if (millis() - lastDisplayUpdate > 5000) {
                    showDefaultDisplay();
                }
                
            } catch (...) {
                reportTaskError(DISPLAY_TASK_INDEX, "显示任务异常");
            }
            
            // 释放互斥锁
            xSemaphoreGive(xDisplayMutex);
        } else {
            reportTaskError(DISPLAY_TASK_INDEX, "获取显示互斥锁超时");
        }
        
        // 等待下一个周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief 处理显示消息
 */
static void processDisplayMessages() {
    DisplayMessage displayMsg;
    DisplayMessage highestPriorityMsg;  // 使用默认构造函数
    bool hasMessage = false;
    
    // 查找优先级最高的消息
    while (safeReceiveFromQueue(xDisplayMsgQueue, &displayMsg, 0)) { // 非阻塞接收
        if (!hasMessage || displayMsg.priority > highestPriorityMsg.priority) {
            highestPriorityMsg = displayMsg;
            hasMessage = true;
        }
    }
    
    // 如果有消息需要显示
    if (hasMessage) {
        updateDisplay(highestPriorityMsg);
        currentDisplay = highestPriorityMsg;
        lastDisplayUpdate = millis();
        
        LOG_VERBOSE("显示消息: 类型=%d, 优先级=%d", 
                   highestPriorityMsg.type, highestPriorityMsg.priority);
    }
}

/**
 * @brief 更新显示内容
 */
static void updateDisplay(const DisplayMessage& msg) {
    // 生命周期管理迁移：使用RTOSSystemManager进行显示管理
    if (!rtosSystemManager || !rtosSystemManager->getDisplayManager()) {
        return;
    }

    auto* displayManager = rtosSystemManager->getDisplayManager();
    
    // 根据消息内容确定显示行数
    if (strlen(msg.line4) > 0) {
        // 4行显示
        displayManager->showText(
            String(msg.line1),
            String(msg.line2),
            String(msg.line3),
            String(msg.line4)
        );
    } else if (strlen(msg.line3) > 0) {
        // 3行显示
        displayManager->showText(
            String(msg.line1),
            String(msg.line2),
            String(msg.line3)
        );
    } else if (strlen(msg.line2) > 0) {
        // 2行显示
        displayManager->showText(
            String(msg.line1),
            String(msg.line2)
        );
    } else {
        // 1行显示
        displayManager->showText(String(msg.line1));
    }
}

/**
 * @brief 显示默认内容（轮换显示）
 */
static void showDefaultDisplay() {
    // 生命周期管理迁移：使用RTOSSystemManager
    if (!rtosSystemManager) {
        return;
    }

    DisplayMessage defaultMsg;  // 使用默认构造函数
    defaultMsg.type = DISPLAY_MSG_SYSTEM_STATUS;
    defaultMsg.priority = 50;  // 低优先级
    defaultMsg.timestamp = millis();
    
    // 根据轮换索引显示不同内容
    switch (displayRotationIndex % 4) {
        case 0:
            showSystemStatusDisplay(defaultMsg);
            break;
            
        case 1:
            showSensorDataDisplay(defaultMsg);
            break;
            
        case 2:
            showNetworkStatusDisplay(defaultMsg);
            break;
            
        case 3:
            showControlStatusDisplay(defaultMsg);
            break;
    }
    
    updateDisplay(defaultMsg);
    displayRotationIndex++;
    lastDisplayUpdate = millis();
}

/**
 * @brief 显示系统状态
 */
static void showSystemStatusDisplay(DisplayMessage& msg) {
    unsigned long uptime = millis() / 1000;
    uint32_t freeHeap = ESP.getFreeHeap();
    
    strncpy(msg.line1, "=== 系统状态 ===", 31);
    snprintf(msg.line2, 31, "运行: %lus", uptime);
    snprintf(msg.line3, 31, "内存: %uB", freeHeap);
    
    // 生命周期管理迁移：使用RTOSSystemManager获取控制代理状态
    if (rtosSystemManager->getControlAgent()) {
        SystemState state = rtosSystemManager->getControlAgent()->getCurrentState();
        snprintf(msg.line4, 31, "状态: %s", LogManager::getSystemStateStringStatic(state));
    } else {
        strncpy(msg.line4, "状态: 未知", 31);
    }
}

/**
 * @brief 显示传感器数据
 */
static void showSensorDataDisplay(DisplayMessage& msg) {
    strncpy(msg.line1, "=== 传感器 ===", 31);
    
    // 显示土壤湿度 - 生命周期管理迁移：使用RTOSSystemManager
    if (rtosSystemManager->getSMS()) {
        int moisture = rtosSystemManager->getSMS()->readSoilMoisture();
        snprintf(msg.line2, 31, "湿度: %d%%", moisture);
    } else {
        strncpy(msg.line2, "湿度: --", 31);
    }
    
    // 显示环境温度 - 生命周期管理迁移：使用RTOSSystemManager
    if (rtosSystemManager->getEnvironmentSensor()) {
        float temp = rtosSystemManager->getEnvironmentSensor()->readTemperature();
        snprintf(msg.line3, 31, "温度: %.1f°C", temp);
    } else {
        strncpy(msg.line3, "温度: --", 31);
    }
    
    // 显示光照强度 - 使用LIS类优化后的统一接口
    if (rtosSystemManager->getLightSensor()) {
        float light = rtosSystemManager->getLightSensor()->readLightLevel();
        snprintf(msg.line4, 31, "光照: %.0flux", light);
    } else {
        strncpy(msg.line4, "光照: --", 31);
    }
}

/**
 * @brief 显示网络状态
 */
static void showNetworkStatusDisplay(DisplayMessage& msg) {
    strncpy(msg.line1, "=== 网络状态 ===", 31);
    
    // 生命周期管理迁移：使用RTOSSystemManager获取网络管理器
    if (rtosSystemManager->getNetworkManager()) {
        auto* networkManager = rtosSystemManager->getNetworkManager();
        
        // WiFi状态
        if (networkManager->isWiFiConnected()) {
            strncpy(msg.line2, "WiFi: 已连接", 31);
        } else {
            strncpy(msg.line2, "WiFi: 未连接", 31);
        }
        
        // MQTT服务器状态（通过RTOSSystemManager检查）
        if (rtosSystemManager && rtosSystemManager->isServerConnected()) {
            strncpy(msg.line3, "MQTT: 已连接", 31);
        } else {
            strncpy(msg.line3, "MQTT: 未连接", 31);
        }
        
        // 网络质量
        NetworkQuality quality = networkManager->getNetworkQuality();
        snprintf(msg.line4, 31, "延迟: %lums", quality.latency);
        
    } else {
        strncpy(msg.line2, "网络: 未初始化", 31);
        strncpy(msg.line3, "", 31);
        strncpy(msg.line4, "", 31);
    }
}

/**
 * @brief 显示控制状态
 */
static void showControlStatusDisplay(DisplayMessage& msg) {
    strncpy(msg.line1, "=== 控制状态 ===", 31);
    
    // 生命周期管理迁移：使用RTOSSystemManager获取控制代理
    if (rtosSystemManager->getControlAgent()) {
        auto* controlAgent = rtosSystemManager->getControlAgent();
        
        // 灌溉状态
        bool irrigationState = controlAgent->getControlOutputState();
        if (irrigationState) {
            strncpy(msg.line2, "灌溉: 开启", 31);
        } else {
            strncpy(msg.line2, "灌溉: 关闭", 31);
        }
        
        // 控制模式
        if (controlAgent->isInForceMode()) {
            strncpy(msg.line3, "模式: 强制", 31);
        } else {
            strncpy(msg.line3, "模式: 自动", 31);
        }
        
        // 阈值设置
        int lowThreshold, highThreshold;
        controlAgent->getThresholds(lowThreshold, highThreshold);
        snprintf(msg.line4, 31, "阈值: %d-%d%%", lowThreshold, highThreshold);
        
    } else {
        strncpy(msg.line2, "控制: 未初始化", 31);
        strncpy(msg.line3, "", 31);
        strncpy(msg.line4, "", 31);
    }
}

/**
 * @brief 显示错误信息
 */
void showErrorDisplay(ErrorCode errorCode, const char* errorMsg) {
    DisplayMessage errorDisplay;  // 使用默认构造函数
    errorDisplay.type = DISPLAY_MSG_ERROR_INFO;
    errorDisplay.priority = 255;  // 最高优先级
    errorDisplay.timestamp = millis();
    
    strncpy(errorDisplay.line1, "!!! 系统错误 !!!", 31);
    snprintf(errorDisplay.line2, 31, "错误码: %d", static_cast<int>(errorCode));
    
    if (errorMsg && strlen(errorMsg) > 0) {
        strncpy(errorDisplay.line3, errorMsg, 31);
        errorDisplay.line3[31] = '\0';
    } else {
        strncpy(errorDisplay.line3, "未知错误", 31);
    }
    
    strncpy(errorDisplay.line4, "请检查系统", 31);
    
    // 立即发送到显示队列
    safeSendToQueue(xDisplayMsgQueue, &errorDisplay, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
}

/**
 * @brief 显示启动信息
 */
void showBootDisplay() {
    DisplayMessage bootDisplay;  // 使用默认构造函数
    bootDisplay.type = DISPLAY_MSG_SYSTEM_STATUS;
    bootDisplay.priority = 200;  // 高优先级
    bootDisplay.timestamp = millis();
    
    strncpy(bootDisplay.line1, "ESP32 土壤监测", 31);
    strncpy(bootDisplay.line2, "系统启动中...", 31);
    strncpy(bootDisplay.line3, "版本: 4.1.6", 31);
    strncpy(bootDisplay.line4, "RTOS架构", 31);
    
    // 立即发送到显示队列
    safeSendToQueue(xDisplayMsgQueue, &bootDisplay, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
}

/**
 * @brief 清除显示内容
 */
void clearDisplay() {
    // 生命周期管理迁移：使用RTOSSystemManager进行显示清除
    if (rtosSystemManager && rtosSystemManager->getDisplayManager()) {
        rtosSystemManager->getDisplayManager()->clear();
    }
}
