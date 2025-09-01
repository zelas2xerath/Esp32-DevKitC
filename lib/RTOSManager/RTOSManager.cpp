/*
 * RTOS任务管理器实现
 * 负责创建和管理所有FreeRTOS任务、队列、信号量和事件组
 * 作者：[Zelas2Xerath]
 * 版本：2.1.3 - RTOS重构版本
 */

#include <RTOSTasks.h>
#include <RTOSCommon.h>
#include <LogManager/LogManager.h>
#include <cstring>  // for memset

// ==================== 全局句柄定义 ====================
// 任务句柄
TaskHandle_t xSensorTaskHandle = nullptr;
TaskHandle_t xNetworkTaskHandle = nullptr;
TaskHandle_t xControlTaskHandle = nullptr;
TaskHandle_t xActuatorTaskHandle = nullptr;
TaskHandle_t xDisplayTaskHandle = nullptr;
TaskHandle_t xWatchdogTaskHandle = nullptr;

// 队列句柄
QueueHandle_t xSensorDataQueue = nullptr;
QueueHandle_t xNetworkSendQueue = nullptr;
QueueHandle_t xControlCmdQueue = nullptr;
QueueHandle_t xDisplayMsgQueue = nullptr;
QueueHandle_t xEventQueue = nullptr;

// 信号量句柄
SemaphoreHandle_t xConfigMutex = nullptr;
SemaphoreHandle_t xDisplayMutex = nullptr;
SemaphoreHandle_t xNetworkMutex = nullptr;
SemaphoreHandle_t xSensorMutex = nullptr;

// 事件组句柄
EventGroupHandle_t xSystemEventGroup = nullptr;

// 任务健康状态
static TaskHealthStatus taskHealthStatus[6];

// 外部系统管理器引用

// ==================== RTOS资源初始化 ====================

