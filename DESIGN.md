# EvoClaw C++ Implementation — Design Document

## 1. 需求分解

### 1.1 核心目标
基于 EvoClaw v1-v5 设计文档，实现一个 C++ 版本的自进化多 Agent 组织框架。

### 1.2 MVP 范围（v1 + v2 核心 + v3 UMI 基础）

**Phase 1（必须实现）：**
1. 通信协议层 — 结构化消息、序列化、消息总线
2. Agent 框架 — 基类、角色、执行/反射接口
3. Router — UMI 能力匹配、ε-greedy 路由
4. 记忆层 — 工作记忆、组织日志（append-only）
5. 进化机制 — 触发条件、A/B 测试基础
6. 治理内核 — 宪法约束、权限裁决

**Phase 2（扩展）：**
7. 双区架构 — 稳定区 + 探索区
8. Compiler — Pattern 检测、插件编译
9. Meta Judge + 挑战集
10. 反熵机制

### 1.3 C++ 技术选型

| 组件 | 技术方案 | 理由 |
|------|----------|------|
| 标准 | C++20/23 | Concepts, coroutines, std::format |
| 构建 | CMake 3.25+ | 跨平台、模块化 |
| 序列化 | nlohmann/json | 结构化消息、配置 |
| 日志 | spdlog | 高性能、结构化 |
| 测试 | Google Test | 单元测试、mock |
| 并发 | std::thread + coroutines | 现代 C++ 并发模型 |
| 插件 | 编译时注册 + 动态加载（可选） | MVP 先用静态注册 |

---

## 2. 架构总览

```
┌─────────────────────────────────────────────────────────────┐
│                    Facade（用户接口层）                       │
├─────────────────────────────────────────────────────────────┤
│                    Governance Kernel                         │
│         （宪法约束、权限裁决、发布/回滚门禁）                   │
├─────────────────────────────────────────────────────────────┤
│                    Router（路由层）                           │
│     UMI 匹配、ε-greedy、能力矩阵、冷启动配额                   │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │  Stable Zone │  │ Explore Zone │  │   Compiler          │  │
│  │  （快路径）   │  │ （慢路径）   │  │ （Pattern→Plugin）   │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                    Agent Pool（Agent 池）                     │
│         Planner / Executor / Critic / Router / Observer      │
├─────────────────────────────────────────────────────────────┤
│                    Message Bus（消息总线）                     │
│         结构化消息、去重、限流、链路追踪                        │
├─────────────────────────────────────────────────────────────┤
│                    Memory Layer（记忆层）                      │
│    Working Memory │ Org Log │ Module Memory（简化版）         │
├─────────────────────────────────────────────────────────────┤
│                    Event Log（不可变事件账本）                  │
│              Append-only，hash chain 完整性校验                │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. 模块设计

### 3.1 Core 模块（core/）

基础类型和工具。

```cpp
// core/types.hpp
namespace evoclaw {

using MessageId = std::string;
using AgentId = std::string;
using TaskId = std::string;
using Timestamp = std::chrono::system_clock::time_point;

enum class TaskType {
    ROUTE,
    EXECUTE,
    CRITIQUE,
    SYNTHESIZE,
    EVOLVE
};

enum class TaskState {
    PENDING,
    PLANNING,
    EXECUTING,
    EVALUATING,
    COMPLETED,
    FAILED,
    RETRY_NEEDED
};

} // namespace evoclaw
```

### 3.2 Protocol 模块（protocol/）

结构化消息系统。

```cpp
// protocol/message.hpp
namespace evoclaw::protocol {

enum class Performative {
    INFORM,      // 通知/汇报
    REQUEST,     // 请求执行
    PROPOSE,     // 提案
    CRITIQUE,    // 批评/反馈
    VOTE,        // 投票
    REPORT,      // 结果报告
    TENSION      // 张力信号
};

struct Message {
    MessageId id;
    Performative performative;
    AgentId sender;
    std::vector<AgentId> receivers;
    nlohmann::json payload;
    nlohmann::json metadata;
    std::optional<MessageId> parent_id;
    
    // 序列化
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static std::optional<Message> from_json(const std::string& json);
};

// protocol/bus.hpp
class MessageBus {
public:
    using Handler = std::function<void(const Message&)>;
    
    void subscribe(AgentId agent_id, Handler handler);
    void send(const Message& msg);
    void broadcast(const Message& msg);
    
    // 去重（基于 message id）
    // 限流（滑动窗口）
    
private:
    std::unordered_map<AgentId, Handler> handlers_;
    std::unordered_set<MessageId> seen_ids_;
    // 限流状态
};

} // namespace evoclaw::protocol
```

### 3.3 UMI 模块（umi/）

通用模块接口系统。

```cpp
// umi/contract.hpp
namespace evoclaw::umi {

// 能力声明
struct CapabilityProfile {
    std::vector<std::string> intent_tags;
    std::vector<std::string> required_tools;
    double estimated_cost_token;
    double success_rate_threshold;
};

// 安全声明
enum class AirbagLevel {
    NONE,
    BASIC,      // 基本熔断
    ENHANCED,   // 熔断 + 沙箱
    MAXIMUM     // 完全隔离
};

struct AirbagSpec {
    AirbagLevel level;
    std::vector<std::string> permission_whitelist;
    std::vector<std::string> blast_radius_scope;
    bool reversible;
};

// UMI 契约（所有模块必须实现）
struct ModuleContract {
    std::string module_id;
    std::string version;
    CapabilityProfile capability;
    AirbagSpec airbag;
    nlohmann::json cost_model;
    
