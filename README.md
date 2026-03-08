# EvoClaw

EvoClaw — 以组织进化为核心的 C++ AI 管家系统

## 项目概述

EvoClaw 是一个自进化多 Agent 组织框架，实现了从 v1.0 到 v4.0 的完整设计演进，并在 P4 阶段补齐了真实 LLM 与进化门禁能力：

- **v1 核心**：多 Agent 组织 + 通信协议 + 进化闭环
- **v2 评估层**：双评估（Meta Judge）+ 挑战集 + 资源预算
- **v3 工具化**：UMI 通用接口 + SLP 语义原语 + Safety Airbag
- **v4 双区架构**：稳定区/探索区 + Compiler + 反熵机制
- **P4 增强**：Bailian Kimi2.5 实接 + 执行流水线 + EWMA 监控 + LLM 进化提案 + A/B 置信门禁
- **P9**：Runtime Config 版本化 + History + Diff + Prune
- **P11**：Governance 自动化 + 审计事件化 + 配置持久化
- **P12**：Rollback UI 闭环
- **P13**：Dashboard 国际化 (EN/ZH)
- **P14**：Dashboard 性能优化

## 架构（6 层）

```
┌─────────────────────────────────────────────┐
│  Facade（用户接口层）                         │
├─────────────────────────────────────────────┤
│  Governance Kernel（治理内核）                │
│  - 宪法约束、权限裁决、发布/回滚门禁           │
├─────────────────────────────────────────────┤
│  Router（路由层）                             │
│  - UMI 匹配、ε-greedy、能力矩阵              │
├─────────────────────────────────────────────┤
│  Agent Pool（Agent 池）                       │
│  - Planner / Executor / Critic               │
├─────────────────────────────────────────────┤
│  Protocol（通信层）                           │
│  - 结构化消息、消息总线、链路追踪              │
├─────────────────────────────────────────────┤
│  Memory + Event Log（记忆层）                 │
│  - 工作记忆、组织日志、不可变事件账本          │
└─────────────────────────────────────────────┘
```

## 快速开始

### 构建

```bash
mkdir build && cd build
cmake ..
make -j4
```

### 运行测试

```bash
cd build
ctest --output-on-failure
```

### 运行 Live 测试（Bailian）

```bash
cd build
EVOCLAW_LIVE_TEST=1 ctest -R "(EvolverTest.ProposeUsesLlmWhenLiveEnabled|LLMTest.LiveBailianRequestWhenEnabled|IntegrationLiveTest.ExecutePipelineWithBailianWhenEnabled)" --output-on-failure
```

### 运行示例

```bash
./examples/evoclaw_basic_usage
./examples/evoclaw_evolution_demo
```

## 目录结构

```
EvoClaw/
├── CMakeLists.txt
├── README.md
├── DESIGN.md              # 架构设计文档
├── src/
│   ├── core/              # 基础类型
│   ├── protocol/          # 通信协议
│   ├── umi/               # 通用模块接口
│   ├── agent/             # Agent 框架
│   ├── router/            # 路由系统
│   ├── memory/            # 记忆层
│   ├── event_log/         # 事件账本
│   ├── governance/        # 治理内核
│   ├── evolution/         # 进化引擎
│   └── facade/            # 用户接口
├── tests/                 # 测试
│   ├── test_core.cpp
│   ├── test_protocol.cpp
│   ├── test_umi.cpp
│   ├── test_agent.cpp
│   ├── test_router.cpp
│   ├── test_memory.cpp
│   ├── test_event_log.cpp
│   ├── test_governance.cpp
│   ├── test_evolution.cpp
│   └── test_integration.cpp
├── examples/              # 示例
│   ├── basic_usage.cpp
│   └── evolution_demo.cpp
└── third_party/           # 第三方库
    └── nlohmann/json.hpp
```

## 模块说明

### Core（core/）
基础类型定义：MessageId, AgentId, TaskId, Timestamp, generate_uuid(), now()

### Protocol（protocol/）
- `Message`：结构化消息（7 种 performatives）
- `MessageBus`：消息总线（去重、限流、广播）

### UMI（umi/）
- `ModuleContract`：通用模块接口契约
- `CapabilityProfile`：能力声明
- `AirbagSpec`：安全声明

### Agent（agent/）
- `Agent`：Agent 基类（execute/reflect）
- `Planner`：任务规划
- `Executor`：任务执行
- `Critic`：质量评估

### Router（router/）
- `CapabilityMatrix`：能力矩阵（评分、排序）
- `Router`：ε-greedy 路由、UMI 匹配、冷启动配额

### Memory（memory/）
- `WorkingMemory`：工作记忆（key-value）
- `OrgLog`：组织日志（append-only JSONL）

### Event Log（event_log/）
- `EventLog`：不可变事件账本（hash chain 完整性校验）

### Governance（governance/）
- `Constitution`：宪法（mission, values, guardrails）
- `GovernanceKernel`：权限裁决、部署门禁、回滚决策

### Evolution（evolution/）
- `Evolver`：张力监控、提案生成、A/B 测试、进化应用
- `Tension`：张力信号（KPI 下降、重复失败等）
- **EWMA 自适应监控**：输出 `ewma_score` / `ewma_volatility` / `adaptive_threshold`
- **LLM 提案增强**：可由 Bailian `kimi-k2.5` 生成结构化 `new_value` patch
- **门禁增强**：最小样本量、p-value、confidence、rollback signal

### Facade（facade/）
- `EvoClawFacade`：系统门面（初始化、任务提交、触发进化）

## 测试

```bash
./tests/evoclaw_test
```

80+ 测试覆盖：
- Core：UUID、时间戳
- Protocol：序列化/反序列化、消息总线
- UMI：契约验证
- Agent：角色执行/反思
- Router：路由、ε-greedy、能力矩阵
- Memory：工作记忆、组织日志
- Event Log：hash chain 完整性
- Governance：宪法检查、权限裁决
- Evolution：张力检测、A/B 测试
- Integration：完整系统工作流

## 示例

### Basic Usage
```cpp
#include "facade/facade.hpp"

EvoClawFacade::Config config;
EvoClawFacade facade(config);
facade.initialize();

// 注册 Agent
facade.register_agent(agent);

// 提交任务
auto result = facade.submit_task(task);

// 触发进化
facade.trigger_evolution();
```

### Evolution Demo
运行 `./examples/evoclaw_evolution_demo` 查看完整演示：
- 初始化系统
- 注册 Planner/Executor/Critic
- 运行成功与失败混合任务，制造真实张力
- 触发进化循环
- 打印完整链路：`tension -> proposal -> ab_test -> apply/reject`
- 验证事件日志完整性

## 技术栈

- **C++23**：Concepts, coroutines, std::format
- **CMake 3.25+**：跨平台构建
- **nlohmann/json**：JSON 序列化
- **Google Test**：单元测试

## 许可证

MIT License

## LLM 配置

EvoClaw 默认从 `~/.openclaw/openclaw.json` 读取 provider 配置，优先顺序：

1. 环境变量覆盖（`EVOCLAW_API_KEY`, `EVOCLAW_BASE_URL`, `EVOCLAW_MODEL`, `EVOCLAW_PROVIDER`）
2. `bailian` provider
3. `mynewapi` provider
4. providers 中的第一个可用项

推荐使用：

- Provider: `bailian`
- Model: `kimi-k2.5`

若未配置 API key，系统自动回退 mock 模式。