ErrorCode initializeRTOSResources() {
    LOG_NOTICE("初始化RTOS资源...");

    // 初始化任务健康状态数组
    memset(taskHealthStatus, 0, sizeof(taskHealthStatus));

    // 创建消息队列
    xSensorDataQueue = xQueueCreate(QUEUE_SIZE_SENSOR_DATA, sizeof(SensorDataMessage));
    if (xSensorDataQueue == nullptr) {
        LOG_ERROR("创建传感器数据队列失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    xNetworkSendQueue = xQueueCreate(QUEUE_SIZE_NETWORK_SEND, sizeof(NetworkSendMessage));
    if (xNetworkSendQueue == nullptr) {
        LOG_ERROR("创建网络发送队列失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    xControlCmdQueue = xQueueCreate(QUEUE_SIZE_CONTROL_CMD, sizeof(ControlCommandMessage));
    if (xControlCmdQueue == nullptr) {
        LOG_ERROR("创建控制指令队列失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    xDisplayMsgQueue = xQueueCreate(QUEUE_SIZE_DISPLAY_MSG, sizeof(DisplayMessage));
    if (xDisplayMsgQueue == nullptr) {
        LOG_ERROR("创建显示消息队列失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    xEventQueue = xQueueCreate(QUEUE_SIZE_EVENT, sizeof(SystemEventMessage));
    if (xEventQueue == nullptr) {
        LOG_ERROR("创建事件队列失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    // 创建互斥信号量
    xConfigMutex = xSemaphoreCreateMutex();
    if (xConfigMutex == nullptr) {
        LOG_ERROR("创建配置互斥锁失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    xDisplayMutex = xSemaphoreCreateMutex();
    if (xDisplayMutex == nullptr) {
        LOG_ERROR("创建显示互斥锁失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    xNetworkMutex = xSemaphoreCreateMutex();
    if (xNetworkMutex == nullptr) {
        LOG_ERROR("创建网络互斥锁失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    xSensorMutex = xSemaphoreCreateMutex();
    if (xSensorMutex == nullptr) {
        LOG_ERROR("创建传感器互斥锁失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    // 创建事件组
    xSystemEventGroup = xEventGroupCreate();
    if (xSystemEventGroup == nullptr) {
        LOG_ERROR("创建系统事件组失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    LOG_NOTICE("RTOS资源初始化完成");
    return ErrorCode::SUCCESS;
}

// ==================== RTOS任务创建 ====================

ErrorCode createRTOSTasks() {
    LOG_NOTICE("创建RTOS任务...");

    // 创建传感器任务
    BaseType_t result = xTaskCreate(
        vSensorTask, // 任务函数
        "SensorTask", // 任务名称
        TASK_STACK_SIZE_SENSOR, // 堆栈大小
        nullptr, // 任务参数
        TASK_PRIORITY_SENSOR, // 任务优先级
        &xSensorTaskHandle // 任务句柄
    );
    if (result != pdPASS) {
        LOG_ERROR("创建传感器任务失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    // 创建网络任务
    result = xTaskCreate(
        vNetworkTask,
        "NetworkTask",
        TASK_STACK_SIZE_NETWORK,
        nullptr,
        TASK_PRIORITY_NETWORK,
        &xNetworkTaskHandle
    );
    if (result != pdPASS) {
        LOG_ERROR("创建网络任务失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    // 创建控制任务
    result = xTaskCreate(
        vControlTask,
        "ControlTask",
        TASK_STACK_SIZE_CONTROL,
        nullptr,
        TASK_PRIORITY_CONTROL,
        &xControlTaskHandle
    );
    if (result != pdPASS) {
        LOG_ERROR("创建控制任务失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    // 创建执行器任务
    result = xTaskCreate(
        vActuatorTask,
        "ActuatorTask",
        TASK_STACK_SIZE_ACTUATOR,
        nullptr,
        TASK_PRIORITY_ACTUATOR,
        &xActuatorTaskHandle
    );
    if (result != pdPASS) {
        LOG_ERROR("创建执行器任务失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    // 创建显示任务
    result = xTaskCreate(
        vDisplayTask,
        "DisplayTask",
        TASK_STACK_SIZE_DISPLAY,
        nullptr,
        TASK_PRIORITY_DISPLAY,
        &xDisplayTaskHandle
    );
    if (result != pdPASS) {
        LOG_ERROR("创建显示任务失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    // 创建看门狗任务
    result = xTaskCreate(
        vWatchdogTask,
        "WatchdogTask",
        TASK_STACK_SIZE_WATCHDOG,
        nullptr,
        TASK_PRIORITY_WATCHDOG,
        &xWatchdogTaskHandle
    );
    if (result != pdPASS) {
        LOG_ERROR("创建看门狗任务失败");
        return ErrorCode::INITIALIZATION_FAILED;
    }
    
    LOG_NOTICE("所有RTOS任务创建完成");
    return ErrorCode::SUCCESS;
}

// ==================== RTOS资源清理 ====================

void deleteRTOSResources() {
    LOG_NOTICE("删除RTOS资源...");
    
    // 删除任务
    if (xSensorTaskHandle != nullptr) {
        vTaskDelete(xSensorTaskHandle);
        xSensorTaskHandle = nullptr;
    }
    if (xNetworkTaskHandle != nullptr) {
        vTaskDelete(xNetworkTaskHandle);
        xNetworkTaskHandle = nullptr;
    }
    if (xControlTaskHandle != nullptr) {
        vTaskDelete(xControlTaskHandle);
        xControlTaskHandle = nullptr;
    }
    if (xActuatorTaskHandle != nullptr) {
        vTaskDelete(xActuatorTaskHandle);
        xActuatorTaskHandle = nullptr;
    }
    if (xDisplayTaskHandle != nullptr) {
        vTaskDelete(xDisplayTaskHandle);
        xDisplayTaskHandle = nullptr;
    }
    if (xWatchdogTaskHandle != nullptr) {
        vTaskDelete(xWatchdogTaskHandle);
        xWatchdogTaskHandle = nullptr;
    }
    
    // 删除队列
    if (xSensorDataQueue != nullptr) {
        vQueueDelete(xSensorDataQueue);
        xSensorDataQueue = nullptr;
    }
    if (xNetworkSendQueue != nullptr) {
        vQueueDelete(xNetworkSendQueue);
        xNetworkSendQueue = nullptr;
    }
    if (xControlCmdQueue != nullptr) {
        vQueueDelete(xControlCmdQueue);
        xControlCmdQueue = nullptr;
    }
    if (xDisplayMsgQueue != nullptr) {
        vQueueDelete(xDisplayMsgQueue);
        xDisplayMsgQueue = nullptr;
    }
    if (xEventQueue != nullptr) {
        vQueueDelete(xEventQueue);
        xEventQueue = nullptr;
    }
    
    // 删除信号量
    if (xConfigMutex != nullptr) {
        vSemaphoreDelete(xConfigMutex);
        xConfigMutex = nullptr;
    }
    if (xDisplayMutex != nullptr) {
        vSemaphoreDelete(xDisplayMutex);
        xDisplayMutex = nullptr;
    }
    if (xNetworkMutex != nullptr) {
        vSemaphoreDelete(xNetworkMutex);
        xNetworkMutex = nullptr;
    }
    if (xSensorMutex != nullptr) {
        vSemaphoreDelete(xSensorMutex);
        xSensorMutex = nullptr;
    }
    
    // 删除事件组
    if (xSystemEventGroup != nullptr) {
        vEventGroupDelete(xSystemEventGroup);
        xSystemEventGroup = nullptr;
    }
    
    LOG_NOTICE("RTOS资源删除完成");
}

// ==================== 任务状态管理 ====================

String getTaskStatusInfo() {
    String statusJson = "{\"tasks\":[";

    // 简化实现：手动报告已知任务状态
    TaskHandle_t* taskHandles[] = {&xSensorTaskHandle, &xNetworkTaskHandle, &xControlTaskHandle,
                                   &xActuatorTaskHandle, &xDisplayTaskHandle, &xWatchdogTaskHandle};

    for (int i = 0; i < 6; i++) {
        const char* taskNames[] = {"SensorTask", "NetworkTask", "ControlTask", "ActuatorTask", "DisplayTask", "WatchdogTask"};
        if (i > 0) statusJson += ",";
        statusJson += "{";
        statusJson += R"("name":")" + String(taskNames[i]) + "\",";
        statusJson += R"("state":")" + String(*taskHandles[i] != nullptr ? "running" : "stopped") + "\",";

        // 获取任务堆栈高水位标记（如果任务存在）
        if (*taskHandles[i] != nullptr) {
            UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(*taskHandles[i]);
            statusJson += "\"stackHighWaterMark\":" + String(stackHighWaterMark);
        } else {
            statusJson += "\"stackHighWaterMark\":0";
        }
        statusJson += "}";
    }

    statusJson += "],";
    statusJson += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    statusJson += "\"minFreeHeap\":" + String(ESP.getMinFreeHeap()) + ",";
    statusJson += "\"uptime\":" + String(millis());
    statusJson += "}";

    return statusJson;
}

bool checkTaskHealth() {
    unsigned long currentTime = millis();
    bool allHealthy = true;

    // 检查每个任务的健康状态
    for (int i = 0; i < 6; i++) {
        if (taskHealthStatus[i].lastHeartbeat > 0) {
            if (currentTime - taskHealthStatus[i].lastHeartbeat > TASK_HEARTBEAT_TIMEOUT_MS) {
                taskHealthStatus[i].isHealthy = false;
                allHealthy = false;
                LOG_WARNING("任务 %d 心跳超时", i);
            } else {
                taskHealthStatus[i].isHealthy = true;
            }
        }
    }

    return allHealthy;
}

ErrorCode restartTask(TaskHandle_t* taskHandle, TaskFunction_t taskFunction,
                     const char* taskName, uint32_t stackSize, UBaseType_t priority) {
    LOG_WARNING("重启任务: %s", taskName);

    // 删除现有任务
    if (*taskHandle != nullptr) {
        vTaskDelete(*taskHandle);
        *taskHandle = nullptr;
        vTaskDelay(pdMS_TO_TICKS(100)); // 等待任务完全删除
    }

    // 创建新任务
    BaseType_t result = xTaskCreate(
        taskFunction,
        taskName,
        stackSize,
        nullptr,
        priority,
        taskHandle
    );

    if (result != pdPASS) {
        LOG_ERROR("重启任务失败: %s", taskName);
        return ErrorCode::INITIALIZATION_FAILED;
    }

    LOG_NOTICE("任务重启成功: %s", taskName);
    return ErrorCode::SUCCESS;
}

// ==================== 辅助函数实现 ====================

void updateTaskHeartbeat(int taskIndex) {
    if (taskIndex >= 0 && taskIndex < 6) {
        taskHealthStatus[taskIndex].lastHeartbeat = millis();
        taskHealthStatus[taskIndex].cycleCount++;
    }
}

void reportTaskError(int taskIndex, const char* errorMsg) {
    if (taskIndex >= 0 && taskIndex < 6) {
        taskHealthStatus[taskIndex].errorCount++;
        strncpy(taskHealthStatus[taskIndex].lastError, errorMsg, 63);
        taskHealthStatus[taskIndex].lastError[63] = '\0';
        LOG_ERROR("任务 %d 错误: %s", taskIndex, errorMsg);
    }
}
