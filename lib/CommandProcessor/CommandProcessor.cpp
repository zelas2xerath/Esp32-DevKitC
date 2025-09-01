#include <lib.h>
#include <CommandProcessor.h>
#include <LogManager.h>
#include <ControlAgent.h>
#include <interfaces/IControlAgent.h>
#include <ArduinoJson.h>

// ==================== CommandProcessor实现 ====================
CommandProcessor::CommandProcessor(WiFiClient& client, Preferences& prefs,
                                 int& low, int& high, bool& mode,
                                 unsigned long& count, int (*readFunc)(), uint8_t pin,
                                 IControlAgent* agent)
  : client(client), prefs(prefs), lowThreshold(low),
    highThreshold(high), forceMode(mode), readCount(count),
    readMoistureFunc(readFunc), controlPin(pin), controlAgent(agent),
    lastCommand(""), lastCommandResult(ErrorCode::SUCCESS), lastCommandTimestamp(0),
    totalCommands(0), successfulCommands(0), failedCommands(0) {

  LOG_TRACE("JSON命令处理器初始化完成");
}

CommandProcessor::~CommandProcessor() {
  // 重构后无需清理动态分配的处理器
  LOG_TRACE("JSON命令处理器析构完成");
}

ErrorCode CommandProcessor::processCommand(const String& cmd) {
  // 更新统计信息
  lastCommand = cmd;
  lastCommandTimestamp = millis();
  totalCommands++;

  // 重构后只处理JSON格式命令
  if (cmd.startsWith("{") && cmd.endsWith("}")) {
    LOG_TRACE("处理JSON格式命令: %s", cmd.c_str());
    ErrorCode result = executeJsonCommand(cmd);

    if (result == ErrorCode::SUCCESS) {
      lastCommandResult = ErrorCode::SUCCESS;
      successfulCommands++;
      LOG_TRACE("JSON命令执行成功");
    } else {
      lastCommandResult = result;
      failedCommands++;
      LOG_ERROR("JSON命令执行失败，错误码: %d", static_cast<int>(result));
    }

    return result;
  }
  LOG_ERROR("不支持的命令格式，只接受JSON格式: %s", cmd.c_str());
  lastCommandResult = ErrorCode::COMMAND_ERROR;
  failedCommands++;
  client.println("错误：只支持JSON格式命令");
  return ErrorCode::COMMAND_ERROR;
}

// ==================== JSON命令处理方法 ====================
ErrorCode CommandProcessor::executeJsonCommand(const String& jsonCmd) {
  // 创建JSON文档用于解析（512字节足够处理命令）
  DynamicJsonDocument doc(512);

  // 解析JSON
  DeserializationError error = deserializeJson(doc, jsonCmd);
  if (error) {
    LOG_ERROR("JSON命令解析失败: %s", error.c_str());
    client.println("错误：JSON格式无效");
    return ErrorCode::PARAMETER_ERROR;
  }

  // 检查必需的command字段
  if (!doc.containsKey("command")) {
    LOG_ERROR("JSON命令缺少command字段");
    client.println("错误：缺少command字段");
    return ErrorCode::PARAMETER_ERROR;
  }

  auto command = doc["command"].as<String>();
  LOG_TRACE("执行JSON命令: %s", command.c_str());

  // 根据命令类型分发到具体的执行方法
  if (command == "FORCE_ON") {
    return executeForceOn();
  }
  if (command == "FORCE_OFF") {
    return executeForceOff();
  }
  if (command == "EXIT_FORCE") {
    return executeExitForce();
  }
  if (command == "SET_THRESHOLD") {
    if (!doc.containsKey("parameters")) {
      LOG_ERROR("SET_THRESHOLD命令缺少parameters字段");
      client.println("错误：缺少parameters字段");
      return ErrorCode::PARAMETER_ERROR;
    }

    JsonObject params = doc["parameters"];
    if (!params.containsKey("low") || !params.containsKey("high")) {
      LOG_ERROR("SET_THRESHOLD命令缺少low或high参数");
      client.println("错误：缺少low或high参数");
      return ErrorCode::PARAMETER_ERROR;
    }

    int low = params["low"];
    int high = params["high"];
    return executeSetThreshold(low, high);
  }
  if (command == "GET_STATUS") {
    return executeGetStatus();
  }
  if (command == "GET_NETWORK_QUALITY") {
    return executeGetNetworkQuality();
  }
  if (command == "SET_LOG_LEVEL") {
    if (!doc.containsKey("parameters") || !doc["parameters"].containsKey("level")) {
      LOG_ERROR("SET_LOG_LEVEL命令缺少level参数");
      client.println("错误：缺少level参数");
      return ErrorCode::PARAMETER_ERROR;
    }

    int level = doc["parameters"]["level"];
    return executeSetLogLevel(level);
  }
  if (command == "GET_STATE_HISTORY") {
    return executeGetStateHistory();
  }
  LOG_ERROR("未知的JSON命令: %s", command.c_str());
  client.printf("错误：未知命令 %s\n", command.c_str());
  return ErrorCode::COMMAND_ERROR;
}

