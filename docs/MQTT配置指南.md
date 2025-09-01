# MQTT配置指南 📡

## 📋 概述

本文档详细介绍ESP-32智能农业监测系统的MQTT配置和使用方法。系统支持TCP和WebSocket双模式连接，提供完整的数据上报和远程控制功能。

## 📊 主题结构设计

### 主题命名规范

```
agriculture/{device_id}/{message_type}/{sub_type}
```

- `agriculture`: 项目前缀
- `{device_id}`: 设备唯一标识符
- `{message_type}`: 消息类型 (data/command/status/config)
- `{sub_type}`: 子类型 (具体数据类型或命令类型)

### 数据上报主题

#### 1. 传感器数据上报
```
agriculture/ESP32_001/data/sensors
```

**消息格式**（重构后包含完整传感器数据）:
```json
{
  "timestamp": 1640995200,
  "device_id": "ESP32_001",
  "sensors": {
    "soil_moisture": {
      "value": 45.6,
      "unit": "%",
      "quality": "good",
      "read_count": 3
    },
    "air_temperature": {
      "value": 25.8,
      "unit": "°C",
      "quality": "good",
      "read_count": 3,
      "error": ""
    },
    "air_humidity": {
      "value": 62.3,
      "unit": "%",
      "quality": "good",
      "error": ""
    },
    "atmospheric_pressure": {
      "value": 1013.25,
      "unit": "hPa",
      "quality": "good",
      "error": ""
    },
    "light_intensity": {
      "value": 1250,
      "unit": "lux",
      "quality": "good",
      "read_count": 3,
      "is_daytime": false
    },
    "water_level": {
      "value": 18.5,
      "unit": "cm",
      "quality": "good",
      "error": "",
      "read_count": 3,
      "is_dry": false
    },
    "soil_temperature": {
      "value": 22.5,
      "unit": "°C",
      "quality": "good",
      "read_count": 3
    },
    "soil_humidity": {
      "value": 45.0,
      "unit": "%",
      "quality": "good"
    },
    "electrical_conductivity": {
      "value": 1200.0,
      "unit": "μS/cm",
      "quality": "good"
    },
    "salinity": {
      "value": 800.0,
      "unit": "ppm",
      "quality": "good"
    },
    "ph": {
      "value": 6.8,
      "unit": "pH",
      "quality": "good"
    },
    "nitrogen": {
      "value": 120.0,
      "unit": "mg/L",
      "quality": "good"
    },
    "phosphorus": {
      "value": 45.0,
      "unit": "mg/L",
      "quality": "good"
    },
    "potassium": {
      "value": 180.0,
      "unit": "mg/L",
      "quality": "good"
    }
  }
}
```

**传感器数据字段说明**:

| 字段名                    | 描述               | 数据范围(value) | 单位(unit) | 传感器来源        |
| ------------------------- | ------------------ | --------------- | ---------- | ----------------- |
| `soil_moisture`           | 土壤湿度（GPIO34） | 0-100           | %          | SMS土壤湿度传感器 |
| `air_temperature`         | 空气温度           | -40 to 85       | °C         | BME280环境传感器  |
| `air_humidity`            | 空气湿度           | 0-100           | %          | BME280环境传感器  |
| `atmospheric_pressure`    | 大气压强           | 300-1100        | hPa        | BME280环境传感器  |
| `light_intensity`         | 光照强度           | 0-65535         | lux        | BH1750光照传感器  |
| `water_level`             | 水位深度           | 0-200           | cm         | 水深传感器        |
| `soil_temperature`        | 土壤温度           | -40 to 80       | °C         | CAS土壤综合传感器 |
| `soil_humidity`           | 土壤湿度（CAS）    | 0-100           | %          | CAS土壤综合传感器 |
| `electrical_conductivity` | 电导率             | 0-20000         | μS/cm      | CAS土壤综合传感器 |
| `salinity`                | 盐分含量           | 0-20000         | ppm        | CAS土壤综合传感器 |
| `ph`                      | 土壤酸碱度         | 3.0-10.0        | pH         | CAS土壤综合传感器 |
| `nitrogen`                | 氮含量             | 0-1999          | mg/L       | CAS土壤综合传感器 |
| `phosphorus`              | 磷含量             | 0-1999          | mg/L       | CAS土壤综合传感器 |
| `potassium`               | 钾含量             | 0-1999          | mg/L       | CAS土壤综合传感器 |

