---
stepsCompleted: [1, 2]
inputDocuments:
  - _bmad-output/planning-artifacts/prd.md
  - _bmad-output/planning-artifacts/architecture.md
  - doc_design/EvoClaw_Unified_Design.md
  - doc_design/evoclaw_v1.md
  - doc_design/evoclaw_v2.md
  - doc_design/evoclaw_v3.md
  - doc_design/evoclaw_v4.md
  - doc_design/gptv5.md
  - doc_design/grokV4.md
  - doc_design/claude_QA.md
  - doc_design/grok_QA.md
  - doc_design/a2a/cortex-architecture.md
  - doc_design/a2a/nltc-protocol-spec.md
  - doc_design/a2a/nltc-agent-integration.md
---

# EvoClaw - Epic Breakdown

## Overview

This document provides the complete epic and story breakdown for EvoClaw, decomposing the requirements from the PRD, Architecture requirements and original design documents into implementable stories.

## Requirements Inventory

### Functional Requirements

FR1: 用户通过 Discord 以自然语言向管家发出任务请求
FR2: 用户通过结构化指令查询系统状态（`/status`、`/health`）
FR3: 用户通过结构化指令管理插件（`/plugins` 查看、启用、禁用）
FR4: 用户通过结构化指令查看进化状态（`/evolution`、`/entropy`）
FR5: 管家以统一人格回应用户——用户无法从回复风格区分底层由哪些 Agent 协作完成
FR6: 用户在任务执行过程中收到进度摘要反馈
FR7: 管家在任务失败时向用户提供失败原因和 ≥1 个恢复选项
FR8: 管家在检测到重复行为模式（≥3 次同类操作）时主动发起建议或提醒（Phase 2）
FR9: Fast Router 采用 ε-greedy 策略将已知任务匹配到已固化的插件并直接执行——10% 随机探索路由 + 新模块 14 天 15% 冷启动配额 + 30 天周期扰动防止路径固化
FR10: Fast Router 将未知复杂任务转发给 Dynamic Holons
FR11: Dynamic Holons 根据任务需求临时组队（规划者、执行者、审查员）
FR12: Holons 中的 Agent 通过消息总线进行发布/订阅和服务调用通信
FR13: Agent 间通过消息传递通信，故障隔离通过进程级或沙盒级机制保证
FR14: 系统在不停机的情况下替换或升级任意组件（热替换）
FR15: 新 Agent 满足标准接口契约后接入系统，无需修改 Router（Phase 3）
FR16: 系统对同一任务的多个独立子步骤支持并行执行策略
FR17: Compiler 从事件日志中识别重复出现（≥3 次同路径成功执行）的多 Agent 协作模式
FR18: Compiler 将识别到的协作模式编译为独立插件，编译产物延迟和 token 消耗均低于原协作路径
FR19: 编译产物在注册到 Fast Router 前经过沙盒测试验证
FR20: 编译产物在 MVP 阶段需经用户确认后才能注册（Phase 2 全自动化）
FR21: 编译后的插件替换原有 Holons 协作路径，同类任务不再触发 Holons 组队
FR22: Operational Critic 对任务输出进行质量评估——Critic 可进化，标准随系统成长调整（Phase 2）
FR23: Meta Judge 独立于 Operational Critic 对进化结果进行盲测——Judge 标准冻结，两者零上下文共享，物理隔离防止评估坍缩（Phase 2）
FR24: 系统监控 5 个熵指标（原语密度、模块密度、平均依赖深度、平均任务轮次、探索积压）并在超阈值时触发压缩/合并/退役（Phase 3）
FR25: 插件具有完整生命周期管理（Draft → Active → Deprecated → Retired）（Phase 3）
FR26: 探索区模块经过 ≥20 次任务验证并通过契约校验和盲测后，自动晋升到稳定区
FR27: 系统维护当前会话的工作记忆，会话结束后压缩提炼
FR28: 系统从用户交互中提取并维护结构化用户偏好模型
FR29: 系统基于用户模型自动适配输出格式和风格——输出格式匹配用户模型中记录的偏好项
FR30: 系统存储和检索历史任务记录（情景记忆）（Phase 2）
FR31: 系统从历史任务中提炼知识并形成语义记忆（Phase 2）
FR32: 系统基于情景记忆进行风险预判——预警的风险中 ≥50% 确实发生或被用户确认有价值（Phase 2）
FR33: 系统维护失败/边界案例的挑战集——4 类来源：失败/回滚任务、低置信成功（侥幸通过）、用户标记不满、Curator 合成挑战（Phase 2）
FR34: Agent 间通信采用混合协议，支持形式约束和语义约束
FR35: 形式约束由形式化验证引擎直接验证，不消耗 LLM context
FR36: 语义约束由 LLM 按需评估
FR37: 小脑自动处理消息打包、验证和冲突检测，LLM 只看精炼摘要
FR38: 约束模板积累复用——复用模板的消息 token 消耗低于首次同类消息
FR39: 约束支持渐进式形式化升级——从纯语义评估逐步升级到完整形式验证（Phase 2）
FR40: 管理者通过 Web 面板查看系统运行状态
FR41: 管理者通过 Web 面板查看进化日志和编译历史
FR42: 管理者通过 Web 面板查看 Agent 健康状态和资源消耗
FR43: 管理者通过 Web 面板查看熵指标趋势（Phase 2）
FR44: 管理者通过 Web 面板进行系统配置管理（Phase 2）
FR45: 管理面板默认展示用户最近 30 天查询频率最高的 3 个指标（Phase 2）
FR46: 所有系统行为、决策、状态变更记录到不可变的事件日志
FR47: 任意进化决策的完整链路从事件日志复现
FR48: 用户模型只存储结构化偏好，不存储原始对话内容
FR49: per-agent token 使用量可追踪，支持全局预算控制
FR50: 宪法变更需用户显式确认并设置 ≥24h 冷静期（Phase 3）
FR51: 管家访问和操作本地文件系统（读写文件、目录管理）
FR52: 管家执行操作系统命令（Shell 命令执行）
FR53: 管家管理系统进程（启动、停止、监控进程）
FR54: 管家进行网络操作（HTTP 请求、API 调用、网页抓取）
FR55: 管家执行代码（运行脚本、编译执行）
FR56: 管家管理定时任务（创建、修改、删除 cron 类任务）
FR57: 系统资源操作按风险等级分级（L0-L3），L2 及以上需用户显式确认
FR58: 所有资源操作记录到事件日志，支持审计回溯
FR59: 系统通过 Discord Bot API 集成，支持文本消息、斜杠命令、嵌入式消息
FR60: 系统通过消息中间件实现组件间通信，支持发布/订阅和服务调用
FR61: 系统通过标准向量检索接口集成向量数据库（Phase 2）
FR62: 系统通过标准接口集成事件存储引擎（Phase 2）
FR63: 系统提供标准接口契约规范文档和开发者 SDK
FR64: 任务完成后系统主动征询用户反馈——确认结果是否一次性满足需求
FR65: 用户反馈不满意时，系统记录完整失败上下文到失败历史
FR66: 系统基于失败历史自动重新尝试任务，目标一次性做好
FR67: 重试成功后，系统将改进路径固化为经验模板
FR68: 同类任务再次出现时，系统自动加载相关经验模板作为执行约束
FR69: Pattern 作为一等抽象（介于原语和插件之间），Compiler 先生成 Pattern 再决定编译路径
FR70: Toolization Loop 9 步闭环：涌现→轨迹规范化→模式提取→契约合成→暂存→治理门→灰度发布→晋升或回滚→Holon 释放
FR71: 进化预算制度——每周期配额有上限，超额触发强制熵缩减（Phase 3）
FR72: Plugin Tool 进入稳定区必须包含 8 个必填字段（Phase 3）
FR73: 编译产物需通过三集评估：Replay + Failure + Challenge（Phase 2）
FR74: SLP 梯度压缩——消息按 S/M/L 级别压缩
FR75: Adapter 协议演进——新旧 schema 通过 adapter 共存，Event Log 永不迁移（Phase 2）
FR76: Maintainer 替换需用户显式触发 + 72h 冷静期（Phase 3）

### NonFunctional Requirements

NFR1: Fast Router 对已知任务的响应延迟 ≤500ms（p95）
NFR2: 组件间消息传递延迟 单跳 ≤50ms（p95）
NFR3: 形式约束验证延迟 ≤100ms（p95）
NFR4: 工作记忆检索延迟 ≤200ms（p95）
NFR5: 用户模型查询延迟 ≤300ms（p95）
NFR6: 事件日志写入不阻塞主流程，排队延迟 ≤10ms（p99）
NFR7: 单个 Agent 进程崩溃不影响系统整体，自动重启 ≤5s
NFR8: 组件间消息传递成功率 ≥99.99%
NFR9: 事件日志写入确认后不可丢失，append-only
NFR10: 组件热替换期间服务不中断
NFR11: LLM API 调用失败自动重试 ≤3 次 + 超时回退 ≤30s
NFR12: L2+ 操作 100% 需用户确认
NFR13: LLM API 密钥不明文存储，不出现在日志中
NFR14: 用户模型不存储原始对话
NFR15: 编译产物沙盒执行
NFR16: 新 Agent 接入不需要修改现有组件
NFR17: 新插件注册不需要重启 Router
NFR18: 存储层可替换
NFR19: 通信层可替换
NFR20: 所有组件独立部署、独立版本管理
NFR21: 进化全链路可审计
NFR22: 系统状态可观测
NFR23: 日志结构化 JSON 格式
NFR24: LLM 接入模型无关，支持 ≥2 家提供商切换

