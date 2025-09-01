/*
 * RTOS传感器任务实现
 * 负责周期性读取所有传感器数据并放入消息队列
 * 作者：[Zelas2Xerath]
 * 版本：2.1.3 - RTOS重构版本
 */

#include <RTOSTasks.h>
#include <RTOSCommon.h>
#include <SensorManager/SensorManager.h>
#include <SystemManager/RTOSSystemManager.h>
#include <LogManager/LogManager.h>
#include <DataProcessing/DataFilter.h>  // 添加数据滤波器支持
#include <LightManagement/LightManager.h>  // 添加智能光照管理支持

// 外部函数声明
extern void updateTaskHeartbeat(int taskIndex);
extern void reportTaskError(int taskIndex, const char* errorMsg);

// 任务索引定义
#define SENSOR_TASK_INDEX 0

// 函数声明
static void readSoilMoistureSensor();
static void readEnvironmentSensor();
static void readLightSensor();
static void readWaterLevelSensor();
static void readCASSensor();

/**
 * @brief 传感器数据采集任务
 * 周期性读取所有传感器数据并发送到队列
 */
void vSensorTask(void *pvParameters) {
    LOG_NOTICE("传感器任务启动");

    TickType_t xLastWakeTime = xTaskGetTickCount();
    constexpr TickType_t xFrequency = pdMS_TO_TICKS(TASK_PERIOD_SENSOR);

    // 等待系统初始化完成
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 初始化数据滤波器管理器
    if (globalDataFilterManager == nullptr) {
        globalDataFilterManager = new DataFilterManager();
        if (globalDataFilterManager && globalDataFilterManager->begin()) {
            LOG_NOTICE("数据滤波器管理器初始化成功");

            // 简化版本：只需要基本的初始化验证
            LOG_TRACE("数据滤波器管理器就绪");
        } else {
            LOG_WARNING("数据滤波器管理器初始化失败，将使用原始数据");
        }
    }

    // 初始化智能光照管理器
    if (globalLightManager == nullptr) {
        globalLightManager = new LightManager();
        if (globalLightManager && globalLightManager->begin()) {
            LOG_NOTICE("智能光照管理器初始化成功");
        } else {
            LOG_WARNING("智能光照管理器初始化失败，光照分析功能将不可用");
        }
    }
    
    while (true) {
        // 更新任务心跳
        updateTaskHeartbeat(SENSOR_TASK_INDEX);
        
        // 等待传感器就绪事件
        EventBits_t eventBits = xEventGroupWaitBits(
            xSystemEventGroup,
            EVENT_SENSOR_READY,
            pdFALSE,  // 不清除事件位
            pdFALSE,  // 等待任意一个事件
            pdMS_TO_TICKS(1000)  // 1秒超时
        );
        
        if (!(eventBits & EVENT_SENSOR_READY)) {
            LOG_VERBOSE("传感器未就绪，跳过本次采集");
            vTaskDelayUntil(&xLastWakeTime, xFrequency);
            continue;
        }
        
        // 获取传感器互斥锁
        if (xSemaphoreTake(xSensorMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
            try {
                // 读取土壤湿度传感器
                readSoilMoistureSensor();
                
                // 读取环境传感器
                readEnvironmentSensor();
                
                // 读取光照传感器
                readLightSensor();
                
                // 读取水位传感器
                readWaterLevelSensor();
                
                // 读取CAS土壤传感器
                readCASSensor();

                // 执行滤波器维护
                if (globalDataFilterManager) {
                    globalDataFilterManager->maintenance();
                }

                // 执行光照管理器维护
                if (globalLightManager) {
                    globalLightManager->maintenance();
                }

            } catch (...) {
                reportTaskError(SENSOR_TASK_INDEX, "传感器读取异常");
            }

            // 释放互斥锁
            xSemaphoreGive(xSensorMutex);
        } else {
            reportTaskError(SENSOR_TASK_INDEX, "获取传感器互斥锁超时");
        }
        
        // 等待下一个周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief 读取土壤湿度传感器数据并发送到消息队列
 *
 * 处理流程：
 * 1. 验证系统管理器和SMS实例有效性
 * 2. 读取传感器数据
 * 3. 使用SMS类的验证方法检查数据有效性
 * 4. 根据结果发送数据消息或错误消息到队列
 *
 * @note 此函数在传感器任务中被周期性调用
 * @note 使用SMS类的isDataValid()方法确保验证逻辑一致性
 */
static void readSoilMoistureSensor() {
    // 验证系统管理器和SMS传感器实例
    if (!rtosSystemManager || !rtosSystemManager->getSMS()) {
        LOG_ERROR("土壤湿度传感器读取失败: 系统管理器或SMS实例无效");
        return;
    }

    // 获取SMS实例引用，避免重复调用getSMS()
    SMS* smsInstance = rtosSystemManager->getSMS();

    // 读取传感器数据
    const int rawMoisture = smsInstance->readSoilMoisture();

    // 应用数据滤波（如果启用）
    int filteredMoisture = rawMoisture;
    if (globalDataFilterManager && globalDataFilterManager->isInitialized()) {
        filteredMoisture = globalDataFilterManager->filterSMSData(rawMoisture);

        if (ENABLE_FILTER_DEBUG_LOG && filteredMoisture != rawMoisture) {
            LOG_TRACE("SMS滤波: 原始值=%d, 滤波值=%d", rawMoisture, filteredMoisture);
        }
    }

    // 使用SMS类的静态方法验证滤波后数据的有效性（确保验证逻辑一致）
    if (SMS::isDataValid(filteredMoisture)) {
        // 获取读取次数（只在数据有效时获取，减少不必要地调用）
        const int readCount = static_cast<int>(smsInstance->getReadCount());

        // 创建土壤湿度数据结构（使用滤波后的数据）
        SoilMoistureData soilData{};
        soilData.moisture = filteredMoisture;
        soilData.readCount = readCount;

        // 创建传感器数据消息
        SensorDataMessage message = createSensorMessage(SENSOR_MSG_SOIL_MOISTURE, &soilData);

        // 发送数据到队列
        if (!safeSendToQueue(xSensorDataQueue, &message, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS))) {
            LOG_WARNING("发送土壤湿度数据到队列失败");
        } else {
            LOG_VERBOSE("土壤湿度数据: %d%% (原始:%d), 读取次数: %d",
                       filteredMoisture, rawMoisture, readCount);
        }
    } else {
        // 处理无效数据或读取错误
        SensorErrorData errorData{};

        // 根据原始返回值确定错误类型
        if (rawMoisture == -1) {
            // 读取失败，使用SMS实例的错误信息
            errorData.errorCode = smsInstance->getLastErrorCode();
            const String errorMsg = smsInstance->getLastErrorMessage();
            strncpy(errorData.errorMsg, errorMsg.c_str(), 63);
        } else {
            // 数据超出范围
            errorData.errorCode = ErrorCode::SENSOR_OUT_OF_RANGE;
            const String errorMsg = "土壤湿度读数超出有效范围: " + String(rawMoisture) + "%";
            strncpy(errorData.errorMsg, errorMsg.c_str(), 63);
        }
        errorData.errorMsg[63] = '\0'; // 确保字符串结束符

        // 发送错误消息到队列
        SensorDataMessage errorMsg = createSensorMessage(SENSOR_MSG_ERROR, &errorData);
        if (!safeSendToQueue(xSensorDataQueue, &errorMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS))) {
            LOG_ERROR("发送土壤湿度错误消息到队列失败");
        } else {
            LOG_WARNING("土壤湿度传感器错误: %s", errorData.errorMsg);
        }
    }
}

/**
 * @brief 读取环境传感器数据
 */
/**
 * @brief 读取环境传感器数据并发送到消息队列
 *
 * 处理流程：
 * 1. 验证系统管理器和ECS实例有效性
 * 2. 读取传感器数据（温度、湿度、气压）
 * 3. 使用ECS类的静态验证方法检查数据有效性
 * 4. 根据结果发送数据消息或错误消息到队列
 *
 * @note 此函数在传感器任务中被周期性调用
 * @note 使用ECS类统一优化后的接口，与SMS、WDS、LIS类保持一致
 */
static void readEnvironmentSensor() {
    // 验证系统管理器和ECS传感器实例
    if (!rtosSystemManager || !rtosSystemManager->getEnvironmentSensor()) {
        LOG_ERROR("环境传感器读取失败: 系统管理器或ECS实例无效");
        return;
    }

    ECS* ecsInstance = rtosSystemManager->getEnvironmentSensor();

    // 读取传感器数据（使用ECS类优化后的接口）
    const float rawTemperature = ecsInstance->readTemperature();
    const float rawHumidity = ecsInstance->readHumidity();
    const float rawPressure = ecsInstance->readPressure();

    // 应用数据滤波（如果启用）
    float filteredTemperature = rawTemperature;
    float filteredHumidity = rawHumidity;
    float filteredPressure = rawPressure;

    if (globalDataFilterManager && globalDataFilterManager->isInitialized()) {
        bool filterSuccess = globalDataFilterManager->filterECSData(
            rawTemperature, rawHumidity, rawPressure,
            filteredTemperature, filteredHumidity, filteredPressure
        );

        if (ENABLE_FILTER_DEBUG_LOG && filterSuccess) {
            if (abs(filteredTemperature - rawTemperature) > 0.5f ||
                abs(filteredHumidity - rawHumidity) > 2.0f ||
                abs(filteredPressure - rawPressure) > 2.0f) {
                LOG_TRACE("ECS滤波: T=%.1f->%.1f°C, H=%.1f->%.1f%%, P=%.1f->%.1fhPa",
                         rawTemperature, filteredTemperature,
                         rawHumidity, filteredHumidity,
                         rawPressure, filteredPressure);
            }
        }
    }

    // 使用ECS类的静态方法验证滤波后数据的有效性（确保验证逻辑一致性）
    if (ECS::isDataValid(filteredTemperature, filteredHumidity, filteredPressure)) {
        // 创建环境数据结构（使用滤波后的数据）
        EnvironmentData envData{};
        envData.temperature = filteredTemperature;
        envData.humidity = filteredHumidity;
        envData.pressure = filteredPressure;

        // 创建传感器数据消息
        SensorDataMessage message = createSensorMessage(SENSOR_MSG_ENVIRONMENT, &envData);

        // 发送数据到队列
        if (!safeSendToQueue(xSensorDataQueue, &message, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS))) {
            LOG_WARNING("发送环境数据到队列失败");
        } else {
            LOG_VERBOSE("环境数据: T=%.1f°C (原始:%.1f), H=%.1f%% (原始:%.1f), P=%.1fhPa (原始:%.1f)",
                       filteredTemperature, rawTemperature,
                       filteredHumidity, rawHumidity,
                       filteredPressure, rawPressure);
        }
    } else {
        // 处理无效数据或读取错误
        SensorErrorData errorData{};

        // 根据错误情况确定错误类型
        if (ecsInstance->getLastErrorCode() != ErrorCode::SUCCESS) {
            // 传感器读取错误，使用ECS实例的错误信息
            errorData.errorCode = ecsInstance->getLastErrorCode();
            const String errorMsg = ecsInstance->getLastErrorMessage();
            strncpy(errorData.errorMsg, errorMsg.c_str(), 63);
        } else {
            // 数据验证失败
            errorData.errorCode = ErrorCode::SENSOR_OUT_OF_RANGE;
            const String errorMsg = "环境传感器数据超出有效范围 - 温度:" + String(rawTemperature, 1) +
                             "°C, 湿度:" + String(rawHumidity, 1) + "%, 气压:" + String(rawPressure, 1) + "hPa";
            strncpy(errorData.errorMsg, errorMsg.c_str(), 63);
        }
        errorData.errorMsg[63] = '\0'; // 确保字符串结束符

        // 发送错误消息到队列
        SensorDataMessage errorMsg = createSensorMessage(SENSOR_MSG_ERROR, &errorData);
        if (!safeSendToQueue(xSensorDataQueue, &errorMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS))) {
            LOG_ERROR("发送环境传感器错误消息到队列失败");
        } else {
            LOG_WARNING("环境传感器错误: %s", errorData.errorMsg);
        }
    }
}

