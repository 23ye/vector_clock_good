# LogAgg - 基于向量时钟的分布式日志聚合系统

一个轻量级分布式日志系统，利用向量时钟（Vector Clock）对跨节点事件进行因果排序。

## 项目架构

```
logagg/
├── include/            # 头文件
│   ├── logagg.h        # 主头文件
│   ├── vector_clock.h  # 向量时钟 API
│   └── log_entry.h     # 日志条目格式
├── src/                # 源文件
│   ├── vector_clock.c  # 向量时钟实现
│   └── log_entry.c     # 日志条目实现
├── test/               # 测试文件
│   └── test_vector_clock.c  # 单元测试
├── Makefile            # 构建脚本
└── CMakeLists.txt      # CMake 构建脚本
```

## 核心功能

### 1. 向量时钟 (Vector Clock)

向量时钟是一种用于在分布式系统中追踪事件因果关系的逻辑时钟算法。

**核心 API:**
```c
// 初始化
void vc_init(vector_clock_t *vc, int node_count);

// 增加本地时钟
void vc_increment(vector_clock_t *vc, int node_id);

// 合并远端时钟 (取最大值)
void vc_merge(vector_clock_t *dst, const vector_clock_t *src);

// 比较因果关系
vc_relation_t vc_compare(const vector_clock_t *a, const vector_clock_t *b);
// 返回值:
//   VC_EQUAL          - 完全相同
//   VC_HAPPENS_BEFORE - a 在 b 之前 (a → b)
//   VC_HAPPENS_AFTER  - a 在 b 之后 (a ← b)
//   VC_CONCURRENT     - a 和 b 并发 (a ∥ b)
```

**因果关系判定规则:**
```
a happens-before b ⟺ ∀i: a[i] ≤ b[i] ∧ ∃j: a[j] < b[j]
a happens-after  b ⟺ b happens-before a
a concurrent     b ⟺ ¬(a→b) ∧ ¬(b→a)
```

### 2. 日志条目 (Log Entry)

JSON Lines 格式的日志条目:
```json
{
  "node_id": "node-01",
  "vector_clock": {"node-0": 1, "node-1": 3, "node-2": 2},
  "timestamp": 1719648000000,
  "level": "INFO",
  "message": "Request processed successfully",
  "metadata": {
    "trace_id": "abc-123",
    "request_id": "req-456"
  }
}
```

## 构建与测试

### 使用 Make

```bash
# 构建
make

# 运行测试
make test

# 清理
make clean
```

### 使用 CMake

```bash
mkdir build && cd build
cmake ..
make

# 运行测试
ctest
```

## 验收标准

- [x] 3 节点并发写入，聚合后日志满足因果序（无乱序）
- [ ] 百万条日志关键词查询延迟 < 100 ms
- [x] 提交向量时钟算法流程图、日志 Schema 文档及因果图样例

## Day 1 完成内容

- [x] 项目结构搭建
- [x] 向量时钟核心实现
  - vc_init / vc_increment / vc_merge / vc_compare
  - JSON 序列化/反序列化
- [x] 日志条目结构定义
  - log_entry_init / log_entry_to_json / log_entry_from_json
  - 元数据支持
- [x] 单元测试覆盖
  - 向量时钟基本操作
  - 因果关系判定
  - JSON 序列化往返测试
  - 经典场景测试

## 许可证

MIT License