### Additional Requirements

**架构 Starter Template：**
- 全新 C++23 项目骨架（按部署单元组织），项目初始化应作为第一个实现 story
- CMake 3.25+, FetchContent 管理 8 个依赖（GTest, nlohmann/json, cpp-httplib, cppzmq, yaml-cpp, spdlog, DPP, libzmq 系统包）
- 编译器要求：GCC 13+ 或 Clang 16+
- lib/ OBJECT library 聚合为 libevoclaw.a

**架构进程模型（4 进程 MVP）：**
- evoclaw-engine: Facade + Router + Holon + Discord Bot + Governance
- evoclaw-compiler: Observer + Evolver + Compiler
- evoclaw-dashboard: REST + SSE + Web 面板
- evoclaw-judge: Phase 2 stub

**架构实现顺序：**
1. 项目骨架初始化（CMake + FetchContent + 目录结构）
2. lib/config → lib/protocol → lib/bus → lib/event_log → lib/common
3. lib/llm + lib/memory
4. src/engine（Facade + Router + Holon + Discord Bot）
5. src/compiler（Observer + Evolver + Compiler）
6. src/dashboard（REST + SSE + Web）
7. scripts/（启动/停止/watchdog）

**架构 MVP 里程碑：**
- M1（基础设施可通信）：lib/ 核心模块 + 两进程 ZeroMQ 通信验证
- M2（单任务端到端）：Discord 发任务 → engine 调 LLM → 返回结果
- M3（进化闭环验证）：重复任务 ≥3 次 → Compiler 识别模式 → 生成产物

**架构关键模式：**
- Google C++ Style 命名规范
- std::expected<T, Error> 错误处理
- 纯虚基类接口 + Factory 创建 + 构造器注入
- Router 线程池 + Holon jthread 并发模型
- ZeroMQ Pub-Sub + DEALER-ROUTER 拓扑
- JSON Lines 事件日志 + 文件系统记忆存储
- Prompt Registry（YAML 元数据 + 纯文本模板）

**架构测试策略：**
- 单元测试：GTest + hand-written fakes（Mock MessageBus 等纯虚基类）
- 集成测试：inproc:// ZeroMQ transport
- 端到端测试：tcp:// 多进程真实部署（手动）

**原始设计文档补充约束：**
- Agent 扩展上限 3-7 个（通信开销 n^1.724 增长）
- 同质 Agent 饱和点约 4 个
- 三不可变支柱：Facade 人格契约、Governance Kernel、Canonical Event Log

### FR Coverage Map

FR1: Epic 2 — Discord 自然语言任务请求
FR2: Epic 8 — 结构化指令查询系统状态
FR3: Epic 8 — 结构化指令管理插件
FR4: Epic 8 — 结构化指令查看进化状态
FR5: Epic 2 — 统一人格回应
FR6: Epic 2 — 进度摘要反馈
FR7: Epic 2 — 失败原因和恢复选项
FR8: Epic 9 — 主动提醒（Phase 2）
FR9: Epic 2 — Fast Router ε-greedy 匹配
FR10: Epic 2 — 未知任务转发 Holons
FR11: Epic 3 — Dynamic Holons 临时组队
FR12: Epic 1 — 消息总线发布/订阅和服务调用
FR13: Epic 1 — 消息传递通信 + 故障隔离
FR14: Epic 8 — 热替换
FR15: Epic 12 — 标准接口契约接入（Phase 3）
FR16: Epic 3 — 并行执行策略
FR17: Epic 5 — Compiler 模式识别
FR18: Epic 5 — 协作模式编译为插件
FR19: Epic 5 — 编译产物沙盒测试
FR20: Epic 5 — 编译产物用户确认
FR21: Epic 5 — 插件替换协作路径
FR22: Epic 10 — Operational Critic（Phase 2）
FR23: Epic 10 — Meta Judge 盲测（Phase 2）
FR24: Epic 11 — 熵指标监控（Phase 3）
FR25: Epic 11 — 插件生命周期管理（Phase 3）
FR26: Epic 10 — 探索区晋升稳定区
FR27: Epic 2 — 工作记忆
FR28: Epic 2 — 用户偏好模型
FR29: Epic 2 — 输出格式自动适配
FR30: Epic 9 — 情景记忆（Phase 2）
FR31: Epic 9 — 语义记忆（Phase 2）
FR32: Epic 9 — 风险预判（Phase 2）
FR33: Epic 9 — 挑战集 4 类来源（Phase 2）
FR34: Epic 7 — 混合协议
FR35: Epic 7 — 形式约束验证
FR36: Epic 7 — 语义约束 LLM 评估
FR37: Epic 7 — 小脑消息处理
FR38: Epic 7 — 约束模板复用
FR39: Epic 10 — 渐进式形式化（Phase 2）
FR40: Epic 8 — Web 面板系统状态
FR41: Epic 8 — Web 面板进化日志
FR42: Epic 8 — Web 面板 Agent 健康
FR43: Epic 9 — Web 面板熵指标（Phase 2）
FR44: Epic 9 — Web 面板配置管理（Phase 2）
FR45: Epic 9 — 面板个性化布局（Phase 2）
FR46: Epic 1 — 不可变事件日志
FR47: Epic 1 — 进化决策链复现
FR48: Epic 2 — 用户模型数据隔离
FR49: Epic 2 — per-agent token 追踪
FR50: Epic 11 — 宪法变更冷静期（Phase 3）
FR51: Epic 4 — 文件系统操作
FR52: Epic 4 — Shell 命令执行
FR53: Epic 4 — 进程管理
FR54: Epic 4 — 网络操作
FR55: Epic 4 — 代码执行
FR56: Epic 4 — 定时任务管理
FR57: Epic 4 — 风险等级分级
FR58: Epic 4 — 资源操作审计
FR59: Epic 2 — Discord Bot API 集成
FR60: Epic 1 — 消息中间件通信
FR61: Epic 12 — 向量数据库集成（Phase 2）
FR62: Epic 12 — 事件存储引擎集成（Phase 2）
FR63: Epic 12 — 标准接口契约文档和 SDK
FR64: Epic 6 — 任务完成反馈征询
FR65: Epic 6 — 失败上下文记录
FR66: Epic 6 — 基于失败历史重试
FR67: Epic 6 — 经验模板固化
FR68: Epic 6 — 经验模板加载
FR69: Epic 5 — Pattern 一等抽象
FR70: Epic 5 — Toolization Loop 9 步闭环
FR71: Epic 11 — 进化预算制度（Phase 3）
FR72: Epic 11 — Plugin Tool 8 必填字段（Phase 3）
FR73: Epic 9 — 三集评估（Phase 2）
FR74: Epic 7 — SLP 梯度压缩
FR75: Epic 9 — Adapter 协议演进（Phase 2）
FR76: Epic 11 — Maintainer 替换隔离（Phase 3）

## Epic List

### Phase 1 — MVP

### Epic 1: 基础设施与进程通信
系统骨架搭建完成，多个进程能互相通信，事件日志可持久化。对齐架构里程碑 M1。
**FRs covered:** FR12, FR13, FR46, FR47, FR60
**NFRs covered:** NFR2, NFR6, NFR7, NFR9, NFR23

### Epic 2: 与管家对话
用户通过 Discord 发出任务，管家以统一人格回应，能调用 LLM 完成简单任务，记住用户偏好。对齐架构里程碑 M2。
**FRs covered:** FR1, FR5, FR6, FR7, FR9, FR10, FR27, FR28, FR29, FR48, FR49, FR59
**NFRs covered:** NFR1, NFR11, NFR13, NFR24

### Epic 3: 智能多 Agent 协作
复杂任务由专家团队协作完成——规划、执行、审查分工明确，支持并行子任务。
**FRs covered:** FR11, FR16
**NFRs covered:** NFR16

### Epic 4: 系统控制与资源操作
管家能操作文件、执行命令、管理进程、网络请求——真正的"管家"能力，风险操作需用户确认。
**FRs covered:** FR51, FR52, FR53, FR54, FR55, FR56, FR57, FR58
**NFRs covered:** NFR12

### Epic 5: 进化与工具化闭环
重复任务越来越快——系统自动识别模式、生成 Pattern、编译为插件、替换原有协作路径。对齐架构里程碑 M3。
**FRs covered:** FR17, FR18, FR19, FR20, FR21, FR69, FR70
**NFRs covered:** NFR15, NFR17

### Epic 6: 质量反馈与经验学习
告诉管家"不行"，它会记住失败上下文并学会一次做好，成功路径固化为经验模板。
**FRs covered:** FR64, FR65, FR66, FR67, FR68