/**
 * @brief 读取光照传感器数据并发送到消息队列
 *
 * 处理流程：
 * 1. 验证系统管理器和LIS实例有效性
 * 2. 读取传感器数据（使用统一的readLightLevel接口）
 * 3. 使用LIS类的静态验证方法检查数据有效性
 * 4. 根据结果发送数据消息或错误消息到队列
 *
 * @note 此函数在传感器任务中被周期性调用
 * @note 使用LIS类统一优化后的接口，与SMS、WDS类保持一致
 */
static void readLightSensor() {
    // 验证系统管理器和LIS传感器实例
    if (!rtosSystemManager || !rtosSystemManager->getLightSensor()) {
        LOG_ERROR("光照传感器读取失败: 系统管理器或LIS实例无效");
        return;
    }

    LIS* lisInstance = rtosSystemManager->getLightSensor();

    // 读取传感器数据（使用统一的readLightLevel接口）
    const float rawLightLevel = lisInstance->readLightLevel();

    // 应用数据滤波（如果启用）
    float filteredLightLevel = rawLightLevel;
    if (globalDataFilterManager && globalDataFilterManager->isInitialized()) {
        filteredLightLevel = globalDataFilterManager->filterLISData(rawLightLevel);

        if (ENABLE_FILTER_DEBUG_LOG && abs(filteredLightLevel - rawLightLevel) > 10.0f) {
            LOG_TRACE("LIS滤波: 原始值=%.1f lux, 滤波值=%.1f lux", rawLightLevel, filteredLightLevel);
        }
    }

    // 使用LIS类的静态方法验证滤波后数据的有效性（确保验证逻辑一致性）
    if (LIS::isDataValid(filteredLightLevel)) {
        // 使用LIS类的白天/夜晚检测方法
        const bool isDaytime = lisInstance->isDaytime();

        // 创建光照数据结构（使用滤波后的数据）
        LightData lightData{};
        lightData.intensity = filteredLightLevel;
        lightData.isDaytime = isDaytime;

        // 创建传感器数据消息
        SensorDataMessage message = createSensorMessage(SENSOR_MSG_LIGHT, &lightData);

        // 发送数据到队列
        if (!safeSendToQueue(xSensorDataQueue, &message, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS))) {
            LOG_WARNING("发送光照数据到队列失败");
        } else {
            LOG_VERBOSE("光照数据: %.1f lux (原始:%.1f), %s",
                       filteredLightLevel, rawLightLevel, isDaytime ? "白天" : "夜晚");
        }

        // 更新智能光照管理器（使用滤波后的数据）
        if (globalLightManager && globalLightManager->isInitialized()) {
            if (!globalLightManager->updateLightData(filteredLightLevel)) {
                LOG_WARNING("光照管理器数据更新失败");
            }
        }
    } else {
        // 处理无效数据或读取错误
        SensorErrorData errorData{};

        // 根据原始返回值确定错误类型
        if (rawLightLevel < 0.0f) {
            // 读取失败，使用LIS实例的错误信息
            errorData.errorCode = lisInstance->getLastErrorCode();
            const String errorMsg = lisInstance->getLastErrorMessage();
            strncpy(errorData.errorMsg, errorMsg.c_str(), 63);
        } else {
            // 数据超出范围
            errorData.errorCode = ErrorCode::SENSOR_OUT_OF_RANGE;
            const String errorMsg = "光照强度读数超出有效范围: " + String(rawLightLevel, 1) + " lux";
            strncpy(errorData.errorMsg, errorMsg.c_str(), 63);
        }
        errorData.errorMsg[63] = '\0'; // 确保字符串结束符

        // 发送错误消息到队列
        SensorDataMessage errorMsg = createSensorMessage(SENSOR_MSG_ERROR, &errorData);
        if (!safeSendToQueue(xSensorDataQueue, &errorMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS))) {
            LOG_ERROR("发送光照传感器错误消息到队列失败");
        } else {
            LOG_WARNING("光照传感器错误: %s", errorData.errorMsg);
        }
    }
}

