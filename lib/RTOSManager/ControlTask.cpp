/*
 * RTOS控制逻辑任务实现
 * 负责处理业务逻辑、自动控制和系统状态管理
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
extern void sendSensorDataToNetwork(const SensorDataMessage& sensorData);

// 任务索引定义
#define CONTROL_TASK_INDEX 2

// 函数声明
static void processSensorData();
static void processSystemEvents();
static void executeControlLogic();
static void updateSystemStatus();
static void processSoilMoistureData(const SensorDataMessage& msg);
static void processEnvironmentData(const SensorDataMessage& msg);
static void processLightData(const SensorDataMessage& msg);
static void processWaterLevelData(const SensorDataMessage& msg);
static void processCASData(const SensorDataMessage& msg);
static void processSensorError(const SensorDataMessage& msg);
static void updateDisplayWithSensorData(const SensorDataMessage& msg);
static void handleThresholdChangedEvent(const SystemEventMessage& event);
static void handleNetworkErrorEvent(const SystemEventMessage& event);
static void handleSensorErrorEvent(const SystemEventMessage& event);
static void handleWaterLevelAlertEvent(const SystemEventMessage& event);
static void handleSoilParameterAlertEvent(const SystemEventMessage& event);
static void handleEnvironmentWarningEvent(const SystemEventMessage& event);

// 控制状态变量
static int currentMoisture = 0;
static unsigned long lastControlTime = 0;

/**
 * @brief 控制逻辑任务
 * 处理传感器数据、执行控制逻辑、生成控制指令
 * 修复：增加堆栈监控，防止堆栈溢出
 */