### Epic 7: 智能通信协议
Agent 间通信高效——形式约束自动验证，消息按 S/M/L 梯度压缩，token 消耗降低。
**FRs covered:** FR34, FR35, FR36, FR37, FR38, FR74
**NFRs covered:** NFR3

### Epic 8: 系统仪表盘与运维
通过 Web 面板和结构化指令查看系统状态、进化日志、Agent 健康，一键启停，热替换，自动重启。
**FRs covered:** FR2, FR3, FR4, FR14, FR40, FR41, FR42
**NFRs covered:** NFR10, NFR20, NFR22

### Phase 2 — 智能增强

### Epic 9: 深度记忆与主动智能
管家能预判风险、主动提醒、从历史中学习，面板增强配置管理和熵指标可视化。
**FRs covered:** FR8, FR30, FR31, FR32, FR33, FR43, FR44, FR45, FR73, FR75
**NFRs covered:** NFR4, NFR5

### Epic 10: 双评估架构
进化质量有保障——Operational Critic 可进化 + Meta Judge 冻结标准零上下文共享，探索区晋升机制，渐进式形式化。
**FRs covered:** FR22, FR23, FR26, FR39

### Phase 3 — 涌现智能

### Epic 11: 反熵自治与进化治理
系统自动瘦身——5 个熵指标监控、进化预算制度、插件生命周期、宪法审查、Maintainer 替换隔离。
**FRs covered:** FR24, FR25, FR50, FR71, FR72, FR76

### Epic 12: 开放平台与扩展
外部开发者可贡献模块，标准接口零摩擦接入，向量数据库和事件存储引擎集成。
**FRs covered:** FR15, FR61, FR62, FR63
**NFRs covered:** NFR16, NFR17, NFR18, NFR19

## Epic 1: 基础设施与进程通信

系统骨架搭建完成，多个进程能互相通信，事件日志可持久化。对齐架构里程碑 M1。

### Story 1.1: 项目骨架与构建系统初始化

As a 开发者,
I want 完整的 C++23 项目骨架（CMake + FetchContent + 目录结构 + 统一错误类型）,
So that 所有后续模块有统一的构建基础和代码规范。

**Acceptance Criteria:**

**Given** 一台安装了 GCC 13+ 和 libzmq3-dev 的 Linux 机器
**When** 执行 `mkdir build && cd build && cmake .. && make`
**Then** 项目编译成功，无错误无警告
**And** CMakeLists.txt 配置 C++23 标准
**And** FetchContent 成功拉取 8 个依赖（GTest, nlohmann/json, cpp-httplib, cppzmq, yaml-cpp, spdlog, DPP）
**And** 目录结构与架构文档"完整项目目录树"一致（lib/, src/, tests/, scripts/, config/, prompts/）
**And** `lib/common/error.h` 定义统一 Error 结构体和 ErrorCode 枚举
**And** GTest 空测试用例能通过 `ctest`

### Story 1.2: 配置加载模块

As a 开发者,
I want YAML 配置文件加载模块（lib/config）,
So that 系统行为可通过配置文件调整而无需重编译。

**Acceptance Criteria:**

**Given** `config/evoclaw.yaml` 包含进程端点、日志级别、数据目录等配置项
**When** 调用 `YamlConfig::Load("config/evoclaw.yaml")`
**Then** 返回 `std::expected<Config, Error>` 包含所有解析后的配置值
**And** 配置项缺失时返回带有明确 context 的 Error
**And** 支持环境变量覆盖（如 `EVOCLAW_LLM_API_KEY`）
**And** `yaml_config_test.cpp` 覆盖正常加载、缺失文件、格式错误 3 种场景

### Story 1.3: 消息协议与序列化

As a 开发者,
I want 标准化的消息类型定义和 JSON 序列化（lib/protocol）,
So that 所有进程使用统一的消息格式通信。

**Acceptance Criteria:**

**Given** 消息信封格式包含 msg_id, msg_type, source, target, timestamp, payload
**When** 创建一条 `task.request` 消息并调用 `Serializer::Serialize()`
**Then** 输出符合架构文档定义的 JSON 格式
**And** `Serializer::Deserialize()` 能还原为相同的消息对象
**And** 事件类型使用点分层级命名（如 `agent.task.completed`）
**And** `lib/protocol/basic_rule_engine.h` 定义 RuleEngine 接口（纯虚基类）
**And** `serializer_test.cpp` 和 `basic_rule_engine_test.cpp` 通过

### Story 1.4: ZeroMQ 消息总线

As a 开发者,
I want ZeroMQ 消息总线封装（lib/bus）支持 Pub-Sub 和 DEALER-ROUTER,
So that 进程间能进行事件广播和异步请求响应通信。

**Acceptance Criteria:**

**Given** `MessageBus` 纯虚基类定义 Publish/Subscribe/Send/Receive 接口
**When** 使用 `ZmqMessageBus` 实现创建 Pub-Sub 连接
**Then** 发布者发送的消息能被所有订阅者接收
**And** DEALER-ROUTER 模式下请求方发送消息后不阻塞，响应方异步返回结果
**And** 消息使用 lib/protocol 的 JSON 序列化格式
**And** 端点地址从 lib/config 配置读取
**And** `zmq_message_bus_test.cpp` 使用 `inproc://` transport 验证 Pub-Sub 和 DEALER-ROUTER 两种模式

### Story 1.5: 事件日志持久化

As a 开发者,
I want append-only 的 JSON Lines 事件日志（lib/event_log）,
So that 所有系统行为被不可变地记录，支持审计回放。

**Acceptance Criteria:**

**Given** `EventStore` 纯虚基类定义 Append/Query/Merge 接口
**When** 调用 `JsonlEventStore::Append(event)` 写入事件
**Then** 事件以 JSON Lines 格式追加到 `~/.evoclaw/data/events/{process}/` 目录
**And** 每条事件包含 event_id, event_type, source_process, timestamp, data, checksum（SHA-256）
**And** 写入为异步非阻塞，排队延迟 ≤10ms（NFR6）
**And** `Query()` 支持按时间范围和事件类型过滤
**And** 进程异常退出后重启，已写入的事件完整可读
**And** `jsonl_event_store_test.cpp` 和 `event_merger_test.cpp` 通过

### Story 1.6: M1 集成验证——双进程通信

As a 开发者,
I want 验证两个独立进程能通过 ZeroMQ 互相通信,
So that M1 里程碑达成，基础设施可信赖。

**Acceptance Criteria:**

**Given** evoclaw-engine 和 evoclaw-compiler 两个进程的 main.cpp 最小实现（启动 + 连接 MessageBus）
**When** 通过 `scripts/evoclaw.sh start` 启动两个进程
**Then** 两个进程独立启动，PID 文件正确写入到 `~/.evoclaw/`
**And** engine 通过 Pub-Sub 发送心跳事件，compiler 成功接收并打印日志
**And** engine 通过 DEALER-ROUTER 发送 echo 请求，compiler 返回 echo 响应
**And** `tests/integration/bus_integration_test.cpp` 使用 `inproc://` 验证上述场景全部通过
**And** `scripts/evoclaw.sh stop` 能正确停止所有进程

## Epic 2: 与管家对话

用户通过 Discord 发出任务，管家以统一人格回应，能调用 LLM 完成简单任务，记住用户偏好。对齐架构里程碑 M2。

### Story 2.1: LLM 客户端与 Prompt Registry

As a 开发者,
I want OpenAI 兼容的 LLM HTTP 客户端和 Prompt Registry（lib/llm）,
So that 系统能调用 LLM 生成回复，prompt 模板可热更新。

**Acceptance Criteria:**

**Given** 环境变量 `EVOCLAW_LLM_API_KEY` 已设置，`prompts/` 目录包含 YAML 元数据 + txt 模板
**When** 调用 `OpenAiClient::Chat(messages)` 发送请求
**Then** 返回 `std::expected<LlmResponse, Error>` 包含回复文本和 token usage
**And** API 调用失败时自动重试 ≤3 次，超时回退 ≤30s（NFR11）
**And** API Key 不出现在日志中（NFR13）
**And** `PromptRegistry::Load("prompts/")` 加载所有模板，支持 `{{variable}}` 占位符替换
**And** PromptRegistry 支持按 id + version 查询模板
**And** `openai_client_test.cpp` 和 `prompt_registry_test.cpp` 通过（使用 mock HTTP）

### Story 2.2: 记忆存储模块

As a 开发者,
I want 文件系统记忆存储（lib/memory）支持工作记忆和用户模型,
So that 系统能记住当前会话上下文和用户偏好。

**Acceptance Criteria:**

**Given** `MemoryStore` 纯虚基类定义 Save/Load/Delete/List 接口
**When** 调用 `FsMemoryStore::Save("working_memory", key, data)` 写入记忆
**Then** 数据以 JSON 文件存储到 `~/.evoclaw/data/memory/working_memory/{key}.json`
**And** 用户模型存储到 `~/.evoclaw/data/memory/user_model/` 目录
**And** 用户模型只存储结构化偏好，不存储原始对话内容（FR48）
**And** `fs_memory_store_test.cpp` 覆盖 Save/Load/Delete/List 和目录不存在自动创建场景

### Story 2.3: Discord Bot 基础连接