    // 验证契约完整性
    [[nodiscard]] bool validate() const;
};

} // namespace evoclaw::umi
```

### 3.4 Agent 模块（agent/）

Agent 基类和具体角色。

```cpp
// agent/agent.hpp
namespace evoclaw::agent {

struct AgentConfig {
    AgentId id;
    std::string role;
    std::string system_prompt;
    umi::ModuleContract contract;  // UMI 契约
    double temperature = 0.3;
};

struct Task {
    TaskId id;
    std::string description;
    nlohmann::json context;
    TaskType type;
    // v2.0 资源预算
    struct Budget {
        int max_tokens;
        int max_tool_calls;
        int max_rounds;
    } budget;
};

struct TaskResult {
    TaskId task_id;
    AgentId agent_id;
    bool success;
    std::string output;
    nlohmann::json metadata;
    double confidence;
};

struct Reflection {
    AgentId agent_id;
    std::string summary;
    std::vector<std::string> improvements;
    Timestamp created_at;
};

class Agent {
public:
    explicit Agent(AgentConfig config);
    virtual ~Agent() = default;
    
    [[nodiscard]] const AgentId& id() const { return config_.id; }
    [[nodiscard]] const std::string& role() const { return config_.role; }
    [[nodiscard]] const umi::ModuleContract& contract() const { return config_.contract; }
    
    // 核心接口
    virtual TaskResult execute(const Task& task) = 0;
    virtual Reflection reflect(const TaskResult& result) = 0;
    
    // 消息处理
    virtual void on_message(const protocol::Message& msg);
    
protected:
    AgentConfig config_;
    std::shared_ptr<protocol::MessageBus> bus_;
};

// 具体角色
class Planner : public Agent {
public:
    TaskResult execute(const Task& task) override;
    Reflection reflect(const TaskResult& result) override;
};

class Executor : public Agent {
public:
    TaskResult execute(const Task& task) override;
    Reflection reflect(const TaskResult& result) override;
};

class Critic : public Agent {
public:
    TaskResult execute(const Task& task) override;  // 评估任务
    Reflection reflect(const TaskResult& result) override;
    
    // Critic 评分
    [[nodiscard]] double score(const TaskResult& result) const;
};

} // namespace evoclaw::agent
```

### 3.5 Router 模块（router/）

任务路由系统。

```cpp
// router/router.hpp
namespace evoclaw::router {

// 能力矩阵单元格
struct CapabilityCell {
    AgentId agent_id;
    std::string intent_tag;
    double score;           // 历史质量评分
    int usage_count;        // 使用次数
    Timestamp last_used;
};

// 能力矩阵
class CapabilityMatrix {
public:
    void update(const AgentId& agent, const std::string& intent, double score);
    [[nodiscard]] std::optional<AgentId> best_for(const std::string& intent) const;
    [[nodiscard]] std::vector<AgentId> candidates_for(const std::string& intent) const;
    
private:
    std::unordered_map<std::string, std::vector<CapabilityCell>> matrix_;
    mutable std::shared_mutex mutex_;
};

struct RoutingConfig {
    double epsilon = 0.1;           // ε-greedy 探索率
    double epsilon_boost = 0.2;     // 探索区有新模块时的提升
    double epsilon_min = 0.05;      // 最小探索率
    double cold_start_quota = 0.15; // 冷启动流量配额
    int cold_start_days = 14;       // 冷启动期
};

class Router {
public:
    explicit Router(RoutingConfig config);
    
    // 注册 Agent
    void register_agent(const std::shared_ptr<agent::Agent>& agent);
    
    // 路由决策
    [[nodiscard]] std::optional<AgentId> route(const agent::Task& task);
    
    // ε-greedy 路由
    [[nodiscard]] AgentId route_with_exploration(const std::string& intent);
    
    // UMI 匹配
    [[nodiscard]] std::vector<AgentId> match_by_contract(
        const umi::CapabilityProfile& required
    ) const;
    
private:
    RoutingConfig config_;
    CapabilityMatrix matrix_;
    std::unordered_map<AgentId, std::shared_ptr<agent::Agent>> agents_;
    std::unordered_map<AgentId, Timestamp> registration_time_;
    
    [[nodiscard]] bool is_cold_start(const AgentId& agent) const;
};

} // namespace evoclaw::router
```

### 3.6 Memory 模块（memory/）

分层记忆系统。

```cpp
// memory/working_memory.hpp
namespace evoclaw::memory {

// 工作记忆 — 当前会话上下文
class WorkingMemory {
public:
    void store(const std::string& key, nlohmann::json value);
    [[nodiscard]] std::optional<nlohmann::json> retrieve(const std::string& key) const;
    void clear();
    
