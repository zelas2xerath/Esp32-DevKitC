/*
 * ESP32 土壤多维度监测与控制系统 - RTOS版本
 * 功能：实时监测土壤温湿度，EC值，盐分，NPK，PH，空气温湿度，气压，自动控制灌溉，支持远程指令控制
 * 硬件：ESP32 + OLED 显示屏 + 土壤传感器 + 水泵控制
 * 架构：基于FreeRTOS的多任务并发架构
 * 作者：[Zelas2Xerath]
 * 版本：6.4.2 Beta - RTOS重构版本
 */

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include <RTOSTasks.h>
#include <RTOSCommon.h>
#include <SystemManager/RTOSSystemManager.h>
#include <LogManager/LogManager.h>

// OLED显示屏对象 (I2C接口：SCL=GPIO21, SDA=GPIO19)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, 21, 19);

// Flash存储对象（用于保存配置参数）
Preferences prefs;

// RTOS系统管理器对象
RTOSSystemManager* rtosSystemManager = nullptr;

// ==================== 函数声明 ====================
void showBootScreen();
void handleSystemError(ErrorCode errorCode, const char* errorMsg);
void systemRestart(const char* reason);

// ==================== 主程序入口 ====================

/**
 * @brief 系统初始化函数
 */
void setup() {
    // 初始化串口
    Serial.begin(115200);
    delay(1000);  // 等待串口稳定

    globalLogManager = new LogManager();
    if (globalLogManager) {
        globalLogManager->begin(ILogger::Level::WARNING, true, 115200);
    } else {
        // 内存分配失败时的备用方案：使用静态方法初始化
        LogManager::beginStatic(ILogger::Level::NOTICE, true, 115200);
        Serial.println("[系统] 使用静态日志管理器作为备用方案");
    }

    LOG_NOTICE("=== ESP32 土壤湿度监测系统启动 ===");
    LOG_NOTICE("版本: 6.4.2 Beta - RTOS重构版本");
    LOG_NOTICE("架构: FreeRTOS多任务并发");

    // 显示启动画面
    showBootScreen();

    // 创建RTOS系统管理器
    rtosSystemManager = new RTOSSystemManager();
    if (!rtosSystemManager) {
        LOG_FATAL("RTOS系统管理器创建失败！内存不足");
        handleSystemError(ErrorCode::INITIALIZATION_FAILED, "系统管理器创建失败");
        return;
    }

    // 初始化RTOS资源
    LOG_NOTICE("初始化RTOS资源...");
    ErrorCode rtosResult = initializeRTOSResources();
    if (rtosResult != ErrorCode::SUCCESS) {
        LOG_FATAL("RTOS资源初始化失败！错误码: %d", static_cast<int>(rtosResult));
        handleSystemError(rtosResult, "RTOS资源初始化失败");
        return;
    }

    // 初始化系统管理器
    LOG_NOTICE("初始化系统组件...");
    ErrorCode initResult = rtosSystemManager->begin(oled, prefs);
    if (initResult != ErrorCode::SUCCESS) {
        LOG_FATAL("系统管理器初始化失败！错误码: %d", static_cast<int>(initResult));
        handleSystemError(initResult, "系统管理器初始化失败");
        return;
    }

    // 创建RTOS任务
    LOG_NOTICE("创建RTOS任务...");
    ErrorCode taskResult = createRTOSTasks();
    if (taskResult != ErrorCode::SUCCESS) {
        LOG_FATAL("RTOS任务创建失败！错误码: %d", static_cast<int>(taskResult));
        handleSystemError(taskResult, "RTOS任务创建失败");
        return;
    }

    // 启动RTOS任务
    LOG_NOTICE("启动RTOS任务...");
    ErrorCode startResult = rtosSystemManager->startRTOSTasks();
    if (startResult != ErrorCode::SUCCESS) {
        LOG_FATAL("RTOS任务启动失败！错误码: %d", static_cast<int>(startResult));
        handleSystemError(startResult, "RTOS任务启动失败");
        return;
    }

    // 设置传感器就绪事件
    xEventGroupSetBits(xSystemEventGroup, EVENT_SENSOR_READY);

    LOG_NOTICE("=== RTOS系统初始化完成！===");
    LOG_NOTICE("任务状态: %s", rtosSystemManager->getTaskStatus().c_str());
    LOG_NOTICE("队列状态: %s", rtosSystemManager->getQueueStatus().c_str());

    // 显示系统就绪信息
    DisplayMessage readyMsg = createDisplayMessage(
        DISPLAY_MSG_SYSTEM_STATUS,
        255,  // 最高优先级
        "系统就绪",
        "RTOS架构",
        "多任务运行",
        "监测启动"
    );
    rtosSystemManager->postDisplayMessage(readyMsg);

    // 发送系统启动事件
    rtosSystemManager->postEvent(Event::SystemStartup, 0, 0, "RTOS系统启动完成");
}

/**
 * @brief 主循环函数（RTOS版本中主要用于监控）
 */