As a 用户,
I want 通过 Discord 向管家发送消息并收到回复,
So that 我有一个便捷的对话入口与管家交互。

**Acceptance Criteria:**

**Given** Discord Bot Token 已配置在 `evoclaw.yaml` 中，Bot 已加入目标服务器
**When** 用户在 Discord 频道发送一条文本消息
**Then** Bot 接收消息并转发给 Facade 处理
**And** Facade 调用 LLM 生成回复后通过 Bot 返回给用户
**And** 支持 Slash Commands 注册（`/status`、`/health` 占位）
**And** Bot 使用 D++ (DPP) 库实现
**And** `discord_bot_test.cpp` 使用 mock DPP 验证消息接收和发送流程

### Story 2.4: Facade 统一人格层

As a 用户,
I want 管家始终以统一的人格风格回应我,
So that 我感觉在和一个"人"对话，而非多个机器人。

**Acceptance Criteria:**

**Given** Facade 配置了人格 prompt（语气、风格、称呼偏好）
**When** 用户发送任务请求
**Then** Facade 将人格 prompt + 用户消息 + 工作记忆上下文组合后发送给 LLM
**And** 回复风格一致，用户无法区分底层由哪些组件处理（FR5）
**And** 任务执行过程中通过 Discord 发送进度摘要（FR6）
**And** 任务失败时提供失败原因和 ≥1 个恢复选项（FR7）
**And** `facade_test.cpp` 验证人格 prompt 注入和上下文组装逻辑

### Story 2.5: Fast Router 基础路由

As a 用户,
I want 管家能快速判断任务类型并选择最佳处理路径,
So that 简单任务秒回，复杂任务交给专家团队。

**Acceptance Criteria:**

**Given** Router 维护已注册插件列表（初始为空）和路由规则
**When** 收到一个任务请求
**Then** Router 先在插件列表中匹配，命中则直接执行（FR9 基础匹配，ε-greedy 参数预留）
**And** 未命中时将任务转发给 Holons 处理（FR10，MVP 阶段 Holons 为单 Agent 直接调 LLM）
**And** 路由决策记录到 Event Log
**And** `router_test.cpp` 验证插件匹配命中/未命中两条路径

### Story 2.6: 工作记忆与用户模型集成

As a 用户,
I want 管家记住我当前对话的上下文和我的偏好,
So that 我不需要重复说明背景，管家的输出风格符合我的习惯。

**Acceptance Criteria:**

**Given** 用户已有历史交互记录存储在 user_model 中
**When** 用户发送新任务
**Then** Facade 从工作记忆加载当前会话上下文注入 LLM prompt
**And** Facade 从用户模型加载偏好（输出格式、语气等）适配回复风格（FR29）
**And** 会话结束后工作记忆压缩提炼并存储（FR27）
**And** 每次交互后提取新的偏好信号更新用户模型（FR28）
**And** per-agent token 使用量记录到 Event Log（FR49）

### Story 2.7: M2 端到端验证

As a 用户,
I want 通过 Discord 发出一个完整任务并收到有用的回复,
So that M2 里程碑达成，单任务端到端流程可用。

**Acceptance Criteria:**

**Given** evoclaw-engine 进程运行中，Discord Bot 在线，LLM API 可用
**When** 用户在 Discord 发送"帮我总结一下 Rust 和 C++ 的优缺点对比"
**Then** 管家以统一人格回复一份结构化的对比总结
**And** 回复格式符合用户模型中的偏好（如表格对比）
**And** 整个流程（消息接收→LLM 调用→回复发送）延迟合理（NFR1 参考）
**And** Event Log 记录完整的任务执行轨迹（接收、路由、LLM 调用、回复）
**And** token 使用量被正确追踪


## Epic 3: 智能多 Agent 协作

复杂任务由专家团队协作完成——规划、执行、审查分工明确，支持并行子任务。

### Story 3.1: Dynamic Holons 组队框架

As a 系统,
I want 根据任务需求动态组建 Agent 团队（规划者、执行者、审查员）,
So that 复杂任务由专业分工的团队协作完成，而非单个 Agent 独立处理。

**Acceptance Criteria:**

**Given** Router 判定一个任务为复杂任务（未命中插件且任务描述包含多步骤关键词）
**When** 任务被转发给 Holon Pool
**Then** HolonPool 创建一个 Holon 组队，包含 Planner、Executor、Critic 三个角色
**And** 每个角色使用独立的 Prompt 模板（从 Prompt Registry 加载）
**And** 组队在独立的 jthread 中运行，支持 stop_token 优雅停止
**And** 组队完成后资源自动释放（线程退出、内存回收）
**And** `holon_pool_test.cpp` 验证组队创建、角色分配、资源释放流程

### Story 3.2: Holon 内部协作流程

As a 系统,
I want Holon 内的 Planner→Executor→Critic 按序协作完成任务,
So that 复杂任务经过规划、执行、审查三个阶段，输出质量有保障。

**Acceptance Criteria:**

**Given** 一个 Holon 组队已创建，包含 Planner、Executor、Critic
**When** 任务开始执行
**Then** Planner 将任务拆解为子步骤列表（调用 LLM）
**And** Executor 按子步骤列表逐步执行（每步调用 LLM 或工具）
**And** Critic 对 Executor 的输出进行质量评估（调用 LLM）
**And** Critic 评估不通过时，反馈给 Executor 重试（最多 2 次）
**And** 每个阶段的输入输出记录到 Event Log
**And** `holon_test.cpp` 验证三阶段协作流程和重试逻辑

### Story 3.3: 并行子任务执行

As a 系统,
I want 对同一任务的多个独立子步骤支持并行执行,
So that 无依赖关系的子任务同时进行，缩短总执行时间。

**Acceptance Criteria:**

**Given** Planner 将任务拆解为 N 个子步骤，其中 M 个子步骤标记为可并行（无依赖关系）
**When** Executor 开始执行
**Then** 可并行的子步骤通过线程池同时提交执行
**And** 有依赖关系的子步骤按依赖顺序串行执行
**And** 任一并行子步骤失败时，其他并行子步骤继续执行，失败子步骤标记错误
**And** 所有子步骤完成后汇总结果交给 Critic 审查
**And** `holon_test.cpp` 验证并行执行和依赖排序逻辑

## Epic 4: 系统控制与资源操作

管家能操作文件、执行命令、管理进程、网络请求——真正的"管家"能力，风险操作需用户确认。

### Story 4.1: 文件系统操作能力

As a 用户,
I want 管家读写本地文件系统,
So that 管家能帮我管理文件和目录，无需手动操作。

**Acceptance Criteria:**

**Given** Holon Executor 需要执行文件操作
**When** 调用 `FileOps` 能力模块
**Then** 支持文件读取、写入、删除、复制、移动操作
**And** 支持目录创建、删除、遍历操作
**And** 文件写入和删除操作分类为 L1（通知用户），目录删除分类为 L2（需确认）
**And** 所有操作记录到 Event Log（FR58），包含操作类型、路径、结果
**And** `file_ops_test.cpp` 验证各操作和风险分级

### Story 4.2: Shell 命令执行能力

As a 用户,
I want 管家执行操作系统 Shell 命令,
So that 管家能帮我完成运维操作和脚本执行。

**Acceptance Criteria:**

**Given** Holon Executor 需要执行 Shell 命令
**When** 调用 `ShellOps` 能力模块
**Then** 命令在沙盒子进程中执行（ulimit/cgroup 资源限制）
**And** 捕获 stdout 和 stderr 输出
**And** 支持执行超时设置（默认 60s）
**And** Shell 命令默认分类为 L2（需用户确认），白名单命令（如 ls、cat）分类为 L0
**And** 命令执行记录到 Event Log，包含命令内容、退出码、输出摘要
**And** `shell_ops_test.cpp` 和 `sandbox_test.cpp` 验证沙盒执行和超时机制

### Story 4.3: 进程管理能力

As a 用户,
I want 管家启动、停止和监控系统进程,
So that 管家能帮我管理服务的运行状态。

**Acceptance Criteria:**

**Given** Holon Executor 需要管理系统进程
**When** 调用 `ProcessOps` 能力模块
**Then** 支持启动进程（返回 PID）、停止进程（发送信号）、查询进程状态（CPU/内存占用）
**And** 进程启动分类为 L2（需确认），进程停止分类为 L3（二次确认）
**And** 进程状态查询分类为 L0（无需确认）
**And** 所有操作记录到 Event Log
**And** `process_ops_test.cpp` 验证进程管理操作和风险分级

### Story 4.4: 网络操作能力

As a 用户,
I want 管家发起 HTTP 请求和 API 调用,
So that 管家能帮我与外部服务交互、抓取网页内容。

**Acceptance Criteria:**

**Given** Holon Executor 需要进行网络操作
**When** 调用 `NetworkOps` 能力模块
**Then** 支持 HTTP GET/POST/PUT/DELETE 请求
**And** 支持自定义 Headers 和请求体
**And** 支持响应超时设置（默认 30s）
**And** 网络请求分类为 L1（通知用户），涉及认证信息的请求分类为 L2（需确认）
**And** 所有操作记录到 Event Log，请求体和响应体按需截断（≤1KB）
**And** `network_ops_test.cpp` 使用 mock HTTP server 验证请求和响应处理

