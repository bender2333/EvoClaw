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
FR39: 约束支持渐进式形式化（FL-0 到 FL-4）（Phase 2）
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
**An* `router_test.cpp` 验证插件匹配命中/未命中两条路径

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
