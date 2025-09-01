/*
 * RTOS网络通信任务实现
 * 负责处理WiFi连接、数据发送和指令接收
 * 作者：[Zelas2Xerath]
 * 版本：2.1.3 - RTOS重构版本
 */

#include <RTOSTasks.h>
#include <RTOSCommon.h>
#include <SystemManager/RTOSSystemManager.h>
#include <NetworkManager/MqttManager.h>
#include <LogManager/LogManager.h>
#include <SensorManager/SensorManager.h>
#include <cstring>

// 外部函数声明
extern void updateTaskHeartbeat(int taskIndex);
extern void reportTaskError(int taskIndex, const char* errorMsg);


// 任务索引定义
#define NETWORK_TASK_INDEX 1

// 函数声明
static void checkNetworkConnection();
static void processMqttManager();
static void processSendQueue();
static void sendHeartbeat();
static void publishSystemStatus();
static void checkAndPublishAlerts();

// 网络重连计数器
static uint32_t reconnectCount = 0;
static unsigned long lastConnectionCheck = 0;

/**
 * @brief 网络通信任务
 * 处理网络连接、数据发送和指令接收
 */
void vNetworkTask(void *pvParameters) {
    LOG_NOTICE("网络任务启动");
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    constexpr TickType_t xFrequency = pdMS_TO_TICKS(TASK_PERIOD_NETWORK);
    
    // 等待系统初始化完成
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    while (true) {
        // 更新任务心跳
        updateTaskHeartbeat(NETWORK_TASK_INDEX);
        
        // 获取网络互斥锁
        if (xSemaphoreTake(xNetworkMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
            try {
                // 检查网络连接状态
                checkNetworkConnection();

                // 处理MQTT管理器循环（重构后新增）
                processMqttManager();

                // 处理发送队列中的消息
                processSendQueue();

                // 发送心跳包
                sendHeartbeat();

                // 发布系统状态
                publishSystemStatus();

                // 检查并发布告警信息
                checkAndPublishAlerts();
                
            } catch (...) {
                reportTaskError(NETWORK_TASK_INDEX, "网络任务异常");
            }
            
            // 释放互斥锁
            xSemaphoreGive(xNetworkMutex);
        } else {
            reportTaskError(NETWORK_TASK_INDEX, "获取网络互斥锁超时");
        }
        
        // 等待下一个周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief 检查网络连接状态
 */
static void checkNetworkConnection() {
    // 生命周期管理迁移：使用RTOSSystemManager进行网络连接检查
    if (!rtosSystemManager || !rtosSystemManager->getNetworkManager()) {
        return;
    }

    auto* networkManager = rtosSystemManager->getNetworkManager();
    unsigned long currentTime = millis();
    
    // 每30秒检查一次连接状态
    if (currentTime - lastConnectionCheck > 30000) {
        lastConnectionCheck = currentTime;
        
        ErrorCode result = networkManager->checkConnection();
        
        if (result == ErrorCode::SUCCESS) {
            // 连接正常，设置事件标志
            if (networkManager->isWiFiConnected()) {
                xEventGroupSetBits(xSystemEventGroup, EVENT_WIFI_CONNECTED);
            }
            // 检查MQTT连接状态（通过RTOSSystemManager）
            if (rtosSystemManager && rtosSystemManager->isServerConnected()) {
                xEventGroupSetBits(xSystemEventGroup, EVENT_SERVER_CONNECTED);
            }
            
            // 重置重连计数器
            reconnectCount = 0;
            
        } else {
            // 连接异常，清除事件标志
            xEventGroupClearBits(xSystemEventGroup, EVENT_WIFI_CONNECTED | EVENT_SERVER_CONNECTED);
            
            // 尝试重连
            reconnectCount++;
            LOG_WARNING("网络连接异常，尝试重连 (第%d次)", reconnectCount);
            
            // 如果重连次数过多，报告严重错误
            if (reconnectCount > 10) {
                reportTaskError(NETWORK_TASK_INDEX, "网络重连失败次数过多");
                
                // 发送系统错误事件
                SystemEventMessage errorEvent = createSystemEventMessage(
                    Event::NetworkError,
                    static_cast<int>(result),
                    static_cast<int>(reconnectCount),
                    "网络连接持续失败"
                );
                safeSendToQueue(xEventQueue, &errorEvent, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
            }
        }
    }
}

/**
 * @brief 处理MQTT管理器循环
 */
static void processMqttManager() {
    if (!rtosSystemManager) {
        return;
    }

    auto* mqttManager = rtosSystemManager->getMqttManager();

    if (mqttManager) {
        // 执行MQTT管理器的循环处理
        ErrorCode result = mqttManager->loop();
        if (result != ErrorCode::SUCCESS) {
            LOG_WARNING("MQTT管理器循环处理失败: %d", static_cast<int>(result));
        }
    }
}

/**
 * @brief 处理发送队列中的消息
 */
static void processSendQueue() {
    // 生命周期管理迁移：使用RTOSSystemManager进行消息发送队列处理
    if (!rtosSystemManager || !rtosSystemManager->getNetworkManager()) {
        return;
    }

    NetworkSendMessage message;
    
    // 检查是否有消息需要发送
    while (safeReceiveFromQueue(xNetworkSendQueue, &message, 0)) { // 非阻塞接收
        
        // 检查MQTT连接状态（通过RTOSSystemManager获取MqttManager）
        if (!rtosSystemManager) {
            LOG_WARNING("RTOSSystemManager不可用，消息发送失败");
            continue;
        }

        auto* mqttManager = rtosSystemManager->getMqttManager();
        if (!mqttManager || !mqttManager->isConnected()) {
            LOG_WARNING("MQTT未连接，消息发送失败");
            continue;
        }

        // 通过MQTT管理器发送消息
        ErrorCode result = mqttManager->publishSensorData(message.payload);
        
        if (result == ErrorCode::SUCCESS) {
            LOG_VERBOSE("发送网络消息成功: %s", message.payload);
        } else {
            LOG_WARNING("发送网络消息失败: %d", static_cast<int>(result));
            
            // 如果发送失败且还有重试次数，重新放入队列
            if (message.retryCount > 0) {
                message.retryCount--;
                safeSendToQueue(xNetworkSendQueue, &message, 0);
                LOG_VERBOSE("消息重新入队，剩余重试次数: %d", message.retryCount);
            }
        }
        
        // 避免发送过快
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief 发送心跳包
 */
static void sendHeartbeat() {
    static unsigned long lastHeartbeat = 0;
    unsigned long currentTime = millis();
    
    // 每60秒发送一次心跳
    if (currentTime - lastHeartbeat > 60000) {
        lastHeartbeat = currentTime;
        
        // 创建心跳消息
        String heartbeatData = R"({"type":"heartbeat","timestamp":)" + String(currentTime) +
                              ",\"uptime\":" + String(currentTime) + 
                              ",\"freeHeap\":" + String(ESP.getFreeHeap()) + "}";
        
        NetworkSendMessage heartbeat = createNetworkMessage(
            NETWORK_MSG_HEARTBEAT,
            heartbeatData.c_str(),
            128  // 中等优先级
        );
        
        safeSendToQueue(xNetworkSendQueue, &heartbeat, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS));
        LOG_VERBOSE("发送心跳包");
    }
}

/**
 * @brief 发布系统状态
 */
static void publishSystemStatus() {
    static unsigned long lastStatusPublish = 0;
    unsigned long currentTime = millis();

    // 每5分钟发布一次系统状态
    if (currentTime - lastStatusPublish > 300000) {
        lastStatusPublish = currentTime;

        // 检查MQTT连接状态
        if (!rtosSystemManager) {
            return;
        }

        auto* mqttManager = rtosSystemManager->getMqttManager();
        if (!mqttManager || !mqttManager->isConnected()) {
            LOG_WARNING("MQTT未连接，无法发布系统状态");
            return;
        }

        // 创建系统状态消息
        String statusMessage = mqttManager->createSystemStatusMessage();

        // 发布系统状态
        ErrorCode result = mqttManager->publishSystemStatus(statusMessage);
        if (result == ErrorCode::SUCCESS) {
            LOG_NOTICE("系统状态发布成功");
        } else {
            LOG_WARNING("系统状态发布失败: %d", static_cast<int>(result));
        }
    }
}

/**
 * @brief 检查并发布告警信息
 */
static void checkAndPublishAlerts() {
    static unsigned long lastAlertCheck = 0;
    unsigned long currentTime = millis();

    // 每2分钟检查一次告警条件
    if (currentTime - lastAlertCheck > 120000) {
        lastAlertCheck = currentTime;

        if (!rtosSystemManager) {
            return;
        }

        auto* mqttManager = rtosSystemManager->getMqttManager();
        if (!mqttManager || !mqttManager->isConnected()) {
            return;
        }

        // 检查内存告警
        uint32_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < 15360) { // 小于15KB发出告警
            String alertMessage = mqttManager->createAlertMessage(
                "warning",
                "memory",
                "系统内存不足: " + String(freeHeap) + "字节",
                "system"
            );

            ErrorCode result = mqttManager->publishAlert(alertMessage);
            if (result == ErrorCode::SUCCESS) {
                LOG_NOTICE("内存告警发布成功");
            }
        }

        // 检查传感器告警（通过系统管理器获取传感器数据）
        // 检查土壤湿度告警
        SMS* sms = rtosSystemManager->getSMS();
        if (sms != nullptr) {
            try {
                if (sms->getLastErrorCode() == ErrorCode::SUCCESS) {
                    int moisture = sms->readSoilMoisture();
                    if (moisture < 20 && moisture > 0) { // 土壤湿度过低且数据有效
                        String alertMessage = mqttManager->createAlertMessage(
                            "warning",
                            "soil_moisture",
                            "土壤湿度过低: " + String(moisture) + "%",
                            "SMS"
                        );
                        ErrorCode result = mqttManager->publishAlert(alertMessage);
                        if (result == ErrorCode::SUCCESS) {
                            LOG_NOTICE("土壤湿度告警发布成功");
                        }
                    }
                }
            } catch (...) {
                LOG_WARNING("土壤湿度传感器告警检查异常");
            }
        }

        // 检查环境传感器告警
        ECS* ecs = rtosSystemManager->getEnvironmentSensor();
        if (ecs != nullptr) {
            try {
                if (ecs->getLastErrorCode() == ErrorCode::SUCCESS) {
                    float temperature = ecs->readTemperature();
                    if (temperature > 40.0f && temperature < 100.0f) { // 温度过高且数据有效
                        String alertMessage = mqttManager->createAlertMessage(
                            "warning",
                            "temperature",
                            "环境温度过高: " + String(temperature, 1) + "°C",
                            "ECS"
                        );
                        ErrorCode result = mqttManager->publishAlert(alertMessage);
                        if (result == ErrorCode::SUCCESS) {
                            LOG_NOTICE("温度告警发布成功");
                        }
                    }
                }
            } catch (...) {
                LOG_WARNING("环境传感器告警检查异常");
            }
        }
    }
}

/**
 * @brief 发送传感器数据到网络
 *
 * @param sensorData 传感器数据消息
 */
void sendSensorDataToNetwork(const SensorDataMessage& sensorData) {
    // 获取MQTT管理器实例
    if (!rtosSystemManager) {
        LOG_WARNING("RTOSSystemManager不可用，无法发送传感器数据");
        return;
    }

    auto* mqttManager = rtosSystemManager->getMqttManager();
    if (!mqttManager) {
        LOG_WARNING("MQTT管理器不可用，无法发送传感器数据");
        return;
    }

    String jsonData;

    // 使用MqttManager的新格式化方法根据传感器类型构建符合规范的JSON数据
    switch (sensorData.type) {
        case SENSOR_MSG_SOIL_MOISTURE:
            jsonData = mqttManager->createSingleSoilMoistureMessage(
                sensorData.data.soilData.moisture,
                sensorData.data.soilData.readCount
            );
            break;

        case SENSOR_MSG_ENVIRONMENT:
            jsonData = mqttManager->createSingleEnvironmentMessage(
                sensorData.data.envData.temperature,
                sensorData.data.envData.humidity,
                sensorData.data.envData.pressure
            );
            break;

        case SENSOR_MSG_LIGHT:
            jsonData = mqttManager->createSingleLightMessage(
                sensorData.data.lightData.intensity,
                sensorData.data.lightData.isDaytime
            );
            break;

        case SENSOR_MSG_WATER_LEVEL:
            jsonData = mqttManager->createSingleWaterLevelMessage(
                sensorData.data.waterData.level,
                sensorData.data.waterData.isLowLevel
            );
            break;

        case SENSOR_MSG_CAS_SOIL:
            jsonData = mqttManager->createSingleCASMessage(
                sensorData.data.casData.temperature,
                sensorData.data.casData.moisture,
                sensorData.data.casData.ph,
                sensorData.data.casData.ec,
                sensorData.data.casData.nitrogen,
                sensorData.data.casData.phosphorus,
                sensorData.data.casData.potassium
            );
            break;

        case SENSOR_MSG_ERROR:
            jsonData = mqttManager->createSensorErrorMessage(
                sensorData.data.errorData.errorCode,
                String(sensorData.data.errorData.errorMsg)
            );
            break;

        default:
            LOG_WARNING("未知传感器消息类型: %d", sensorData.type);
            return; // 未知类型，不发送
    }

    // 创建网络发送消息
    NetworkSendMessage networkMsg = createNetworkMessage(
        NETWORK_MSG_SENSOR_DATA,
        jsonData.c_str(),
        200  // 高优先级
    );

    // 发送到网络队列
    if (!safeSendToQueue(xNetworkSendQueue, &networkMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS))) {
        LOG_WARNING("传感器数据发送到网络队列失败");
    }
}
