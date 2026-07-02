#ifndef I18N_H
#define I18N_H

/*
 * 国际化支持
 *
 * 编译时选择语言:
 *   gcc -DLANG_CN ...  # 中文
 *   gcc -DLANG_EN ...  # 英文 (默认)
 */

#ifdef LANG_CN

/* ==================== 中文 ==================== */

/* 通用 */
#define I18N_ERROR              "错误"
#define I18N_WARNING            "警告"
#define I18N_INFO               "信息"
#define I18N_SUCCESS            "成功"
#define I18N_FAILED             "失败"
#define I18N_PASS               "通过"
#define I18N_DONE               "完成"

/* 构建 */
#define I18N_BUILDING           "正在构建 LogAgg..."
#define I18N_BUILD_COMPLETE     "构建完成！"
#define I18N_BUILD_FAILED       "构建失败！"
#define I18N_COMPILE            "编译"
#define I18N_LINK               "链接"
#define I18N_CREATE_LIB         "创建静态库"

/* 测试 */
#define I18N_RUNNING_TESTS      "正在运行单元测试..."
#define I18N_TEST_RESULTS       "测试结果"
#define I18N_TESTS_PASSED       "个测试通过"
#define I18N_TESTS_FAILED       "个测试失败"
#define I18N_INTEGRATION_TESTS  "集成测试"
#define I18N_CREATING_TEST_DATA "创建测试数据..."

/* 向量时钟 */
#define I18N_VC_INIT            "初始化向量时钟"
#define I18N_VC_INCREMENT       "递增时钟"
#define I18N_VC_MERGE           "合并时钟"
#define I18N_VC_COMPARE         "比较时钟"

/* Agent */
#define I18N_AGENT_STARTING     "启动 Agent..."
#define I18N_AGENT_STARTED      "Agent 已启动"
#define I18N_AGENT_STOPPED      "Agent 已停止"
#define I18N_AGENT_NODE_ID      "节点 ID"
#define I18N_AGENT_LOG_FILE     "日志文件"
#define I18N_AGENT_SERVER       "服务器"
#define I18N_AGENT_POLL_INTERVAL "轮询间隔"
#define I18N_AGENT_NODE_COUNT   "节点数量"
#define I18N_AGENT_SENT         "已发送"
#define I18N_AGENT_LOG_ENTRIES  "条日志"
#define I18N_AGENT_PRESS_CTRL   "按 Ctrl+C 停止"
#define I18N_AGENT_RECEIVED_SIGNAL "收到信号，正在停止..."

/* Server */
#define I18N_SERVER_STARTING    "启动 Server..."
#define I18N_SERVER_STARTED     "Server 已启动"
#define I18N_SERVER_STOPPED     "Server 已停止"
#define I18N_SERVER_PORT        "端口"
#define I18N_SERVER_STORAGE     "存储目录"
#define I18N_SERVER_WAITING     "等待 Agent 连接..."
#define I18N_SERVER_RECEIVED    "已接收"
#define I18N_SERVER_LOGS        "条日志"
#define I18N_SERVER_STATISTICS  "服务器统计"
#define I18N_SERVER_TOTAL_RECEIVED "总接收"
#define I18N_SERVER_TOTAL_STORED   "总存储"
#define I18N_SERVER_TOTAL_ERRORS   "总错误"
#define I18N_SERVER_BYTES_RECEIVED "接收字节"

/* Store */
#define I18N_STORE_LOADING      "正在加载日志..."
#define I18N_STORE_LOADED       "已加载"
#define I18N_STORE_SORTING      "正在排序日志"
#define I18N_STORE_CAUSAL_SORT  "因果排序"
#define I18N_STORE_TIME_SORT    "时间排序"
#define I18N_STORE_NODE_SORT    "节点排序"
#define I18N_STORE_NO_LOGS      "未找到日志"
#define I18N_STORE_TOTAL        "总计"

/* Query */
#define I18N_QUERY_USAGE        "用法"
#define I18N_QUERY_REQUIRED     "必需参数"
#define I18N_QUERY_OPTIONS      "可选参数"
#define I18N_QUERY_EXAMPLES     "示例"
#define I18N_QUERY_RESULTS      "查询结果"
#define I18N_QUERY_KEYWORD      "关键词"
#define I18N_QUERY_NODE         "节点"
#define I18N_QUERY_LEVEL        "级别"
#define I18N_QUERY_ANY          "任意"
#define I18N_QUERY_NO_RESULTS   "未找到匹配的日志"
#define I18N_QUERY_STATISTICS   "日志统计"
#define I18N_QUERY_LOGS_BY_NODE "各节点日志数"

/* 日志级别 */
#define I18N_LEVEL_DEBUG        "调试"
#define I18N_LEVEL_INFO         "信息"
#define I18N_LEVEL_WARN         "警告"
#define I18N_LEVEL_ERROR        "错误"
#define I18N_LEVEL_FATAL        "致命"

/* 错误消息 */
#define I18N_ERR_INVALID_PARAM  "无效参数"
#define I18N_ERR_MISSING_PARAM  "缺少必需参数"
#define I18N_ERR_OPEN_FILE      "无法打开文件"
#define I18N_ERR_CREATE_DIR     "无法创建目录"
#define I18N_ERR_CREATE_SOCKET  "无法创建 socket"
#define I18N_ERR_BIND_PORT      "无法绑定端口"
#define I18N_ERR_ALLOC_MEMORY   "内存分配失败"
#define I18N_ERR_SEND_FAILED    "发送失败"
#define I18N_ERR_RECV_FAILED    "接收失败"
#define I18N_ERR_PARSE_JSON     "JSON 解析失败"