    // 生成多粒度摘要
    [[nodiscard]] std::string one_line_summary() const;
    [[nodiscard]] std::string paragraph_summary() const;
    
private:
    std::unordered_map<std::string, nlohmann::json> data_;
    std::vector<std::string> conversation_history_;
};

// memory/org_log.hpp

// 组织日志条目
struct OrgLogEntry {
    std::string entry_id;
    Timestamp timestamp;
    TaskId task_id;
    AgentId agent_id;
    std::string module_version;
    double duration_ms;
    double critic_score;
    bool user_feedback_positive;
    nlohmann::json metadata;
};

// Append-only 组织日志
class OrgLog {
public:
    explicit OrgLog(const std::filesystem::path& log_dir);
    
    void append(const OrgLogEntry& entry);
    
    // 查询接口
    [[nodiscard]] std::vector<OrgLogEntry> query_by_agent(
        const AgentId& agent,
        int limit = 100
    ) const;
    
    [[nodiscard]] std::vector<OrgLogEntry> query_by_time_range(
        Timestamp start,
        Timestamp end
    ) const;
    
    // 统计
    [[nodiscard]] double average_score(const AgentId& agent) const;
    
private:
    std::filesystem::path log_dir_;
    std::ofstream log_file_;
    std::mutex mutex_;
};

} // namespace evoclaw::memory
```

### 3.7 Event Log 模块（event_log/）

不可变事件账本。

```cpp
// event_log/event_log.hpp
namespace evoclaw::event_log {

enum class EventType {
    EVOLUTION,
    CIRCUIT_BREAK,
    ROLLBACK,
    AGENT_SPAWN,
    AGENT_RETIRE,
    GUARDRAIL_BLOCK,
    CONFIG_CHANGE,
    TASK_COMPLETE,
    TASK_FAILED
};

struct Event {
    std::string id;
    Timestamp timestamp;
    EventType type;
    std::string actor;
    std::string target;
    std::string action;
    nlohmann::json details;
    std::string prev_hash;
    std::string hash;
};

// 不可变事件账本（hash chain）
class EventLog {
public:
    explicit EventLog(const std::filesystem::path& log_path);
    
    void append(Event event);
    
    [[nodiscard]] std::vector<Event> query(
        const std::unordered_map<std::string, nlohmann::json>& filters
    ) const;
    
    // 完整性校验
    [[nodiscard]] bool verify_integrity() const;
    
private:
    std::filesystem::path log_path_;
    std::ofstream log_file_;
    std::mutex mutex_;
    
    [[nodiscard]] std::optional<Event> last_event() const;
    [[nodiscard]] static std::string calculate_hash(const Event& event);
};

} // namespace evoclaw::event_log
```

### 3.8 Governance 模块（governance/）

治理内核。

```cpp
// governance/constitution.hpp
namespace evoclaw::governance {

// 宪法约束
struct Constitution {
    std::string mission;
    std::vector<std::string> values;
    std::vector<std::string> guardrails;  // 硬性约束
    nlohmann::json evolution_policy;
    
    // 检查是否违反宪法
    [[nodiscard]] bool check_violation(const nlohmann::json& action) const;
};

// governance/governance_kernel.hpp

enum class Permission {
    ALLOW,
    DENY,
    REQUIRE_CONFIRMATION
};

class GovernanceKernel {
public:
    explicit GovernanceKernel(Constitution constitution);
    
    // 权限裁决
    [[nodiscard]] Permission evaluate_action(
        const std::string& actor,
        const std::string& action,
        const nlohmann::json& details
    ) const;
    
    // 发布门禁
    [[nodiscard]] bool allow_deployment(
        const umi::ModuleContract& contract,
        const std::vector<double>& ab_test_scores
    ) const;
    
    // 回滚决策
    [[nodiscard]] bool should_rollback(
        const AgentId& agent,
        double recent_score,
        double baseline_score
    ) const;
    
private:
    Constitution constitution_;
    event_log::EventLog& event_log_;
};

} // namespace evoclaw::governance
```

### 3.9 Evolution 模块（evolution/）

进化引擎。

```cpp
// evolution/tension.hpp
namespace evoclaw::evolution {

enum class TensionType {
    KPI_DECLINE,           // KPI 下降
    REPEATED_FAILURE,      // 重复失败
    REFLECTION_SUGGESTION, // 反思建议
    MANUAL                 // 手动触发
};

struct Tension {
    std::string id;
    TensionType type;
    std::optional<AgentId> source_agent;
    std::string description;
    std::string severity;  // low, medium, high
    nlohmann::json metadata;
};

// evolution/evolution_proposal.hpp

enum class EvolutionType {
    ADJUSTMENT,    // 调整提示词
    REPLACEMENT,   // 替换模块
    RESTRUCTURING  // 重构（需确认）
};

struct EvolutionProposal {
    std::string id;
    EvolutionType type;
    AgentId target_agent;
    std::string description;
    nlohmann::json old_value;
    nlohmann::json new_value;
    std::string rationale;
    std::vector<std::string> tension_ids;
};

// evolution/ab_test.hpp

struct ABTestResult {
    AgentId control_id;
    AgentId candidate_id;
    double control_score;
    double candidate_score;
    int sample_size;
    double p_value;
    bool significant;
};

// evolution/evolver.hpp

class Evolver {
public:
    struct Config {
        double kpi_decline_threshold = 0.8;  // 低于历史均值 80% 触发
        int consecutive_failures = 10;
        double min_improvement = 0.05;       // 5% 改进门槛
        double p_value_threshold = 0.05;
    };
    