void vControlTask(void *pvParameters) {
    LOG_NOTICE("控制任务启动 - 堆栈大小: %d字节", TASK_STACK_SIZE_CONTROL);

    TickType_t xLastWakeTime = xTaskGetTickCount();
    constexpr TickType_t xFrequency = pdMS_TO_TICKS(TASK_PERIOD_CONTROL);

    // 等待系统初始化完成
    vTaskDelay(pdMS_TO_TICKS(4000));

    // 堆栈监控变量
    static unsigned long lastStackCheck = 0;

    while (true) {
        // 更新任务心跳
        updateTaskHeartbeat(CONTROL_TASK_INDEX);

        // 定期检查堆栈使用情况（每60秒检查一次）
        unsigned long currentTime = millis();
        if (currentTime - lastStackCheck > 60000) {
            lastStackCheck = currentTime;
            UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(nullptr);
            LOG_VERBOSE("控制任务堆栈剩余: %u字节", stackHighWaterMark * sizeof(StackType_t));

            // 如果堆栈使用超过80%，发出警告
            if (stackHighWaterMark * sizeof(StackType_t) < (TASK_STACK_SIZE_CONTROL * 0.2)) {
                LOG_WARNING("控制任务堆栈使用率过高！剩余: %u字节",
                           stackHighWaterMark * sizeof(StackType_t));
            }
        }

        try {
            // 处理传感器数据
            processSensorData();

            // 处理系统事件
            processSystemEvents();

            // 执行控制逻辑
            executeControlLogic();

            // 更新系统状态
            updateSystemStatus();

        } catch (...) {
            reportTaskError(CONTROL_TASK_INDEX, "控制任务异常");
        }

        // 等待下一个周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief 处理传感器数据
 */
static void processSensorData() {
    SensorDataMessage sensorMsg;
    
    // 处理所有待处理的传感器数据
    while (safeReceiveFromQueue(xSensorDataQueue, &sensorMsg, 0)) { // 非阻塞接收
        
        // 根据传感器类型处理数据
        switch (sensorMsg.type) {
            case SENSOR_MSG_SOIL_MOISTURE:
                processSoilMoistureData(sensorMsg);
                break;
                
            case SENSOR_MSG_ENVIRONMENT:
                processEnvironmentData(sensorMsg);
                break;
                
            case SENSOR_MSG_LIGHT:
                processLightData(sensorMsg);
                break;
                
            case SENSOR_MSG_WATER_LEVEL:
                processWaterLevelData(sensorMsg);
                break;
                
            case SENSOR_MSG_CAS_SOIL:
                processCASData(sensorMsg);
                break;
                
            case SENSOR_MSG_ERROR:
                processSensorError(sensorMsg);
                break;
                
            default:
                LOG_WARNING("未知传感器消息类型: %d", sensorMsg.type);
                break;
        }
        
        // 将传感器数据转发到网络任务
        sendSensorDataToNetwork(sensorMsg);
        
        // 更新显示
        updateDisplayWithSensorData(sensorMsg);
    }
}

/**
 * @brief 处理土壤湿度数据
 */
static void processSoilMoistureData(const SensorDataMessage& msg) {
    currentMoisture = msg.data.soilData.moisture;
    
    LOG_VERBOSE("处理土壤湿度数据: %d%%", currentMoisture);
    
    // 检查是否需要自动控制 - 生命周期管理迁移：使用RTOSSystemManager
    if (rtosSystemManager && rtosSystemManager->getControlAgent()) {
        auto* controlAgent = rtosSystemManager->getControlAgent();
        
        // 只有在非强制模式下才执行自动控制
        if (!controlAgent->isInForceMode() && controlAgent->isIrrigationEnabled()) {
            // 创建自动灌溉控制指令
            ControlCommandMessage controlCmd = createControlMessage(CONTROL_CMD_AUTO_IRRIGATION, &currentMoisture);
            
            // 发送到执行器任务
            if (!safeSendToQueue(xControlCmdQueue, &controlCmd, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS))) {
                LOG_WARNING("发送自动灌溉指令失败");
            }
        }
    }
}

/**
 * @brief 处理环境数据
 */
static void processEnvironmentData(const SensorDataMessage& msg) {
    float temperature = msg.data.envData.temperature;
    float humidity = msg.data.envData.humidity;
    float pressure = msg.data.envData.pressure;
    
    LOG_VERBOSE("处理环境数据: 温度%.1f°C, 湿度%.1f%%, 气压%.1fhPa", 
               temperature, humidity, pressure);
    
    // 检查环境异常
    if (temperature > 40 || temperature < -10) {
        LOG_WARNING("环境温度异常: %.1f°C", temperature);
        
        // 发送系统警告事件
        SystemEventMessage warningEvent = createSystemEventMessage(
            Event::EnvironmentWarning,
            static_cast<int>(temperature * 10),
            0,
            "环境温度异常"
        );
        safeSendToQueue(xEventQueue, &warningEvent, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
    }
}

/**
 * @brief 处理光照数据
 */
static void processLightData(const SensorDataMessage& msg) {
    float intensity = msg.data.lightData.intensity;
    bool isDaytime = msg.data.lightData.isDaytime;
    
    LOG_VERBOSE("处理光照数据: %.1f lux, %s", intensity, isDaytime ? "白天" : "夜晚");
    
    // 根据光照情况调整系统行为
    // 例如：夜间降低传感器采集频率等
}

/**
 * @brief 处理水位数据
 */
static void processWaterLevelData(const SensorDataMessage& msg) {
    float level = msg.data.waterData.level;
    bool isLowLevel = msg.data.waterData.isLowLevel;
    
    LOG_VERBOSE("处理水位数据: %.1fcm, %s", level, isLowLevel ? "低水位" : "正常");
    
    // 如果水位过低，禁用灌溉功能 - 生命周期管理迁移：使用RTOSSystemManager
    if (isLowLevel && rtosSystemManager && rtosSystemManager->getControlAgent()) {
        LOG_WARNING("水位过低，禁用灌溉功能");
        
        // 创建系统状态数据
        SystemStateData stateData{};
        stateData.state = SystemState::WARNING;
        stateData.errorCode = ErrorCode::WATER_LEVEL_LOW;
        strncpy(stateData.errorMsg, "水位过低", 63);
        stateData.errorMsg[63] = '\0';

        // 创建系统状态变更指令
        ControlCommandMessage stateCmd = createControlMessage(CONTROL_CMD_SYSTEM_STATE, &stateData);
        
        safeSendToQueue(xControlCmdQueue, &stateCmd, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
        
        // 发送低水位警报事件
        SystemEventMessage alertEvent = createSystemEventMessage(
            Event::WaterLevelAlert,
            static_cast<int>(level * 10),
            0,
            "水位过低警报"
        );
        safeSendToQueue(xEventQueue, &alertEvent, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
    }
}

/**
 * @brief 处理CAS传感器数据
 * 修复：优化String使用，减少堆栈消耗，防止堆栈溢出
 */
static void processCASData(const SensorDataMessage& msg) {
    const SoilSensorData& casData = msg.data.casData;

    LOG_VERBOSE("处理CAS数据: 温度%.1f°C, 湿度%.1f%%, pH%.1f, EC%.1f",
               casData.temperature, casData.moisture, casData.ph, casData.ec);

    // 检查土壤参数是否异常 - 使用char数组替代String，减少堆栈使用
    bool hasAlarm = false;
    static char alarmMsg[128];  // 使用静态变量，避免栈分配
    alarmMsg[0] = '\0';  // 清空字符串

    if (casData.ph < 5.5 || casData.ph > 8.5) {
        hasAlarm = true;
        char phStr[32];
        snprintf(phStr, sizeof(phStr), "pH异常(%.1f) ", casData.ph);
        strncat(alarmMsg, phStr, sizeof(alarmMsg) - strlen(alarmMsg) - 1);
    }

    if (casData.ec > 3000) {
        hasAlarm = true;
        char ecStr[32];
        snprintf(ecStr, sizeof(ecStr), "EC过高(%.0f) ", casData.ec);
        strncat(alarmMsg, ecStr, sizeof(alarmMsg) - strlen(alarmMsg) - 1);
    }

    if (hasAlarm) {
        LOG_WARNING("CAS传感器报警: %s", alarmMsg);

        // 发送土壤参数警报事件
        SystemEventMessage alarmEvent = createSystemEventMessage(
            Event::SoilParameterAlert,
            static_cast<int>(casData.ph * 10),
            casData.ec,
            alarmMsg
        );
        safeSendToQueue(xEventQueue, &alarmEvent, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
    }
}

/**
 * @brief 处理传感器错误
 */
static void processSensorError(const SensorDataMessage& msg) {
    ErrorCode errorCode = msg.data.errorData.errorCode;
    const char* errorMsg = msg.data.errorData.errorMsg;
    
    LOG_ERROR("传感器错误: %s (错误码: %d)", errorMsg, static_cast<int>(errorCode));
    
    // 发送传感器错误事件
    SystemEventMessage errorEvent = createSystemEventMessage(
        Event::SensorError,
        static_cast<int>(errorCode),
        0,
        errorMsg
    );
    safeSendToQueue(xEventQueue, &errorEvent, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
}

/**
 * @brief 处理系统事件
 */
static void processSystemEvents() {
    SystemEventMessage eventMsg;
    
    // 处理所有待处理的系统事件
    while (safeReceiveFromQueue(xEventQueue, &eventMsg, 0)) { // 非阻塞接收
        
        LOG_NOTICE("处理系统事件: %d, 参数: %d, %d, 消息: %s", 
                  static_cast<int>(eventMsg.eventType), 
                  eventMsg.param1, eventMsg.param2, eventMsg.message);
        
        // 根据事件类型执行相应处理
        switch (eventMsg.eventType) {
            case Event::ThresholdChanged:
                handleThresholdChangedEvent(eventMsg);
                break;
                
            case Event::NetworkError:
                handleNetworkErrorEvent(eventMsg);
                break;
                
            case Event::SensorError:
                handleSensorErrorEvent(eventMsg);
                break;
                
            case Event::WaterLevelAlert:
                handleWaterLevelAlertEvent(eventMsg);
                break;
                
            case Event::SoilParameterAlert:
                handleSoilParameterAlertEvent(eventMsg);
                break;
                
            case Event::EnvironmentWarning:
                handleEnvironmentWarningEvent(eventMsg);
                break;
                
            default:
                LOG_VERBOSE("未处理的事件类型: %d", static_cast<int>(eventMsg.eventType));
                break;
        }
    }
}

/**
 * @brief 执行控制逻辑
 */
static void executeControlLogic() {
    unsigned long currentTime = millis();
    
    // 每10秒执行一次控制逻辑检查
    if (currentTime - lastControlTime > 10000) {
        lastControlTime = currentTime;
        
        // 生命周期管理迁移：使用RTOSSystemManager进行控制逻辑管理
        if (rtosSystemManager && rtosSystemManager->getControlAgent()) {
            auto* controlAgent = rtosSystemManager->getControlAgent();
            
            // 更新状态机
            controlAgent->updateStateMachine();
            
            // 检查系统状态
            SystemState currentState = controlAgent->getCurrentState();
            if (currentState != SystemState::NORMAL) {
                LOG_VERBOSE("系统状态: %s", 
                           LogManager::getSystemStateStringStatic(currentState));
            }
        }
    }
}

/**
 * @brief 更新系统状态
 * 修复：优化字符串操作，减少堆栈使用，防止堆栈溢出
 */
static void updateSystemStatus() {
    // 定期更新系统运行状态
    static unsigned long lastStatusUpdate = 0;
    unsigned long currentTime = millis();

    if (currentTime - lastStatusUpdate > 30000) { // 每30秒更新一次
        lastStatusUpdate = currentTime;

        // 使用静态缓冲区，避免在栈上创建临时字符串
        static char runtimeStr[32];
        static char memoryStr[32];
        static char moistureStr[32];

        snprintf(runtimeStr, sizeof(runtimeStr), "运行时间: %lus", currentTime / 1000);
        snprintf(memoryStr, sizeof(memoryStr), "空闲内存: %uB", ESP.getFreeHeap());
        snprintf(moistureStr, sizeof(moistureStr), "湿度: %d%%", currentMoisture);

        // 创建系统状态显示消息
        DisplayMessage statusMsg = createDisplayMessage(
            DISPLAY_MSG_SYSTEM_STATUS,
            100,  // 中等优先级
            "系统运行正常",
            runtimeStr,
            memoryStr,
            moistureStr
        );

        safeSendToQueue(xDisplayMsgQueue, &statusMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
    }
}

/**
 * @brief 更新显示器显示传感器数据
 * 修复：优化字符串操作，使用静态缓冲区减少堆栈使用
 */
static void updateDisplayWithSensorData(const SensorDataMessage& msg) {
    DisplayMessage displayMsg;

    // 使用静态缓冲区，避免在栈上创建临时字符串
    static char line1[32], line2[32], line3[32];

    switch (msg.type) {
        case SENSOR_MSG_SOIL_MOISTURE:
            snprintf(line1, sizeof(line1), "湿度: %d%%", msg.data.soilData.moisture);
            snprintf(line2, sizeof(line2), "读取: %d次", msg.data.soilData.readCount);
            displayMsg = createDisplayMessage(
                DISPLAY_MSG_SENSOR_DATA,
                150,  // 较高优先级
                line1,
                line2
            );
            break;

        case SENSOR_MSG_ENVIRONMENT:
            snprintf(line1, sizeof(line1), "温度: %.1f°C", msg.data.envData.temperature);
            snprintf(line2, sizeof(line2), "湿度: %.1f%%", msg.data.envData.humidity);
            snprintf(line3, sizeof(line3), "气压: %.1fhPa", msg.data.envData.pressure);
            displayMsg = createDisplayMessage(
                DISPLAY_MSG_SENSOR_DATA,
                120,
                line1,
                line2,
                line3
            );
            break;

        default:
            return; // 其他类型暂不显示
    }

    safeSendToQueue(xDisplayMsgQueue, &displayMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
}

// ==================== 事件处理函数实现 ====================

/**
 * @brief 处理阈值变更事件
 */
static void handleThresholdChangedEvent(const SystemEventMessage& event) {
    LOG_NOTICE("处理阈值变更事件: 低=%d, 高=%d", event.param1, event.param2);

    // 更新显示
    DisplayMessage thresholdMsg = createDisplayMessage(
        DISPLAY_MSG_CONTROL_INFO,
        150,
        "阈值已更新",
        ("低: " + String(event.param1) + "%").c_str(),
        ("高: " + String(event.param2) + "%").c_str()
    );
    safeSendToQueue(xDisplayMsgQueue, &thresholdMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
}

/**
 * @brief 处理网络错误事件
 */
static void handleNetworkErrorEvent(const SystemEventMessage& event) {
    LOG_WARNING("处理网络错误事件: %s", event.message);

    // 清除网络连接事件标志
    xEventGroupClearBits(xSystemEventGroup, EVENT_WIFI_CONNECTED | EVENT_SERVER_CONNECTED);

    // 更新显示
    DisplayMessage networkMsg = createDisplayMessage(
        DISPLAY_MSG_ERROR_INFO,
        200,
        "网络错误",
        event.message,
        "检查网络连接"
    );
    safeSendToQueue(xDisplayMsgQueue, &networkMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
}

/**
 * @brief 处理传感器错误事件
 */
static void handleSensorErrorEvent(const SystemEventMessage& event) {
    LOG_WARNING("处理传感器错误事件: %s", event.message);

    // 清除传感器就绪事件标志
    xEventGroupClearBits(xSystemEventGroup, EVENT_SENSOR_READY);

    // 更新显示
    DisplayMessage sensorMsg = createDisplayMessage(
        DISPLAY_MSG_ERROR_INFO,
        180,
        "传感器错误",
        event.message,
        "检查传感器连接"
    );
    safeSendToQueue(xDisplayMsgQueue, &sensorMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
}

/**
 * @brief 处理水位警报事件
 */
static void handleWaterLevelAlertEvent(const SystemEventMessage& event) {
    LOG_WARNING("处理水位警报事件: %s", event.message);

    // 禁用灌溉功能 - 生命周期管理迁移：使用RTOSSystemManager
    if (rtosSystemManager && rtosSystemManager->getControlAgent()) {
        rtosSystemManager->getControlAgent()->setIrrigationEnabled(false);
    }

    // 更新显示
    DisplayMessage waterMsg = createDisplayMessage(
        DISPLAY_MSG_ERROR_INFO,
        220,
        "水位警报",
        event.message,
        "灌溉已禁用"
    );
    safeSendToQueue(xDisplayMsgQueue, &waterMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
}

/**
 * @brief 处理土壤参数警报事件
 */
static void handleSoilParameterAlertEvent(const SystemEventMessage& event) {
    LOG_WARNING("处理土壤参数警报事件: %s", event.message);

    // 更新显示
    DisplayMessage soilMsg = createDisplayMessage(
        DISPLAY_MSG_ERROR_INFO,
        160,
        "土壤参数异常",
        event.message,
        "检查土壤状态"
    );
    safeSendToQueue(xDisplayMsgQueue, &soilMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
}

/**
 * @brief 处理环境警告事件
 */
static void handleEnvironmentWarningEvent(const SystemEventMessage& event) {
    LOG_WARNING("处理环境警告事件: %s", event.message);

    // 更新显示
    DisplayMessage envMsg = createDisplayMessage(
        DISPLAY_MSG_ERROR_INFO,
        140,
        "环境警告",
        event.message,
        "检查环境条件"
    );
    safeSendToQueue(xDisplayMsgQueue, &envMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
}
