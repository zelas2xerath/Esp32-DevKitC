/*
 * RTOS看门狗任务实现
 * 负责监控系统健康状态和任务运行状态
 * 作者：[Zelas2Xerath]
 * 版本：2.1.3 - RTOS重构版本
 */

#include <RTOSTasks.h>
#include <RTOSCommon.h>
#include <SensorManager/SensorManager.h>
#include <SystemManager/RTOSSystemManager.h>
#include <LogManager/LogManager.h>

// 外部函数声明
extern void updateTaskHeartbeat(int taskIndex);
extern void reportTaskError(int taskIndex, const char* errorMsg);
extern bool checkTaskHealth();
extern String getTaskStatusInfo();
extern ErrorCode restartTask(TaskHandle_t* taskHandle, TaskFunction_t taskFunction, 
                            const char* taskName, uint32_t stackSize, UBaseType_t priority);

// 任务索引定义
#define WATCHDOG_TASK_INDEX 5

// 函数声明
static void checkTaskHealthStatus();
static void checkSystemResources();
static void checkNetworkHealth();
static void checkSensorHealth();
static void performSystemMaintenance();
static void attemptTaskRecovery();
static void checkTaskStackUsage();
static void cleanupExpiredMessages();
static void resetErrorCounters();
static void logSystemStatistics();
static void performSystemRestart(const char* reason);

// 看门狗状态
static uint32_t systemResetCount = 0;
static unsigned long lastHealthCheck = 0;
static unsigned long lastResourceCheck = 0;
static bool systemHealthy = true;

/**
 * @brief 系统看门狗任务
 * 监控系统健康状态，检测任务异常，执行系统恢复
 */