/**
 * @brief 读取水位传感器数据并发送到消息队列
 *
 * 接口统一优化后的水位传感器读取函数，具有以下改进：
 * 1. 统一使用readWaterDepth()接口，移除冗余的readWaterLevel()
 * 2. 使用WDS类内置的数据验证方法，确保验证逻辑一致性
 * 3. 与SMS类保持一致的设计模式和调用方式
 * 4. 简化代码逻辑，提高可读性和维护性
 *
 * 处理流程：
 * 1. 验证系统管理器和WDS实例有效性
 * 2. 读取传感器数据（使用统一的readWaterDepth接口）
 * 3. 使用WDS类的静态验证方法检查数据有效性
 * 4. 根据结果发送数据消息或错误消息到队列
 *
 * @note 此函数在传感器任务中被周期性调用
 * @note 使用WDS类统一优化后的接口，与SMS类保持一致
 */
static void readWaterLevelSensor() {
    // 验证系统管理器和WDS传感器实例
    if (!rtosSystemManager || !rtosSystemManager->getWaterSensor()) {
        LOG_ERROR("水位传感器读取失败: 系统管理器或WDS实例无效");
        return;
    }

    // 获取WDS实例引用，避免重复调用getWaterSensor()
    WDS* wdsInstance = rtosSystemManager->getWaterSensor();
    const double rawWaterDepth = wdsInstance->readWaterDepth();

    // 应用数据滤波（如果启用）
    double filteredWaterDepth = rawWaterDepth;
    if (globalDataFilterManager && globalDataFilterManager->isInitialized()) {
        filteredWaterDepth = globalDataFilterManager->filterWDSData(rawWaterDepth);

        if (ENABLE_FILTER_DEBUG_LOG && abs(filteredWaterDepth - rawWaterDepth) > 0.1) {
            LOG_TRACE("WDS滤波: 原始值=%.2fcm, 滤波值=%.2fcm", rawWaterDepth, filteredWaterDepth);
        }
    }

    // 使用WDS类的静态方法验证滤波后数据的有效性（确保验证逻辑一致性）
    if (WDS::isDataValid(filteredWaterDepth)) {
        // 使用WDS类的低水位检测方法
        const bool isLowLevel = wdsInstance->isLowWaterLevel();

        // 创建水位数据结构（使用滤波后的数据，转换为float以保持数据结构兼容性）
        WaterLevelData waterData{};
        waterData.level = static_cast<float>(filteredWaterDepth);
        waterData.isLowLevel = isLowLevel;

        // 创建传感器数据消息
        SensorDataMessage message = createSensorMessage(SENSOR_MSG_WATER_LEVEL, &waterData);

        // 发送数据到队列
        if (!safeSendToQueue(xSensorDataQueue, &message, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS))) {
            LOG_WARNING("发送水位数据到队列失败");
        } else {
            LOG_VERBOSE("水位数据: %.1fcm (原始:%.1fcm), %s",
                       filteredWaterDepth, rawWaterDepth, isLowLevel ? "低水位" : "正常");
        }
    } else {
        // 处理无效数据或读取错误
        SensorErrorData errorData{};

        // 根据返回值确定错误类型
        if (rawWaterDepth < 0.0) {
            // 读取失败，使用WDS实例的错误信息
            errorData.errorCode = wdsInstance->getLastErrorCode();
            String errorMsg = wdsInstance->getLastErrorMessage();
            strncpy(errorData.errorMsg, errorMsg.c_str(), 63);
        } else {
            // 数据超出范围
            errorData.errorCode = ErrorCode::SENSOR_OUT_OF_RANGE;
            String errorMsg = "水位读数超出有效范围: " + String(rawWaterDepth, 1) + "cm";
            strncpy(errorData.errorMsg, errorMsg.c_str(), 63);
        }
        errorData.errorMsg[63] = '\0'; // 确保字符串结束符

        // 发送错误消息到队列
        SensorDataMessage errorMsg = createSensorMessage(SENSOR_MSG_ERROR, &errorData);
        if (!safeSendToQueue(xSensorDataQueue, &errorMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS))) {
            LOG_ERROR("发送水位传感器错误消息到队列失败");
        } else {
            LOG_WARNING("水位传感器错误: %s", errorData.errorMsg);
        }
    }
}