void loop() {
    // 在RTOS架构中，主要的业务逻辑都在各个任务中执行
    // loop()函数主要用于系统监控和异常处理

    static unsigned long lastMonitorTime = 0;
    unsigned long currentTime = millis();

    // 每30秒进行一次系统监控
    if (currentTime - lastMonitorTime > 30000) {
        lastMonitorTime = currentTime;

        if (rtosSystemManager) {
            // 检查任务健康状态
            if (!rtosSystemManager->areTasksHealthy()) {
                LOG_WARNING("检测到任务异常");

                // 尝试恢复异常任务
                String taskStatus = rtosSystemManager->getTaskStatus();
                LOG_WARNING("任务状态: %s", taskStatus.c_str());
            }

            // 检查系统资源
            SystemResourceInfo resourceInfo = rtosSystemManager->getSystemResourceInfo();
            if (resourceInfo.freeHeapSize < 10240) { // 小于10KB
                LOG_WARNING("系统内存不足: %u字节", resourceInfo.freeHeapSize);
            }

            // 记录系统运行状态
            LOG_VERBOSE("系统运行正常 - 运行时间: %lus, 空闲内存: %u字节",
                       currentTime / 1000, resourceInfo.freeHeapSize);
        }
    }

    // 检查是否需要处理配网模式
    if (rtosSystemManager && rtosSystemManager->isInConfigMode()) {
        rtosSystemManager->handleConfigMode();
    }

    // 短暂延时，让出CPU时间给其他任务
    delay(1000);
}

// ==================== 辅助函数实现 ====================

/**
 * @brief 显示启动画面
 */
void showBootScreen() {
    // 初始化OLED显示屏
    oled.begin();
    oled.clearBuffer();
    oled.setFont(u8g2_font_wqy12_t_gb2312);

    // 显示启动信息
    oled.drawUTF8(0, 16, "ESP32 土壤监测");
    oled.drawUTF8(0, 32, "RTOS架构启动中...");
    oled.drawUTF8(0, 48, "版本: 6.4.2 Beta");
    oled.sendBuffer();

    delay(2000);
}

/**
 * @brief 处理系统错误
 */
void handleSystemError(ErrorCode errorCode, const char* errorMsg) {
    LOG_FATAL("系统严重错误: %s (错误码: %d)", errorMsg, static_cast<int>(errorCode));

    // 显示错误信息
    oled.clearBuffer();
    oled.setFont(u8g2_font_wqy12_t_gb2312);
    oled.drawUTF8(0, 16, "!!! 系统错误 !!!");
    oled.drawUTF8(0, 32, errorMsg);
    oled.drawUTF8(0, 48, ("错误码: " + String(static_cast<int>(errorCode))).c_str());
    oled.sendBuffer();

    // 等待一段时间后重启
    delay(5000);
    systemRestart("系统初始化失败");
}

/**
 * @brief 系统重启
 */
void systemRestart(const char* reason) {
    LOG_FATAL("系统重启: %s", reason);

    // 清理RTOS资源
    if (rtosSystemManager) {
        rtosSystemManager->stopRTOSTasks();
        delete rtosSystemManager;
        rtosSystemManager = nullptr;
    }

    deleteRTOSResources();

    // ==================== 修复：清理全局日志管理器 ====================
    // 在系统重启前清理全局日志管理器，避免内存泄漏
    if (globalLogManager) {
        delete globalLogManager;
        globalLogManager = nullptr;
        Serial.println("[系统] 全局日志管理器已清理");
    }

    // 显示重启信息
    oled.clearBuffer();
    oled.setFont(u8g2_font_wqy12_t_gb2312);
    oled.drawUTF8(0, 16, "系统重启中...");
    oled.drawUTF8(0, 32, reason);
    oled.sendBuffer();

    delay(3000);
    ESP.restart();
}

// ==================== FreeRTOS钩子函数 ====================

/**
 * @brief 自定义空闲任务钩子函数
 * 注意：不使用vApplicationIdleHook以避免与ESP32框架冲突
 */
void customIdleHook() {
    // 在空闲时可以执行一些低优先级的任务
    // 例如：看门狗喂狗、垃圾回收等
}

/**
 * @brief 堆栈溢出钩子函数
 */
extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    LOG_FATAL("任务堆栈溢出: %s", pcTaskName);

    // 记录堆栈溢出信息
    if (rtosSystemManager) {
        rtosSystemManager->reportError(
            ErrorCode::STACK_OVERFLOW,
            ("任务堆栈溢出: " + String(pcTaskName)).c_str(),
            SystemState::CRITICAL
        );
    }

    // 系统重启
    systemRestart("任务堆栈溢出");
}

/**
 * @brief 内存分配失败钩子函数
 */
extern "C" void vApplicationMallocFailedHook(void) {
    LOG_FATAL("内存分配失败");

    if (rtosSystemManager) {
        rtosSystemManager->reportError(
            ErrorCode::MEMORY_ALLOCATION_FAILED,
            "内存分配失败",
            SystemState::CRITICAL
        );
    }

    systemRestart("内存分配失败");
}

/**
 * @brief 任务删除钩子函数
 */
extern "C" void vApplicationTaskDeleteHook(TaskHandle_t xTaskToDelete) {
    char* taskName = pcTaskGetName(xTaskToDelete);
    LOG_WARNING("任务被删除: %s", taskName ? taskName : "未知任务");
}