| 子字段名   | 描述                                           | 数据范围                                                     |
| ---------- | ---------------------------------------------- | ------------------------------------------------------------ |
| quality    | 数据质量等级                                   | `good`: 数据在正常范围内 <br />`fair`: 数据在可接受范围内 <br />`poor`: 数据接近边界值 <br />`out_of_range`: 数据超出有效范围 <br />`error`: 传感器读取错误 |
| error      | 错误信息的详细描述<br />返回空字符串表示无错误 | `SENSOR_READ_ERROR`：传感器读取错误 <br />`SENSOR_TIMEOUT`：传感器超时 <br />`SENSOR_CALIBRATION_ERROR`：传感器校准错误 <br />`SENSOR_OUT_OF_RANGE`：传感器读数超出范围 <br />`SENSOR_CONNECTION_ERROR`：传感器连接错误 <br />`SENSOR_INITIALIZATION_FAILED`：传感器初始化失败 <br />`SENSOR_CONFIGURATION_ERROR`：传感器配置错误<br />"LIS传感器未初始化" |
| read_count | 读取次数                                       | >=0                                                          |
| is_dry     | 水泵缺水状态                                   | true：缺水<br />false：不缺水<br />                          |
| is_daytime | 检测当前是否为白天                             | true：白天<br />false：夜晚<br />                            |

#### 2. 系统状态上报
```
agriculture/ESP32_001/status/system
```

**消息格式**:
```json
{
  "timestamp": 1640995200,
  "device_id": "ESP32_001",
  "status": {
    "online": true,
    "system_state": "NORMAL",
    "wifi_rssi": -45,
    "free_heap": 185000,
    "uptime": 86400,
    "tasks": {
      "sensor_task": "healthy",
      "network_task": "healthy",
      "control_task": "healthy",
      "display_task": "healthy",
      "watchdog_task": "healthy"
    },
    "irrigation": {
      "active": false,
      "last_activation": 1640991600,
      "total_runtime": 1800
    }
  }
}
```

#### 3. 告警信息上报
```
agriculture/ESP32_001/data/alerts
```

**消息格式**:
```json
{
  "timestamp": 1640995200,
  "device_id": "ESP32_001",
  "alert": {
    "level": "WARNING",
    "type": "SENSOR_ERROR",
    "message": "土壤湿度传感器读取异常",
    "sensor": "soil_moisture",
    "value": null,
    "threshold": null,
    "action_required": true
  }
}
```

### 命令下发主题

#### 1. 控制命令
```
agriculture/ESP32_001/command/control
```

**支持的命令**:

**强制开启灌溉**:
```json
{
  "command": "FORCE_ON",
  "parameters": {
    "duration": 30
  },
  "message_id": "cmd_001",
  "timestamp": 1640995200
}
```

**强制关闭灌溉**:
```json
{
  "command": "FORCE_OFF",
  "parameters": {},
  "message_id": "cmd_002",
  "timestamp": 1640995200
}
```

**设置灌溉阈值**:
```json
{
  "command": "SET_THRESHOLD",
  "parameters": {
    "low_threshold": 30,
    "high_threshold": 70
  },
  "message_id": "cmd_003",
  "timestamp": 1640995200
}
```

#### 2. 系统命令
```
agriculture/ESP32_001/command/system
```

**重启系统**:
```json
{
  "command": "RESTART",
  "parameters": {
    "delay": 5
  },
  "message_id": "cmd_004",
  "timestamp": 1640995200
}
```

**更新配置**:
```json
{
  "command": "UPDATE_CONFIG",
  "parameters": {
    "config_type": "irrigation",
    "config_data": {
      "enabled": true,
      "auto_mode": true,
      "max_duration": 300
    }
  },
  "message_id": "cmd_005",
  "timestamp": 1640995200
}
```

#### 3. 查询命令
```
agriculture/ESP32_001/command/query
```

**获取系统状态**:
```json
{
  "command": "GET_STATUS",
  "parameters": {
    "include": ["system", "sensors", "network"]
  },
  "message_id": "cmd_006",
  "timestamp": 1640995200
}
```

### 命令响应主题

#### 命令执行结果
```
agriculture/ESP32_001/response/command
```

**响应格式**:
```json
{
  "message_id": "cmd_001",
  "command": "FORCE_ON",
  "status": "SUCCESS",
  "result": {
    "irrigation_started": true,
    "duration": 30,
    "start_time": 1640995200
  },
  "timestamp": 1640995205
}
```

**错误响应**:
```json
{
  "message_id": "cmd_002",
  "command": "SET_THRESHOLD",
  "status": "ERROR",
  "error": {
    "code": "INVALID_PARAMETER",
    "message": "阈值参数超出有效范围",
    "details": "low_threshold必须在0-100之间"
  },
  "timestamp": 1640995210
}
```

## 🔐 安全配置

### TLS/SSL加密

#### 1. 证书配置
```cpp
// 在NetworkManager中配置TLS
const char* ca_cert = R"(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
...
-----END CERTIFICATE-----
)";

// 设置CA证书
client.setCACert(ca_cert);
```

#### 2. 客户端证书认证
```cpp
// 设置客户端证书和私钥
const char* client_cert = "...";
const char* client_key = "...";

client.setCertificate(client_cert);
client.setPrivateKey(client_key);
```

