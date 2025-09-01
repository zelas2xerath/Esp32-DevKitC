/*
 * RTOS执行器任务实现
 * 负责执行硬件控制操作（继电器、水泵等）
 * 作者：[Zelas2Xerath]
 * 版本：2.1.3 - RTOS重构版本
 */

#include <RTOSTasks.h>
#include <RTOSCommon.h>
#include <LogManager/LogManager.h>
#include <SystemManager/RTOSSystemManager.h>
#include <cstring>  // for memset, strncpy

// 外部函数声明
extern void updateTaskHeartbeat(int taskIndex);
extern void reportTaskError(int taskIndex, const char* errorMsg);

// 任务索引定义
#define ACTUATOR_TASK_INDEX 3

// 函数声明
static void processControlCommands();
static void monitorActuatorStatus();
static void executeAutoIrrigation(const ControlCommandMessage& cmd);
static void executeForceControl(bool state);
static void executeExitForceMode();
static void executeSetThreshold(const ControlCommandMessage& cmd);
static void executeSystemStateChange(const ControlCommandMessage& cmd);
static void updateIrrigationDisplay(bool state, const char* mode);
static void sendIrrigationStatusUpdate(bool state, const char* mode, int moisture);

// 执行器状态
static bool currentIrrigationState = false;
static unsigned long lastStateChangeTime = 0;

/**
 * @brief 硬件执行器任务
 * 处理控制指令，执行硬件操作
 * 修复：增加堆栈监控，防止堆栈溢出
 */
