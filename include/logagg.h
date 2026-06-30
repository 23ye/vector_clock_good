//项目主头文件
#ifndef LOGAGG_H
#define LOGAGG_H

/*
 * LogAgg - 基于向量时钟的分布式日志聚合系统
 *
 * 核心模块:
 *   - vector_clock: 向量时钟实现
 *   - log_entry:    日志条目格式
 *   - agent:        日志采集代理 (Day 2)
 *   - server:       聚合服务器 (Day 3)
 *   - query:        查询接口 (Day 5)
 *   - index:        倒排索引 (Day 6)
 */

#include "vector_clock.h"
#include "log_entry.h"

/* 版本信息 */
#define LOGAGG_VERSION_MAJOR 0
#define LOGAGG_VERSION_MINOR 1
#define LOGAGG_VERSION_PATCH 0

/* 默认配置 */
#define LOGAGG_DEFAULT_PORT      9999
#define LOGAGG_DEFAULT_LOG_DIR   "./logs"
#define LOGAGG_MAX_ENTRY_SIZE    4096

#endif /* LOGAGG_H */