### Story 4.5: 代码执行与定时任务能力

As a 用户,
I want 管家执行代码脚本和管理定时任务,
So that 管家能帮我运行脚本和配置自动化任务。

**Acceptance Criteria:**

**Given** Holon Executor 需要执行代码或管理定时任务
**When** 调用代码执行或定时任务能力
**Then** 代码执行在沙盒子进程中运行，支持 Python/Bash/Node 脚本
**And** 代码执行分类为 L2（需确认），编译执行分类为 L3（二次确认）
**And** 定时任务支持创建、修改、删除操作
**And** 定时任务创建/修改分类为 L2（需确认），删除分类为 L2
**And** 所有操作记录到 Event Log
**And** 相关测试验证沙盒执行和定时任务 CRUD

### Story 4.6: 风险分级与权限管理

As a 用户,
I want 系统资源操作按风险等级分级控制,
So that 高风险操作需要我明确确认，低风险操作自动执行不打扰。

**Acceptance Criteria:**

**Given** `config/risk_levels.yaml` 定义了操作到风险等级的映射
**When** 任意能力模块执行操作前
**Then** RiskClassifier 根据操作类型和参数判定风险等级（L0/L1/L2/L3）
**And** L0 操作直接执行，无需通知
**And** L1 操作执行后通知用户（通过 Discord 消息）
**And** L2 操作暂停执行，通过 Discord 请求用户确认，确认后继续
**And** L3 操作暂停执行，通过 Discord 请求用户二次确认（间隔 ≥5s）
**And** 用户拒绝时操作取消，记录到 Event Log
**And** `risk_classifier_test.cpp` 和 `permission_manager_test.cpp` 验证分级和确认流程


## Epic 5: 进化与工具化闭环

重复任务越来越快——系统自动识别模式、生成 Pattern、编译为插件、替换原有协作路径。对齐架构里程碑 M3。

### Story 5.1: Observer 协作轨迹监听

As a 系统,
I want Compiler 进程中的 Observer 持续监听事件日志中的任务执行轨迹,
So that 系统能发现重复出现的协作模式。

**Acceptance Criteria:**

**Given** evoclaw-compiler 进程运行中，订阅 engine 的 Pub-Sub 事件
**When** engine 完成一个 Holon 协作任务
**Then** Observer 从事件日志中提取完整的协作轨迹（任务类型、Agent 角色序列、子步骤列表、输入输出摘要）
**And** Observer 将轨迹规范化为标准格式（去除时间戳、具体参数，保留结构骨架）
**And** 规范化轨迹存储到 Pattern Registry 的候选池
**And** `observer_test.cpp` 验证轨迹提取、规范化和存储流程

### Story 5.2: 模式识别与 Pattern 生成

As a 系统,
I want Observer 识别重复出现的协作模式并生成 Pattern,
So that 被验证的协作路径能被抽象为可复用的模板。

**Acceptance Criteria:**

**Given** Pattern Registry 候选池中积累了多条规范化轨迹
**When** 同一模式（结构骨架匹配）出现 ≥3 次且全部成功执行（FR17）
**Then** Observer 调用 LLM 将匹配的轨迹合并为一个 Pattern（已验证的原语序列 + 模块调用模板）（FR69）
**And** Pattern 包含：模式名称、触发条件、步骤模板、参数化占位符、来源轨迹 ID 列表
**And** Pattern 状态标记为 `candidate`，等待 Compiler 决定是否编译
**And** `pattern_registry_test.cpp` 验证模式匹配和 Pattern 生成逻辑

### Story 5.3: Compiler 编译 Pattern 为插件

As a 系统,
I want Compiler 将 Pattern 编译为独立插件,
So that 重复任务由轻量插件直接执行，不再需要 Holons 协作。

**Acceptance Criteria:**

**Given** 一个 Pattern 状态为 `candidate`
**When** Compiler 开始编译
**Then** Compiler 调用 LLM 将 Pattern 的步骤模板编译为确定性执行脚本（FR18）
**And** 编译产物包含：接口定义、执行逻辑、参数 schema、来源 Pattern ID
**And** 编译产物的预期延迟和 token 消耗低于原协作路径（FR18）
**And** 编译产物状态标记为 `draft`，进入沙盒测试流程
**And** `compiler_test.cpp` 验证编译流程和产物格式

### Story 5.4: 沙盒测试与用户确认

As a 系统,
I want 编译产物在注册前经过沙盒测试和用户确认,
So that 只有质量合格的插件才能进入生产环境。

**Acceptance Criteria:**

**Given** 一个编译产物状态为 `draft`
**When** 进入沙盒测试流程
**Then** 系统使用来源轨迹的历史输入数据在沙盒环境中回放测试（FR19）
**And** 测试结果与原 Holons 协作输出对比，相似度 ≥80% 视为通过
**And** 沙盒测试通过后，通过 Discord 向用户展示编译产物摘要并请求确认（FR20）
**And** 用户确认后产物状态变为 `active`，注册到 Router 插件列表
**And** 用户拒绝后产物状态变为 `rejected`，记录拒绝原因
**And** 全流程记录到 Event Log（提案→测试→确认/拒绝→注册）
**And** `evolver_test.cpp` 验证沙盒测试和确认流程

### Story 5.5: 插件替换与 M3 闭环验证

As a 用户,
I want 编译后的插件替换原有 Holons 协作路径，同类任务明显加速,
So that Toolization Loop 完整闭环，系统越用越快。

**Acceptance Criteria:**

**Given** 一个插件已注册到 Router 插件列表（状态 `active`）
**When** 用户发出同类任务
**Then** Router 匹配到插件并直接执行，不触发 Holons 组队（FR21）
**And** 插件执行延迟明显低于原 Holons 协作路径
**And** 执行结果记录到 Event Log，标记为插件执行（区别于 Holons 执行）
**And** M3 验收：重复同类任务 ≥3 次 → Compiler 识别模式 → 生成产物 → Router 调用插件，完整闭环至少跑通一次
**And** `engine_compiler_test.cpp` 集成测试验证端到端闭环

### Story 5.6: Toolization Loop 9 步治理流程

As a 系统,
I want Toolization Loop 遵循 9 步治理流程,
So that 进化过程规范可控，每步有明确的门控和审计记录。

**Acceptance Criteria:**

**Given** 一个协作任务完成
**When** 进入 Toolization Loop 流程
**Then** 9 步依次执行（FR70）：涌现（Holons 协作完成）→ 轨迹规范化（Observer 提取）→ 模式提取（Pattern 生成）→ 契约合成（编译产物生成）→ 暂存（draft 状态）→ 治理门（沙盒测试 + 用户确认）→ 灰度发布（观察期 ≥10 次调用）→ 晋升或回滚（失败率 >20% 回滚）→ Holon 释放（同类任务不再触发组队）
**And** 每步状态变更记录到 Event Log，支持审计回放
**And** 灰度发布期间新版本失败率 >20% 时自动回滚到 Holons 路径
**And** `evolver_test.cpp` 验证 9 步状态机和回滚逻辑


## Epic 6: 质量反馈与经验学习

告诉管家"不行"，它会记住失败上下文并学会一次做好，成功路径固化为经验模板。

### Story 6.1: 任务完成反馈征询

As a 用户,
I want 任务完成后管家主动问我满不满意,
So that 系统能收集真实反馈，持续改进输出质量。

**Acceptance Criteria:**

**Given** 一个任务执行完成并返回结果给用户
**When** 结果通过 Discord 发送后
**Then** 管家附带反馈请求（thumbs up/down 表情反应 + 可选文字说明）（FR64）
**And** 用户 30 分钟内未反馈视为满意（默认正面）
**And** 反馈结果记录到 Event Log（事件类型 `task.feedback.positive` 或 `task.feedback.negative`）
**And** `facade_test.cpp` 验证反馈征询消息发送和超时默认逻辑

### Story 6.2: 失败上下文记录

As a 系统,
I want 用户反馈不满意时完整记录失败上下文,
So that 系统有足够信息在下次做得更好。

**Acceptance Criteria:**

**Given** 用户对任务结果反馈不满意（thumbs down 或文字说明包含负面信号）
**When** 系统收到负面反馈
**Then** 系统记录完整失败上下文到 Event Log（FR65）：原始任务描述、执行路径摘要（Agent 角色序列 + 子步骤）、最终输出结果、用户具体不满点（文字说明）
**And** 失败上下文同时写入 `~/.evoclaw/data/memory/experience_templates/` 目录
**And** 失败记录关联任务 ID，支持后续查询
**And** 相关测试验证失败上下文的完整性和存储

### Story 6.3: 基于失败历史的智能重试

As a 用户,
I want 管家基于失败历史自动改进并重试任务,
So that 同样的错误不会犯第二次。

**Acceptance Criteria:**

**Given** 系统记录了一个任务的失败上下文
**When** 用户选择重试（或系统自动触发重试）
**Then** 系统将历次失败上下文（失败原因、用户不满点）作为约束注入 LLM prompt（FR66）
**And** 重试时 Planner 的 prompt 包含"避免以下问题：{失败历史摘要}"
**And** 重试结果再次征询用户反馈
**And** 最多自动重试 2 次，仍不满意则告知用户并记录
**And** 相关测试验证失败约束注入和重试流程