void vActuatorTask(void *pvParameters) {
    LOG_NOTICE("执行器任务启动 - 堆栈大小: %d字节", TASK_STACK_SIZE_ACTUATOR);

    TickType_t xLastWakeTime = xTaskGetTickCount();
    constexpr TickType_t xFrequency = pdMS_TO_TICKS(TASK_PERIOD_ACTUATOR);

    // 等待系统初始化完成
    vTaskDelay(pdMS_TO_TICKS(5000));

    // 堆栈监控变量
    static unsigned long lastStackCheck = 0;

    while (true) {
        // 更新任务心跳
        updateTaskHeartbeat(ACTUATOR_TASK_INDEX);

        // 定期检查堆栈使用情况（每60秒检查一次）
        unsigned long currentTime = millis();
        if (currentTime - lastStackCheck > 60000) {
            lastStackCheck = currentTime;
            UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(nullptr);
            LOG_VERBOSE("执行器任务堆栈剩余: %u字节", stackHighWaterMark * sizeof(StackType_t));

            // 如果堆栈使用超过80%，发出警告
            if (stackHighWaterMark * sizeof(StackType_t) < (TASK_STACK_SIZE_ACTUATOR * 0.2)) {
                LOG_WARNING("执行器任务堆栈使用率过高！剩余: %u字节",
                           stackHighWaterMark * sizeof(StackType_t));
            }
        }

        try {
            // 处理控制指令队列
            processControlCommands();

            // 监控执行器状态
            monitorActuatorStatus();

        } catch (...) {
            reportTaskError(ACTUATOR_TASK_INDEX, "执行器任务异常");
        }

        // 等待下一个周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief 处理控制指令
 */
static void processControlCommands() {
    ControlCommandMessage cmdMsg;
    
    // 处理所有待处理的控制指令
    while (safeReceiveFromQueue(xControlCmdQueue, &cmdMsg, 0)) { // 非阻塞接收
        
        LOG_VERBOSE("处理控制指令: %d", cmdMsg.type);
        
        // 根据指令类型执行相应操作
        switch (cmdMsg.type) {
            case CONTROL_CMD_AUTO_IRRIGATION:
                executeAutoIrrigation(cmdMsg);
                break;
                
            case CONTROL_CMD_FORCE_ON:
                executeForceControl(true);
                break;
                
            case CONTROL_CMD_FORCE_OFF:
                executeForceControl(false);
                break;
                
            case CONTROL_CMD_EXIT_FORCE:
                executeExitForceMode();
                break;
                
            case CONTROL_CMD_SET_THRESHOLD:
                executeSetThreshold(cmdMsg);
                break;
                
            case CONTROL_CMD_SYSTEM_STATE:
                executeSystemStateChange(cmdMsg);
                break;
                
            default:
                LOG_WARNING("未知控制指令类型: %d", cmdMsg.type);
                break;
        }
    }
}

/**
 * @brief 执行自动灌溉控制
 */
static void executeAutoIrrigation(const ControlCommandMessage& cmd) {
    // 生命周期管理迁移：使用RTOSSystemManager
    if (!rtosSystemManager || !rtosSystemManager->getControlAgent()) {
        return;
    }

    auto* controlAgent = rtosSystemManager->getControlAgent();
    int moisture = cmd.data.autoIrrigation.moisture;
    
    // 执行自动控制
    bool stateChanged = controlAgent->autoControl(moisture);
    
    if (stateChanged) {
        bool newState = controlAgent->getControlOutputState();
        currentIrrigationState = newState;
        lastStateChangeTime = millis();
        
        LOG_NOTICE("自动灌溉控制: %s (湿度: %d%%)", 
                  newState ? "开启" : "关闭", moisture);
        
        // 更新显示
        updateIrrigationDisplay(newState, "自动");
        
        // 发送状态变化通知
        sendIrrigationStatusUpdate(newState, "auto", moisture);
    }
}

/**
 * @brief 执行强制控制
 */
static void executeForceControl(bool state) {
    // 生命周期管理迁移：使用RTOSSystemManager
    if (!rtosSystemManager || !rtosSystemManager->getControlAgent()) {
        return;
    }

    auto* controlAgent = rtosSystemManager->getControlAgent();
    
    ErrorCode result = controlAgent->forceControl(state);
    
    if (result == ErrorCode::SUCCESS) {
        currentIrrigationState = state;
        lastStateChangeTime = millis();
        
        LOG_NOTICE("强制控制执行: %s", state ? "开启" : "关闭");
        
        // 设置强制控制事件标志
        xEventGroupSetBits(xSystemEventGroup, EVENT_FORCE_CONTROL);
        
        // 更新显示
        updateIrrigationDisplay(state, "强制");
        
        // 发送状态变化通知
        sendIrrigationStatusUpdate(state, "force", 0);
        
    } else {
        LOG_ERROR("强制控制执行失败: %d", static_cast<int>(result));
        reportTaskError(ACTUATOR_TASK_INDEX, "强制控制执行失败");
    }
}

/**
 * @brief 执行退出强制模式
 */
static void executeExitForceMode() {
    // 生命周期管理迁移：使用RTOSSystemManager
    if (!rtosSystemManager || !rtosSystemManager->getControlAgent()) {
        return;
    }

    auto* controlAgent = rtosSystemManager->getControlAgent();
    
    ErrorCode result = controlAgent->exitForceMode();
    
    if (result == ErrorCode::SUCCESS) {
        LOG_NOTICE("退出强制控制模式");
        
        // 清除强制控制事件标志
        xEventGroupClearBits(xSystemEventGroup, EVENT_FORCE_CONTROL);
        
        // 更新显示
        updateIrrigationDisplay(currentIrrigationState, "自动");
        
        // 发送状态变化通知
        sendIrrigationStatusUpdate(currentIrrigationState, "exit_force", 0);
        
    } else {
        LOG_ERROR("退出强制模式失败: %d", static_cast<int>(result));
        reportTaskError(ACTUATOR_TASK_INDEX, "退出强制模式失败");
    }
}

/**
 * @brief 执行设置阈值
 */
static void executeSetThreshold(const ControlCommandMessage& cmd) {
    // 生命周期管理迁移：使用RTOSSystemManager
    if (!rtosSystemManager || !rtosSystemManager->getControlAgent()) {
        return;
    }

    auto* controlAgent = rtosSystemManager->getControlAgent();
    int lowThreshold = cmd.data.threshold.lowThreshold;
    int highThreshold = cmd.data.threshold.highThreshold;
    
    ErrorCode result = controlAgent->setThresholds(lowThreshold, highThreshold);
    
    if (result == ErrorCode::SUCCESS) {
        LOG_NOTICE("阈值设置成功: 低=%d%%, 高=%d%%", lowThreshold, highThreshold);
        
        // 更新显示
        DisplayMessage thresholdMsg = createDisplayMessage(
            DISPLAY_MSG_CONTROL_INFO,
            180,  // 高优先级
            "阈值已更新",
            ("低: " + String(lowThreshold) + "%").c_str(),
            ("高: " + String(highThreshold) + "%").c_str()
        );
        safeSendToQueue(xDisplayMsgQueue, &thresholdMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
        
        // 发送阈值变化通知
        NetworkSendMessage thresholdUpdate = createNetworkMessage(
            NETWORK_MSG_STATUS_UPDATE,
            (R"({"type":"threshold_update","low":)" + String(lowThreshold) +
             ",\"high\":" + String(highThreshold) + "}").c_str(),
            150
        );
        safeSendToQueue(xNetworkSendQueue, &thresholdUpdate, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
        
    } else {
        LOG_ERROR("阈值设置失败: %d", static_cast<int>(result));
        reportTaskError(ACTUATOR_TASK_INDEX, "阈值设置失败");
    }
}

/**
 * @brief 执行系统状态变更
 */
static void executeSystemStateChange(const ControlCommandMessage& cmd) {
    // 生命周期管理迁移：使用RTOSSystemManager
    if (!rtosSystemManager || !rtosSystemManager->getControlAgent()) {
        return;
    }

    auto* controlAgent = rtosSystemManager->getControlAgent();
    SystemState newState = cmd.data.systemState.state;
    ErrorCode errorCode = cmd.data.systemState.errorCode;
    const char* errorMsg = cmd.data.systemState.errorMsg;
    
    ErrorCode result = controlAgent->setSystemState(newState, errorCode);
    
    if (result == ErrorCode::SUCCESS) {
        LOG_NOTICE("系统状态变更: %s", LogManager::getSystemStateStringStatic(newState));
        
        // 如果是错误状态，可能需要停止灌溉
        if (newState >= SystemState::ERROR) {
            if (currentIrrigationState) {
                currentIrrigationState = false;
                lastStateChangeTime = millis();
                
                LOG_WARNING("由于系统错误，停止灌溉");
                updateIrrigationDisplay(false, "错误停止");
            }
        }
        
        // 更新显示 - 修复：使用静态缓冲区避免String拼接
        static char statusStr[64];
        snprintf(statusStr, sizeof(statusStr), "状态: %s", LogManager::getSystemStateStringStatic(newState));

        DisplayMessage stateMsg = createDisplayMessage(
            DISPLAY_MSG_SYSTEM_STATUS,
            200,  // 最高优先级
            statusStr,
            errorMsg && strlen(errorMsg) > 0 ? errorMsg : "系统正常"
        );
        safeSendToQueue(xDisplayMsgQueue, &stateMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
        
    } else {
        LOG_ERROR("系统状态变更失败: %d", static_cast<int>(result));
        reportTaskError(ACTUATOR_TASK_INDEX, "系统状态变更失败");
    }
}

/**
 * @brief 监控执行器状态
 */
static void monitorActuatorStatus() {
    static unsigned long lastMonitorTime = 0;
    unsigned long currentTime = millis();
    
    // 每5秒监控一次执行器状态
    if (currentTime - lastMonitorTime > 5000) {
        lastMonitorTime = currentTime;
        
        // 生命周期管理迁移：使用RTOSSystemManager
        if (rtosSystemManager && rtosSystemManager->getControlAgent()) {
            auto* controlAgent = rtosSystemManager->getControlAgent();
            
            // 检查控制输出状态是否与记录一致
            bool actualState = controlAgent->getControlOutputState();
            if (actualState != currentIrrigationState) {
                LOG_WARNING("执行器状态不一致，实际: %s, 记录: %s", 
                           actualState ? "开启" : "关闭",
                           currentIrrigationState ? "开启" : "关闭");
                
                // 同步状态
                currentIrrigationState = actualState;
                updateIrrigationDisplay(actualState, "状态同步");
            }
            
            // 检查是否长时间处于开启状态（防止过度灌溉）
            if (currentIrrigationState && currentTime - lastStateChangeTime > 300000) { // 5分钟
                LOG_WARNING("灌溉时间过长，强制关闭");
                
                controlAgent->forceControl(false);
                currentIrrigationState = false;
                lastStateChangeTime = currentTime;
                
                updateIrrigationDisplay(false, "超时关闭");
                sendIrrigationStatusUpdate(false, "timeout_stop", 0);
            }
        }
    }
}

/**
 * @brief 更新灌溉状态显示
 * 修复：优化String使用，使用静态缓冲区减少堆栈使用
 */
static void updateIrrigationDisplay(bool state, const char* mode) {
    // 使用静态缓冲区，避免在栈上创建临时字符串
    static char stateStr[32], modeStr[32], timeStr[32];

    snprintf(stateStr, sizeof(stateStr), "灌溉: %s", state ? "开启" : "关闭");
    snprintf(modeStr, sizeof(modeStr), "模式: %s", mode);
    snprintf(timeStr, sizeof(timeStr), "时间: %lus", (millis() - lastStateChangeTime) / 1000);

    DisplayMessage irrigationMsg = createDisplayMessage(
        DISPLAY_MSG_CONTROL_INFO,
        160,  // 高优先级
        stateStr,
        modeStr,
        timeStr
    );

    safeSendToQueue(xDisplayMsgQueue, &irrigationMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
}

/**
 * @brief 发送灌溉状态更新到网络
 * 修复：优化String使用，减少堆栈消耗，防止堆栈溢出
 */
static void sendIrrigationStatusUpdate(bool state, const char* mode, int moisture) {
    // 使用静态缓冲区构建JSON，避免String对象的堆栈分配
    static char statusJson[256];

    if (moisture > 0) {
        snprintf(statusJson, sizeof(statusJson),
                 R"({"type":"irrigation_status","timestamp":%lu,"state":%s,"mode":"%s","moisture":%d})",
                 millis(),
                 state ? "true" : "false",
                 mode,
                 moisture);
    } else {
        snprintf(statusJson, sizeof(statusJson),
                 R"({"type":"irrigation_status","timestamp":%lu,"state":%s,"mode":"%s"})",
                 millis(),
                 state ? "true" : "false",
                 mode);
    }

    NetworkSendMessage statusUpdate = createNetworkMessage(
        NETWORK_MSG_STATUS_UPDATE,
        statusJson,
        180  // 高优先级
    );

    safeSendToQueue(xNetworkSendQueue, &statusUpdate, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
}