    explicit Evolver(Config config, GovernanceKernel& governance);
    
    // 监控张力
    [[nodiscard]] std::vector<Tension> monitor(
        const memory::OrgLog& log
    ) const;
    
    // 生成进化提案
    [[nodiscard]] std::vector<EvolutionProposal> propose(
        const std::vector<Tension>& tensions
    ) const;
    
    // A/B 测试
    [[nodiscard]] ABTestResult run_ab_test(
        const EvolutionProposal& proposal,
        const std::vector<agent::Task>& test_tasks
    );
    
    // 应用进化
    bool apply_evolution(
        const EvolutionProposal& proposal,
        const ABTestResult& test_result
    );
    
private:
    Config config_;
    GovernanceKernel& governance_;
    event_log::EventLog& event_log_;
};

} // namespace evoclaw::evolution
```

### 3.10 Facade 模块（facade/）

用户接口层。

```cpp
// facade/facade.hpp
namespace evoclaw::facade {

class EvoClawFacade {
public:
    struct Config {
        std::filesystem::path log_dir = "./logs";
        std::filesystem::path config_path = "./constitution.json";
        router::RoutingConfig router_config;
        evolution::Evolver::Config evolver_config;
    };
    
    explicit EvoClawFacade(Config config);
    
    // 初始化
    void initialize();
    
    // 注册 Agent
    void register_agent(std::shared_ptr<agent::Agent> agent);
    
    // 提交任务
    [[nodiscard]] agent::TaskResult submit_task(const agent::Task& task);
    
    // 触发进化（手动或自动）
    void trigger_evolution();
    
    // 获取系统状态
    [[nodiscard]] nlohmann::json get_status() const;
    
    // 获取能力矩阵视图
    [[nodiscard]] nlohmann::json get_capability_matrix() const;
    
private:
    Config config_;
    
    // 核心组件
    std::unique_ptr<protocol::MessageBus> bus_;
    std::unique_ptr<router::Router> router_;
    std::unique_ptr<memory::OrgLog> org_log_;
    std::unique_ptr<event_log::EventLog> event_log_;
    std::unique_ptr<governance::GovernanceKernel> governance_;
    std::unique_ptr<evolution::Evolver> evolver_;
    
    // Agent 池
    std::unordered_map<AgentId, std::shared_ptr<agent::Agent>> agents_;
    
    void run_evolution_cycle();
};

} // namespace evoclaw::facade
```

---

## 4. 目录结构

```
EvoClaw/
├── CMakeLists.txt
├── README.md
├── DESIGN.md
├── constitution.json          # 宪法配置
├── src/
│   ├── core/                  # 基础类型
│   │   ├── types.hpp
│   │   └── utils.hpp
│   ├── protocol/              # 通信协议
│   │   ├── message.hpp
│   │   ├── message.cpp
│   │   └── bus.hpp
│   ├── umi/                   # 通用模块接口
│   │   ├── contract.hpp
│   │   └── validator.hpp
│   ├── agent/                 # Agent 框架
│   │   ├── agent.hpp
│   │   ├── agent.cpp
│   │   ├── planner.hpp
│   │   ├── executor.hpp
│   │   └── critic.hpp
│   ├── router/                # 路由系统
│   │   ├── router.hpp
│   │   ├── router.cpp
│   │   └── capability_matrix.hpp
│   ├── memory/                # 记忆层
│   │   ├── working_memory.hpp
│   │   └── org_log.hpp
│   ├── event_log/             # 事件账本
│   │   ├── event_log.hpp
│   │   └── event_log.cpp
│   ├── governance/            # 治理内核
│   │   ├── constitution.hpp
│   │   └── governance_kernel.hpp
│   ├── evolution/             # 进化引擎
│   │   ├── tension.hpp
│   │   ├── evolver.hpp
│   │   └── evolver.cpp
│   └── facade/                # 用户接口
│       └── facade.hpp
├── tests/                     # 测试
│   ├── CMakeLists.txt
│   ├── test_protocol.cpp
│   ├── test_agent.cpp
│   ├── test_router.cpp
│   ├── test_memory.cpp
│   ├── test_event_log.cpp
│   ├── test_governance.cpp
│   ├── test_evolution.cpp
│   └── test_integration.cpp
├── examples/                  # 示例
│   ├── basic_usage.cpp
│   ├── evolution_demo.cpp
│   └── full_system.cpp
└── third_party/               # 第三方库
    ├── json.hpp               # nlohmann/json（单头文件）
    └── ...
```

---

## 5. 构建配置

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.25)
project(EvoClaw VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 依赖
find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)

# 核心库
add_library(evoclaw_core
    src/protocol/message.cpp
    src/protocol/bus.cpp
    src/agent/agent.cpp
    src/router/router.cpp
    src/memory/org_log.cpp
    src/event_log/event_log.cpp
    src/evolution/evolver.cpp
)

target_include_directories(evoclaw_core PUBLIC src)
target_link_libraries(evoclaw_core PUBLIC
    Threads::Threads
    spdlog::spdlog
)

# 测试
enable_testing()
find_package(GTest REQUIRED)
add_subdirectory(tests)

# 示例
add_subdirectory(examples)
```