### Story 6.4: 经验模板固化与加载

As a 系统,
I want 重试成功后将改进路径固化为经验模板，同类任务自动加载,
So that 系统从失败中学习，提升首次完成率。

**Acceptance Criteria:**

**Given** 一个任务经过重试后成功（用户反馈满意）
**When** 重试成功确认
**Then** 系统将改进路径固化为经验模板（FR67）：`{failure_pattern, correction_strategy, success_path}`
**And** 经验模板以 YAML 文件存储到 `experience_templates/` 目录，按任务类别索引
**And** 同类任务再次出现时，Router 在路由前查询匹配的经验模板（FR68）
**And** 匹配到的经验模板作为执行约束注入 Holon 的 Planner prompt
**And** 经验模板被成功应用 ≥3 次后，标记为 Compiler 编译候选
**And** 相关测试验证模板固化、匹配和加载流程


## Epic 7: 智能通信协议

Agent 间通信高效——形式约束自动验证，消息按 S/M/L 梯度压缩，token 消耗降低。

### Story 7.1: 混合协议消息格式

As a 开发者,
I want Agent 间消息支持形式约束和语义约束的混合格式,
So that 可量化的约束自动验证，不可量化的语义保留 LLM 评估。

**Acceptance Criteria:**

**Given** 消息协议定义了 `constraints` 字段，包含 `formal` 和 `semantic` 两个子字段
**When** 创建一条 Agent 间消息
**Then** `formal` 字段包含可机器验证的约束（数值范围、类型检查、布尔条件）（FR34）
**And** `semantic` 字段包含需要 LLM 评估的语义约束（质量要求、风格要求）
**And** 消息格式向后兼容——无约束的消息等同于纯语义消息
**And** `serializer_test.cpp` 验证混合格式的序列化和反序列化

### Story 7.2: 规则引擎形式约束验证

As a 系统,
I want 形式约束由规则引擎直接验证，不消耗 LLM context,
So that 可量化的约束验证快速且零 token 成本。

**Acceptance Criteria:**

**Given** 一条消息包含 `formal` 约束（如 `{"max_tokens": 500, "output_format": "json"}`）
**When** 小脑调用 RuleEngine 验证
**Then** BasicRuleEngine 在本地验证所有形式约束（FR35），支持数值比较、范围检查、类型匹配、布尔逻辑
**And** 单条约束验证延迟 ≤100ms（NFR3）
**And** 验证结果（通过/失败 + 失败原因）附加到消息元数据
**And** RuleEngine 为纯虚基类，Phase 2 可替换为更强的验证引擎
**And** `basic_rule_engine_test.cpp` 验证各类约束验证场景

### Story 7.3: 语义约束 LLM 评估

As a 系统,
I want 语义约束由 LLM 按需评估,
So that 不可量化的质量要求也能被检查。

**Acceptance Criteria:**

**Given** 一条消息包含 `semantic` 约束（如 `{"quality": "专业且简洁", "tone": "正式"}`）
**When** 小脑判定需要语义评估（L2 级消息）
**Then** 小脑将语义约束和消息内容组合为评估 prompt，调用 LLM 评估（FR36）
**And** LLM 返回评估结果（通过/不通过 + 改进建议）
**And** L0/L1 级消息跳过语义评估，直接放行
**And** 语义评估支持批量化——攒够 N 条后批量调 LLM
**And** `cerebellum_test.cpp` 验证语义评估触发条件和批量化逻辑

### Story 7.4: 小脑消息预处理管线

As a 系统,
I want 小脑自动处理消息打包、验证和冲突检测,
So that LLM 只看精炼摘要，context 窗口利用率最大化。

**Acceptance Criteria:**

**Given** MessageBus 收到一条原始消息
**When** 消息进入小脑预处理管线
**Then** `Cerebellum::Pack()` 根据消息级别压缩 context（FR37）
**And** `Cerebellum::Validate()` 对 L2 消息调用 RuleEngine 验证形式约束，L0/L1 跳过
**And** `Cerebellum::DetectConflict()` 检测消息约束与已有约束的冲突
**And** 处理后的消息只包含精炼摘要，原始 context 存储在 Event Log 可追溯
**And** 异步模式：L0/L1 消息不阻塞分发，L2 验证失败后追溯处理
**And** `cerebellum_test.cpp` 验证完整管线流程

### Story 7.5: SLP 梯度压缩

As a 系统,
I want 消息按 S/M/L 三个级别梯度压缩,
So that 不同重要程度的消息使用不同的 context 预算。

**Acceptance Criteria:**

**Given** 一条消息需要压缩
**When** 小脑根据消息风险等级选择压缩级别
**Then** S 级（≤20 字符）：只保留意图标签 + 置信度（FR74）
**And** M 级（≤150 字符）：保留背景摘要 + 证据引用
**And** L 级（无限制）：保留完整上下文
**And** 小脑根据消息的风险等级和约束复杂度自动选择压缩级别
**And** 压缩前的完整消息存储在 Event Log，压缩后的摘要用于 Agent 间传递
**And** `cerebellum_test.cpp` 验证三级压缩和自动选择逻辑

### Story 7.6: 约束模板积累与复用

As a 系统,
I want 约束模板积累复用，降低重复消息的 token 消耗,
So that 通信效率随使用自动提升。

**Acceptance Criteria:**

**Given** 系统处理过多条同类消息的约束
**When** 小脑检测到约束模式重复出现（≥3 次相同结构）
**Then** 小脑将该约束模式提取为模板，存储到约束模板库（FR38）
**And** 后续同类消息直接引用模板 ID，不重复传递完整约束
**And** 复用模板的消息 token 消耗低于首次同类消息
**And** 模板库支持版本管理，模板更新时旧版本保留
**And** `cerebellum_test.cpp` 验证模板提取、复用和 token 消耗对比


## Epic 8: 系统仪表盘与运维

通过 Web 面板和结构化指令查看系统状态、进化日志、Agent 健康，一键启停，热替换，自动重启。

### Story 8.1: Dashboard REST API 与基础面板

As a 管理者,
I want 通过 Web 面板查看系统运行状态,
So that 我能实时了解系统健康状况。

**Acceptance Criteria:**

**Given** evoclaw-dashboard 进程运行中，订阅 engine/compiler 的 Pub-Sub 事件
**When** 管理者访问 Web 面板（`http://localhost:{port}`）
**Then** 面板展示系统运行状态：各进程存活状态、CPU/内存占用、运行时长（FR40）
**And** REST API 提供 `/api/status` 端点返回 JSON 格式系统状态
**And** 面板使用 SSE（Server-Sent Events）实时推送状态更新
**And** `rest_api_test.cpp` 和 `sse_handler_test.cpp` 验证 API 和推送逻辑

### Story 8.2: 进化日志与编译历史面板

As a 管理者,
I want 通过 Web 面板查看进化日志和编译历史,
So that 我能追踪系统的进化过程和编译产物状态。

**Acceptance Criteria:**

**Given** Compiler 已产出编译产物（Pattern 和/或插件）
**When** 管理者查看进化日志页面
**Then** 面板展示编译历史列表：时间、模式名称、产物类型、状态（draft/active/rejected/回滚）（FR41）
**And** 点击单条记录展示详情：来源轨迹、编译参数、沙盒测试结果、用户确认记录
**And** REST API 提供 `/api/evolution/history` 端点
**And** 数据从 Event Log 文件系统读取（不走 IPC）

### Story 8.3: Agent 健康状态面板

As a 管理者,
I want 通过 Web 面板查看 Agent 健康状态和资源消耗,
So that 我能发现异常 Agent 并及时处理。

**Acceptance Criteria:**

**Given** 各进程定期发送心跳事件到 Pub-Sub
**When** 管理者查看 Agent 健康页面
**Then** 面板展示每个 Agent/进程的健康状态：存活、最后心跳时间、token 消耗累计、任务成功率（FR42）
**And** 心跳超时（>30s 未收到）的 Agent 标记为异常（红色）
**And** REST API 提供 `/api/agents/health` 端点
**And** `rest_api_test.cpp` 验证健康数据聚合和超时检测

### Story 8.4: 结构化指令实现

As a 用户,
I want 通过 Discord 结构化指令快速查询系统状态,
So that 不需要打开 Web 面板也能了解系统情况。

**Acceptance Criteria:**

**Given** Discord Bot 已注册 Slash Commands
**When** 用户输入 `/status`
**Then** 返回系统运行状态摘要（各进程存活、运行时长）（FR2）
**When** 用户输入 `/health`
**Then** 返回 Agent 健康状态摘要（异常 Agent 高亮）（FR2）
**When** 用户输入 `/plugins`
**Then** 返回已注册插件列表（名称、状态、调用次数）（FR3）
**When** 用户输入 `/evolution`
**Then** 返回最近 5 条进化事件摘要（FR4）
**And** `discord_bot_test.cpp` 验证各指令的响应格式

### Story 8.5: 组件热替换

