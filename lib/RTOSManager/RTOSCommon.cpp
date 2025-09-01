/*
 * RTOS通用函数实现
 * 提供消息创建、队列操作等通用功能
 * 作者：[Zelas2Xerath]
 * 版本：2.1.3 - RTOS重构版本
 */

#include <RTOSCommon.h>
#include <RTOSTasks.h>  // 包含队列句柄声明
#include <LogManager/LogManager.h>
#include <cstring>  // for memset, strncpy

// ==================== 消息创建函数 ====================

/**
 * @brief 创建传感器数据消息
 */
SensorDataMessage createSensorMessage(const SensorMessageType type, const void* data) {
    SensorDataMessage message;  // 使用默认构造函数
    message.type = type;
    message.timestamp = millis();
    
    if (data != nullptr) {
        switch (type) {
            case SENSOR_MSG_SOIL_MOISTURE: {
                auto soilData = static_cast<const SoilMoistureData*>(data);
                message.data.soilData = *soilData;
                break;
            }

            case SENSOR_MSG_ENVIRONMENT: {
                auto envData = static_cast<const EnvironmentData*>(data);
                message.data.envData = *envData;
                break;
            }

            case SENSOR_MSG_LIGHT: {
                auto lightData = static_cast<const LightData*>(data);
                message.data.lightData = *lightData;
                break;
            }

            case SENSOR_MSG_WATER_LEVEL: {
                auto waterData = static_cast<const WaterLevelData*>(data);
                message.data.waterData = *waterData;
                break;
            }

            case SENSOR_MSG_CAS_SOIL: {
                auto casData = static_cast<const SoilSensorData*>(data);
                message.data.casData = *casData;
                break;
            }

            case SENSOR_MSG_ERROR: {
                auto errorData = static_cast<const SensorErrorData*>(data);
                message.data.errorData = *errorData;
                break;
            }
        }
    }
    
    return message;
}

/**
 * @brief 创建网络发送消息
 */
NetworkSendMessage createNetworkMessage(const NetworkMessageType type, const char* payload, const uint8_t priority) {
    NetworkSendMessage message;  // 使用默认构造函数
    message.type = type;
    message.timestamp = millis();
    message.priority = priority;
    message.retryCount = 3; // 默认重试3次
    
    if (payload != nullptr) {
        strncpy(message.payload, payload, 1023);
        message.payload[1023] = '\0';
    }
    
    return message;
}

/**
 * @brief 创建控制指令消息
 */
ControlCommandMessage createControlMessage(const ControlCommandType type, const void* data) {
    ControlCommandMessage message;  // 使用默认构造函数
    message.type = type;
    message.timestamp = millis();
    
    if (data != nullptr) {
        switch (type) {
            case CONTROL_CMD_AUTO_IRRIGATION: {
                const auto moisture = static_cast<const int*>(data);
                message.data.autoIrrigation.moisture = *moisture;
                break;
            }
            
            case CONTROL_CMD_FORCE_ON:
            case CONTROL_CMD_FORCE_OFF: {
                const auto state = static_cast<const bool*>(data);
                message.data.forceControl.state = *state;
                break;
            }
            
            case CONTROL_CMD_SET_THRESHOLD: {
                auto threshold = static_cast<const ThresholdData*>(data);
                message.data.threshold = *threshold;
                break;
            }

            case CONTROL_CMD_SYSTEM_STATE: {
                auto stateData = static_cast<const SystemStateData*>(data);
                message.data.systemState = *stateData;
                break;
            }

            case CONTROL_CMD_EXIT_FORCE:
                // 退出强制模式不需要额外数据
                // break;

            default:
                // 未知指令类型，不设置数据
                break;
        }
    }
    
    return message;
}

/**
 * @brief 创建显示消息
 */