void vWatchdogTask(void *pvParameters) {
    LOG_NOTICE("看门狗任务启动");
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    constexpr TickType_t xFrequency = pdMS_TO_TICKS(TASK_PERIOD_WATCHDOG);
    
    // 等待系统完全启动
    vTaskDelay(pdMS_TO_TICKS(10000));
    
    while (true) {
        // 更新任务心跳
        updateTaskHeartbeat(WATCHDOG_TASK_INDEX);
        
        try {
            // 检查任务健康状态
            checkTaskHealthStatus();
            
            // 检查系统资源使用情况
            checkSystemResources();
            
            // 检查网络连接状态
            checkNetworkHealth();
            
            // 检查传感器状态
            checkSensorHealth();
            
            // 执行系统维护
            performSystemMaintenance();
            
        } catch (...) {
            reportTaskError(WATCHDOG_TASK_INDEX, "看门狗任务异常");
        }
        
        // 等待下一个周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief 检查任务健康状态
 */
static void checkTaskHealthStatus() {
    unsigned long currentTime = millis();
    
    // 每30秒检查一次任务健康状态
    if (currentTime - lastHealthCheck > 30000) {
        lastHealthCheck = currentTime;
        
        bool allTasksHealthy = checkTaskHealth();
        
        if (!allTasksHealthy) {
            LOG_WARNING("检测到任务健康状态异常");
            systemHealthy = false;
            
            // 尝试重启异常任务
            attemptTaskRecovery();
            
        } else {
            if (!systemHealthy) {
                LOG_NOTICE("所有任务健康状态恢复正常");
                systemHealthy = true;
            }
        }
        
        // 记录任务状态信息
        String taskStatus = getTaskStatusInfo();
        LOG_VERBOSE("任务状态: %s", taskStatus.c_str());
    }
}

/**
 * @brief 检查系统资源使用情况
 */
static void checkSystemResources() {
    unsigned long currentTime = millis();
    
    // 每60秒检查一次系统资源
    if (currentTime - lastResourceCheck > 60000) {
        lastResourceCheck = currentTime;
        
        // 检查堆内存使用情况
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t minFreeHeap = ESP.getMinFreeHeap();
        
        LOG_VERBOSE("内存状态: 空闲=%u, 最小空闲=%u", freeHeap, minFreeHeap);
        
        // 如果可用内存过低，发出警告
        if (freeHeap < 10240) { // 小于10KB
            LOG_WARNING("系统内存不足: %u字节", freeHeap);
            
            // 发送内存不足事件
            SystemEventMessage memoryEvent = createSystemEventMessage(
                Event::SystemError,
                freeHeap,
                minFreeHeap,
                "系统内存不足"
            );
            safeSendToQueue(xEventQueue, &memoryEvent, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
            
            // 如果内存严重不足，考虑重启系统
            if (freeHeap < 5120) { // 小于5KB
                LOG_FATAL("内存严重不足，准备重启系统");
                performSystemRestart("内存不足");
            }
        }
        
        // 检查任务堆栈使用情况
        checkTaskStackUsage();
    }
}

/**
 * @brief 检查网络健康状态
 */
static void checkNetworkHealth() {
    // 生命周期管理迁移：使用RTOSSystemManager进行网络健康检查
    if (!rtosSystemManager || !rtosSystemManager->getNetworkManager()) {
        return;
    }

    auto* networkManager = rtosSystemManager->getNetworkManager();
    
    // 检查网络连接状态
    if (!networkManager->isWiFiConnected()) {
        static unsigned long wifiDisconnectTime = 0;
        
        if (wifiDisconnectTime == 0) {
            wifiDisconnectTime = millis();
        } else if (millis() - wifiDisconnectTime > 300000) { // 5分钟无连接
            LOG_WARNING("WiFi长时间断开，尝试重启网络");
            
            // 发送网络重启事件
            SystemEventMessage networkEvent = createSystemEventMessage(
                Event::NetworkError,
                0,
                0,
                "WiFi长时间断开"
            );
            safeSendToQueue(xEventQueue, &networkEvent, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
            
            wifiDisconnectTime = 0; // 重置计时器
        }
    }
}

/**
 * @brief 检查传感器健康状态
 */
static void checkSensorHealth() {
    // 生命周期管理迁移：使用RTOSSystemManager进行传感器健康检查
    if (!rtosSystemManager) {
        return;
    }
    
    // 检查传感器是否响应
    static unsigned long lastSensorCheck = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastSensorCheck > 120000) { // 每2分钟检查一次
        lastSensorCheck = currentTime;
        
        bool sensorHealthy = true;
        
        // 检查土壤湿度传感器 - 生命周期管理迁移：使用RTOSSystemManager
        if (rtosSystemManager->getSMS()) {
            ErrorCode smsResult = rtosSystemManager->getSMS()->getLastErrorCode();
            if (smsResult != ErrorCode::SUCCESS) {
                LOG_WARNING("土壤湿度传感器异常: %d", static_cast<int>(smsResult));
                sensorHealthy = false;
            }
        }
        
        // 检查环境传感器 - 生命周期管理迁移：使用RTOSSystemManager
        if (rtosSystemManager->getEnvironmentSensor()) {
            ErrorCode ecsResult = rtosSystemManager->getEnvironmentSensor()->getLastErrorCode();
            if (ecsResult != ErrorCode::SUCCESS) {
                LOG_WARNING("环境传感器异常: %d", static_cast<int>(ecsResult));
                sensorHealthy = false;
            }
        }
        
        // 如果传感器异常，设置传感器未就绪事件
        if (!sensorHealthy) {
            xEventGroupClearBits(xSystemEventGroup, EVENT_SENSOR_READY);
            
            // 发送传感器错误事件
            SystemEventMessage sensorEvent = createSystemEventMessage(
                Event::SensorError,
                0,
                0,
                "传感器健康检查失败"
            );
            safeSendToQueue(xEventQueue, &sensorEvent, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
        } else {
            xEventGroupSetBits(xSystemEventGroup, EVENT_SENSOR_READY);
        }
    }
}

/**
 * @brief 执行系统维护
 */
static void performSystemMaintenance() {
    static unsigned long lastMaintenance = 0;
    unsigned long currentTime = millis();
    
    // 每小时执行一次系统维护
    if (currentTime - lastMaintenance > 3600000) { // 1小时
        lastMaintenance = currentTime;
        
        LOG_NOTICE("执行系统维护");
        
        // 清理队列中的过期消息
        cleanupExpiredMessages();
        
        // 重置错误计数器
        resetErrorCounters();
        
        // 记录系统运行统计
        logSystemStatistics();
    }
}

/**
 * @brief 尝试任务恢复
 */
static void attemptTaskRecovery() {
    LOG_WARNING("尝试恢复异常任务");
    
    // 检查并重启传感器任务
    if (xSensorTaskHandle == nullptr || eTaskGetState(xSensorTaskHandle) == eDeleted) {
        LOG_WARNING("传感器任务异常，尝试重启");
        restartTask(&xSensorTaskHandle, vSensorTask, "SensorTask", 
                   TASK_STACK_SIZE_SENSOR, TASK_PRIORITY_SENSOR);
    }
    
    // 检查并重启网络任务
    if (xNetworkTaskHandle == nullptr || eTaskGetState(xNetworkTaskHandle) == eDeleted) {
        LOG_WARNING("网络任务异常，尝试重启");
        restartTask(&xNetworkTaskHandle, vNetworkTask, "NetworkTask", 
                   TASK_STACK_SIZE_NETWORK, TASK_PRIORITY_NETWORK);
    }
    
    // 检查并重启控制任务
    if (xControlTaskHandle == nullptr || eTaskGetState(xControlTaskHandle) == eDeleted) {
        LOG_WARNING("控制任务异常，尝试重启");
        restartTask(&xControlTaskHandle, vControlTask, "ControlTask", 
                   TASK_STACK_SIZE_CONTROL, TASK_PRIORITY_CONTROL);
    }
    
    // 检查并重启执行器任务
    if (xActuatorTaskHandle == nullptr || eTaskGetState(xActuatorTaskHandle) == eDeleted) {
        LOG_WARNING("执行器任务异常，尝试重启");
        restartTask(&xActuatorTaskHandle, vActuatorTask, "ActuatorTask", 
                   TASK_STACK_SIZE_ACTUATOR, TASK_PRIORITY_ACTUATOR);
    }
    
    // 检查并重启显示任务
    if (xDisplayTaskHandle == nullptr || eTaskGetState(xDisplayTaskHandle) == eDeleted) {
        LOG_WARNING("显示任务异常，尝试重启");
        restartTask(&xDisplayTaskHandle, vDisplayTask, "DisplayTask", 
                   TASK_STACK_SIZE_DISPLAY, TASK_PRIORITY_DISPLAY);
    }
}

/**
 * @brief 检查任务堆栈使用情况
 */
static void checkTaskStackUsage() {
    // 检查各个任务的堆栈使用情况
    if (xSensorTaskHandle != nullptr) {
        UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(xSensorTaskHandle);
        if (stackHighWaterMark < 512) { // 剩余堆栈小于512字节
            LOG_WARNING("传感器任务堆栈使用过高，剩余: %u", stackHighWaterMark);
        }
    }
    
    if (xNetworkTaskHandle != nullptr) {
        UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(xNetworkTaskHandle);
        if (stackHighWaterMark < 1024) { // 剩余堆栈小于1024字节
            LOG_WARNING("网络任务堆栈使用过高，剩余: %u", stackHighWaterMark);
        }
    }
    
    // 其他任务的堆栈检查...
}

/**
 * @brief 清理过期消息
 */
static void cleanupExpiredMessages() {
    // 清理传感器数据队列中的过期消息
    // 这里可以实现更复杂的清理逻辑
    LOG_VERBOSE("清理过期消息");
}

/**
 * @brief 重置错误计数器
 */
static void resetErrorCounters() {
    // 重置各种错误计数器
    LOG_VERBOSE("重置错误计数器");
}

/**
 * @brief 记录系统运行统计
 */
static void logSystemStatistics() {
    unsigned long uptime = millis() / 1000;
    uint32_t freeHeap = ESP.getFreeHeap();
    
    LOG_NOTICE("系统统计 - 运行时间: %lus, 空闲内存: %u, 重启次数: %u", 
              uptime, freeHeap, systemResetCount);
}

/**
 * @brief 执行系统重启
 */
static void performSystemRestart(const char* reason) {
    LOG_FATAL("系统重启: %s", reason);
    
    systemResetCount++;
    
    // 保存重启原因到NVS
    // 这里可以添加保存重启原因的代码
    
    // 延迟一段时间后重启
    vTaskDelay(pdMS_TO_TICKS(5000));
    ESP.restart();
}