As a 开发者,
I want 在不停机的情况下替换或升级任意组件,
So that 系统持续可用，升级无感知。

**Acceptance Criteria:**

**Given** 一个新版本组件（如新版本插件或 Agent 模块）准备上线
**When** 通过 `plugin.register` 消息注册新版本
**Then** Router 将新版本标记为 `current`，旧版本标记为 `old`（FR14）
**And** 新请求路由到 `current` 版本
**And** 进行中的旧请求继续使用 `old` 版本
**And** 旧版本所有进行中请求完成后（或超时 30s），卸载 `old` 版本
**And** 替换期间请求成功率 = 100%（NFR10）
**And** 观察期内新版本失败率 >20% 时自动回滚
**And** `router_test.cpp` 验证双版本共存和回滚逻辑

### Story 8.6: 进程管理脚本与 Watchdog

As a 开发者,
I want 一键启停所有进程，崩溃自动重启,
So that 运维操作简单可靠。

**Acceptance Criteria:**

**Given** `scripts/evoclaw.sh` 和 `scripts/watchdog.sh` 已实现
**When** 执行 `evoclaw.sh start`
**Then** 按序启动 engine → compiler → dashboard，PID 文件写入 `~/.evoclaw/`
**When** 执行 `evoclaw.sh stop`
**Then** 按逆序停止所有进程，清理 PID 文件
**When** 任一进程崩溃
**Then** watchdog 在 ≤3s 内检测到并自动重启（NFR7）
**And** 崩溃事件记录到 Event Log
**And** 连续崩溃 ≥3 次时 watchdog 停止重启并告警


## Epic 9: 深度记忆与主动智能

管家能预判风险、主动提醒、从历史中学习，面板增强配置管理和熵指标可视化。

### Story 9.1: 情景记忆存储与检索

As a 系统,
I want 存储和检索历史任务记录（情景记忆）,
So that 系统能从历史经验中学习和预判。

**Acceptance Criteria:**

**Given** 一个任务执行完成
**When** 任务结果和执行轨迹写入 Event Log 后
**Then** 系统将任务摘要（任务类型、输入摘要、输出摘要、执行路径、结果评价）提取为情景记忆（FR30）
**And** 情景记忆存储到 `~/.evoclaw/data/memory/episodic/` 目录，按日期组织
**And** 支持按时间范围、任务类型、关键词过滤检索
**And** 检索延迟 ≤200ms（NFR4），超时返回空结果
**And** 情景记忆 90 天后自动归档到冷存储
**And** `fs_memory_store_test.cpp` 验证情景记忆的存储、检索和归档

### Story 9.2: 语义记忆提炼

As a 系统,
I want 从历史任务中提炼知识并形成语义记忆,
So that 系统积累的知识能跨任务复用。

**Acceptance Criteria:**

**Given** 情景记忆中积累了 ≥10 条同类任务记录
**When** 小脑定期蒸馏（每周一次）
**Then** 系统调用 LLM 从同类情景记忆中提炼通用知识（FR31）：常见模式、最佳实践、常见陷阱
**And** 提炼结果存储为语义记忆（`~/.evoclaw/data/memory/semantic/`）
**And** 语义记忆永久存储，支持按主题检索
**And** 语义记忆查询延迟 ≤300ms（NFR5）
**And** 相关测试验证蒸馏触发条件和语义记忆格式

### Story 9.3: 风险预判与主动提醒

As a 用户,
I want 管家基于历史经验预判风险并主动提醒,
So that 潜在问题在发生前就被预警。

**Acceptance Criteria:**

**Given** 用户发出一个新任务
**When** Router 路由任务前
**Then** 系统查询情景记忆中同类任务的历史记录（FR32）
**And** 如果历史记录中存在失败案例（失败率 >30%），主动向用户预警风险和建议
**And** 预警以建议形式呈现（非阻塞），用户可忽略
**And** 预警的风险中 ≥50% 确实发生或被用户确认有价值
**And** 管家在检测到重复行为模式（≥3 次同类操作）时主动发起建议（FR8）
**And** 主动提醒通过 Discord Bot 发送，不打断当前任务流
**And** 相关测试验证风险预判触发条件和提醒发送

### Story 9.4: 挑战集维护

As a 系统,
I want 维护失败和边界案例的挑战集,
So that 进化过程有高质量的测试数据源。

**Acceptance Criteria:**

**Given** 系统运行过程中产生各类事件
**When** 以下 4 类事件发生时
**Then** 自动收录到挑战集（FR33）：
1. 失败/回滚任务——执行失败或插件被回滚的完整上下文
2. 低置信成功——Critic 评分在及格线附近（侥幸通过）的任务
3. 用户标记不满——用户反馈负面的任务
4. Curator 合成挑战——系统基于已有挑战自动生成的变体测试用例
**And** 挑战集存储到 `~/.evoclaw/data/memory/challenge_set/` 目录
**And** 挑战集用于编译产物的三集评估（FR73）：Replay Set + Failure Set + Challenge Set
**And** 相关测试验证 4 类来源的收录逻辑

### Story 9.5: 三集评估验证

As a 系统,
I want 编译产物通过三集评估验证,
So that 插件质量经过多维度检验。

**Acceptance Criteria:**

**Given** 一个编译产物进入验证阶段
**When** 执行三集评估
**Then** Replay Set 评估：使用历史成功任务回放，产物输出与原输出对比（FR73）
**And** Failure Set 评估：使用已知失败案例，验证产物不会重蹈覆辙
**And** Challenge Set 评估：使用边界案例，验证产物的鲁棒性
**And** 三集评估全部通过后产物才能进入灰度发布
**And** 评估结果记录到 Event Log，包含每集的通过率和失败详情
**And** 相关测试验证三集评估流程

### Story 9.6: Adapter 协议演进

As a 系统,
I want 新旧消息 schema 通过 adapter 共存,
So that 协议升级不需要迁移历史数据。

**Acceptance Criteria:**

**Given** 消息协议需要升级（新增字段或修改结构）
**When** 新版本 schema 部署后
**Then** 新消息使用新 schema，旧消息通过 adapter 按需转换为新格式读取（FR75）
**And** Event Log 永不迁移——旧事件保持原始格式存储
**And** adapter 注册表维护 schema 版本到转换函数的映射
**And** 读取旧事件时自动检测版本并应用对应 adapter
**And** 相关测试验证跨版本读取和 adapter 转换

### Story 9.7: Web 面板增强——熵指标与配置管理

As a 管理者,
I want 通过 Web 面板查看熵指标趋势和管理系统配置,
So that 我能监控系统复杂度并调整运行参数。

**Acceptance Criteria:**

**Given** 系统运行中持续收集熵相关数据
**When** 管理者访问面板增强页面
**Then** 展示熵指标趋势图（FR43）：原语密度、模块密度、平均依赖深度、平均任务轮次、探索积压
**And** 支持系统配置在线修改（FR44）：LLM 参数、编译阈值、风险等级映射
**And** 面板默认展示用户最近 30 天查询频率最高的 3 个指标（FR45）
**And** 配置修改记录到 Event Log，支持回滚
**And** REST API 提供 `/api/entropy/trends` 和 `/api/config` 端点


## Epic 10: 双评估架构

进化质量有保障——Operational Critic 可进化 + Meta Judge 冻结标准零上下文共享，探索区晋升机制，渐进式形式化。

### Story 10.1: Operational Critic 质量评估

As a 系统,
I want Operational Critic 对任务输出进行质量评估,
So that 低质量输出在到达用户前被拦截。

**Acceptance Criteria:**

**Given** 一个 Holon 或插件完成任务输出
**When** 输出进入 Critic 评估环节
**Then** Operational Critic 调用 LLM 对输出进行多维度评估（FR22）：完整性、准确性、格式合规、用户偏好匹配
**And** 评估结果为数值评分（0-100）+ 维度明细 + 改进建议
**And** 评分低于阈值（默认 60 分）时触发重试或降级
**And** Critic 的评估标准可进化——随系统成长调整评分权重和阈值
**And** 评估结果记录到 Event Log
**And** 相关测试验证评估流程和阈值触发逻辑

### Story 10.2: Meta Judge 独立盲测

As a 系统,
I want Meta Judge 独立于 Operational Critic 对进化结果进行盲测,
So that 进化质量有独立的第二道防线。

**Acceptance Criteria:**

**Given** 一个编译产物通过沙盒测试准备上线
**When** 进入 Meta Judge 盲测环节
**Then** Meta Judge 在独立进程（evoclaw-judge）中运行，与 Critic 零上下文共享（FR23）
**And** Meta Judge 使用独立的 LLM session（不共享 prompt 历史）
**And** Judge 标准冻结——不随系统进化调整，作为固定基准线
**And** 盲测方式：给 Judge 编译产物的输入输出，不告知是插件还是 Holons 产出
**And** Judge 评分与 Critic 评分独立记录，两者差异 >30 分时触发人工审查
**And** 相关测试验证物理隔离和盲测流程

### Story 10.3: 探索区晋升机制

As a 系统,
I want 探索区模块经过充分验证后自动晋升到稳定区,
So that 新能力经过考验后才面向用户。