/**
 * @brief 读取CAS土壤传感器数据并发送到消息队列
 *
 * 第六阶段优化后的CAS传感器读取函数，具有以下改进：
 * 1. 使用CAS类优化后的统一接口，与SMS、WDS、LIS、ECS类保持一致
 * 2. 使用CAS类内置的数据验证方法，确保验证逻辑一致性
 * 3. 与SMS、WDS、LIS、ECS类保持一致的设计模式和调用方式
 * 4. 简化代码逻辑，提高可读性和维护性
 *
 * 处理流程：
 * 1. 验证系统管理器和CAS实例有效性
 * 2. 读取传感器数据（土壤温度、湿度、pH、EC等）
 * 3. 使用CAS类的静态验证方法检查数据有效性
 * 4. 根据结果发送数据消息或错误消息到队列
 *
 * @note 此函数在传感器任务中被周期性调用
 * @note 使用CAS类统一优化后的接口，与SMS、WDS、LIS、ECS类保持一致
 */
static void readCASSensor() {
    // 验证系统管理器和CAS传感器实例
    if (!rtosSystemManager || !rtosSystemManager->getCASSensor()) {
        LOG_ERROR("CAS传感器读取失败: 系统管理器或CAS实例无效");
        return;
    }

    CAS* casInstance = rtosSystemManager->getCASSensor();

    // 读取传感器数据（使用CAS类优化后的接口）
    SoilSensorData rawCasData;
    const ErrorCode result = casInstance->readSoilData(rawCasData);

    // 简化版本：直接使用原始数据，不进行CAS滤波
    SoilSensorData filteredCasData = rawCasData;

    // 使用CAS类的静态方法验证滤波后数据的有效性（确保验证逻辑一致性）
    if (result == ErrorCode::SUCCESS && CAS::isDataValid(filteredCasData)) {
        // 创建传感器数据消息（使用滤波后的数据）
        SensorDataMessage message = createSensorMessage(SENSOR_MSG_CAS_SOIL, &filteredCasData);

        // 发送数据到队列
        if (!safeSendToQueue(xSensorDataQueue, &message, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS))) {
            LOG_WARNING("发送CAS数据到队列失败");
        } else {
            LOG_VERBOSE("CAS数据: T=%.1f°C (原始:%.1f), M=%.1f%% (原始:%.1f), pH=%.2f (原始:%.2f), EC=%u μS/cm (原始:%u)",
                       filteredCasData.temperature, rawCasData.temperature,
                       filteredCasData.moisture, rawCasData.moisture,
                       filteredCasData.ph, rawCasData.ph,
                       filteredCasData.ec, rawCasData.ec);
        }
    } else {
        // 处理无效数据或读取错误
        SensorErrorData errorData{};

        // 根据错误情况确定错误类型
        if (result != ErrorCode::SUCCESS) {
            // 传感器读取错误，使用CAS实例的错误信息
            errorData.errorCode = casInstance->getLastErrorCode();
            const String errorMsg = casInstance->getLastErrorMessage();
            strncpy(errorData.errorMsg, errorMsg.c_str(), 63);
        } else {
            // 数据验证失败
            errorData.errorCode = ErrorCode::SENSOR_OUT_OF_RANGE;
            const String errorMsg = "CAS传感器数据超出有效范围 - 温度:" + String(rawCasData.temperature, 1) +
                             "°C, 湿度:" + String(rawCasData.moisture, 1) + "%, pH:" + String(rawCasData.ph, 2) +
                             ", EC:" + String(rawCasData.ec) + " μS/cm";
            strncpy(errorData.errorMsg, errorMsg.c_str(), 63);
        }
        errorData.errorMsg[63] = '\0'; // 确保字符串结束符

        // 发送错误消息到队列
        SensorDataMessage errorMsg = createSensorMessage(SENSOR_MSG_ERROR, &errorData);
        if (!safeSendToQueue(xSensorDataQueue, &errorMsg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS))) {
            LOG_ERROR("发送CAS传感器错误消息到队列失败");
        } else {
            LOG_WARNING("CAS传感器错误: %s", errorData.errorMsg);
        }
    }
}
