# P3 Task: OrgLog 时间范围查询 + Router 能力矩阵持久化

## Feature 1: OrgLog 时间范围查询

### 文件
- `src/memory/org_log.hpp` + `org_log.cpp` — 添加 `query_by_time_range` 方法

### 设计
```cpp
// 在 OrgLog 类中添加：

// 按时间范围查询日志条目
[[nodiscard]] std::vector<OrgLogEntry> query_by_time_range(
    Timestamp start,
    Timestamp end
) const;

// 按 agent 和时间范围查询
[[nodiscard]] std::vector<OrgLogEntry> query_by_agent_and_time(
    const AgentId& agent_id,
    Timestamp start,
    Timestamp end
) const;

// 获取时间范围内的统计摘要
struct TimeRangeStats {
    int total_tasks = 0;
    int successful_tasks = 0;
    double avg_duration_ms = 0.0;
    double avg_critic_score = 0.0;
    std::unordered_map<AgentId, int> tasks_by_agent;
};

[[nodiscard]] TimeRangeStats get_stats_for_range(
    Timestamp start,
    Timestamp end
) const;
```

### Server 端点
- `GET /api/logs?start=<iso8601>&end=<iso8601>` — 时间范围查询
- `GET /api/logs/stats?start=<iso8601>&end=<iso8601>` — 时间范围统计

## Feature 2: Router 能力矩阵持久化

### 文件
- `src/router/router.hpp` + `router.cpp` — 添加 `save_matrix` / `load_matrix` 方法

### 设计
```cpp
// 在 Router 类中添加：

// 保存能力矩阵到文件
void save_matrix(const std::filesystem::path& path) const;

// 从文件加载能力矩阵
void load_matrix(const std::filesystem::path& path);

// 矩阵 JSON 格式：
// {
//   "version": "1.0",
//   "saved_at": "2026-02-25T16:30:00",
//   "matrix": {
//     "agent-id": {
//       "intent-tag": { "score": 0.85, "observations": 42 }
//     }
//   }
// }
```

### Facade 集成
- 在 `initialize()` 中尝试加载已有矩阵
- 添加 `save_state()` 方法保存当前状态
- 在 `trigger_evolution()` 后自动保存

### Server 端点
- `POST /api/matrix/save` — 手动触发保存
- `GET /api/matrix` — 获取当前矩阵（已有，确认格式）

## 测试

### tests/test_org_log_query.cpp
```cpp
TEST(OrgLogQueryTest, QueryByTimeRange) {
    // 创建多个不同时间的条目
    // 验证时间范围查询返回正确子集
}

TEST(OrgLogQueryTest, QueryByAgentAndTime) {
    // 验证按 agent + 时间过滤
}

TEST(OrgLogQueryTest, GetStatsForRange) {
    // 验证统计计算正确
}
```

### tests/test_matrix_persistence.cpp
```cpp
TEST(MatrixPersistenceTest, SaveAndLoad) {
    // 创建 Router，注册 agent，路由任务
    // 保存矩阵
    // 创建新 Router，加载矩阵
    // 验证矩阵内容一致
}

TEST(MatrixPersistenceTest, LoadNonexistentFileIsNoOp) {
    // 加载不存在的文件不应崩溃
}
```

## CMakeLists.txt
- 添加新测试文件到 tests/CMakeLists.txt

## 验收标准
1. `make -j4` 编译通过
2. 所有测试通过（包括新增测试）
3. `/api/logs?start=...&end=...` 返回正确过滤结果
4. `/api/logs/stats?start=...&end=...` 返回统计摘要
5. `/api/matrix/save` 成功保存文件
6. 重启 server_demo 后矩阵数据恢复

## 完成后
```bash
openclaw system event --text "Done: P3 OrgLog time query + matrix persistence" --mode now
```
