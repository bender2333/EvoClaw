# EvoClaw Architecture Document

> C++23 自进化多智能体组织框架  
> Version: 1.0.0 | Last Updated: 2026-02-25

## 1. 概述

EvoClaw 是一个以组织进化为核心的 AI 多智能体框架，实现了：

- **UMI (Unified Module Interface)**: 统一模块契约，定义 Agent 能力边界
- **SLP (Semantic Language Primitive)**: 语义原语，多粒度文本压缩
- **双区架构**: 稳定区 + 探索区，平衡可靠性与创新
- **Toolization Loop**: 模式检测 → 编译 → 降维
- **反熵机制**: 防止系统僵化，保持适应性

## 2. 系统架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        Web Dashboard                             │
│                    (REST API + SSE Push)                         │
├─────────────────────────────────────────────────────────────────┤
│                        EvoClawFacade                             │
│              (统一入口，协调所有子系统)                            │
├──────────┬──────────┬──────────┬──────────┬─────────────────────┤
│  Router  │  Evolver │ Compiler │ Entropy  │    ZoneManager      │
│ (ε-贪婪) │ (张力→提案)│(模式编译)│ (反熵)   │   (双区管理)         │
├──────────┴──────────┴──────────┴──────────┴─────────────────────┤
│                         Agent Layer                              │
│            Planner  │  Executor  │  Critic  │  ...              │
├─────────────────────────────────────────────────────────────────┤
│                        Core Services                             │
│   MessageBus │ WorkingMemory │ OrgLog │ EventLog │ Governance   │
├─────────────────────────────────────────────────────────────────┤
│                        Infrastructure                            │
│        LLMClient  │  BudgetTracker  │  SLPCompressor            │
└─────────────────────────────────────────────────────────────────┘
```

## 3. 目录结构

```
src/
├── core/           # 基础类型定义
│   └── types.hpp   # AgentId, TaskId, Timestamp, TaskType
├── umi/            # 统一模块接口
│   └── contract.hpp # ModuleContract, CapabilityProfile, AirbagSpec
├── protocol/       # 通信协议
│   └── bus.hpp/cpp # MessageBus 发布订阅
├── agent/          # Agent 框架
│   ├── agent.hpp/cpp    # Agent 基类
│   ├── planner.cpp      # 规划 Agent
│   ├── executor.cpp     # 执行 Agent
│   └── critic.cpp       # 评审 Agent
├── router/         # 智能路由
│   ├── router.hpp/cpp         # ε-greedy 路由器
│   └── capability_matrix.hpp  # 能力矩阵
├── memory/         # 记忆系统
│   ├── working_memory.hpp/cpp # 工作记忆
│   └── org_log.hpp/cpp        # 组织日志
├── event_log/      # 事件日志
│   └── event_log.hpp/cpp      # 哈希链事件日志
├── governance/     # 治理内核
│   └── governance_kernel.hpp/cpp # 宪法 + 提案审批
├── evolution/      # 进化系统
│   ├── tension.hpp            # 张力检测
│   ├── evolver.hpp/cpp        # 进化器
│   └── evolution_budget.hpp/cpp # 进化预算
├── llm/            # LLM 集成
│   └── llm_client.hpp/cpp     # OpenAI 兼容客户端
├── budget/         # 资源预算
│   └── budget_tracker.hpp/cpp # Token 追踪
├── slp/            # 语义原语
│   └── semantic_primitive.hpp/cpp # S/M/L 压缩
├── zone/           # 双区架构
│   ├── zone.hpp               # Zone 枚举 + Policy
│   └── zone_manager.hpp/cpp   # 区域管理
├── compiler/       # 模式编译
│   ├── pattern.hpp            # Pattern 结构
│   └── compiler.hpp/cpp       # 模式检测 + 编译
├── entropy/        # 反熵机制
│   └── entropy_monitor.hpp/cpp # 熵度量 + 建议
├── facade/         # 统一门面
│   └── facade.hpp/cpp         # EvoClawFacade
└── server/         # Web 服务
    ├── server.hpp/cpp         # HTTP + SSE
    └── dashboard.hpp          # 嵌入式 HTML