**Acceptance Criteria:**

**Given** 一个新模块（Agent 或插件）注册到探索区
**When** 模块在探索区累计完成 ≥20 次任务验证
**Then** 系统自动触发晋升评估（FR26）
**And** 晋升条件：任务成功率 ≥90% + 契约校验通过 + Meta Judge 盲测通过
**And** 全部条件满足时模块自动晋升到稳定区，Router 正式路由
**And** 任一条件不满足时模块保留在探索区，继续积累验证
**And** 探索区模块不直接面向用户——只在内部测试任务中使用
**And** 晋升/保留决策记录到 Event Log
**And** 相关测试验证晋升条件检查和状态转换

### Story 10.4: 渐进式形式化升级

As a 系统,
I want 约束支持从纯语义逐步升级到完整形式验证,
So that 通信协议的严谨性随使用自动提升。

**Acceptance Criteria:**

**Given** 两个 Agent 之间建立通信
**When** 通信历史积累
**Then** 约束从纯语义评估（Level 0）逐步升级（FR39）：
- Level 0：纯语义约束，LLM 评估
- Level 1：基础形式约束（数值范围、类型检查）+ 语义约束
- Level 2：完整形式约束（布尔逻辑、依赖关系）+ 语义约束
- Level 3：高级形式验证（不变量检查、状态机验证）
- Level 4：完整形式化（所有约束均可机器验证）
**And** 升级过程双向协商——两个 Agent 都同意后才升级
**And** 升级触发条件：同类约束模板积累 ≥10 个且验证通过率 100%
**And** 随时可降级——验证引擎超时或失败时自动降级到上一级
**And** 相关测试验证升级/降级状态机


## Epic 11: 反熵自治与进化治理

系统自动瘦身——5 个熵指标监控、进化预算制度、插件生命周期、宪法审查、Maintainer 替换隔离。

### Story 11.1: 熵指标监控与自动压缩

As a 系统,
I want 监控 5 个熵指标并在超阈值时自动触发压缩,
So that 系统复杂度保持可控，不会无限膨胀。

**Acceptance Criteria:**

**Given** 系统运行中持续收集组织状态数据
**When** Entropy Monitor 定期计算（每小时）
**Then** 监控 5 个熵指标（FR24）：原语密度（原语数/活跃模块数）、模块密度（模块总数/活跃用户数）、平均依赖深度（模块间依赖链平均长度）、平均任务轮次（任务完成平均交互轮数）、探索积压（探索区待晋升模块数）
**And** 每个指标有独立的警戒阈值和危险阈值
**And** 警戒阈值触发告警（通知管理者）
**And** 危险阈值触发自动压缩：合并功能重叠的模块、退役低使用率模块、压缩冗余原语
**And** 压缩操作需用户确认后执行
**And** 熵指标历史趋势存储，支持 Web 面板可视化
**And** 相关测试验证指标计算和阈值触发逻辑

### Story 11.2: 插件生命周期管理

As a 系统,
I want 插件具有完整的生命周期管理,
So that 插件从创建到退役全程可控可追溯。

**Acceptance Criteria:**

**Given** 一个编译产物通过验证
**When** 插件进入生命周期管理
**Then** 插件状态按 Draft → Active → Deprecated → Retired 流转（FR25）
**And** Draft：编译产物待验证
**And** Active：通过验证，Router 正式路由
**And** Deprecated：使用率持续下降（30 天使用率 <5%）或被新版本替代，标记为待退役
**And** Retired：退役归档，Router 不再路由，数据保留可恢复
**And** 状态转换记录到 Event Log，每次转换包含原因说明
**And** Plugin Tool 进入稳定区必须包含 8 个必填字段（FR72）：接口定义、权限声明、成本估算、证据链、失败树、回滚方案、版本号、爆炸半径
**And** 相关测试验证状态机和必填字段校验

### Story 11.3: 进化预算制度

As a 系统,
I want 每个进化周期有资源配额上限,
So that 进化速度受控，不会因过度进化导致系统不稳定。

**Acceptance Criteria:**

**Given** 系统配置了进化预算参数
**When** 一个进化周期（默认 7 天）开始
**Then** 系统执行预算约束（FR71）：每周期插件新增上限（默认 3 个）、原语新增上限（默认 10 个）、探索资源占比上限（默认 15%）
**And** 接近预算上限（>80%）时发出预警
**And** 超额时触发强制熵缩减——暂停新编译，优先处理探索区积压
**And** 预算参数可通过配置调整
**And** 预算使用情况记录到 Event Log，支持 Web 面板查看
**And** 相关测试验证预算计算和超额触发逻辑

### Story 11.4: 宪法审查与变更门控

As a 用户,
I want 宪法变更需要我明确确认并设置冷静期,
So that 系统核心规则不会被意外修改。

**Acceptance Criteria:**

**Given** 系统检测到需要修改宪法（`config/constitution.yaml`）的进化提案
**When** 提案进入宪法变更流程
**Then** 系统暂停提案执行，通过 Discord 向用户展示变更内容和影响分析（FR50）
**And** 用户确认后进入冷静期（≥24h），冷静期内可撤回
**And** 冷静期结束后变更自动生效
**And** 变更全过程记录到 Event Log（提案→用户确认→冷静期开始→生效/撤回）
**And** 相关测试验证冷静期计时和撤回逻辑

### Story 11.5: Maintainer 替换隔离

As a 系统,
I want Maintainer（Observer + Evolver）替换需用户显式触发且有 72h 冷静期,
So that 进化系统的核心组件不会被自动替换，防止自指悖论。

**Acceptance Criteria:**

**Given** 系统检测到 Maintainer 组件需要替换或升级
**When** 替换请求产生
**Then** 系统拒绝自动替换——Maintainer 不可自动替换（FR76）
**And** 只有用户通过 Discord 显式触发替换命令时才启动流程
**And** 用户确认后进入 72h 冷静期，期间可撤回
**And** 冷静期内旧版本继续运行，新版本在探索区并行验证
**And** 冷静期结束后执行替换，旧版本归档
**And** 替换全过程记录到 Event Log
**And** 相关测试验证自动替换拒绝和冷静期逻辑


## Epic 12: 开放平台与扩展

外部开发者可贡献模块，标准接口零摩擦接入，向量数据库和事件存储引擎集成。

### Story 12.1: 标准接口契约规范

As a 开发者,
I want 标准接口契约规范文档和开发者 SDK,
So that 我能按规范开发新模块并零摩擦接入系统。

**Acceptance Criteria:**

**Given** 系统定义了标准接口契约
**When** 外部开发者查阅文档
**Then** 契约规范包含：capability_profile（能力声明）、airbag_spec（安全规格）、health_probe（健康检查）、message_schema（消息格式）（FR63）
**And** 提供开发者 SDK（C++ 头文件 + 示例代码），实现契约接口即可接入
**And** 新 Agent 满足契约后接入系统，无需修改 Router 或其他组件（FR15）
**And** SDK 包含契约校验工具——开发者本地验证模块是否满足契约
**And** 文档和 SDK 存放在 `docs/` 目录

### Story 12.2: 新模块零摩擦接入

As a 开发者,
I want 新 Agent 满足标准接口后即可接入系统,
So that 系统能力可以通过社区贡献持续扩展。

**Acceptance Criteria:**

**Given** 一个外部开发者按契约规范实现了新 Agent 模块
**When** 模块提交接入
**Then** Contract Validator 自动校验模块是否满足所有契约接口（FR15）
**And** 校验通过后模块自动注册到探索区
**And** 模块在探索区经过 ≥20 次验证后可晋升稳定区（FR26）
**And** 整个接入过程无需修改 Router 或其他现有组件（NFR16）
**And** 新插件注册不需要重启 Router（NFR17）
**And** 相关测试验证契约校验和自动注册流程

### Story 12.3: 向量数据库集成

As a 系统,
I want 通过标准接口集成向量数据库,
So that 记忆检索从文件系统升级为语义向量检索，提升检索质量。

**Acceptance Criteria:**

**Given** MemoryStore 抽象接口已定义
**When** 实现向量数据库后端
**Then** 新增 `VectorMemoryStore` 实现类，通过标准向量检索接口集成（FR61）
**And** 支持语义相似度检索（embedding + cosine similarity）
**And** 替换文件系统后端后，所有功能测试通过率 = 100%（NFR18）
**And** 检索延迟满足 NFR4（≤200ms）和 NFR5（≤300ms）
**And** 相关测试验证向量检索和后端替换

### Story 12.4: 事件存储引擎集成

As a 系统,
I want 通过标准接口集成专业事件存储引擎,
So that 事件日志支持全文检索和时序查询，提升审计和分析能力。

**Acceptance Criteria:**

**Given** EventStore 抽象接口已定义
**When** 实现事件存储引擎后端
**Then** 新增专业存储实现类，支持事件写入、全文检索、时序查询（FR62）
**And** 替换 JSON Lines 后端后，所有功能测试通过率 = 100%（NFR18）
**And** 事件写入不阻塞主流程（NFR6）
**And** Event Log 历史数据通过 Adapter 按需迁移（FR75）
**And** 相关测试验证全文检索、时序查询和后端替换