---

## 6. 关键设计决策

### 6.1 为什么用 C++ 而不是 Python？

1. **性能**：Router 决策、消息路由、日志写入需要低延迟
2. **类型安全**：UMI 契约在编译期验证更多约束
3. **资源控制**：内存、线程、文件句柄精确控制
4. **部署**：单二进制文件，无依赖地狱

### 6.2 消息传递：共享内存 vs 序列化？

- **同进程**：使用 `std::shared_ptr<const Message>` 零拷贝传递
- **跨进程（未来）**：序列化到共享内存或 socket
- **现在**：单进程多线程，直接对象传递

### 6.3 Agent 执行模型

```cpp
// 协程风格的异步执行
task::TaskResult Agent::execute(const task::Task& task) {
    // 1. 加载上下文（从 Working Memory）
    // 2. 调用 LLM（或模拟）
    // 3. 处理结果
    // 4. 更新记忆
    // 5. 返回
}
```

### 6.4 插件系统（MVP 简化）

- **Phase 1**：静态编译注册，Agent 类型硬编码
- **Phase 2**：动态加载 `.so` 插件
- **Phase 3**：Compiler 自动生成插件

---

## 7. 测试策略

### 7.1 单元测试

每个模块独立测试，使用 Google Test + gmock。

```cpp
// test_router.cpp
TEST(RouterTest, UMICapabilityMatching) {
    router::Router router({});
    
    auto planner = std::make_shared<agent::Planner>(config);
    router.register_agent(planner);
    
    task::Task task{...};
    auto agent_id = router.route(task);
    
    EXPECT_TRUE(agent_id.has_value());
}

TEST(RouterTest, EpsilonGreedyExploration) {
    // 测试 ε-greedy 探索率
}
```

### 7.2 集成测试

完整工作流测试。

```cpp
TEST(IntegrationTest, FullEvolutionCycle) {
    facade::EvoClawFacade system({...});
    system.initialize();
    
    // 注册 Agent
    // 提交任务
    // 触发进化
    // 验证结果
}
```

---

## 8. 实施计划

| 阶段 | 模块 | 时间 | 验收标准 |
|------|------|------|----------|
| P1 | Core + Protocol | 2 天 | 消息序列化/反序列化测试通过 |
| P2 | UMI + Agent | 2 天 | Planner/Executor/Critic 可实例化 |
| P3 | Router | 2 天 | ε-greedy 路由测试通过 |
| P4 | Memory + Event Log | 2 天 | Append-only 日志 + hash chain 验证 |
| P5 | Governance | 1 天 | 宪法约束检查测试通过 |
| P6 | Evolution | 2 天 | A/B 测试框架 + 张力检测 |
| P7 | Facade + 集成 | 2 天 | 完整工作流 demo 运行 |
| P8 | 测试覆盖 | 2 天 | 80%+ 代码覆盖率 |

**总计：约 15 天 MVP**

---

## 9. 待确认问题

1. **LLM 调用**：是否需要真实 LLM 集成？还是先用 mock？
2. **持久化**：OrgLog/EventLog 用文件还是嵌入式数据库（SQLite）？
3. **并发模型**：Agent 执行是单线程还是线程池？
4. **配置格式**：JSON 还是 YAML 还是 TOML？

---

*设计先行，代码后行。*


## 4. P9 设计：Runtime Config 版本化与 Diff 审计

### 4.1 目标
在现有 runtime patch / snapshot / rollback 基础上，引入**版本化配置历史**与**结构化 diff 审计**，解决以下问题：

1. 当前 snapshot 为全量快照，不利于长期审计
2. proposal 应用后只能看到 before/after，不能直接查询“改了哪些字段”
3. 缺少连续版本号，难以表达 agent runtime config 的演进轨迹

### 4.2 设计原则
- **版本单调递增**：每个 agent 独立维护 runtime config version
- **diff 优先**：持久化时记录 before/after 与字段级 diff
- **兼容现有 snapshot 机制**：rollback 仍然基于快照恢复，不破坏现有路径
- **最小侵入**：先做 facade/agent 级审计，不改 Evolver 协议结构

### 4.3 新增对象

```cpp
struct RuntimeConfigVersionRecord {
    AgentId agent_id;
    std::uint64_t version;
    std::string proposal_id;          // 可为空，表示手工 patch
    nlohmann::json before;
    nlohmann::json after;
    nlohmann::json diff;
    Timestamp changed_at;
};
```

### 4.4 diff 语义
`diff` 使用扁平字段映射：

```json
{
  "temperature": { "before": 0.3, "after": 0.4 },
  "success_rate_threshold": { "before": 0.75, "after": 0.82 },
  "system_prompt": { "before": "...", "after": "..." }
}
```

规则：
- 仅记录发生变化的字段
- 数组字段整体替换，不做 element-level diff
- 不支持任意 JSON patch RFC，保持审计可读性优先

### 4.5 Facade 责任
新增 facade 内部状态：
- `runtime_config_versions_[agent_id] -> current_version`
- `runtime_config_history_ -> vector<RuntimeConfigVersionRecord>`

新增接口：
- `get_agent_runtime_version(agent_id)`
- `get_agent_runtime_history(agent_id)`
- `get_agent_runtime_diff(agent_id, from_version, to_version)`