DisplayMessage createDisplayMessage(const DisplayMessageType type, const uint8_t priority,
                                   const char* line1, const char* line2,
                                   const char* line3, const char* line4) {
    DisplayMessage message;  // 使用默认构造函数
    message.type = type;
    message.timestamp = millis();
    message.priority = priority;
    
    if (line1 != nullptr) {
        strncpy(message.line1, line1, 31);
        message.line1[31] = '\0';
    }
    
    if (line2 != nullptr) {
        strncpy(message.line2, line2, 31);
        message.line2[31] = '\0';
    }
    
    if (line3 != nullptr) {
        strncpy(message.line3, line3, 31);
        message.line3[31] = '\0';
    }
    
    if (line4 != nullptr) {
        strncpy(message.line4, line4, 31);
        message.line4[31] = '\0';
    }
    
    return message;
}

/**
 * @brief 创建系统事件消息
 */
SystemEventMessage createSystemEventMessage(const Event eventType, const int param1, const int param2, const char* message) {
    SystemEventMessage eventMsg;  // 使用默认构造函数
    eventMsg.eventType = eventType;
    eventMsg.timestamp = millis();
    eventMsg.param1 = param1;
    eventMsg.param2 = param2;
    
    if (message != nullptr) {
        strncpy(eventMsg.message, message, 63);
        eventMsg.message[63] = '\0';
    }
    
    return eventMsg;
}

// ==================== 队列操作函数 ====================

/**
 * @brief 安全发送消息到队列
 */
bool safeSendToQueue(QueueHandle_t queue, void* message, TickType_t timeout) {
    if (queue == nullptr || message == nullptr) {
        LOG_ERROR("队列或消息为空");
        return false;
    }
    
    BaseType_t result = xQueueSend(queue, message, timeout);
    
    if (result != pdPASS) {
        if (timeout == 0) {
            LOG_VERBOSE("队列发送失败（非阻塞）");
        } else {
            LOG_WARNING("队列发送超时");
        }
        return false;
    }
    
    return true;
}

/**
 * @brief 安全从队列接收消息
 */
bool safeReceiveFromQueue(QueueHandle_t queue, void* message, TickType_t timeout) {
    if (queue == nullptr || message == nullptr) {
        LOG_ERROR("队列或消息缓冲区为空");
        return false;
    }
    
    BaseType_t result = xQueueReceive(queue, message, timeout);
    
    if (result != pdPASS) {
        if (timeout == 0) {
            // 非阻塞接收失败是正常的，不记录日志
        } else {
            LOG_VERBOSE("队列接收超时");
        }
        return false;
    }
    
    return true;
}

// ==================== 辅助函数 ====================

/**
 * @brief 获取消息类型字符串
 */
const char* getSensorMessageTypeString(SensorMessageType type) {
    switch (type) {
        case SENSOR_MSG_SOIL_MOISTURE: return "土壤湿度";
        case SENSOR_MSG_ENVIRONMENT: return "环境数据";
        case SENSOR_MSG_LIGHT: return "光照数据";
        case SENSOR_MSG_WATER_LEVEL: return "水位数据";
        case SENSOR_MSG_CAS_SOIL: return "CAS土壤";
        case SENSOR_MSG_ERROR: return "传感器错误";
        default: return "未知";
    }
}

/**
 * @brief 获取网络消息类型字符串
 */
const char* getNetworkMessageTypeString(NetworkMessageType type) {
    switch (type) {
        case NETWORK_MSG_SENSOR_DATA: return "传感器数据";
        case NETWORK_MSG_STATUS_UPDATE: return "状态更新";
        case NETWORK_MSG_HEARTBEAT: return "心跳包";
        case NETWORK_MSG_COMMAND_RESP: return "指令响应";
        case NETWORK_MSG_ERROR_REPORT: return "错误报告";
        default: return "未知";
    }
}

/**
 * @brief 获取控制指令类型字符串
 */
