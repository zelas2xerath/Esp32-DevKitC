#ifndef RTOS_TASKS_H
#define RTOS_TASKS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>
#include <lib.h>

// ==================== 任务优先级定义 ====================
// 优先级范围：1-25，数值越大优先级越高
#define TASK_PRIORITY_SENSOR      3    // 传感器任务 - 中等优先级
#define TASK_PRIORITY_NETWORK     4    // 网络任务 - 较高优先级
#define TASK_PRIORITY_CONTROL     5    // 控制任务 - 高优先级
#define TASK_PRIORITY_ACTUATOR    6    // 执行器任务 - 最高优先级
#define TASK_PRIORITY_DISPLAY     2    // 显示任务 - 较低优先级
#define TASK_PRIORITY_WATCHDOG    7    // 看门狗任务 - 最高优先级

// ==================== 任务堆栈大小定义 ====================
#define TASK_STACK_SIZE_SENSOR    4096  // 传感器任务堆栈
#define TASK_STACK_SIZE_NETWORK   8192  // 网络任务堆栈（需要更多内存）
#define TASK_STACK_SIZE_CONTROL   6144  // 控制任务堆栈（修复：增加到6KB解决堆栈溢出）
#define TASK_STACK_SIZE_ACTUATOR  4096  // 执行器任务堆栈（修复：增加到4KB解决堆栈溢出）
#define TASK_STACK_SIZE_DISPLAY   4096  // 显示任务堆栈（预防性增加）
#define TASK_STACK_SIZE_WATCHDOG  3072  // 看门狗任务堆栈（预防性增加）

// ==================== 队列大小定义 ====================
#define QUEUE_SIZE_SENSOR_DATA    10    // 传感器数据队列大小
#define QUEUE_SIZE_NETWORK_SEND   20    // 网络发送队列大小
#define QUEUE_SIZE_CONTROL_CMD    5     // 控制指令队列大小
#define QUEUE_SIZE_DISPLAY_MSG    8     // 显示消息队列大小
#define QUEUE_SIZE_EVENT          15    // 事件队列大小

// ==================== 任务周期定义（毫秒）====================
#define TASK_PERIOD_SENSOR        5000  // 传感器读取周期：5秒
#define TASK_PERIOD_NETWORK       1000  // 网络检查周期：1秒
#define TASK_PERIOD_CONTROL       2000  // 控制逻辑周期：2秒
#define TASK_PERIOD_ACTUATOR      500   // 执行器检查周期：0.5秒
#define TASK_PERIOD_DISPLAY       1000  // 显示更新周期：1秒
#define TASK_PERIOD_WATCHDOG      10000 // 看门狗检查周期：10秒

// ==================== 事件组标志位定义 ====================
#define EVENT_WIFI_CONNECTED      BIT0  // WiFi连接成功
#define EVENT_SERVER_CONNECTED    BIT1  // 服务器连接成功
#define EVENT_SENSOR_READY        BIT2  // 传感器就绪
#define EVENT_CONFIG_UPDATED      BIT3  // 配置更新
#define EVENT_SYSTEM_ERROR        BIT4  // 系统错误
#define EVENT_FORCE_CONTROL       BIT5  // 强制控制模式
#define EVENT_IRRIGATION_ENABLE   BIT6  // 灌溉启用
#define EVENT_DISPLAY_UPDATE      BIT7  // 显示更新请求

// ==================== 全局句柄声明 ====================
// 任务句柄
extern TaskHandle_t xSensorTaskHandle;
extern TaskHandle_t xNetworkTaskHandle;
extern TaskHandle_t xControlTaskHandle;
extern TaskHandle_t xActuatorTaskHandle;
extern TaskHandle_t xDisplayTaskHandle;
extern TaskHandle_t xWatchdogTaskHandle;

// 队列句柄
extern QueueHandle_t xSensorDataQueue;
extern QueueHandle_t xNetworkSendQueue;
extern QueueHandle_t xControlCmdQueue;
extern QueueHandle_t xDisplayMsgQueue;
extern QueueHandle_t xEventQueue;

// 信号量句柄
extern SemaphoreHandle_t xConfigMutex;
extern SemaphoreHandle_t xDisplayMutex;
extern SemaphoreHandle_t xNetworkMutex;
extern SemaphoreHandle_t xSensorMutex;

// 事件组句柄
extern EventGroupHandle_t xSystemEventGroup;

// ==================== 任务函数声明 ====================

/**
 * @brief 传感器数据采集任务
 * @param pvParameters 任务参数
 */
void vSensorTask(void *pvParameters);

/**
 * @brief 网络通信任务
 * @param pvParameters 任务参数
 */
void vNetworkTask(void *pvParameters);

/**
 * @brief 控制逻辑任务
 * @param pvParameters 任务参数
 */
void vControlTask(void *pvParameters);

/**
 * @brief 硬件执行器任务
 * @param pvParameters 任务参数
 */
void vActuatorTask(void *pvParameters);

/**
 * @brief 显示更新任务
 * @param pvParameters 任务参数
 */
void vDisplayTask(void *pvParameters);

/**
 * @brief 系统看门狗任务
 * @param pvParameters 任务参数
 */
void vWatchdogTask(void *pvParameters);

// ==================== RTOS管理函数声明 ====================

/**
 * @brief 初始化所有RTOS资源（队列、信号量、事件组）
 * @return ErrorCode 错误码
 */
ErrorCode initializeRTOSResources();

/**
 * @brief 创建所有RTOS任务
 * @return ErrorCode 错误码
 */
ErrorCode createRTOSTasks();

/**
 * @brief 删除所有RTOS资源
 */
void deleteRTOSResources();

/**
 * @brief 获取任务运行状态信息
 * @return String 任务状态JSON字符串
 */
String getTaskStatusInfo();

/**
 * @brief 检查任务健康状态
 * @return bool 所有任务是否正常运行
 */
bool checkTaskHealth();

/**
 * @brief 重启指定任务
 * @param taskHandle 任务句柄
 * @param taskFunction 任务函数
 * @param taskName 任务名称
 * @param stackSize 堆栈大小
 * @param priority 优先级
 * @return ErrorCode 错误码
 */
ErrorCode restartTask(TaskHandle_t* taskHandle, TaskFunction_t taskFunction, 
                     const char* taskName, uint32_t stackSize, UBaseType_t priority);

#endif // RTOS_TASKS_H