### 4.6 生命周期
- patch 应用成功后：
  1. 读取 before/after
  2. 计算 diff
  3. 当前 agent version += 1
  4. 追加 RuntimeConfigVersionRecord
- rollback 成功后：
  - 同样记作一次新版本，而不是“删除历史”

### 4.7 持久化
新增文件：
- `log_dir/runtime_config_history.json`
- `log_dir/runtime_config_versions.json`

初始化时自动恢复；`save_state()` 时自动落盘。

### 4.8 Status / 可观测性增强
`get_status()` 增加轻量 runtime config 摘要：

```json
{
  "runtime_config": {
    "tracked_agents": 2,
    "history_entries": 5,
    "agents": [
      {
        "agent_id": "executor-1",
        "current_version": 2,
        "history_count": 2,
        "latest_changed_at": "2026-03-07T13:48:00"
      }
    ]
  }
}
```

约束：
- `status` 只放**摘要**，不内嵌完整 history / diff，避免 payload 过大
- `latest_changed_at` 取该 agent 最新一条 version record 的 `changed_at`
- 没有 runtime history 的 agent 仍应出现在摘要中，`current_version=0`，`history_count=0`

### 4.9 History 治理 / 裁剪
新增 facade 接口：
- `clear_old_runtime_config_history(std::size_t keep_last_per_agent)`

语义：
- 按 **agent 维度** 分组裁剪
- 每个 agent 仅保留最近 `keep_last_per_agent` 条 `RuntimeConfigVersionRecord`
- **不重写 / 不重编号** `current_version`
  - 例如 agent 当前 version 已到 12，裁剪后即使只剩最近 3 条，当前 version 仍为 12
- `runtime_config_versions_` 保留当前最新 version，作为单调递增计数器
- `get_status().runtime_config.history_entries` 与 `history_count` 必须反映裁剪后结果

查询一致性约束：
- `get_agent_runtime_history(agent_id)` 返回裁剪后的剩余历史
- `get_agent_runtime_diff(agent_id, from_version, to_version)`：
  - 若 `from_version` 或 `to_version` 所需 snapshot 已被裁剪掉，返回 `version_not_found`
  - 不尝试从外部重建被裁剪历史

持久化语义：
- 裁剪后调用 `save_state()`，落盘文件只保存裁剪后的 history
- `runtime_config_versions.json` 仍保留最新 version 计数，不回退

### 4.10 Server API 暴露
在 `server` 层暴露 runtime config 相关 REST 接口：

- `GET /api/runtime-config/version?agent_id=<id>`
  - 返回 `facade.get_agent_runtime_version(agent_id)`
- `GET /api/runtime-config/history?agent_id=<id>`
  - 返回 `facade.get_agent_runtime_history(agent_id)`
- `GET /api/runtime-config/diff?agent_id=<id>&from_version=<n>&to_version=<n>`
  - 返回 `facade.get_agent_runtime_diff(agent_id, from_version, to_version)`
- `POST /api/runtime-config/history/prune`
  - body: `{ "keep_last_per_agent": 2 }`
  - 调用 `clear_old_runtime_config_history()` 后 `save_state()`
  - 返回裁剪后的 runtime status 摘要

约束：
- 参数校验失败返回 `400`
- facade 返回 `{"error":"agent_not_found"...}` / `{"error":"version_not_found"...}` 时：
  - 查询类接口返回 `404`
- `prune` 成功返回 `200`，包含：
  - `ok: true`
  - `keep_last_per_agent`
  - `runtime_config`（来自最新 `get_status()` 的摘要）
- 不在 server 层复制业务逻辑，只做参数解析、错误映射、调用 facade

### 4.11 测试要求
- patch 成功后 version 递增
- history 能反映 before/after/diff
- rollback 产生新版本记录
- 持久化后重启可恢复版本与历史
- `get_status()` 能返回 runtime config 摘要，并正确反映 version / history_count
- 裁剪后每个 agent 只保留最近 N 条 history
- 裁剪后 current_version 不回退
- 裁剪后对已丢失版本做 diff 查询返回 `version_not_found`
- server API 能正确返回 version/history/diff/prune 结果与 HTTP status

### 4.12 Dashboard 前端对接
Dashboard 接入 runtime config API，先做轻量可观测性与治理入口，不引入复杂前端状态机。

展示目标：
- 在 agent 卡片中显示：
  - `runtime_version`
  - `runtime_history_count`
  - `runtime_latest_changed_at`
- 增加单独的 Runtime Config 面板，展示：
  - tracked agents
  - total history entries
  - 说明当前 runtime history 是否已启用版本化/裁剪治理
- 增加一个简单 prune 操作入口：
  - 输入 `keep_last_per_agent`
  - 调用 `POST /api/runtime-config/history/prune`
  - 成功后刷新 status / agents / event feed 提示

前端约束：
- 优先复用 `/api/status` 中的 `runtime_config` 摘要，避免无谓逐 agent 请求
- 不在 dashboard 首屏自动拉全量 history/diff，避免 payload 和渲染开销
- prune 失败时写入 event feed，成功时也写入一条本地提示事件
- 保持现有 dashboard 布局风格一致，不引入外部依赖