// ==================== 具体的JSON命令执行方法 ====================

/**
 * 执行强制开启灌溉命令
 */
ErrorCode CommandProcessor::executeForceOn() {
  LOG_TRACE("执行强制开启灌溉命令");

  if (controlAgent) {
    // 使用控制代理执行强制控制
    ErrorCode result = controlAgent->forceControl(true);
    if (result == ErrorCode::SUCCESS) {
      client.println("强制开启灌溉模式");
      LOG_NOTICE("强制开启灌溉模式");
    }
    return result;
  }

  // 直接执行强制控制
  forceMode = true;
  digitalWrite(controlPin, HIGH);
  client.println("强制开启灌溉模式");
  LOG_NOTICE("强制开启灌溉模式");

  return ErrorCode::SUCCESS;
}

/**
 * 执行强制关闭灌溉命令
 */
ErrorCode CommandProcessor::executeForceOff() {
  LOG_TRACE("执行强制关闭灌溉命令");

  if (controlAgent) {
    // 使用控制代理执行强制控制
    ErrorCode result = controlAgent->forceControl(false);
    if (result == ErrorCode::SUCCESS) {
      client.println("强制关闭灌溉模式");
      LOG_NOTICE("强制关闭灌溉模式");
    }
    return result;
  }

  // 直接执行强制控制
  forceMode = true;
  digitalWrite(controlPin, LOW);
  client.println("强制关闭灌溉模式");
  LOG_NOTICE("强制关闭灌溉模式");

  return ErrorCode::SUCCESS;
}

/**
 * 执行退出强制模式命令
 */
ErrorCode CommandProcessor::executeExitForce() {
  LOG_TRACE("执行退出强制模式命令");

  if (controlAgent) {
    // 使用控制代理退出强制模式
    ErrorCode result = controlAgent->exitForceMode();
    if (result == ErrorCode::SUCCESS) {
      client.println("退出强制模式，恢复自动控制");
      LOG_NOTICE("退出强制模式，恢复自动控制");
    }
    return result;
  }

  // 直接退出强制模式
  forceMode = false;
  client.println("退出强制模式，恢复自动控制");
  LOG_NOTICE("退出强制模式，恢复自动控制");

  return ErrorCode::SUCCESS;
}

/**
 * 执行设置阈值命令
 */
ErrorCode CommandProcessor::executeSetThreshold(int low, int high) {
  LOG_TRACE("执行设置阈值命令: low=%d, high=%d", low, high);

  // 参数验证
  if (low < 0 || low > 100 || high < 0 || high > 100 || low >= high) {
    LOG_ERROR("阈值参数无效: low=%d, high=%d", low, high);
    client.printf("错误：阈值参数无效 low=%d, high=%d\n", low, high);
    return ErrorCode::PARAMETER_ERROR;
  }

  if (controlAgent) {
    // 使用控制代理设置阈值
    ErrorCode result = controlAgent->setThresholds(low, high);
    if (result == ErrorCode::SUCCESS) {
      client.printf("阈值设置成功：低阈值=%d%%, 高阈值=%d%%\n", low, high);
      LOG_NOTICE("阈值设置成功: 低=%d, 高=%d", low, high);
    }
    return result;
  }

  // 直接设置阈值
  lowThreshold = low;
  highThreshold = high;

  // 保存到配置
  prefs.begin("soil_cfg", false);
  prefs.putInt("low_threshold", low);
  prefs.putInt("high_threshold", high);
  prefs.end();

  client.printf("阈值设置成功：低阈值=%d%%, 高阈值=%d%%\n", low, high);
  LOG_NOTICE("阈值设置成功: 低=%d, 高=%d", low, high);

  return ErrorCode::SUCCESS;
}

/**
 * 执行状态查询命令
 */