```

## 4. 核心组件

### 4.1 UMI 契约 (Unified Module Interface)

每个 Agent 必须声明 UMI 契约：

```cpp
struct ModuleContract {
    std::string module_id;      // 唯一标识
    std::string version;        // 语义版本
    CapabilityProfile capability; // 能力声明
    AirbagSpec airbag;          // 安全气囊
    std::map<std::string, double> cost_model; // 成本模型
};

struct CapabilityProfile {
    std::vector<std::string> intent_tags;     // 意图标签
    std::vector<std::string> required_tools;  // 所需工具
    double success_rate_threshold;            // 成功率阈值
    double estimated_cost_token;              // 预估 token 成本
};

struct AirbagSpec {
    AirbagLevel level;                        // BASIC/STANDARD/STRICT
    std::vector<std::string> permissions;     // 权限白名单
    bool reversible;                          // 是否可逆
    int max_blast_radius;                     // 最大影响范围
};
```

### 4.2 Agent 基类

```cpp
class Agent {
public:
    virtual TaskResult execute(const Task& task) = 0;
    
    // LLM 调用（自动累计 token）
    LLMResponse call_llm(const std::string& system_prompt,
                         const std::string& user_message);
    
    // 预算检查
    bool is_budget_exceeded(const Task& task) const;
    
    // Token 消耗追踪
    int tokens_consumed() const;
    int prompt_tokens_consumed() const;
    int completion_tokens_consumed() const;
    
protected:
    AgentConfig config_;
    std::shared_ptr<MessageBus> bus_;
    std::shared_ptr<LLMClient> llm_client_;
    int tokens_consumed_ = 0;
};
```

### 4.3 Router (ε-greedy 路由)

```cpp
class Router {
public:
    // 注册 Agent
    void register_agent(std::shared_ptr<Agent> agent);
    
    // 智能路由：ε 概率探索，1-ε 概率利用
    std::optional<AgentId> route(const Task& task);
    
    // 能力矩阵持久化
    void save_matrix(const std::filesystem::path& path) const;
    void load_matrix(const std::filesystem::path& path);
    
private:
    RoutingConfig config_;  // epsilon, cold_start_quota, etc.
    CapabilityMatrix matrix_;
    ZoneManager* zone_manager_;
};
```

### 4.4 双区架构 (ZoneManager)

```cpp
enum class Zone { STABLE, EXPLORATION };

struct ZonePolicy {
    Zone zone;
    double token_budget_ratio;  // 探索区 = 0.7
    int min_success_count;      // 晋升门槛
    double min_success_rate;    // 晋升成功率
    bool can_face_user;         // 探索区 = false
};

class ZoneManager {
public:
    void assign_zone(const AgentId& agent, Zone zone);
    bool is_promotion_eligible(const AgentId& agent) const;
    void promote_to_stable(const AgentId& agent);
    void record_result(const AgentId& agent, bool success);
};
```

### 4.5 Compiler (模式编译)

```cpp
struct Pattern {
    std::string id;
    std::vector<std::string> step_sequence;  // Agent 调用序列
    int observation_count;
    double avg_success_rate;
};

class Compiler {
public:
    // 从 OrgLog 检测重复模式
    std::vector<Pattern> detect_patterns(int min_occurrences = 3) const;
    
    // 编译为可路由的快捷方式（生成 UMI 契约草稿）
    Pattern compile(const Pattern& pattern) const;
};
```

### 4.6 反熵机制 (EntropyMonitor)

```cpp
struct EntropyMetrics {
    int active_agent_count;
    double routing_concentration;  // 路由集中度
    double avg_task_rounds;
    double entropy_score;          // 综合熵值 0-1
};

class EntropyMonitor {
public:
    EntropyMetrics measure() const;
    std::vector<AntiEntropyAction> suggest_actions(double threshold) const;
    void apply_cold_start_perturbation(Router& router, double ratio);
};
```

### 4.7 SLP 语义原语

```cpp
enum class Granularity { SMALL, MEDIUM, LARGE };

struct SemanticPrimitive {
    std::string id;
    Granularity granularity;
    std::string content;
    int token_count;
};

class SLPCompressor {
public:
    // LLM 优先，失败时截断降级
    SemanticPrimitive compress(const std::string& text, Granularity g) const;
    