### 4.13 Dashboard 测试要求
- status 刷新后 runtime config 摘要能正确渲染
- agent 卡片能显示 runtime version / history_count / latest_changed_at
- prune 操作提交成功后能刷新页面状态
- prune 参数非法时能在前端显示失败事件

### 4.14 Dashboard slice 2：History / Diff 钻取
新增一个 Runtime Config Inspector 面板，提供按 agent 的审计钻取能力。

展示与交互：
- 面板展示：
  - 当前选中的 `agent_id`
  - runtime history 列表（version / proposal_id / changed_at）
  - diff 查询控件：`from_version`、`to_version`
  - diff 结果展示区
- 用户可通过以下方式选中 agent：
  - 点击 agent 卡片中的“Inspect Runtime”按钮，或
  - 在 inspector 中切换 agent
- history 按需加载：
  - 仅在用户选中 agent 时请求 `GET /api/runtime-config/history?agent_id=...`
- diff 按需加载：
  - 用户点击 Compare 后请求 `GET /api/runtime-config/diff?...`

前端约束：
- 首屏仍不自动拉全量 history/diff
- diff 结果用可读的 JSON / preformatted block 展示，不引入外部 JSON viewer
- 若 history 为空，明确显示 empty state
- 若 diff 返回 `version_not_found`，明确提示“所需版本已被裁剪或不存在”
- 若请求失败，写入 event feed，同时在 inspector 中显示错误文本

### 4.15 Dashboard slice 2 测试要求
- dashboard HTML 包含 runtime inspector 区域与 compare 控件
- 包含 history/diff 相关标签或按钮文案
- 保持既有 dashboard 功能与测试不回归

### 4.16 Dashboard slice 3：交互打磨
目标：把 runtime inspector 从“能用”提升到“顺手”。

优化项：
- history 条目支持点击：
  - 点击一条时，自动将 `to_version` 设为该条 version
  - 若存在前一个版本，则自动将 `from_version` 设为 `version - 1`
- diff 展示增强：
  - 若 diff 为 object，按 key/value 格式化展示
  - 保留原始 JSON 可读性，但尽量减少“整坨 JSON”的观感负担
- loading / error 体验：
  - history 加载、diff compare 中按钮和提示文案更明确
  - compare 成功后给出简短成功提示
  - compare 失败时保留最近一次结果或明确标记 unavailable
- inspector 状态保持：
  - prune 后若当前选中 agent 仍存在，自动重新加载其 history
  - status/agents refresh 不应无故清空 inspector

约束：
- 仍不引入前端框架和外部依赖
- 保持 `dashboard.hpp` 单文件方案
- 测试仍以静态 HTML 断言为主，不引入复杂浏览器自动化

### 4.17 Dashboard slice 3 测试要求
- dashboard HTML 包含可点击/交互增强相关文案或元素 id
- 包含 history item / compare / diff output 的增强标识
- 全量测试保持通过

### 4.18 Dashboard slice 4：Runtime Diff 结构化高亮
目标：把 runtime diff 从“文本可读”提升为“审计友好”。

展示目标：
- diff 面板按字段分组展示变更：
  - top-level fields
  - `contract.*` 子字段
- 每个字段展示：
  - field name
  - before
  - after
- 仍保留 preformatted / text-first 风格，不引入前端框架
- 若 diff 为空：
  - 显示明确 empty-state：`No runtime config differences between selected versions.`
- 若 diff unavailable / pruned:
  - 显示更明确状态文案，而不是模糊失败提示

交互要求：
- compare 成功后，优先渲染结构化 diff 卡片/块
- 若 payload 不是 object 或结构不规则，则回退到当前文本格式化展示
- `contract.version`、`contract.success_rate_threshold` 等子字段应在 UI 中以完整路径显示，避免歧义

约束：
- 不改 server/facade diff API 协议
- 仍优先修改 `dashboard.hpp`
- 测试继续采用静态 HTML 断言，不引入浏览器自动化

### 4.19 Dashboard slice 4 测试要求
- dashboard HTML 包含结构化 diff 区域或样式标识
- 包含 before/after 展示文案或元素 id/class
- 全量测试保持通过

## 5. P11 Runtime Governance 自动化

### 5.1 目标
在已有 runtime version/history/diff/prune 能力之上，增加**自动治理**，避免长期运行时 history 无限制增长。

### 5.2 最小实现（slice 1）
新增 facade 配置项：
- `runtime_history_keep_last_per_agent`（`std::size_t`，默认 `0`）

语义：
- `0` = 不自动裁剪
- `>0` = 每个 agent 最多保留最近 N 条 runtime history

### 5.3 自动治理触发点
自动治理在以下场景触发：
1. 成功记录新的 runtime version 后
2. `save_state()` 落盘前兜底执行一次
3. `load_runtime_config_history()` / 初始化恢复后，如果配置启用自动治理，可在内存中立即收敛到阈值

约束：
- **不回退 / 不重编号** `runtime_config_versions_`
- 自动治理仅影响 `runtime_config_history_`
- `get_agent_runtime_diff()` 若命中被自动裁剪掉的版本，仍返回 `version_not_found`

### 5.4 Status 暴露
`get_status().runtime_config` 新增 `governance` 摘要，例如：