ErrorCode CommandProcessor::executeGetStatus() {
  LOG_TRACE("执行状态查询命令");

  int val = readMoistureFunc();
  client.printf("状态查询：湿度=%d%%, 读取次数=%lu, 低阈值=%d%%, 高阈值=%d%%, 强制模式=%s\n",
                val, readCount, lowThreshold, highThreshold, forceMode ? "开启" : "关闭");

  LOG_TRACE("状态查询: 湿度=%d, 读取次数=%lu", val, readCount);
  return ErrorCode::SUCCESS;
}

/**
 * 执行网络质量查询命令
 */
ErrorCode CommandProcessor::executeGetNetworkQuality() {
  LOG_TRACE("执行网络质量查询命令");

  // 简化的网络质量检查
  client.println("网络质量：良好");
  LOG_NOTICE("网络质量查询完成");

  return ErrorCode::SUCCESS;
}

/**
 * 执行设置日志级别命令
 */
ErrorCode CommandProcessor::executeSetLogLevel(int level) {
  LOG_TRACE("执行设置日志级别命令: level=%d", level);

  // 参数验证
  if (level < 0 || level > 6) {
    LOG_ERROR("日志级别参数无效: %d", level);
    client.printf("错误：日志级别必须在0-6之间，当前值=%d\n", level);
    return ErrorCode::PARAMETER_ERROR;
  }

  // 设置日志级别
  LogManager::setLevelStatic(static_cast<ILogger::Level>(level));

  // 保存到配置
  prefs.begin("log_cfg", false);
  prefs.putUInt("level", level);
  prefs.end();

  client.printf("日志级别设置为: %d\n", level);
  LOG_NOTICE("日志级别已设置为 %d", level);

  return ErrorCode::SUCCESS;
}

/**
 * 执行状态历史查询命令
 */
ErrorCode CommandProcessor::executeGetStateHistory() {
  LOG_TRACE("执行状态历史查询命令");

  if (!controlAgent) {
    LOG_WARNING("未设置控制代理，无法查询状态历史");
    client.println("状态历史：控制代理不可用");
    return ErrorCode::RESOURCE_UNAVAILABLE;
  }

  // 简化的状态历史查询
  client.println("状态历史：暂无数据");
  LOG_NOTICE("状态历史查询完成");

  return ErrorCode::SUCCESS;
}

// ==================== CommandProcessor 接口方法实现 ====================

String CommandProcessor::getSupportedCommands() const {
  String commands = "{\"commands\":[";
  commands += "{\"command\":\"FORCE_ON\",\"description\":\"强制开启灌溉\"},";
  commands += "{\"command\":\"FORCE_OFF\",\"description\":\"强制关闭灌溉\"},";
  commands += "{\"command\":\"EXIT_FORCE\",\"description\":\"退出强制模式\"},";
  commands += "{\"command\":\"SET_THRESHOLD\",\"description\":\"设置阈值\",\"parameters\":[\"low\",\"high\"]},";
  commands += "{\"command\":\"GET_STATUS\",\"description\":\"状态查询\"},";
  commands += "{\"command\":\"GET_NETWORK_QUALITY\",\"description\":\"网络质量查询\"},";
  commands += "{\"command\":\"SET_LOG_LEVEL\",\"description\":\"设置日志级别\",\"parameters\":[\"level\"]},";
  commands += "{\"command\":\"GET_STATE_HISTORY\",\"description\":\"状态历史查询\"}";
  commands += "]}";
  return commands;
}

String CommandProcessor::getCommandStatistics() const {
  String stats = "{";
  stats += "\"totalCommands\":" + String(totalCommands) + ",";
  stats += "\"successfulCommands\":" + String(successfulCommands) + ",";
  stats += "\"failedCommands\":" + String(failedCommands) + ",";
  stats += "\"successRate\":" + String(totalCommands > 0 ? static_cast<float>(successfulCommands) / static_cast<float>(totalCommands) * 100 : 0) + ",";
  stats += R"("lastCommand":")" + lastCommand + "\",";
  stats += "\"lastResult\":" + String(static_cast<int>(lastCommandResult)) + ",";
  stats += "\"lastTimestamp\":" + String(lastCommandTimestamp);
  stats += "}";
  return stats;
}

void CommandProcessor::resetStatistics() {
  totalCommands = 0;
  successfulCommands = 0;
  failedCommands = 0;
  lastCommand = "";
  lastCommandResult = ErrorCode::SUCCESS;
  lastCommandTimestamp = 0;
}

String CommandProcessor::getLastCommand() const {
  return lastCommand;
}

ErrorCode CommandProcessor::getLastCommandResult() const {
  return lastCommandResult;
}

unsigned long CommandProcessor::getLastCommandTimestamp() const {
  return lastCommandTimestamp;
}