    // 三粒度同时压缩
    MultiGranularitySummary compress_all(const std::string& text) const;
};
```

## 5. REST API

| Endpoint | Method | 描述 |
|----------|--------|------|
| `/api/status` | GET | 系统状态 |
| `/api/agents` | GET | Agent 列表 |
| `/api/task` | POST | 提交任务 |
| `/api/evolve` | POST | 触发进化 |
| `/api/budget` | GET | Token 预算报告 |
| `/api/evolution/budget` | GET | 进化预算状态 |
| `/api/zones` | GET | 双区状态 |
| `/api/patterns` | GET | 已检测/编译的 Pattern |
| `/api/entropy` | GET | 熵度量 + 反熵建议 |
| `/api/logs` | GET | OrgLog 查询 (支持时间范围) |
| `/api/logs/stats` | GET | 时间范围统计 |
| `/api/matrix` | GET | 能力矩阵 |
| `/api/matrix/save` | POST | 保存矩阵 |
| `/api/events` | GET | SSE 实时事件流 |

## 6. 配置

### 6.1 LLM 配置

API Key 解析顺序：
1. 环境变量 `EVOCLAW_API_KEY`
2. `~/.openclaw/openclaw.json` → `models.providers.mynewapi.apiKey`
3. Mock 模式降级

```cpp
LLMClient client = llm::create_from_env();
// base_url: http://localhost:3000/v1
// model: claude-opus-4-6
```

### 6.2 Router 配置

```cpp
struct RoutingConfig {
    double epsilon = 0.1;        // 探索概率
    double epsilon_boost = 0.2;  // 新模块晋升时临时提升
    double epsilon_min = 0.05;   // 最低探索率
    double cold_start_quota = 0.15; // 冷启动流量配额
    int cold_start_days = 14;    // 冷启动期
};
```

### 6.3 Evolution 配置

```cpp
struct Evolver::Config {
    int consecutive_failures = 10;
    double min_improvement = 0.05;
    int max_evolution_cycles_per_hour = 5;
    int max_proposals_per_cycle = 3;
    int evolution_token_budget = 10000;
};
```

## 7. 构建与测试

```bash
# 构建
mkdir -p build && cd build
cmake ..
make -j4

# 运行测试
./tests/evoclaw_test

# 启动 Demo Server
export EVOCLAW_API_KEY="your-api-key"
./examples/evoclaw_server_demo
# Dashboard: http://localhost:8080
```

## 8. 依赖

- C++23 编译器 (GCC 13+ / Clang 16+)
- CMake 3.25+
- nlohmann/json (header-only)
- cpp-httplib (header-only, `third_party/httplib.h`)
- spdlog (可选，日志)
- Google Test (测试，自动下载)

## 9. 版本历史

| 版本 | 日期 | 内容 |
|------|------|------|
| Phase 1-4 | 2026-02-25 | 核心框架：UMI, Agent, Router, Memory, Governance, Evolution |
| P0 | 2026-02-25 | 真实 LLM 集成 + Safety Airbag |
| P1 | 2026-02-25 | BudgetTracker + EvolutionBudget + SLP |
| P2 | 2026-02-25 | 双区架构 + Compiler + 反熵机制 |
| P3 | 2026-02-25 | OrgLog 时间查询 + 矩阵持久化 |

## 10. 测试覆盖

当前：71 tests, 21 suites

- `test_core.cpp` — 基础类型
- `test_protocol.cpp` — MessageBus
- `test_umi.cpp` — UMI 契约
- `test_agent.cpp` — Agent 执行
- `test_router.cpp` — 路由逻辑
- `test_memory.cpp` — WorkingMemory
- `test_event_log.cpp` — 事件日志完整性
- `test_governance.cpp` — 治理内核
- `test_evolution.cpp` — 进化器
- `test_llm.cpp` — LLM 客户端
- `test_budget.cpp` — 预算追踪
- `test_slp.cpp` — 语义原语
- `test_zone.cpp` — 双区管理
- `test_compiler.cpp` — 模式编译
- `test_entropy.cpp` — 反熵机制
- `test_org_log_query.cpp` — OrgLog 查询
- `test_matrix_persistence.cpp` — 矩阵持久化
- `test_integration.cpp` — 端到端集成

---

*EvoClaw — 让 AI 组织自我进化*
