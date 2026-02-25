# P1 Implementation Task — 资源预算 + 进化预算 + SLP 语义原语

## 目标
补全 v2/v3/v5 设计文档中的核心功能。

## 1. 资源预算执行（v2）

Task.budget 已定义（max_tokens, max_tool_calls, max_rounds），P0 已加了 budget 检查，但需要完善：

### 修改 `src/agent/agent.hpp`
- Budget 结构体添加默认值：max_tokens=4096, max_tool_calls=10, max_rounds=5
- 添加 token 消耗追踪：`int tokens_consumed_` 成员

### 修改 `src/agent/agent.cpp`
- `call_llm` 后累加 tokens_consumed_（prompt_tokens + completion_tokens）
- `is_budget_exceeded` 用实际消耗值判断

### 修改 `src/facade/facade.cpp`
- `submit_task` 中记录 token 消耗到 OrgLogEntry
- `get_status` 中返回总 token 消耗统计

### 新建 `src/budget/budget_tracker.hpp` + `budget_tracker.cpp`
```cpp
namespace evoclaw::budget {

class BudgetTracker {
public:
    void record(const AgentId& agent, int prompt_tokens, int completion_tokens);
    
    [[nodiscard]] int total_tokens() const;
    [[nodiscard]] int agent_tokens(const AgentId& agent) const;
    [[nodiscard]] nlohmann::json report() const;
    
    // 全局预算限制
    void set_global_limit(int max_tokens);
    [[nodiscard]] bool is_global_budget_exceeded() const;
    
private:
    struct AgentUsage {
        int prompt_tokens = 0;
        int completion_tokens = 0;
        int calls = 0;
    };
    std::unordered_map<AgentId, AgentUsage> usage_;
    int global_limit_ = 0;  // 0 = 无限制
    mutable std::mutex mutex_;
};

} // namespace evoclaw::budget
```

## 2. 进化预算（v5）

限制进化频率和成本，防止进化失控。

### 修改 `src/evolution/evolver.hpp`
Config 添加：
```cpp
struct Config {
    // ... 现有字段 ...
    int max_evolution_cycles_per_hour = 5;
    int max_proposals_per_cycle = 3;
    int evolution_token_budget = 10000;  // 每轮进化最大 token
};
```

### 修改 `src/evolution/evolver.cpp`
- `monitor` 前检查进化频率（上次进化时间）
- `propose` 限制提案数量
- 进化过程中的 LLM 调用计入进化预算

### 新建 `src/evolution/evolution_budget.hpp`
```cpp
class EvolutionBudget {
public:
    explicit EvolutionBudget(int max_cycles_per_hour, int token_budget);
    
    [[nodiscard]] bool can_evolve() const;
    void record_cycle();
    void record_tokens(int tokens);
    [[nodiscard]] nlohmann::json status() const;
    
private:
    int max_cycles_per_hour_;
    int token_budget_;
    int tokens_used_ = 0;
    std::vector<Timestamp> cycle_history_;
};
```

## 3. SLP 语义原语（v3）

多粒度摘要压缩（S/M/L），用于记忆管理和上下文压缩。

### 新建 `src/slp/semantic_primitive.hpp` + `semantic_primitive.cpp`
```cpp
namespace evoclaw::slp {

enum class Granularity {
    SMALL,    // S: 一句话摘要（<50 tokens）
    MEDIUM,   // M: 段落摘要（<200 tokens）
    LARGE     // L: 完整摘要（<500 tokens）
};

struct SemanticPrimitive {
    std::string id;
    Granularity granularity;
    std::string content;
    std::string source_id;       // 原始内容 ID
    int token_count;
    Timestamp created_at;
};

class SLPCompressor {
public:
    explicit SLPCompressor(std::shared_ptr<llm::LLMClient> llm_client);
    
    // 压缩文本到指定粒度
    [[nodiscard]] SemanticPrimitive compress(
        const std::string& text,
        Granularity granularity
    ) const;
    
    // 生成三级摘要
    struct MultiGranularitySummary {
        SemanticPrimitive small;
        SemanticPrimitive medium;
        SemanticPrimitive large;
    };
    [[nodiscard]] MultiGranularitySummary compress_all(const std::string& text) const;
    
    // Mock 模式（无 LLM 时截断）
    [[nodiscard]] static SemanticPrimitive mock_compress(
        const std::string& text,
        Granularity granularity
    );
    
private:
    std::shared_ptr<llm::LLMClient> llm_client_;
    
    [[nodiscard]] static int max_tokens_for(Granularity g);
    [[nodiscard]] static std::string prompt_for(Granularity g);
};

} // namespace evoclaw::slp
```

### 集成到 Memory 层
修改 `src/memory/working_memory.hpp`：
- `one_line_summary()` 使用 SLP S 级压缩
- `paragraph_summary()` 使用 SLP M 级压缩
- 添加 `set_compressor(shared_ptr<SLPCompressor>)`

### 集成到 Facade
- `initialize()` 中创建 SLPCompressor 并注入 WorkingMemory
- 任务结果存入记忆时自动生成 S/M/L 摘要

## 4. 测试

### tests/test_budget.cpp
- BudgetTracker 记录和查询
- 全局预算限制检查
- Agent 级别统计

### tests/test_evolution_budget.cpp
- 进化频率限制
- 进化 token 预算

### tests/test_slp.cpp
- Mock 压缩（截断模式）
- 三级摘要生成
- 粒度 token 限制

## 5. Dashboard API 更新

### 新增 REST 端点
- `GET /api/budget` — 返回 token 消耗报告
- `GET /api/evolution/budget` — 返回进化预算状态

### Dashboard HTML 更新
- 添加 Budget 面板（显示 token 消耗、Agent 级别统计）
- 进化面板添加预算状态

## 6. 更新 CMakeLists.txt
添加新的 .cpp 文件：
- src/budget/budget_tracker.cpp
- src/evolution/evolution_budget.cpp（如果需要单独 cpp）
- src/slp/semantic_primitive.cpp

## 回测要求
1. `./tests/evoclaw_test` — 所有测试通过
2. 启动 server_demo，提交任务，验证 budget 统计正确
3. 触发进化，验证进化预算限制生效
4. Dashboard 显示 budget 信息
5. SSE 推送 budget 相关事件

---
开始实现。
