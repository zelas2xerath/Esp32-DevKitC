# MQTT功能使用说明 📡

## 📋 概述

本文档详细介绍Tauri-UI项目中MQTT功能的使用方法，包括设备控制、状态查询、系统管理等功能。

## 🎯 功能特性

### ✅ 已实现功能

1. **完整的MQTT主题结构支持**
   - 数据上报：`agriculture/{device_id}/data/sensors`
   - 系统状态：`agriculture/{device_id}/status/system`
   - 告警信息：`agriculture/{device_id}/data/alerts`
   - 控制命令：`agriculture/{device_id}/command/control`
   - 系统命令：`agriculture/{device_id}/command/system`
   - 查询命令：`agriculture/{device_id}/command/query`
   - 命令响应：`agriculture/{device_id}/response/command`

2. **设备ID自动标准化**
   - 支持多种输入格式自动转换为标准格式
   - 标准格式：`ESP32_XXX`

3. **完整的命令集合**
   - 控制命令：强制开启/关闭灌溉、设置阈值
   - 系统命令：重启、配置更新、固件更新
   - 查询命令：状态查询、传感器数据查询

4. **错误处理和重连机制**
   - 自动重连功能
   - 指数退避重试策略
   - 详细的错误日志

## 🚀 前端使用方法

### 1. 导入MQTT命令API

```typescript
import { mqttCommands } from '../utils/tauri';
```

### 2. 基本设备控制

#### 强制开启灌溉
```typescript
// 开启灌溉30秒
await mqttCommands.forceIrrigationOn('ESP32_001', 30);

// 使用默认时长（30秒）
await mqttCommands.forceIrrigationOn('ESP32_001');
```

#### 强制关闭灌溉
```typescript
await mqttCommands.forceIrrigationOff('ESP32_001');
```

#### 设置灌溉阈值
```typescript
await mqttCommands.setThreshold('ESP32_001', 30, 70);
```

### 3. 系统管理

#### 重启设备
```typescript
// 5秒后重启
await mqttCommands.restartDevice('ESP32_001', 5);

// 使用默认延迟（5秒）
await mqttCommands.restartDevice('ESP32_001');
```

#### 更新配置
```typescript
// 更新灌溉配置
await mqttCommands.sendSystemCommand('ESP32_001', 'UPDATE_CONFIG', {
  config_type: 'irrigation',
  config_data: {
    enabled: true,
    auto_mode: true,
    max_duration: 300
  }
});
```

### 4. 状态查询

#### 获取设备状态
```typescript
// 查询所有状态信息
await mqttCommands.getDeviceStatus('ESP32_001');

// 查询特定状态信息
await mqttCommands.sendQueryCommand('ESP32_001', 'GET_STATUS', {
  include: ['system', 'sensors']
});
```

#### 获取传感器数据
```typescript
// 查询所有传感器数据
await mqttCommands.getSensorData('ESP32_001');

// 查询特定传感器数据
await mqttCommands.sendQueryCommand('ESP32_001', 'GET_SENSORS', {
  sensors: ['soil_moisture', 'temperature']
});
```

### 5. 在Vue组件中使用

```vue
<template>
  <div class="device-controls">
    <button @click="handleForceOn" :disabled="!isOnline">
      强制开启灌溉
    </button>
    <button @click="handleForceOff" :disabled="!isOnline">
      强制关闭灌溉
    </button>
    <button @click="handleRestart" :disabled="!isOnline">
      重启设备
    </button>
    <button @click="handleGetStatus" :disabled="!isOnline">
      查询状态
    </button>
  </div>
</template>

<script setup lang="ts">
import { mqttCommands } from '../utils/tauri';

const props = defineProps<{
  deviceId: string;
  isOnline: boolean;
}>();

const emit = defineEmits<{
  (e: 'commandSent', result: string): void;
  (e: 'error', error: string): void;
}>();

const handleForceOn = async () => {
  try {
    const result = await mqttCommands.forceIrrigationOn(props.deviceId, 30);
    emit('commandSent', result);
  } catch (error) {
    emit('error', error.message);
  }
};

const handleForceOff = async () => {
  try {
    const result = await mqttCommands.forceIrrigationOff(props.deviceId);
    emit('commandSent', result);
  } catch (error) {
    emit('error', error.message);
  }
};

const handleRestart = async () => {
  try {
    const result = await mqttCommands.restartDevice(props.deviceId, 5);
    emit('commandSent', result);
  } catch (error) {
    emit('error', error.message);
  }
};

const handleGetStatus = async () => {
  try {
    const result = await mqttCommands.getDeviceStatus(props.deviceId);
    emit('commandSent', result);
  } catch (error) {
    emit('error', error.message);
  }
};
</script>
```

## 🔧 后端配置

### 1. MQTT客户端配置

在`src-tauri/src/models.rs`中配置MQTT客户端：

```rust
pub struct AppConfig {
    // MQTT客户端配置
    pub mqtt_enabled: bool,
    pub mqtt_broker_host: String,
    pub mqtt_broker_port: u16,
    pub mqtt_client_id: String,
    pub mqtt_username: Option<String>,
    pub mqtt_password: Option<String>,
    pub mqtt_keep_alive: u64,
    pub mqtt_qos: u8,
    
    // MQTT Broker配置（内置）
    pub mqtt_broker_enabled: bool,
    pub mqtt_internal_broker_port: u16,
    pub mqtt_broker_bind_address: String,
    pub mqtt_broker_max_connections: usize,
    pub mqtt_broker_max_packet_size: usize,
}
```

### 2. 启动MQTT服务

```rust
// 在应用启动时初始化MQTT服务
let mqtt_service = MqttService::new(device_manager, database, app_handle);
mqtt_service.start().await?;
```

## 📊 消息格式

### 1. 传感器数据上报

```json
{
  "timestamp": 1640995200,
  "device_id": "ESP32_001",
  "sensors": {
    "soil_moisture": {
      "value": 45.6,
      "unit": "%",
      "quality": "good"
    },
    "air_temperature": {
      "value": 25.8,
      "unit": "°C",
      "quality": "good"
    }
  }
}
```

### 2. 控制命令

```json
{
  "command": "FORCE_ON",
  "parameters": {
    "duration": 30
  },
  "message_id": "cmd_12345678",
  "timestamp": 1640995200
}
```

### 3. 命令响应

```json
{
  "message_id": "cmd_12345678",
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

## 🐛 故障排除

### 1. 连接问题

- 检查MQTT Broker地址和端口
- 验证网络连接
- 查看认证信息是否正确

### 2. 命令发送失败

- 确认设备在线状态
- 检查设备ID格式
- 验证命令参数

### 3. 消息接收问题

- 检查主题订阅是否正确
- 验证消息格式
- 查看错误日志

## 📝 最佳实践

1. **错误处理**：始终使用try-catch包装MQTT命令调用
2. **状态检查**：发送命令前检查设备在线状态
3. **日志记录**：记录所有MQTT操作的结果
4. **用户反馈**：提供清晰的操作状态反馈
5. **重试机制**：对于关键操作实现重试逻辑

## 🔗 相关文档

- [MQTT配置指南](./MQTT配置指南.md)
- [故障排除指南](./故障排除指南.md)
- [API参考文档](./API参考文档.md)

---

**📡 通过MQTT连接万物，让智能农业更简单！**