const char* getControlCommandTypeString(ControlCommandType type) {
    switch (type) {
        case CONTROL_CMD_AUTO_IRRIGATION: return "自动灌溉";
        case CONTROL_CMD_FORCE_ON: return "强制开启";
        case CONTROL_CMD_FORCE_OFF: return "强制关闭";
        case CONTROL_CMD_EXIT_FORCE: return "退出强制";
        case CONTROL_CMD_SET_THRESHOLD: return "设置阈值";
        case CONTROL_CMD_SYSTEM_STATE: return "系统状态";
        default: return "未知";
    }
}

/**
 * @brief 获取显示消息类型字符串
 */
const char* getDisplayMessageTypeString(DisplayMessageType type) {
    switch (type) {
        case DISPLAY_MSG_SENSOR_DATA: return "传感器数据";
        case DISPLAY_MSG_SYSTEM_STATUS: return "系统状态";
        case DISPLAY_MSG_ERROR_INFO: return "错误信息";
        case DISPLAY_MSG_NETWORK_INFO: return "网络信息";
        case DISPLAY_MSG_CONTROL_INFO: return "控制信息";
        default: return "未知";
    }
}

/**
 * @brief 验证消息完整性
 */
bool validateSensorMessage(const SensorDataMessage& message) {
    // 检查时间戳是否合理
    if (message.timestamp == 0 || message.timestamp > millis() + 1000) {
        return false;
    }
    
    // 根据消息类型验证数据
    switch (message.type) {
        case SENSOR_MSG_SOIL_MOISTURE:
            return message.data.soilData.moisture >= 0 &&
                message.data.soilData.moisture <= 100;
            
        case SENSOR_MSG_ENVIRONMENT:
            return message.data.envData.temperature > -50 &&
                message.data.envData.temperature < 100 &&
                message.data.envData.humidity >= 0 &&
                message.data.envData.humidity <= 100;
            
        case SENSOR_MSG_LIGHT:
            return message.data.lightData.intensity >= 0;
            
        case SENSOR_MSG_WATER_LEVEL:
            return message.data.waterData.level >= 0;
            
        case SENSOR_MSG_CAS_SOIL:
            return message.data.casData.temperature > -50 &&
                message.data.casData.temperature < 100;
            
        case SENSOR_MSG_ERROR:
            return message.data.errorData.errorCode != ErrorCode::SUCCESS;
            
        default:
            return false;
    }
}

/**
 * @brief 获取队列使用情况
 */
String getQueueUsageInfo() {
    String info = "{";
    
    if (xSensorDataQueue != nullptr) {
        UBaseType_t waiting = uxQueueMessagesWaiting(xSensorDataQueue);
        UBaseType_t spaces = uxQueueSpacesAvailable(xSensorDataQueue);
        info += R"("sensorQueue":{"waiting":)" + String(waiting) +
               ",\"spaces\":" + String(spaces) + "},";
    }
    
    if (xNetworkSendQueue != nullptr) {
        UBaseType_t waiting = uxQueueMessagesWaiting(xNetworkSendQueue);
        UBaseType_t spaces = uxQueueSpacesAvailable(xNetworkSendQueue);
        info += R"("networkQueue":{"waiting":)" + String(waiting) +
               ",\"spaces\":" + String(spaces) + "},";
    }
    
    if (xControlCmdQueue != nullptr) {
        UBaseType_t waiting = uxQueueMessagesWaiting(xControlCmdQueue);
        UBaseType_t spaces = uxQueueSpacesAvailable(xControlCmdQueue);
        info += R"("controlQueue":{"waiting":)" + String(waiting) +
               ",\"spaces\":" + String(spaces) + "},";
    }
    
    if (xDisplayMsgQueue != nullptr) {
        UBaseType_t waiting = uxQueueMessagesWaiting(xDisplayMsgQueue);
        UBaseType_t spaces = uxQueueSpacesAvailable(xDisplayMsgQueue);
        info += R"("displayQueue":{"waiting":)" + String(waiting) +
               ",\"spaces\":" + String(spaces) + "}";
    }
    
    info += "}";
    return info;
}