```json
{
  "runtime_config": {
    "tracked_agents": 2,
    "history_entries": 4,
    "governance": {
      "auto_prune_enabled": true,
      "keep_last_per_agent": 2
    }
  }
}
```

### 5.5 测试要求
- 配置阈值后，连续写入多个 runtime version 会自动裁剪到最近 N 条
- `current_version` 仍保持单调递增，不因自动裁剪而回退
- 自动裁剪后持久化重启，history 仍符合阈值
- `get_status()` 正确反映 governance 配置
- 对已被自动裁剪的旧版本做 diff 查询返回 `version_not_found`

### 5.6 Server/API 暴露（slice 2）
在 server 层暴露 runtime governance 查询与更新接口：

- `GET /api/runtime-config/governance`
  - 返回当前 runtime governance 配置摘要
- `POST /api/runtime-config/governance`
  - body: `{ "keep_last_per_agent": 2 }`
  - 在线更新自动治理阈值并立即生效
  - 如果阈值从 0 改为 >0，应立即对现有 history 做收敛
  - 成功后返回最新 governance + runtime_config 摘要

约束：
- 仍不改变 `runtime_config_versions_` 的单调性
- 更新阈值后需要 `save_state()`，确保当前收敛后的状态落盘
- 参数非法返回 `400`
- 不额外发明复杂配置结构，先只暴露 `keep_last_per_agent`

### 5.7 测试要求（slice 2）
- governance GET 能返回当前 `auto_prune_enabled` 与 `keep_last_per_agent`
- governance POST 能更新阈值并立即反映到 status/runtime summary
- 当 POST 设置较小阈值时，现有 history 会立即被裁剪
- 裁剪后持久化文件内容与返回状态一致
- 非法 body / 非法阈值返回 `400`

### 5.8 Dashboard 接 governance（slice 3）
Dashboard 增加 runtime governance 的可视化与更新入口。

展示目标：
- 在 overview / operations 中展示：
  - `auto_prune_enabled`
  - `keep_last_per_agent`
- 提供一个独立表单更新 runtime governance：
  - 输入 `keep_last_per_agent`
  - 调用 `POST /api/runtime-config/governance`
  - 成功后刷新 `status`、runtime summary、当前 inspector 状态

交互要求：
- 首屏通过 `/api/status` 读取 governance 摘要，不做额外查询风暴
- 提交 governance 更新后：
  - 写一条成功/失败 event feed
  - 若当前选中 agent 仍存在，自动刷新其 history 与 diff 状态
- 当阈值为 `0` 时，UI 明确表示 auto-prune disabled

### 5.9 测试要求（slice 3）
- dashboard HTML 包含 governance 标签、输入框、按钮或相关 id
- 包含 `auto_prune_enabled` / `keep_last_per_agent` 相关文案
- 保持既有 dashboard 与 server 测试不回归

### 5.10 Governance 事件化（slice 4）
为 runtime history 自动治理增加审计事件，进入现有事件流（SSE + /api/events）。

事件目标：
- 当 auto-prune 实际裁剪了 history 时，发出 `runtime_config_history_pruned` 事件
- 事件包含：
  - `trigger`（如 `auto`）
  - `keep_last_per_agent`
  - `history_entries_before` / `history_entries_after`
  - `pruned_entries`
  - `removed_by_agent`（每个 agent 的裁剪条数）
- message 文案可直接在 event feed 中阅读，包含“谁被裁了多少条”的摘要

约束：
- 没有实际裁剪（pruned_entries=0）时不刷无意义事件
- 不改变版本号单调性与现有 diff 语义
- 不新增复杂事件管道，复用现有 `emit_event` / SSE

### 5.11 测试要求（slice 4）
- 启用 auto-prune 并触发裁剪后，能捕获 `runtime_config_history_pruned` 事件
- 事件字段包含裁剪前后条数、阈值、按 agent 的裁剪统计
- 裁剪后版本号保持不回退
- 全量测试保持通过

## 6. P13 Dashboard 国际化（i18n）

### 6.1 目标
为 Dashboard 增加中英文切换能力，让用户可以选择界面语言。

### 6.2 设计要点
- 在 header 增加语言切换按钮（EN / 中文）
- 使用简单字典映射，不引入外部 i18n 库
- 支持的语言：
  - `en` — English（默认）
  - `zh` — 简体中文
- 用户选择后保存到 `localStorage`，下次打开自动恢复
- 所有 UI 文本通过 `t(key)` 函数获取，支持动态插值

### 6.3 翻译范围（第一版）
- 页面标题
- 卡片标题（System Status / Real-time Event Feed / Agents / Operations / Runtime Config Inspector / Evolution History）
- 统计标签（Agents / Tasks / Events / Integrity / Runtime Tracked Agents / Runtime History Entries / Runtime Auto Prune / Runtime Keep Last）
- 按钮文本（Submit Task / Trigger Evolution / Prune Runtime History / Update Runtime Governance / Compare Versions / Inspect Runtime）
- 表单标签
- 状态文本（Status: Running / Starting / SSE reconnecting）
- Event feed 事件类型

### 6.4 测试要求
- Dashboard HTML 包含语言切换按钮
- 切换语言后所有 UI 文本立即更新
- 刷新页面后语言偏好保持
- 默认语言为英文