### 访问控制

#### 1. 主题权限配置
```json
{
  "acl": [
    {
      "username": "ESP32_001",
      "topic": "agriculture/ESP32_001/+/+",
      "access": "readwrite"
    },
    {
      "username": "ESP32_001",
      "topic": "agriculture/broadcast/+/+",
      "access": "read"
    }
  ]
}
```

#### 2. 设备认证
```cpp
// 使用设备ID和密钥生成认证信息
String generateClientId() {
    String deviceId = "ESP32_001";
    String timestamp = String(millis());
    String signature = hmacSha256(deviceId + timestamp, deviceSecret);
    return deviceId + "|" + timestamp + "|" + signature;
}
```

## 🔄 连接管理

### 自动重连机制

```cpp
class MQTTReconnectManager {
private:
    int retryCount = 0;
    unsigned long lastRetryTime = 0;
    const int maxRetries = 10;
    const unsigned long baseDelay = 1000; // 1秒
    
public:
    bool shouldRetry() {
        if (retryCount >= maxRetries) return false;
        
        unsigned long currentTime = millis();
        unsigned long delay = baseDelay * (1 << retryCount); // 指数退避
        
        if (currentTime - lastRetryTime >= delay) {
            lastRetryTime = currentTime;
            retryCount++;
            return true;
        }
        return false;
    }
    
    void resetRetryCount() {
        retryCount = 0;
    }
};
```

### 心跳监测

```cpp
void checkMQTTConnection() {
    static unsigned long lastHeartbeat = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        if (mqttClient.connected()) {
            // 发送心跳消息
            publishHeartbeat();
            lastHeartbeat = currentTime;
        } else {
            // 尝试重连
            reconnectMQTT();
        }
    }
}
```

## 📈 性能优化

### 消息缓存

```cpp
class MessageBuffer {
private:
    struct Message {
        String topic;
        String payload;
        unsigned long timestamp;
        int qos;
        bool retain;
    };
    
    std::queue<Message> messageQueue;
    const size_t maxQueueSize = 100;
    
public:
    void addMessage(const String& topic, const String& payload, int qos = 1, bool retain = false) {
        if (messageQueue.size() >= maxQueueSize) {
            messageQueue.pop(); // 移除最旧的消息
        }
        
        Message msg = {topic, payload, millis(), qos, retain};
        messageQueue.push(msg);
    }
    
    bool sendPendingMessages(PubSubClient& client) {
        while (!messageQueue.empty() && client.connected()) {
            Message msg = messageQueue.front();
            
            if (client.publish(msg.topic.c_str(), msg.payload.c_str(), msg.retain)) {
                messageQueue.pop();
            } else {
                return false; // 发送失败，保留消息
            }
        }
        return true;
    }
};
```

### 数据压缩

```cpp
// 使用JSON压缩减少消息大小
String compressJsonMessage(const JsonDocument& doc) {
    String compressed;
    serializeJson(doc, compressed);
    
    // 移除不必要的空格和换行
    compressed.replace(" ", "");
    compressed.replace("\n", "");
    
    return compressed;
}
```

## 🐛 故障排除

### 常见MQTT连接问题

#### 1. 连接被拒绝
```
错误代码: -2 (MQTT_CONNECT_FAILED)
原因: 网络连接问题
解决: 检查WiFi连接和服务器地址
```

#### 2. 认证失败
```
错误代码: 4 (MQTT_CONNECT_BAD_CREDENTIALS)
原因: 用户名或密码错误
解决: 验证MQTT认证信息
```

#### 3. 客户端ID冲突
```
错误代码: 2 (MQTT_CONNECT_ID_REJECTED)
原因: 客户端ID已被使用
解决: 使用唯一的客户端ID
```

### 调试工具

#### 1. MQTT客户端测试
```bash
# 使用mosquitto客户端测试
mosquitto_pub -h mqtt.example.com -t "agriculture/ESP32_001/data/sensors" -m '{"test": true}'
mosquitto_sub -h mqtt.example.com -t "agriculture/ESP32_001/+/+"
```

#### 2. 网络抓包分析
```bash
# 使用tcpdump抓取MQTT流量
tcpdump -i wlan0 -A -s 0 port 1883
```

#### 3. 日志分析
```cpp
// 启用详细MQTT日志
#define MQTT_DEBUG 1

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    LOG_VERBOSE("MQTT消息接收: 主题=%s, 长度=%d", topic, length);
    // 处理消息...
}
```

## 📚 参考资料

- [MQTT 3.1.1 协议规范](http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html)
- [PubSubClient库文档](https://pubsubclient.knolleary.net/)
- [ESP32 MQTT示例](https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi/examples)
- [MQTT安全最佳实践](https://www.hivemq.com/blog/mqtt-security-fundamentals/)

---

**📡 通过MQTT连接万物，让数据流动起来！**
