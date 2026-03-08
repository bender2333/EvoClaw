# EvoClaw 技术架构全图
**版本：整合版（v1.0-v5.0 + Architecture Decision Document）**
**状态：设计阶段，面向实现**

---

## 目录

1. [系统总体架构图](#1-系统总体架构图)
2. [核心组件详解](#2-核心组件详解)
3. [技术模块分解](#3-技术模块分解)
4. [数据流与通信架构](#4-数据流与通信架构)
5. [存储层架构](#5-存储层架构)
6. [进化引擎架构](#6-进化引擎架构)
7. [世界模型架构（v5.0）](#7-世界模型架构v50)
8. [安全与治理架构](#8-安全与治理架构)
9. [项目目录结构](#9-项目目录结构)
10. [阶段实施分解](#10-阶段实施分解)
11. [关键技术决策摘要](#11-关键技术决策摘要)
12. [依赖与基础设施](#12-依赖与基础设施)

---

## 1. 系统总体架构图

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              EvoClaw 系统边界                                    │
│                                                                                 │
│  ┌──────────────────────────────────────────────────────────────────────────┐  │
│  │                        用户交互层（Facade）                               │  │
│  │                                                                          │  │
│  │   Discord Bot ◄──►  Facade Agent  ◄──►  Web Dashboard                   │  │
│  │      (D++ DPP)         (统一人格)         (REST + SSE)                   │  │
│  └───────────────────────────────┬──────────────────────────────────────────┘  │
│                                  │ ZeroMQ DEALER-ROUTER                        │
│  ┌───────────────────────────────▼──────────────────────────────────────────┐  │
│  │                      执行层（evoclaw-engine）                             │  │
│  │                                                                          │  │
│  │  ┌─────────────┐    ┌──────────────────────────────────────────────────┐ │  │
│  │  │ Fast Router │    │           Dynamic Holons Pool                    │ │  │
│  │  │  (System 1) │    │                                                  │ │  │
│  │  │ ε-greedy    │    │  ┌──────────┐ ┌──────────┐ ┌──────────────────┐ │ │  │
│  │  │ 路由决策    │───►│  │ Planner  │ │Executor  │ │ Critic           │ │ │  │
│  │  │             │    │  │ (规划者) │ │(执行者)  │ │ (审查员)         │ │ │  │
│  │  │ 能力矩阵    │    │  └──────────┘ └──────────┘ └──────────────────┘ │ │  │
│  │  │ 插件注册表  │    │  System 2: 多步推理 + 动态组队（Holon）           │ │  │
│  │  └─────────────┘    └──────────────────────────────────────────────────┘ │  │
│  │                                                                          │  │
│  │  ┌─────────────┐    ┌─────────────┐    ┌──────────────────────────────┐ │  │
│  │  │ Governance  │    │   Safety    │    │  Capabilities                │ │  │
│  │  │   Gate      │    │   Airbag    │    │  (file/shell/process/net)     │ │  │
│  │  │ 宪法约束    │    │ Blast Radius│    │  沙盒隔离执行                │ │  │
│  │  │ 冷静期      │    │ Circuit     │    │                              │ │  │
│  │  │             │    │ Breaker     │    │                              │ │  │
│  │  └─────────────┘    └─────────────┘    └──────────────────────────────┘ │  │
│  └───────────────────────────────┬──────────────────────────────────────────┘  │
│                                  │                                             │
│  ┌───────────────────────────────┼──────────────────────────────────────────┐  │
│  │                     组织通信总线（lib/bus + lib/protocol）                │  │
│  │                                                                          │  │
│  │  ZeroMQ Pub-Sub（事件广播）+ DEALER-ROUTER（请求响应）                   │  │
│  │  Cerebellum（小脑）：打包 → 验证 → 冲突检测 → 分发                       │  │
│  │  SLP 原语标准化通信 ｜ 渐进式形式化（FL-0 → FL-4）                        │  │
│  └───────────┬──────────────────────────────────────────┬───────────────────┘  │
│              │                                          │                      │
│  ┌───────────▼──────────────────┐    ┌─────────────────▼──────────────────┐   │
│  │   进化健康层                  │    │         世界模型层（v5.0）          │   │
│  │  （evoclaw-judge, Phase 2）  │    │                                    │   │
│  │                              │    │  Task Space | Causal Structure     │   │
│  │  Observer  ←── Surprise ─── │    │  Organization State                │   │
│  │  Meta Judge（盲测裁决）      │    │  持久化认知结构（程序骨架）          │   │
│  │  Challenge Curator           │    │  Modeler（假设修订）               │   │
│  │  Dormancy Controller         │    │                                    │   │
│  └───────────┬──────────────────┘    └────────────────────────────────────┘   │
│              │                                                                 │
│  ┌───────────▼──────────────────────────────────────────────────────────────┐  │
│  │                    进化层（evoclaw-compiler）                             │  │
│  │                                                                          │  │
│  │  ┌───────────┐  ┌───────────┐  ┌───────────────┐  ┌──────────────────┐  │  │
│  │  │ Observer  │  │  Evolver  │  │   Compiler    │  │ Pattern Registry │  │  │
│  │  │ 健康监控  │  │ 变更设计  │  │ 模式识别/编译 │  │ 协作模式库       │  │  │
│  │  │ 熵度量    │  │ A/B 提案  │  │ Toolization   │  │ Draft→Active     │  │  │
│  │  │           │  │           │  │ Loop 核心     │  │                  │  │  │
│  │  └───────────┘  └───────────┘  └───────────────┘  └──────────────────┘  │  │
│  │                                                                          │  │
│  │  ┌─────────────────────────────────────────────────────────────────┐    │  │
│  │  │                   双区架构                                       │    │  │
│  │  │  稳定区（Stable Zone）        ║  探索区（Exploration Zone）       │    │  │
│  │  │  UMI 标准化模块              ║  实验性模块（预算×0.7）            │    │  │
│  │  │  SLP 原语 Active 集          ║  候选 Pattern                    │    │  │
│  │  │  能力矩阵                    ║  晋升流程（≥20次验证）             │    │  │
│  │  └─────────────────────────────────────────────────────────────────┘    │  │
│  └───────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌──────────────────────────────────────────────────────────────────────────┐  │
│  │                    基础设施层（lib/）                                      │  │
│  │                                                                          │  │
│  │  event_log  │  memory  │  llm client  │  bus  │  protocol  │  config    │  │
│  │  JSON Lines  │  FS存储  │  OpenAI API  │  ZMQ  │  消息类型  │  YAML      │  │
│  └──────────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 核心组件详解

### 2.1 四大核心组件

| 组件 | 别名 | 职责 | 进程归属 |
|------|------|------|----------|
| Fast Router | System 1 | 意图识别、插件匹配、ε-greedy 路由、能力矩阵查询 | engine |
| Dynamic Holons | System 2 | 临时组队（Planner+Executor+Critic）、复杂任务多步推理 | engine |
| Toolization Compiler | 进化编译引擎 | 协作模式识别（≥3次）→编译→注册到 Fast Router | compiler |
| Organization Bus | 通信总线 | ZeroMQ Pub-Sub + DEALER-ROUTER，小脑预处理 | lib/bus |

### 2.2 进化健康层组件

| 组件 | 触发条件 | 输出 |
|------|----------|------|
| Observer | 每次任务完成 | Surprise Score、健康报告、熵度量 |
| Meta Judge | A/B 测试请求（Phase 2） | 盲测裁决结果 |
| Challenge Curator | 失败事件 / 手动注入 | 挑战集维护、校准数据集 |
| Dormancy Controller | 14天无触发条件 | 进化引擎休眠/唤醒控制 |
| **Modeler（v5.0新增）** | 30天惊讶持续偏差 | 世界模型假设修订提案 |

### 2.3 Toolization Loop 的编译产物层级

```
L0：路径缓存（Path Cache）
    ├── 最轻量：保留协作路径，跳过规划，仍需 LLM 调用
    ├── 触发：同类路径 ≥2 次成功执行
    └── 成本：相比首次执行降低 ~40%

L1：半自动模板（Semi-Auto Template）
    ├── 固定骨架，LLM 只填参数
    ├── 触发：L0 路径缓存 ≥3 次语义相似成功
    └── 成本：相比首次执行降低 ~70%

L2：完整插件（Full Plugin / UMI Module）
    ├── 无 LLM 调用，确定性执行
    ├── 触发：L1 模板被验证 ≥20 次 + Meta Judge 通过
    └── 成本：相比首次执行降低 ~95%（System 1 秒回）
```

### 2.4 Holon 组队机制

```
任务到达（Fast Router 无法匹配）
        │
        ▼
Planner（规划者）
  - 将任务拆解为子任务 DAG
  - 声明所需 capability 类型
  - 写入预期轮次到世界模型
        │
        ▼ 并行
┌───────────────────┐
│ Executor × N      │
│ - 执行子任务      │
│ - 调用外部工具    │
│ - 写入中间结果    │
└───────────────────┘
        │
        ▼
Critic（审查员）
  - 评估输出质量
  - 计算实际 vs 预期差距（Surprise Score 输入）
  - confidence < 0.6 → 触发重试
        │
        ▼
Synthesizer（整合者）
  - 整合多 Executor 结果
  - 生成面向用户的最终输出
  - 写入 Event Log
```

---

## 3. 技术模块分解

### 3.1 lib/ 共享库模块

#### `lib/bus/` — 消息总线

| 接口/类 | 功能 | 关键方法 |
|---------|------|----------|
| `MessageBus`（纯虚基类） | 消息收发抽象 | `Publish()`, `Subscribe()`, `Send()`, `Receive()` |
| `ZmqMessageBus` | ZeroMQ 实现 | DEALER-ROUTER 请求响应 + Pub-Sub 广播 |
| 消息 QoS | L0: fire-and-forget, L1: at-least-once | msg_id 幂等去重 |

#### `lib/protocol/` — 通信协议

| 组件 | 功能 |
|------|------|
| `Message` 结构 | msg_id, msg_type, source, target, timestamp, payload |
| `MessageTypes` | 枚举所有消息类型（task.request, evolution.compile.started 等） |
| `SLPrimitive` | SLP 原语枚举（DELEGATE, REQUEST_INFO, PROPOSE_CHANGE 等 12 个） |
| `Serializer` | nlohmann/json 序列化/反序列化 |
| `RuleEngine`（接口） | FL-0~FL-2 规则验证（数值比较/布尔逻辑/范围检查） |
| `BasicRuleEngine` | MVP 规则引擎实现，Phase 2 可替换为 Z3 |
| **`Cerebellum`** | 小脑：`Pack()` → `Validate()` → `DetectConflict()` → 分发 |

**Cerebellum 处理管线：**
```
原始消息
  │
  ▼ Pack()
  按 L0/L1/L2 密度压缩 context（S/M/L 三档）
  │
  ▼ Validate()
  L2 消息调用 RuleEngine；L0/L1 跳过（异步追溯）
  │
  ▼ DetectConflict()
  检查消息与当前组织状态的冲突
  │
  ▼
分发给目标 Agent
```

#### `lib/event_log/` — 事件日志（系统基石）

| 组件 | 功能 |
|------|------|
| `EventStore`（纯虚基类） | `Append()`, `Query()`, `VerifyIntegrity()` |
| `JsonlEventStore` | 每进程独立 JSON Lines 文件，每条事件附 SHA-256 校验 |
| `EventMerger` | 定期合并各进程日志到 canonical log |

**事件格式：**
```json
{
  "event_id": "uuid-v4",
  "event_type": "agent.task.completed",
  "source_process": "engine",
  "timestamp": "ISO-8601",
  "data": { "task_id": "...", "surprise_score": 0.23, ... },
  "checksum": "sha256:..."
}
```

**事件类型命名规范（点分层级）：**
```
agent.task.{started|completed|failed|retried}
evolution.pattern.{detected|compiled|promoted|retired}
evolution.plugin.{registered|promoted|rollback}
governance.constitution.{triggered|modified}
system.holon.{spawned|dissolved|crashed}
world_model.assumption.{revised|validated|rejected}
entropy.{measured|threshold_exceeded|compression_triggered}
```

#### `lib/memory/` — 分层记忆

| 记忆层 | 存储位置 | 生命周期 | 用途 |
|--------|----------|----------|------|
| 工作记忆 | `working_memory/` | 会话期间，结束后压缩 | 当前任务上下文 |
| 情景记忆 | `episodic/YYYY-MM/` | 90天→归档 | 历史任务叙事 |
| 语义记忆 | `semantic/` | 永久 | Agent 画像、协作模式 |
| 用户模型 | `user_model/` | 永久（只增不改） | 结构化偏好画像 |
| 挑战集 | `challenge_set/` | 永久 | 失败/边界案例，进化数据源 |

**记忆从 Event Log 的派生关系：**
```
Event Log（唯一真实来源）
  ├── 工作记忆：从 task.started/completed 事件提取活跃上下文
  ├── 情景记忆：从 task.completed 事件构建任务叙事
  ├── 语义记忆：从情景记忆高频模式提炼（周期性）
  └── 挑战集：从 task.failed + risk_event 事件收录
```

#### `lib/llm/` — LLM 客户端

| 组件 | 功能 |
|------|------|
| `LlmClient`（纯虚基类） | `Complete()`, `StreamComplete()` |
| `OpenAiClient` | HTTP 调用 OpenAI 兼容 API（cpp-httplib） |
| `PromptRegistry` | YAML 元数据（id/version/depends_on/output_schema）+ 纯文本模板 |
| Prompt 模板占位符 | `{{variable}}` 替换，运行时热加载 |

**PromptRegistry YAML 格式：**
```yaml
id: compiler.pattern_recognition
version: 2
depends_on: [holon.executor]
output_schema: json
template_file: pattern_recognition.txt
```

#### `lib/config/` + `lib/common/`

| 组件 | 技术 | 用途 |
|------|------|------|
| `YamlConfig` | yaml-cpp | 加载 `evoclaw.yaml`，运行时热更新 |
| `ThreadPool` | std::jthread + 队列 | 异步 LLM 调用，替代 std::async |

### 3.2 src/engine/ 执行层模块

#### Facade（用户交互层）

```
用户消息 → Facade Agent
  ├── 统一人格维护（系统提示词注入）
  ├── 意图解析（调用 Router）
  ├── 进度反馈（SSE 流式推送）
  ├── 主动服务触发（扫描用户模型，30min 周期）
  └── 多平台通道
       ├── Discord Bot（D++ DPP）：Slash Commands + 自然语言
       └── Web API（cpp-httplib server）：REST + SSE
```

#### Router（路由决策核心）

```
接收意图
  │
  ▼
查询插件注册表（确定性匹配）
  ├── 命中 L2 插件 → System 1 秒回（无 LLM）
  └── 未命中 → 查询能力矩阵
          │
          ▼
       ε-greedy 路由
       ├── 概率 (1-ε): 选能力矩阵评分最高模块
       └── 概率 ε:    从所有注册模块随机选（ε=0.1，新模块晋升期 ε=0.2）
          │
          ▼
       分发给 Holon Pool（System 2）
```

#### Holon Pool（动态执行池）

```
HolonPool
  ├── max_concurrent_holons: 配置
  ├── stable_budget_factor: 1.0（稳定区）
  ├── explore_budget_factor: 0.7（探索区，预算×0.7）
  │
  ├── spawn_holon(task) → Holon(std::jthread + std::stop_token)
  │     ├── 系统操作必须在 sandbox 子进程中执行
  │     └── 崩溃不影响 engine 主线程
  │
  └── 插件热替换协议（FR14）
        ├── 新版本标记 current，旧版本标记 old
        ├── 新请求 → current，进行中请求 → old
        ├── 旧版本所有请求完成后（或超时 30s）卸载
        └── 观察期：≥10次调用内失败率 >20% → 自动回滚
```

#### Governance Gate（治理门控）

```
宪法约束（constitution.yaml）
  ├── 财务操作：必须用户确认（冷静期 ≥24h）
  ├── 原始对话：永不外传
  ├── 进化步骤：必须完整可审计
  └── 宪法修改：用户显式确认 + 24h 冷静期

风险分级（risk_levels.yaml）
  ├── L0：只读操作，无需确认
  ├── L1：写操作，系统日志记录
  ├── L2：系统配置/外部服务，需用户实时确认
  └── L3：财务/不可逆操作，需用户 + 冷静期确认
```

#### Safety Airbag（安全工程层）

| 机制 | 实现 | 触发条件 |
|------|------|----------|
| Blast Radius Limit | permissions 白名单，探索区不超稳定区范围 | 所有执行 |
| Reversibility Gate | staging 环境预执行，L3 操作强制 | L3 操作 |
| Circuit Breaker | 连续失败 N 次后断路，防止雪崩 | 失败率 > 阈值 |
| Sandbox | 系统操作在子进程 + ulimit/cgroup 中执行 | FR51-58 能力调用 |

### 3.3 src/compiler/ 进化层模块

```
evoclaw-compiler 进程

Observer（惊讶引擎，v5.0 重构）
  ├── 输入：Event Log 的 task.completed 事件流
  ├── 计算 Surprise Score（幅度 × 方向 × 置信度加权）
  ├── 惊讶分类路由：
  │    ├── 高负惊讶（>0.4） → P0 触发模块修复
  │    ├── 高正惊讶（>0.3） → P1 进入 Compiler 注意队列
  │    ├── 持续负惊讶（连续5次）→ P1 组织级审查
  │    └── 系统性偏差 → P2 触发 Modeler
  └── 输出：健康报告 + 熵度量（每30任务）

Evolver（变更设计）
  ├── 读取 Observer 健康报告
  ├── 设计变更包（调整/替换/重构三级）
  ├── 执行 Modeler 假设验证方案
  └── 触发 A/B 测试流程

Compiler（Toolization Loop 核心）
  ├── 监听 Event Log：task.completed 协作轨迹
  ├── 正惊讶队列：监听异常成功事件
  ├── 模式识别算法：
  │    ├── 精确匹配：相同路径 ≥2 次
  │    └── 语义聚类：相似路径 ≥3 次（向量相似度 >0.85）
  ├── 编译流程：
  │    ├── 生成 L0 路径缓存（立即）
  │    ├── 生成 L1 半自动模板（≥3次确认）
  │    └── 生成 L2 完整插件（≥20次 + 沙盒测试）
  └── 假设守卫（Guard）：插件嵌入运行时假设检查，违反时降级回 Holons

PatternRegistry（模式库）
  └── 状态机：Draft → Pending → Active → Deprecated
```

**反熵机制（Anti-Entropy）：**

```
熵度量体系（Observer 每次健康报告更新）：
  1. 模块总数（稳定区）             目标：8-30，超过触发合并审查
  2. 原语总数（Active SLP）         目标：8-20，超过触发合并审查
  3. 任务平均轮次（30天滑动窗口）    上升趋势 > 20% 触发告警
  4. 依赖深度（模块依赖链最大深度）  > 5 触发整合审查

反熵触发操作（超阈值自动执行，无需人工）：
  1. 原语合并审查 → 重叠率最高的3对 → A/B 测试
  2. 模块整合审查 → 评分最相近的模块对 → 合并可行性分析
  3. Pattern 加速编译 → 缩短探索区积压
  4. 冷启动扰动 → 30天未被路由的稳定区模块强制5%流量
```

### 3.4 src/dashboard/ 可观测性模块

```
evoclaw-dashboard 进程

REST API（cpp-httplib server）
  GET  /api/v1/status          系统整体状态
  GET  /api/v1/entropy         熵指标当前值
  GET  /api/v1/modules         模块注册表 + 能力矩阵
  GET  /api/v1/evolution       进化时间线
  GET  /api/v1/world-model     世界模型快照（v5.0）
  POST /api/v1/confirm/{id}    用户确认操作

SSE 实时推送（/api/v1/events）
  ├── 订阅 ZeroMQ Pub-Sub 实时事件
  └── 推送给前端：任务进度、进化事件、熵告警
```

### 3.5 世界模型模块（v5.0）

```
世界模型（程序骨架，持久化存储，非 LLM 状态）

TaskSpace（任务空间）
  ├── task_type_id
  ├── description（供 Router 语义匹配）
  ├── expected_rounds（历史均值，滑动平均 window=30）
  ├── expected_success_rate（初始0.7，随交互更新）
  ├── confidence（样本越多置信度越高）
  └── last_surprise（最近惊讶幅度+方向）

CausalStructure（因果结构）
  ├── 冷启动：20个手工编码核心因果关系
  ├── 自动归纳：(condition, action) 对出现≥5次且一致 → 写入
  ├── strength 置信度（观察次数 + 一致性）
  └── 退化检测：结果开始不一致 → strength 下降

OrganizationState（组织状态）
  ├── 已注册模块 + UMI 契约摘要
  ├── 能力矩阵快照
  ├── 活跃 SLP 原语 + 使用频率
  ├── 探索区状态（活跃实验数/积压 Pattern）
  └── 系统熵值当前读数

Modeler（假设修订者）
  ├── 触发：模型级惊讶（30天发散）/ 强制检查（90天）
  ├── 输入：Observer 惊讶聚合报告 + 世界模型快照
  ├── 输出：假设修订提案 + 验证方案
  └── 验证流程：探索区实验≥30次 → 惊讶幅度降低≥20% → 写入
```

---

## 4. 数据流与通信架构

### 4.1 任务执行完整数据流

```
用户输入
  │
  ▼
[Facade] 意图解析 + 用户模型查询
  │
  ▼
[Router] 意图分类 → 查询插件注册表
  │
  ├── L2 插件命中 ────────────────────────► System 1 秒回（<500ms）
  │                                               │
  └── 未命中 ─► Holon 组队                        │
                   │                              │
                   ▼                              │
              [Planner] 任务拆解                   │
                   │                              │
                   ▼                              │
              [Executor×N] 并行执行                │
                   │                              │
                   ▼                              │
              [Critic] 质量评估 → Surprise Score  │
                   │                              │
                   ▼                              │
              [Synthesizer] 结果整合               │
                   │                              │
                   ▼                              ▼
              [Event Log] 写入 task.completed ◄──────
                   │
                   ▼
              [Observer] 计算 Surprise Score
                   │
         ┌─────────┼──────────┐
         ▼         ▼          ▼
    高负惊讶   高正惊讶    系统性偏差
    模块修复  Compiler   Modeler 审查
    流程触发  注意队列
```

### 4.2 进程间通信拓扑

```
ZeroMQ 端点（配置在 evoclaw.yaml）:
  engine:   ipc:///tmp/evoclaw-engine.sock
  compiler: ipc:///tmp/evoclaw-compiler.sock
  dashboard:ipc:///tmp/evoclaw-dashboard.sock
  judge:    ipc:///tmp/evoclaw-judge.sock（Phase 2）

通信模式：
  engine  ──DEALER-ROUTER──►  compiler   (编译请求/结果)
  engine  ──Pub-Sub────────►  dashboard  (事件广播，实时)
  engine  ──DEALER-ROUTER──►  judge      (盲测请求, Phase 2)
  compiler──Pub-Sub────────►  dashboard  (编译进度事件)

消息信封：
  {
    "msg_id": "uuid",           // 幂等去重
    "msg_type": "task.request",
    "source": "engine",
    "target": "compiler",
    "timestamp": "ISO-8601",
    "payload": { ... }
  }

QoS 等级：
  L0: fire-and-forget  → 心跳、日志、通知（无验证）
  L1: at-least-once    → 任务请求、编译结果（msg_id 幂等）
  L2: exactly-once     → Phase 2（需引入 WAL 机制）
```

### 4.3 SLP 原语通信标准

```
核心原语集（12个，v3.0 定义，v5.0 继承）：
  DELEGATE        - 委托执行子任务
  REQUEST_INFO    - 请求信息/上下文
  PROPOSE_CHANGE  - 提议变更
  REPORT_STATUS   - 状态汇报
  SIGNAL_COMPLETE - 任务完成信号
  ESCALATE        - 上报人工干预
  ACKNOWLEDGE     - 确认接收
  REJECT          - 拒绝执行
  REVISE          - 请求修订
  MERGE_CONTEXT   - 合并上下文
  VALIDATE        - 请求验证
  COMPILE_HINT    - 向 Compiler 暗示编译机会

消息密度（梯度压缩）：
  S（简约）: ≤20字 摘要，快速路由
  M（标准）: ≤150字 段落，背景说明
  L（完整）: 原始上下文，按需加载
```

---

## 5. 存储层架构

### 5.1 运行时数据目录

```
~/.evoclaw/
├── data/
│   ├── events/
│   │   ├── engine/         # engine 进程写（按日期分文件）
│   │   │   └── 2026-03-08.jsonl
│   │   ├── compiler/       # compiler 进程写
│   │   └── canonical/      # 每日合并 + SHA-256 校验
│   │       └── 2026-03-08.canonical.jsonl
│   │
│   └── memory/
│       ├── working_memory/    # 当前会话，会话结束压缩
│       │   └── session_{id}.json
│       ├── episodic/
│       │   └── 2026-03/       # 按月归档
│       │       └── task_{id}.json
│       ├── semantic/          # 永久，提炼的知识
│       │   ├── agent_profiles/
│       │   └── collaboration_patterns/
│       ├── user_model/
│       │   └── user.json      # 结构化偏好（不存原始对话）
│       └── challenge_set/     # 失败案例，进化数据源
│           └── case_{id}.json
│
├── world_model/               # v5.0 世界模型持久化
│   ├── task_space.json
│   ├── causal_structure.json
│   └── organization_state.json
│
├── prompts/                   # Prompt Registry（热更新）
│   ├── facade/
│   ├── router/
│   ├── compiler/
│   └── holon/
│
├── plugins/                   # 编译产物注册
│   ├── stable/                # L2 完整插件
│   │   └── {plugin_id}/
│   │       ├── manifest.yaml  # UMI 契约
│   │       └── executable
│   └── patterns/              # L0/L1 产物
│       └── {pattern_id}.json
│
├── config/
│   └── evoclaw.yaml           # 运行时配置
│
└── logs/                      # spdlog 日志（按天轮转）
    └── 2026-03-08.log
```

### 5.2 存储接口抽象层

```cpp
// MVP: 文件系统实现
// Phase 2: 向量库（语义记忆）+ ElasticSearch（事件查询）
// 通过纯虚基类保证可替换性

class EventStore {
  virtual Expected<void, Error> Append(Event e) = 0;
  virtual Expected<Events, Error> Query(EventFilter f) = 0;
};

class MemoryStore {
  virtual Expected<Memory, Error> Load(string_view id) = 0;
  virtual Expected<void, Error> Save(const Memory& m) = 0;
};

class WorldModelStore {  // v5.0 新增
  virtual Expected<TaskSpace, Error> LoadTaskSpace() = 0;
  virtual Expected<void, Error> UpdateTaskType(TaskType t) = 0;
  virtual Expected<CausalFact, Error> AppendCausalFact(CausalFact f) = 0;
};
```

### 5.3 记忆 TTL 与生命周期管理

| 记忆类型 | 默认 TTL | 压缩策略 | 升级条件 |
|----------|----------|----------|----------|
| 工作记忆 | 会话结束 | 压缩为情景记忆摘要 | - |
| 情景记忆 | 90 天 | 归档到冷存储 | 高频模式 → 语义记忆 |
| 语义记忆 | 永久 | 低访问 → 降级标记 | - |
| 用户模型 | 永久 | 只增不改（有版本历史） | - |
| 挑战集 | 永久 | 被覆盖的案例 → 归档 | - |

---

## 6. 进化引擎架构

### 6.1 进化触发条件

```
数据驱动触发（非定时器）：

任务级触发：
  ├── 模块挑战集通过率连续5次 < 基线80%
  ├── Operational Critic 与用户反馈背离率 > 25%
  ├── 某类意图 48h 内 ≥3 次 Failed 终态
  └── Router 对某类新意图置信度持续 < 0.5 超过20次

进化级触发（v5.0 惊讶驱动）：
  ├── 高负惊讶（Magnitude > 0.4）→ P0 立即
  ├── 持续负惊讶（同类型连续5次）→ P1 组织级
  └── 系统性偏差（多模块持续惊讶）→ P2 Modeler

宪法级触发（任意次数立即）：
  └── 任何宪法约束被触碰

强制健康检查：
  └── 休眠超过90天（防止漂移积累）
```

### 6.2 进化动作三级分类

| 级别 | 操作 | 执行方式 | 验收门槛 |
|------|------|----------|----------|
| L1 调整 | 修改模块 Prompt / 更新 RAG 知识库 | 自动执行，用户不感知 | Critic 评分恢复 |
| L2 替换 | 新版本模块替换旧版本 | A/B 测试通过后自动执行 | p<0.05 + 幅度>5% |
| L3 重构 | 新增/拆分/合并模块，调整路由 | 用户确认后执行 | A/B 测试 + 用户确认 |

### 6.3 A/B 测试协议

```
创建 Candidate 版本（与 Control 并行）
  │
  ▼
历史任务集 50/50 随机分配（基础验证）
  │
  ▼
挑战集并行评估（v2.0 引入，核心验证）
  │
  ▼
Meta Judge 盲测裁决（Phase 2，MVP 由用户确认替代）
  │
  ├── 通过：历史集不下降 + 挑战集提升 p<0.05 幅度>5%
  │     │
  │     ▼
  │   进入监控期（L1:7天 / L2:14天 / L3:30天）
  │     │
  │     ├── 稳定 → 全量推广，写入进化日志
  │     └── 下滑 → 一键回滚
  │
  └── 未通过：归档（不丢弃），写入挑战集，等待下次触发
```

### 6.4 双区架构

```
稳定区（Stable Zone）
  ├── 所有通过 A/B 测试的 UMI 模块
  ├── Active 状态的 SLP 原语（目标 8-20 个）
  ├── 注册到能力矩阵
  └── 资源预算系数：1.0

═══════════════════════════════════
探索区（Exploration Zone）
  ├── 新晋升候选模块
  ├── Draft/Pending 状态的 Pattern
  ├── Modeler 假设验证实验
  ├── 不可直接面向用户
  └── 资源预算系数：0.7（预算×0.7）
═══════════════════════════════════

晋升路径（探索区 → 稳定区）：
  ≥20 次任务成功率高于挑战集基线
  → Compiler 生成 UMI 契约草稿
  → Contract Validator 校验
  → A/B 测试
  → 通过 → 注册到能力矩阵
  → 原探索区实例保留7天后退役
```

---

## 7. 世界模型架构（v5.0）

### 7.1 惊讶信号三层传导

```
单次任务完成
  │
  ▼
Surprise Score 计算：
  幅度（Magnitude）= |实际 - 期望| / 期望
  方向（Direction）= 正惊讶 / 负惊讶
  权重（Weight）   = 期望置信度加权
  │
  ▼ 按幅度和模式分类
  │
  ├── 任务级（单次高惊讶）
  │     ├── 负惊讶 > 0.4 → 模块修复流程（P0）
  │     ├── 正惊讶 > 0.3 → Compiler 注意队列（P1）
  │     └── 正常 < 0.1 → 静默更新 TaskSpace 期望值
  │
  ├── 组织级（持续模式）
  │     ├── 同类型连续5次负惊讶 → Pattern/矩阵重组（P1）
  │     └── 正惊讶聚类 → 未建模因果结构发现
  │
  └── 模型级（系统性偏差）
        ├── 30天惊讶趋势发散 → Modeler 假设审查（P2）
        └── 跨域一致性破坏 → 深层假设修订
```

### 7.2 世界模型与组织架构的映射

| 哲学概念 | 工程实现 |
|----------|----------|
| 系统的"自我" | OrganizationState（对自身结构的模型） |
| 好奇心 | 惊讶信号驱动的自动探索行为 |
| 预测 | TaskSpace 中的 expected_rounds + expected_success_rate |
| 惊讶 | Surprise Score（预测误差的量化） |
| 模型更新 | Observer 更新 TaskSpace，Modeler 修订深层假设 |
| 行动更新 | Router 优先选择置信度最高的路径 |
| 反思 | Modeler 质疑当前因果结构假设 |
| 自我叙事 | Event Log + 进化时间线（功能性虚构，整合行为） |

---

## 8. 安全与治理架构

### 8.1 安全层次

```
L4 宪法层（不可进化）
  ├── 财务操作无授权禁止
  ├── 原始对话不外传
  ├── 进化步骤必须可审计
  └── 宪法修改需用户确认 + 24h 冷静期

L3 治理层（Governance Gate）
  ├── 进化预算控制
  ├── 探索区隔离
  └── Maintainer 不可自替

L2 操作层（Safety Airbag）
  ├── Blast Radius Limit（权限白名单）
  ├── Reversibility Gate（L3操作 staging 预执行）
  └── Circuit Breaker（失败熔断）

L1 执行层（Sandbox）
  ├── 系统操作子进程隔离
  ├── ulimit + cgroup 资源限制
  └── 所有操作写入 Event Log（不可变）
```

### 8.2 进化安全约束

```
自指悖论防护：
  └── Maintainer（Observer + Evolver）不可自动替换
      只能由用户显式操作触发

评估独立性（Phase 2）：
  └── Meta Judge 与 Operational Critic 物理隔离
      A/B 裁决盲测（不知道哪个是候选版本）

进化漂移控制：
  ├── 宪法定期审查
  ├── Observer 健康报告包含意图对齐度评分
  └── 进化 bisect：通过 Event Log 二分回溯定位退化源头

进化预算制度：
  ├── 每周期插件新增额度上限
  ├── 原语新增额度上限（目标维持 8-20 个）
  └── 探索区资源比例上限（5%-15%）

进化熔断器：
  └── 连续2个编译产物回滚 → Compiler 暂停编译48h
      人工介入审查后才能恢复
```

---

## 9. 项目目录结构

```
EvoClaw/
├── CMakeLists.txt                        # C++23, FetchContent
├── cmake/
│   ├── dependencies.cmake                # 所有外部依赖
│   └── compiler_options.cmake            # GCC 13+ / Clang 16+ 检查
├── .clang-format
├── .clang-tidy
│
├── lib/                                  # ═══ libevoclaw.a (共享) ═══
│   ├── CMakeLists.txt
│   ├── bus/                              # MessageBus（ZMQ 封装）
│   │   ├── message_bus.h                 # 纯虚基类
│   │   └── zmq_message_bus.h/.cpp
│   ├── protocol/                         # 通信协议
│   │   ├── message.h                     # 消息结构
│   │   ├── message_types.h               # 事件类型枚举
│   │   ├── slp_primitives.h              # SLP 12个原语
│   │   ├── serializer.h/.cpp             # nlohmann/json
│   │   ├── rule_engine.h                 # 纯虚基类（FL-0~FL-2）
│   │   ├── basic_rule_engine.h/.cpp      # MVP 规则引擎
│   │   └── cerebellum.h/.cpp             # 小脑：打包/验证/冲突检测
│   ├── event_log/                        # Event Log（系统基石）
│   │   ├── event_store.h                 # 纯虚基类
│   │   ├── jsonl_event_store.h/.cpp      # per-process JSON Lines
│   │   └── event_merger.h/.cpp           # 日志合并 + SHA-256
│   ├── memory/                           # 分层记忆
│   │   ├── memory_store.h                # 纯虚基类
│   │   ├── fs_memory_store.h/.cpp        # 文件系统实现
│   │   ├── memory_types.h                # 工作/情景/语义/用户模型/挑战集
│   │   └── memory_index.h                # 内存索引（防全量扫描）
│   ├── world_model/                      # 世界模型（v5.0）
│   │   ├── world_model_store.h           # 纯虚基类
│   │   ├── fs_world_model_store.h/.cpp   # 文件系统实现
│   │   ├── task_space.h                  # 任务空间数据结构
│   │   ├── causal_structure.h            # 因果结构
│   │   └── organization_state.h          # 组织状态
│   ├── llm/                              # LLM 客户端
│   │   ├── llm_client.h                  # 纯虚基类
│   │   ├── openai_client.h/.cpp          # OpenAI API（cpp-httplib）
│   │   ├── prompt_registry.h             # YAML元数据 + 模板
│   │   └── prompt_registry.cpp
│   ├── config/                           # 配置
│   │   ├── config.h
│   │   └── yaml_config.h/.cpp            # yaml-cpp
│   └── common/
│       └── thread_pool.h                 # 自建线程池（替代 std::async）
│
├── src/
│   ├── engine/                           # ═══ evoclaw-engine ═══
│   │   ├── main.cpp
│   │   ├── engine.h/.cpp
│   │   ├── facade/
│   │   │   ├── facade.h/.cpp             # 统一人格，意图解析
│   │   │   └── discord_bot.h/.cpp        # D++ DPP
│   │   ├── router/
│   │   │   ├── router.h/.cpp             # 意图路由核心
│   │   │   ├── capability_matrix.h       # 模块评分矩阵
│   │   │   ├── epsilon_greedy.h          # ε-greedy 路由策略
│   │   │   └── plugin_registry.h         # L2 插件注册表
│   │   ├── holon/
│   │   │   ├── holon_pool.h/.cpp         # 动态执行池
│   │   │   ├── holon.h/.cpp              # 单次组队实例
│   │   │   └── capabilities/
│   │   │       ├── file_ops.h            # FR51 文件操作
│   │   │       ├── shell_ops.h           # FR52 Shell 执行
│   │   │       ├── process_ops.h         # FR53 进程管理
│   │   │       ├── network_ops.h         # FR54 网络操作
│   │   │       ├── code_exec.h           # FR55 代码执行
│   │   │       └── cron_ops.h            # FR56 定时任务
│   │   ├── governance/
│   │   │   ├── governance_gate.h/.cpp    # 宪法约束执行
│   │   │   ├── constitution.h            # 宪法规则加载
│   │   │   └── entropy_monitor.h         # 熵指标计算（Phase 3 完整）
│   │   └── safety/
│   │       ├── risk_classifier.h         # L0-L3 风险分级
│   │       ├── sandbox.h                 # 子进程沙盒
│   │       └── permission_manager.h      # Blast Radius Limit
│   │
│   ├── compiler/                         # ═══ evoclaw-compiler ═══
│   │   ├── main.cpp
│   │   ├── observer.h/.cpp               # 惊讶引擎（v5.0 重构）
│   │   ├── surprise_calculator.h/.cpp    # Surprise Score 计算
│   │   ├── evolver.h/.cpp                # 变更设计
│   │   ├── compiler.h/.cpp               # Toolization Loop 核心
│   │   ├── pattern_registry.h/.cpp       # Draft→Active 状态机
│   │   ├── challenge_curator.h/.cpp      # 挑战集维护
│   │   ├── dormancy_controller.h/.cpp    # 休眠/唤醒控制
│   │   └── modeler.h/.cpp                # 世界模型假设修订（v5.0）
│   │
│   ├── judge/                            # ═══ evoclaw-judge (Phase 2) ═══
│   │   ├── main.cpp
│   │   └── meta_judge.h                  # 盲测裁决（stub）
│   │
│   └── dashboard/                        # ═══ evoclaw-dashboard ═══
│       ├── main.cpp
│       ├── rest_api.h/.cpp               # REST API
│       ├── sse_handler.h/.cpp            # 实时事件推送
│       └── static/
│           └── index.html                # 管理面板 UI
│
├── tests/
│   ├── lib/                              # 单元测试（镜像 lib/ 结构）
│   │   ├── bus/zmq_message_bus_test.cpp
│   │   ├── protocol/cerebellum_test.cpp
│   │   ├── event_log/jsonl_event_store_test.cpp
│   │   ├── memory/fs_memory_store_test.cpp
│   │   ├── world_model/task_space_test.cpp
│   │   └── llm/prompt_registry_test.cpp
│   ├── src/                              # 组件单元测试
│   │   ├── engine/router_test.cpp
│   │   ├── engine/epsilon_greedy_test.cpp
│   │   ├── engine/holon_pool_test.cpp
│   │   ├── compiler/observer_test.cpp
│   │   ├── compiler/surprise_calculator_test.cpp
│   │   └── compiler/compiler_test.cpp
│   └── integration/
│       ├── bus_integration_test.cpp      # M1 验收
│       ├── engine_compiler_test.cpp      # M3 验收
│       └── event_log_merge_test.cpp
│
├── scripts/
│   ├── evoclaw.sh                        # start/stop（顺序: engine→compiler→dashboard）
│   └── watchdog.sh                       # 3s 自动重启
│
├── prompts/                              # Prompt Registry
│   ├── facade/{system_persona,task_analysis}.{yaml,txt}
│   ├── router/intent_classification.{yaml,txt}
│   ├── compiler/{pattern_recognition,compilation}.{yaml,txt}
│   └── holon/{planner,executor,critic}.{yaml,txt}
│
└── config/
    ├── evoclaw.yaml.example
    ├── risk_levels.yaml                  # L0-L3 操作白名单
    └── constitution.yaml                 # 宪法约束规则
```

---

## 10. 阶段实施分解

### Phase 0（当前）：架构冻结 + 环境准备

**目标：** 项目骨架可编译，依赖管理就绪

**任务：**
- [ ] 创建完整目录结构（参照第9节）
- [ ] 配置 CMakeLists.txt + cmake/dependencies.cmake
  - FetchContent: GTest, nlohmann/json, cpp-httplib, cppzmq, yaml-cpp, spdlog, D++
  - 系统依赖: `apt install libzmq3-dev`
  - 编译器: GCC 13+ 或 Clang 16+（`std::expected` 完整支持）
- [ ] 配置 .clang-format + .clang-tidy（Google C++ Style）
- [ ] 空项目能 `cmake --build` 通过

**验收：** CI 能编译，无警告

---

### Phase 1（M1里程碑）：基础设施可通信

**目标：** 两进程通过 ZeroMQ 互发消息，Event Log 可用

**任务（严格顺序）：**

**P1-1: lib/config/**
- [ ] `YamlConfig`：加载 `evoclaw.yaml`，类型安全 Get<T>()
- [ ] 单元测试覆盖
- [ ] 验收：读取配置文件并断言字段值

**P1-2: lib/protocol/**
- [ ] `Message` 结构定义（所有必需字段）
- [ ] `MessageTypes` 枚举（含事件类型点分命名）
- [ ] `SLPrimitive` 枚举（12个原语）
- [ ] `Serializer`：ToJson / FromJson，`std::expected` 返回
- [ ] `BasicRuleEngine`：FL-0~FL-2 规则（数值比较/布尔/范围）
- [ ] 单元测试覆盖

**P1-3: lib/bus/**
- [ ] `MessageBus` 纯虚基类接口
- [ ] `ZmqMessageBus`：
  - Pub-Sub（事件广播，pub/sub socket）
  - DEALER-ROUTER（请求响应，不锁步）
  - IPC transport：`ipc:///tmp/evoclaw-{process}.sock`
  - L1 QoS：msg_id 幂等去重
- [ ] 集成测试：两进程互发 echo，`bus_integration_test.cpp`

**P1-4: lib/event_log/**
- [ ] `EventStore` 纯虚基类
- [ ] `JsonlEventStore`：
  - per-process JSON Lines（按日期目录）
  - 每条事件 SHA-256 校验
  - append-only 约束（写后只读）
  - 写入后 kill -9 重启数据完整
- [ ] `EventMerger`：每日合并到 canonical log
- [ ] 单元测试（含崩溃恢复测试）

**P1-5: lib/common/**
- [ ] `ThreadPool`：固定线程数，`Submit()` 返回 `std::future<T>`

**M1 验收（全部满足）：**
- evoclaw-engine 和 evoclaw-compiler 两进程独立启动，PID 文件写入
- engine Pub-Sub 发心跳，compiler 接收并打印
- engine DEALER-ROUTER 发 echo 请求，compiler 返回响应
- `bus_integration_test.cpp` 全部通过

---

### Phase 2（M2里程碑）：单任务端到端

**目标：** 用户通过 Discord 发任务，engine 通过 LLM 处理，返回结果

**任务：**

**P2-1: lib/llm/**
- [ ] `LlmClient` 纯虚基类（`Complete()`, `StreamComplete()`）
- [ ] `OpenAiClient`：HTTP 调用（cpp-httplib），JSON 解析，重试（≤3次）
- [ ] `PromptRegistry`：YAML 元数据加载，模板热更新，依赖图校验
- [ ] 初始 Prompt 模板集：facade/system_persona, router/intent_classification, holon/{planner,executor,critic}

**P2-2: lib/memory/**
- [ ] `MemoryStore` 纯虚基类
- [ ] `FsMemoryStore`：按类型分目录，单条 JSON 文件，内存索引
- [ ] TTL 管理：工作记忆会话后压缩，检索硬超时 200ms
- [ ] 单元测试

**P2-3: lib/world_model/（v5.0，仅数据结构）**
- [ ] `TaskSpace` 数据结构（冷启动 12 个基础任务类型）
- [ ] `CausalStructure` 数据结构（冷启动 20 个手工编码因果关系）
- [ ] `OrganizationState` 数据结构
- [ ] `WorldModelStore` 纯虚基类 + `FsWorldModelStore` 实现

**P2-4: src/engine/facade/**
- [ ] `Facade`：统一人格注入（Prompt），意图预处理
- [ ] `DiscordBot`：D++ DPP，Slash Commands + 自然语言混合
- [ ] 进度反馈（SSE 流式推送到前端 / Discord 编辑消息）

**P2-5: src/engine/router/**
- [ ] `Router`：意图分类（调用 LLM + PromptRegistry）
- [ ] `CapabilityMatrix`：task_type × module × score 二维表
- [ ] `EpsilonGreedy`：ε=0.1 路由，新模块晋升期 ε=0.2
- [ ] `PluginRegistry`：L2 插件注册/查询/热替换

**P2-6: src/engine/holon/**
- [ ] `HolonPool`：动态创建/销毁 Holon（jthread），预算系数管理
- [ ] `Holon`：Planner→Executor→Critic→Synthesizer 流程
- [ ] 系统能力（capabilities/）：file_ops, shell_ops（沙盒隔离）
- [ ] 热替换协议：current/old 版本共存，观察期自动回滚

**P2-7: src/engine/safety/**
- [ ] `RiskClassifier`：L0-L3 操作分级（基于 risk_levels.yaml）
- [ ] `Sandbox`：子进程 + ulimit/cgroup 隔离
- [ ] `PermissionManager`：Blast Radius Limit 白名单执行

**P2-8: src/engine/governance/**
- [ ] `GovernanceGate`：宪法约束执行，冷静期管理
- [ ] `Constitution`：加载 constitution.yaml，约束检查 API

**M2 验收（手动验证）：**
- Discord 发送任务 → engine 意图解析 → Holon 组队 → LLM 调用 → 结果返回
- 端到端延迟 < 30s（含 LLM 调用）
- 任务结果写入 Event Log，完整可查

---

### Phase 3（M3里程碑）：进化闭环验证

**目标：** Toolization Loop 至少闭合一次（D1 核心假设验证）

**任务：**

**P3-1: src/compiler/observer/ + surprise_calculator/**
- [ ] Observer：监听 Event Log `task.completed` 事件流
- [ ] `SurpriseCalculator`：三维 Surprise Score（幅度/方向/置信度加权）
- [ ] 惊讶分类路由（P0/P1/P2）
- [ ] 世界模型任务级更新（TaskSpace 期望值滑动平均）
- [ ] 熵度量计算（4个指标）
- [ ] 单元测试：mock Event Log，验证计算逻辑

**P3-2: src/compiler/compiler/**
- [ ] 协作轨迹监听（订阅 Event Log）
- [ ] 模式识别：精确匹配（≥2次）+ 语义聚类（≥3次）
- [ ] L0 路径缓存生成（立即）
- [ ] L1 半自动模板生成（≥3次确认）
- [ ] 假设守卫（Guard）嵌入
- [ ] 正惊讶队列监听

**P3-3: src/compiler/pattern_registry/**
- [ ] Draft → Pending → Active → Deprecated 状态机
- [ ] 沙盒测试触发（5个标准探针任务）
- [ ] 与 Router 的注册接口

**P3-4: src/compiler/evolver/**
- [ ] 读取 Observer 健康报告
- [ ] L1/L2 进化变更包设计
- [ ] A/B 测试 50/50 流量分配
- [ ] 监控期管理（7/14/30 天）

**P3-5: src/compiler/challenge_curator/**
- [ ] 失败事件自动收录（`task.failed` 事件）
- [ ] 手动注入 API
- [ ] 挑战集版本管理

**P3-6: src/compiler/dormancy_controller/**
- [ ] 休眠条件检测（14天所有指标正常）
- [ ] 唤醒条件检测
- [ ] 90天强制健康检查

**P3-7: src/dashboard/**
- [ ] REST API（所有端点）
- [ ] SSE 实时事件推送
- [ ] 基础 Web 面板（熵指标、进化时间线、模块列表）

**M3 验收：**
- 相同类型任务执行 ≥3 次 → Compiler 识别模式 → 生成 L0/L1 产物
- 编译产物写入 Pattern Registry，状态从 Draft → Active
- 管理面板显示进化事件时间线
- Toolization Loop 完整闭合一次（核心假设 D1 验证）

---

### Phase 4：Cerebellum + SLP 完整化

**目标：** 消息通信从纯 JSON 升级为 SLP 语义标准化

**任务：**
- [ ] `Cerebellum` 完整实现（Pack/Validate/DetectConflict 三步管线）
- [ ] SLP 原语约束模板积累机制（渐进式形式化 FL-0→FL-2）
- [ ] 梯度压缩（S/M/L 三档密度）全链路支持
- [ ] Cerebellum 性能测试（L2 消息处理 ≤50ms）

---

### Phase 5：世界模型激活（v5.0 完整）

**目标：** 惊讶三层次循环打通，Modeler 上线

**任务：**
- [ ] 因果结构自动归纳（从 Event Log 历史推断）
- [ ] Modeler 触发条件检测
- [ ] 惊讶聚合报告生成（含热图、趋势、聚类）
- [ ] 假设修订提案格式 + 存储
- [ ] 提案 → 探索区验证 → 写入世界模型完整流程
- [ ] 90天强制健康检查集成
- [ ] v5.0 成功指标收集（世界模型预测准确率、惊讶幅度趋势等）

---

### Phase 6（Meta Judge）：双评估架构完整化

**目标：** 去除人工确认瓶颈，实现全自动进化验证

**任务：**
- [ ] `evoclaw-judge` 进程激活
- [ ] Meta Judge：独立 LLM session + 独立状态存储（逻辑隔离）
- [ ] 盲测裁决协议（不知道哪个是候选版本）
- [ ] A/B 测试门控从「用户确认」升级为「Meta Judge 盲测」
- [ ] L2 完整插件生成（含 Meta Judge 验证）

---

### Phase 7：规模化与可靠性

**目标：** 系统可长期稳定运行，能力自主增长

**任务：**
- [ ] 存储层升级（向量库替代语义记忆文件系统）
- [ ] 进化 bisect（通过 Event Log 定位退化源头）
- [ ] 多 LLM 后端切换（`LlmClient` 接口扩展）
- [ ] 模块「端粒」机制（生命预算动态调节）
- [ ] 12个月成功指标监控：自主编译 ≥10 个插件，跨域能力迁移 ≥1 次
- [ ] 渐进式复杂度升级（FL-3/FL-4 形式化，Z3 评估）

---

## 11. 关键技术决策摘要

| 决策编号 | 决策内容 | 选择 | 核心理由 |
|----------|----------|------|----------|
| ADR-1 | 进程模型 | 4类进程（engine/compiler/dashboard/judge-stub） | 减少 MVP 复杂度，judge Phase 2 激活 |
| ADR-2 | 进程间通信 | ZeroMQ DEALER-ROUTER + Pub-Sub | 避免 REQ-REP 锁步死锁，IPC 延迟 <1ms |
| ADR-3 | 消息序列化 | nlohmann/json | 已有依赖，调试友好 |
| ADR-4 | 存储方案 | 文件系统 JSON/JSONL | MVP 数据量小，抽象接口预留替换 |
| ADR-5 | 形式验证 | 规则引擎（FL-0~FL-2），Z3 Phase 2 | 降低 MVP 复杂度，保留升级路径 |
| ADR-6 | LLM 集成 | OpenAI 兼容 API，httplib 直调 | 最简，单后端 MVP 够用 |
| ADR-7 | Discord Bot | D++ (DPP) | 原生 C++，活跃维护 |
| ADR-8 | Prompt 管理 | PromptRegistry（YAML元数据+模板） | 版本管理 + 依赖声明 + 热更新 |
| ADR-9 | 异步调用 | 自建 ThreadPool | std::async 线程模型不可控 |
| ADR-10 | 错误处理 | std::expected<T, Error>（C++23） | 编译期强制处理，禁止异常 |
| ADR-11 | 编译产物分级 | L0路径缓存/L1模板/L2完整插件 | 降低 D1 风险（Toolization Loop 闭合） |
| ADR-12 | QoS | MVP 两级（fire-forget + at-least-once） | exactly-once 需 WAL，Phase 2 实现 |
| ADR-13 | 进化门控（MVP） | 用户确认替代 Meta Judge | Meta Judge Phase 2，先验证核心假设 |
| ADR-14 | 哲学底座（v5.0） | 自由能原理（惊讶驱动，世界模型） | 从"规则驱动进化"到"预测误差驱动进化" |

---

## 12. 依赖与基础设施

### 12.1 完整依赖清单

| 依赖 | 版本 | 管理方式 | 用途 |
|------|------|----------|------|
| GTest | latest | FetchContent | 测试框架 |
| nlohmann/json | ≥3.11 | FetchContent | JSON 序列化 |
| cpp-httplib | latest | FetchContent | HTTP client（LLM）+ server（Dashboard）|
| cppzmq | ≥4.9 | FetchContent | ZeroMQ C++ binding |
| libzmq | ≥4.3 | 系统包（apt） | ZeroMQ 核心库 |
| yaml-cpp | ≥0.8 | FetchContent | YAML 配置解析 |
| spdlog | ≥1.12 | FetchContent | 日志（异步，多 sink）|
| D++ (DPP) | ≥10.0 | FetchContent | Discord Bot |

### 12.2 系统环境要求

```bash
# 系统依赖
apt install libzmq3-dev cmake ninja-build

# 编译器（必须，std::expected 完整支持）
GCC 13+ 或 Clang 16+

# 环境变量
export EVOCLAW_LLM_API_KEY="sk-..."
export EVOCLAW_DISCORD_TOKEN="..."

# 运行时目录（自动创建）
mkdir -p ~/.evoclaw/{data,world_model,prompts,plugins,config,logs}
```

### 12.3 三个关键里程碑的风险监控

| 里程碑 | 验证的核心假设 | 失败信号 | 应对方案 |
|--------|---------------|----------|----------|
| M1 | T1（C++ 开发效率）+ T3（ZMQ 延迟） | 基础设施超期 >1周 | 评估 Rust/Python+C 混合 |
| M2 | T5（LLM 延迟可接受）+ U4（Discord 适合） | 端到端 >60s 或频繁超时 | 降级策略：更换 LLM 提供商 |
| M3 | D1（Toolization Loop 可闭合）+ U1（任务有足够重复性） | 3周内无法识别任何模式 | 引入语义聚类，降低编译阈值到 ≥2次 |

### 12.4 渐进升级路径

| 维度 | MVP | 升级触发 | Phase N 目标 |
|------|-----|----------|-------------|
| 通信形式化 | FL-0~FL-2（规则引擎）| 误判率 >5% | FL-3~FL-4（Z3） |
| 存储 | 文件系统 JSON | 检索延迟 >200ms | 向量库 + ES |
| 进程模型 | 4类进程 | engine CPU >80% | 6类进程 |
| 编译产物 | L0+L1 | L1 覆盖率 >60% | L2 完整插件 |
| QoS | 2级 | 丢失率 >0.01% | exactly-once（WAL）|
| 进化评估 | 用户确认 | 编译产物 >10/月 | Meta Judge 逻辑隔离 |

---

*EvoClaw Technical Architecture · 整合版 · 面向实现*
*涵盖 v1.0-v5.0 设计演进 + Architecture Decision Document + PRD*