#else

/* ==================== English ==================== */

/* Common */
#define I18N_ERROR              "Error"
#define I18N_WARNING            "Warning"
#define I18N_INFO               "Info"
#define I18N_SUCCESS            "Success"
#define I18N_FAILED             "Failed"
#define I18N_PASS               "PASS"
#define I18N_DONE               "Done"

/* Build */
#define I18N_BUILDING           "Building LogAgg..."
#define I18N_BUILD_COMPLETE     "Build complete!"
#define I18N_BUILD_FAILED       "Build failed!"
#define I18N_COMPILE            "CC"
#define I18N_LINK               "LINK"
#define I18N_CREATE_LIB         "AR"

/* Test */
#define I18N_RUNNING_TESTS      "Running unit tests..."
#define I18N_TEST_RESULTS       "Results"
#define I18N_TESTS_PASSED       "passed"
#define I18N_TESTS_FAILED       "failed"
#define I18N_INTEGRATION_TESTS  "Integration Tests"
#define I18N_CREATING_TEST_DATA "Creating test data..."

/* Vector Clock */
#define I18N_VC_INIT            "Initialize"
#define I18N_VC_INCREMENT       "Increment"
#define I18N_VC_MERGE           "Merge"
#define I18N_VC_COMPARE         "Compare"

/* Agent */
#define I18N_AGENT_STARTING     "Starting Agent..."
#define I18N_AGENT_STARTED      "Agent started"
#define I18N_AGENT_STOPPED      "Agent stopped"
#define I18N_AGENT_NODE_ID      "Node ID"
#define I18N_AGENT_LOG_FILE     "Log File"
#define I18N_AGENT_SERVER       "Server"
#define I18N_AGENT_POLL_INTERVAL "Poll Interval"
#define I18N_AGENT_NODE_COUNT   "Node Count"
#define I18N_AGENT_SENT         "Sent"
#define I18N_AGENT_LOG_ENTRIES  "log entries"
#define I18N_AGENT_PRESS_CTRL   "Press Ctrl+C to stop"
#define I18N_AGENT_RECEIVED_SIGNAL "Received signal, stopping..."

/* Server */
#define I18N_SERVER_STARTING    "Starting Server..."
#define I18N_SERVER_STARTED     "Server started"
#define I18N_SERVER_STOPPED     "Server stopped"
#define I18N_SERVER_PORT        "Port"
#define I18N_SERVER_STORAGE     "Storage"
#define I18N_SERVER_WAITING     "Waiting for Agent connections..."
#define I18N_SERVER_RECEIVED    "Received"
#define I18N_SERVER_LOGS        "logs"
#define I18N_SERVER_STATISTICS  "Server Statistics"
#define I18N_SERVER_TOTAL_RECEIVED "Total Received"
#define I18N_SERVER_TOTAL_STORED   "Total Stored"
#define I18N_SERVER_TOTAL_ERRORS   "Total Errors"
#define I18N_SERVER_BYTES_RECEIVED "Bytes Received"

/* Store */
#define I18N_STORE_LOADING      "Loading logs..."
#define I18N_STORE_LOADED       "Loaded"
#define I18N_STORE_SORTING      "Sorting logs"
#define I18N_STORE_CAUSAL_SORT  "causal"
#define I18N_STORE_TIME_SORT    "time"
#define I18N_STORE_NODE_SORT    "node"
#define I18N_STORE_NO_LOGS      "No logs found"
#define I18N_STORE_TOTAL        "Total"

/* Query */
#define I18N_QUERY_USAGE        "Usage"
#define I18N_QUERY_REQUIRED     "Required"
#define I18N_QUERY_OPTIONS      "Options"
#define I18N_QUERY_EXAMPLES     "Examples"
#define I18N_QUERY_RESULTS      "Query Results"
#define I18N_QUERY_KEYWORD      "Keyword"
#define I18N_QUERY_NODE         "Node"
#define I18N_QUERY_LEVEL        "Level"
#define I18N_QUERY_ANY          "(any)"
#define I18N_QUERY_NO_RESULTS   "No matching logs found"
#define I18N_QUERY_STATISTICS   "Log Statistics"
#define I18N_QUERY_LOGS_BY_NODE "Logs by Node"

/* Log Levels */
#define I18N_LEVEL_DEBUG        "DEBUG"
#define I18N_LEVEL_INFO         "INFO"
#define I18N_LEVEL_WARN         "WARN"
#define I18N_LEVEL_ERROR        "ERROR"
#define I18N_LEVEL_FATAL        "FATAL"

/* Error Messages */
#define I18N_ERR_INVALID_PARAM  "Invalid parameter"
#define I18N_ERR_MISSING_PARAM  "Missing required parameter"
#define I18N_ERR_OPEN_FILE      "Cannot open file"
#define I18N_ERR_CREATE_DIR     "Cannot create directory"
#define I18N_ERR_CREATE_SOCKET  "Cannot create socket"
#define I18N_ERR_BIND_PORT      "Cannot bind port"
#define I18N_ERR_ALLOC_MEMORY   "Memory allocation failed"
#define I18N_ERR_SEND_FAILED    "Send failed"
#define I18N_ERR_RECV_FAILED    "Receive failed"
#define I18N_ERR_PARSE_JSON     "JSON parse failed"

#endif /* LANG_CN */

#endif /* I18N_H */