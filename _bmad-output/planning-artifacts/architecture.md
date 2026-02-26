---
stepsCompleted: [1, 2, 3, 4, 5, 6, 7, 8]
workflowType: 'architecture'
lastStep: 8
status: 'complete'
completedAt: '2026-02-26'
inputDocuments:
  - _bmad-output/planning-artifacts/prd.md
  - doc_design/evoclaw_v1.md
  - doc_design/evoclaw_v2.md
  - doc_design/evoclaw_v3.md
  - doc_design/evoclaw_v4.md
  - doc_design/EvoClaw_Unified_Design.md
  - doc_design/gptv5.md
  - doc_design/grokV4.md
  - doc_design/a2a/cortex-architecture.md
  - doc_design/a2a/nltc-protocol-spec.md
  - doc_design/a2a/nltc-agent-integration.md
  - doc_design/a2a/integration-plan.md
  - doc_design/claude_QA.md
  - doc_design/grok_QA.md
  - doc_design/苏格拉底圆桌.md
workflowType: 'architecture'
project_name: 'EvoClaw'
user_name: 'bender'
date: '2026-02-26'
---

# Architecture Decision Document

_This document builds collaboratively through step-by-step discovery. Sections are appended as we work through each architectural decision together._

## Project Context Analysis

### Requirements Overview

**Functional Requirements（63 条，9 个子域）：**

| 子域 | FR 数 | 架构含义 |
|------|-------|---------|
| 任务交互（FR1-8） | 8 | Facade 层：统一人格、双通道（Discord + Web）、进度反馈、主动提醒 → 需要 Facade 组件屏蔽内部复杂度 |
| 组织协作（FR9-16） | 8 | 执行层：Fast Router 分流、Dynamic Holons 动态组队、进程隔离、热替换、并行执行 → 需要进程级隔离 + 消息总线 |
| 进化编译（FR17-26） | 10 | 进化层：Compiler 模式识别（≥3 次）、沙盒测试、双评估（Critic + Meta Judge 物理隔离）、探索区晋升（≥20 次验证）→ 需要独立的进化子系统 + 双区隔离 |
| 记忆（FR27-33） | 7 | 记忆层：工作记忆、用户模型、情景/语义记忆、风险预判（≥50% 准确率）、挑战集 → 需要多层存储 + 小脑自动管理 |
| 通信协议（FR34-39） | 6 | 横切层：混合协议（形式+语义约束）、小脑自动打包/验证/冲突检测、渐进式形式化 FL-0→FL-4 → 需要 Cortex 通信子系统 |
| 监控治理（FR40-45） | 6 | Governance + Facade：Web 面板、进化日志、Agent 健康、熵指标、个性化布局 → 需要可观测性基础设施 |
| 安全审计（FR46-50） | 5 | 横切层：不可变事件日志、决策链复现、隐私保护、token 预算、宪法冷静期 ≥24h → 需要 append-only 事件存储 |
| 系统控制（FR51-58） | 8 | 执行层扩展：文件/Shell/进程/网络/代码/cron 操作、风险分级 L0-L3、操作审计 → 需要沙盒化的系统操作代理 |
| 集成接口（FR59-63） | 5 | 横切层：Discord Bot、消息中间件、向量库、事件存储、开发者 SDK → 需要抽象接口层 |

**Non-Functional Requirements（24 条，架构驱动项）：**

| 类别 | 关键指标 | 架构影响 |
|------|---------|---------|
| 性能 | Router ≤500ms, 消息 ≤50ms, 约束验证 ≤100ms | 进程内通信优先，形式约束本地求解 |
| 可靠性 | 消息 ≥99.99%, 崩溃恢复 ≤5s, 零停机热替换 | 进程隔离 + 监督树 + 消息持久化 |
| 安全性 | L2+ 操作 100% 确认, 沙盒执行, 密钥不明文 | 权限门控 + 沙盒环境 + 密钥管理 |
| 可扩展性 | 新 Agent 零修改接入, 存储/通信层可替换 | 标准接口契约 + 抽象层 |
| 可维护性 | 独立部署, 100% 决策链可复现, 统一 JSON 日志 | 微服务式组件 + 结构化日志 |
| 集成性 | ≥2 家 LLM 切换 | LLM 客户端抽象 |
<!-- PLACEHOLDER_REMAINING -->

### Scale & Complexity

- **Primary domain:** Backend AI Multi-Agent System
- **Complexity level:** Enterprise（七层架构 + 双区隔离 + 自进化引擎 + 仿脑通信）
- **Estimated architectural components:** ~15 个核心组件（4 大组件 + Cortex 通信栈 + Governance + 记忆层 + Safety Airbag + 可观测性）
- **进程模型:** 每个 Agent/组件为独立进程，通过消息中间件通信

### Technical Constraints & Dependencies

| 约束 | 来源 | 影响 |
|------|------|------|
| C++ 后端核心 | 用户决策 | 高性能但开发周期较长，需要成熟的 IPC 方案 |
| 形式约束求解 ≤100ms | NFR3 | Z3/SMT 求解器集成，变量上限 100，约束上限 50 |
| Context 预算 ≤200 tokens 进出 LLM | Cortex 设计 | 小脑必须极度压缩信息，LLM 只看精炼摘要 |
| 探索区资源预算比稳定区低 30% | v4 设计 | 需要 per-agent token 追踪和预算执行机制 |
| 前端技术待定 | 用户决策 | Web 面板架构需预留前端技术无关的 API 层 |
| 单用户 MVP，多用户预留 | PRD Section 6.5 | 框架层面预留用户隔离，MVP 不实现 |

### Cross-Cutting Concerns Identified

1. **事件日志（Event Log）**— 贯穿所有层，所有行为/决策/状态变更的唯一真实来源，append-only 不可变
2. **Safety Airbag** — 横切执行层，四类机制（Staging/Reversibility Gate/Circuit Breaker/Blast Radius Limit）
3. **标准接口契约** — 横切所有组件，定义 capability_profile、airbag_spec、health_probe 等标准接口
4. **反熵监控** — 横切进化层和执行层，7 个熵指标超阈值触发自动压缩/合并/退役
5. **进化预算** — 横切进化层，每周期插件新增额度、原语新增额度、探索资源比例均有上限
6. **可观测性** — 横切所有组件，统一 JSON 日志 + Web 面板 + 结构化指令查询

### MVP 架构简化策略（ADR 辩论共识）

**ADR-1 层次简化：** MVP 采用四层架构（Facade / 执行 / 进化 / 基础设施），Phase 2 逐步展开为七层。评估组件（Critic）在 MVP 阶段归入 engine 进程内的 Holon 角色，Meta Judge 为 Phase 2 独立进程。

**ADR-2 进程模型：** ~~初始分析为 6 类进程~~，经约束松弛 #4 和 #9 分析，MVP 最终采纳 4 类进程——
1. evoclaw-engine（Facade + Router + Holon 工作池 + Discord Bot + Governance）
2. evoclaw-compiler（Observer + Evolver + Compiler + Pattern Registry）
3. evoclaw-dashboard（REST API + SSE + Web 面板）
4. evoclaw-judge（Meta Judge，Phase 2 stub）

**ADR-3 通信方案：** MVP 用 ZeroMQ——Pub-Sub（事件广播）+ DEALER-ROUTER（请求响应，异步不锁步）。端点定义在 `evoclaw.yaml` 配置文件中（MVP 不需要服务发现）。两级 QoS：fire-and-forget / at-least-once（msg_id 幂等去重）。exactly-once 标注 Phase 2（需 WAL）。抽象层 API 参考 DDS 概念模型，预留 Phase 2 迁移。

**ADR-4 存储方案：** MVP 全文件系统——记忆用 JSON 文件（按日期目录），事件日志用 JSON Lines（每天一个文件，写入后只读 + 哈希校验）。通过抽象接口层（MemoryStore / EventStore）预留 Phase 2 切换到向量库和 ElasticSearch。

**ADR-5 Cortex 简化：** MVP 只新建 Layer II（语义约束/LLM 评估）+ Layer III（形式约束/Z3 C API + 超时沙盒）。Layer I 简化为固定格式 JSON 摘要，Layer IV/V/VI 映射到已有组件（Router / Holon 状态机 / 记忆服务）。

### Key Technical Risks

| 风险 | 影响 | 缓解策略 |
|------|------|---------|
| Z3 C API 集成复杂度 | 形式约束验证是核心创新，集成失败影响通信协议 | 先用 Z3 Python 绑定验证可行性，再移植到 C API；保留降级到纯语义评估的回退路径 |
| 进程间通信延迟 | NFR2 要求 ≤50ms 单跳，ZeroMQ 在高负载下可能超标 | MVP 单机部署，使用 IPC 传输（unix socket）而非 TCP；Phase 2 压测后决定是否迁移 DDS |
| Compiler 模式识别准确率 | 误编译会产生低质量插件，污染稳定区 | 双重门控（沙盒测试 + Meta Judge 盲测）；MVP 阶段编译产物需用户确认（FR20） |
| 自进化系统的不可预测性 | 进化方向可能偏离用户意图 | 宪法约束 + 进化预算 + 冷静期 + 完整审计链；Maintainer 不可自替 |
| 单机部署的资源竞争 | 6 类进程共享 CPU/内存，Compiler 长任务可能饿死 Router | 进程优先级分级（Router > Holon > Compiler）；Compiler 使用 nice 降低优先级 |

### Pre-mortem 失败场景分析

**场景 1：Toolization Loop 永远无法闭合**

- 死法：Compiler 3 个月内识别不出任何可编译模式——要么任务太多样导致无重复路径，要么协作轨迹太复杂无法降维
- 影响：核心假设无法验证，项目失去差异化价值
- 预防措施：
  - 编译产物不限于"完整插件"，定义 3 种产物形态：完整插件 / 半自动模板（人工填参数）/ 协作路径缓存（跳过规划直接执行）
  - MVP 阶段 Compiler 先产出"模式半成品"（Pattern Semi-product），降低闭环门槛
  - 前 2 周刻意用同类任务喂数据，确保至少有 1 条路径被重复 ≥3 次
  - 如果纯路径匹配不够，引入语义聚类——相似但不完全相同的路径也可合并编译

**场景 2：通信开销吞噬性能预算**

- 死法：混合协议的形式约束验证 + 语义评估 + 小脑打包的总开销超过直接传递原始消息，NFR2（≤50ms）无法达标
- 影响：通信层从"优势"变成"瓶颈"，被迫退化为简单 JSON 传递
- 预防措施：
  - 消息分级：L0（fire-and-forget，无验证）/ L1（语义摘要，无形式验证）/ L2（完整混合验证）——MVP 80% 消息走 L0/L1
  - 形式约束验证异步化——不阻塞消息传递，验证失败后追溯处理
  - 语义评估批量化——小脑攒够 N 条后批量调 LLM，而非逐条评估
  - 设置通信预算：单条消息总处理时间硬上限 50ms，超时自动降级到 L0

**场景 3：进化失控——编译产物质量劣化**

- 死法：Compiler 产出的插件通过了沙盒测试但在真实场景中表现差，用户体验下降却难以定位原因（因为 Router 已经在用插件替代 Holons）
- 影响：用户信任崩塌，系统越"进化"越差
- 预防措施：
  - 插件上线后设置"观察期"（≥10 次真实调用），期间同时运行插件和 Holons 原路径，对比结果质量
  - 引入"结果质量关联"——如果插件上线后同类任务的用户满意度下降，自动触发回滚
  - 进化熔断器：连续 2 个编译产物被回滚时，Compiler 暂停编译 48h，人工介入审查
  - 每次编译保留 Holons 原路径快照，确保随时可回滚

**场景 4：记忆系统成为性能黑洞**

- 死法：随着使用时间增长，记忆层数据膨胀，检索延迟从 ms 级退化到秒级，小脑的"自动管理"变成"自动拖慢"
- 影响：NFR4（≤200ms）和 NFR5（≤300ms）无法维持，用户感知到系统变慢
- 预防措施：
  - MVP 记忆用文件系统，但建立内存索引（按日期 + 任务类型），避免全量扫描
  - 设置记忆 TTL：工作记忆 24h 压缩，情景记忆 90 天归档，超期数据移入冷存储
  - 记忆检索设置硬超时 200ms，超时返回空结果而非阻塞
  - 监控记忆数据量增长曲线，设置阈值告警（如单日新增 >100MB 触发告警）
  - 小脑定期自动蒸馏：每周将情景记忆中的高频模式提炼为语义记忆，每月归档低访问记忆

### Assumption Mapping（假设风险矩阵）

**致命象限（低确定性 + 高影响）：**

| ID | 假设 | 来源 | 依赖它的内容 | 风险 |
|----|------|------|-------------|------|
| D1 | Toolization Loop 能在真实场景中闭合——重复协作模式可被自动识别并编译 | PRD 1.5, Unified Design 7.2 | FR17-21, 成功标准 2.2, MVP 验收标准 2-3, 项目核心差异化 | 全新概念，无同类先例；任务多样性或轨迹复杂度可能导致无法编译 |
| U1 | 用户任务存在足够重复性，使 Compiler 有数据可编译（≥3 次同路径成功执行） | PRD 旅程 2, FR17 | D1 的前提条件，Compiler 触发阈值，用户旅程 2 的可实现性 | 通用管家场景下任务可能高度多样化，缺乏重复路径 |
| O2 | 单人开发者能在 3 个月内交付 C++ MVP（6 类进程 + Z3 + ZeroMQ + Discord Bot + Web 面板） | PRD 7.5 风险表 | Phase 1 时间线，MVP 范围所有 Must-Have | C++ 多 Agent AI 系统缺乏生态支持，工作量极大 |
| T1 | C++ 在多 Agent AI 场景下开发效率可接受 | architecture.md 技术约束 | 整个 MVP 时间线，Z3 C API 集成，ZeroMQ 集成 | 无成熟 Agent 框架，开发周期长 |

**风险链：T1→O2→U1→D1，任何一环断裂都导致核心价值无法验证。**

**需关注象限（中等确定性 + 高/中影响）：**

| ID | 假设 | 确定性 | 影响 | 缓解状态 |
|----|------|--------|------|----------|
| T5 | LLM API 延迟（秒级）和可用性支撑实时多 Agent 协作 | MEDIUM | HIGH | NFR11 定义降级策略（重试 ≤3 次 + 超时 ≤30s） |
| T2 | Z3 在 100 变量/50 约束规模下 ≤100ms 完成验证 | MEDIUM | MEDIUM | 预留降级到纯语义评估的回退路径 |
| D2 | 混合协议（形式+语义）比纯 NL 或纯结构化更优 | LOW | MEDIUM | 可降级为结构化 JSON，放弃渐进式形式化 |
| D4 | 小脑能在 ≤200 tokens 预算内有效管理记忆上下文 | MEDIUM | MEDIUM | 可放宽 token 预算缓解 |
| O3 | 6 类进程共享单机 CPU/内存不产生严重竞争 | MEDIUM | MEDIUM | 进程优先级分级 + cgroup 限制 |
| D3 | 双评估架构（Critic + Meta Judge 物理隔离）有效防止评估塌陷 | MEDIUM | MEDIUM | 宪法约束 + 用户确认作为最后防线 |

**安全象限（高确定性，成熟技术支撑）：**

| ID | 假设 | 确定性 | 影响 | 理由 |
|----|------|--------|------|------|
| T3 | ZeroMQ IPC（unix socket）满足 ≤50ms 延迟 | HIGH | HIGH | 单机 IPC 延迟通常微秒级，50ms 预算极宽裕 |
| D6 | Event Log（append-only）作为唯一真实来源 | HIGH | HIGH | Event Sourcing 是成熟架构模式 |
| D7 | 进程级隔离是 Agent 故障隔离的正确粒度 | HIGH | MEDIUM | Erlang/OTP supervisor tree 已验证 |
| D5 | ε-greedy 路由策略平衡利用与探索 | HIGH | LOW | 经典多臂老虎机策略，可平滑升级 |
| T4 | 文件系统存储满足 MVP 记忆层性能需求 | HIGH | LOW | 短期数据量小，已预留抽象接口层替换 |
| O1 | 单用户单机部署足以验证核心假设 | HIGH | LOW | 最简部署模型，已预留扩展点 |
| U3 | 用户愿意手动确认编译产物（MVP 阶段） | HIGH | LOW | 用户即开发者本人 |
| U4 | Discord 是 MVP 合适的交互渠道 | HIGH | LOW | API 成熟，Facade 层已做抽象 |

### Constraint Relaxation（约束松弛分析）

| # | 约束 | 判定 | 核心建议 |
|---|------|------|----------|
| 1 | C++ 后端核心 | ⚠️ 过紧 | MVP 考虑 Rust 或 Python+C 混合，降低致命风险链 T1→O2 |
| 2 | ≤200 tokens Context 预算 | ⚠️ 过紧 | 改为分级预算（L0: ≤100 / L1: ≤500 / L2: ≤1000），保留总占比 ≤15% 软上限 |
| 3 | ≤50ms 单跳延迟（NFR2） | ✅ 合理 | 保持不变，IPC 微秒级，50ms 极宽裕且为分布式预留空间 |
| 4 | 进程级隔离（6 类进程） | ⚠️ 过紧 | MVP 从 6 类减到 4 类——Facade+Router+Holon 合并为单进程多线程；Meta Judge/系统控制沙盒/Compiler 保持独立进程 |
| 5 | Z3 C API 形式验证 | 🔄 可替代 | MVP 用规则引擎（数值比较/布尔逻辑/范围检查）覆盖 FL-0~FL-2，预留 FormalVerifier 抽象接口，Phase 2 按需引入 Z3 |
| 6 | 文件系统存储（ADR-4） | ✅ 合理 | 保持不变，JSON Lines 天然 append-only，抽象接口层已预留替换路径 |
| 7 | ≥3 次重复触发编译（FR17） | ⚠️ 过紧 | 降低到 ≥2 次精确匹配或 ≥3 次语义相似匹配；引入"模式半成品"——1 次执行生成模板，2 次确认后编译为完整插件 |
| 8 | 探索区预算低 30% | ✅ 合理 | 保持不变，与设计哲学一致，per-agent token 追踪乘 0.7 系数即可 |
| 9 | Meta Judge 物理隔离 | 🔄 可替代 | MVP 移除（PRD 已标为 Nice-to-Have），用户确认（FR20）作为唯一门控；Phase 2 引入逻辑隔离（独立 LLM session + 独立状态存储） |
| 10 | Append-only 不可变事件日志 | ✅ 合理 | 保持不变，Event Sourcing 成熟模式，进化可审计性/决策链复现/挑战集构建的基础 |

**松弛优化总结：**
- 4 个保持不变（✅），4 个建议放松（⚠️），2 个建议替代方案（🔄）
- 最大优化机会：约束 1（语言选择）+ 约束 5（Z3→规则引擎），可为 MVP 节省 4-8 周开发时间，不损失核心创新验证能力
- 约束 2 的分级预算直接解决假设 D4（小脑 context 管理）的不确定性
- 约束 7 的放松配合"模式半成品"概念，直接缓解致命风险 D1（Toolization Loop 闭合）

### Analogy Mining（类比挖掘）

**类比 1：JIT 编译器（HotSpot / V8）→ Toolization Loop**

Toolization Loop 与 HotSpot 分层编译同构：解释执行→C1→C2 对应 Holons 协作→路径缓存→模板→完整插件。可借鉴：
- **分级编译产物**：L0 协作路径缓存（跳过规划直接执行，仍需 LLM）→ L1 半自动模板（固定骨架，LLM 只填参数）→ L2 完整插件（无 LLM，确定性执行）
- **推测性编译 + 假设守卫（Guard）**：Compiler 基于历史数据做推测性假设（如"此类任务输入格式总是 X"），插件嵌入运行时假设检查，违反时自动降级回 Holons
- **热度衰减（Counter Half-life）**：模式热度随时间衰减，30 天前重复 5 次但近期未出现的模式，优先级低于近 7 天重复 3 次的模式

**类比 2：Erlang/OTP 监督树 → 忒休斯之船热替换**

OTP 的 supervisor tree + 热代码加载是进程级隔离 + 热替换的工业级验证。可借鉴：
- **显式监督策略**：one_for_one（独立 Agent 崩溃只重启自身）/ one_for_all（Holon 成员崩溃重启整个组队）/ rest_for_one（Compiler 崩溃重启 Compiler + Pattern Registry）
- **双版本共存热替换**：新版本插件注册时不立即替换旧版本——新请求路由到新版本，进行中请求继续用旧版本，旧版本所有请求完成后卸载
- **崩溃报告 → Challenge Set**：Agent 崩溃的完整上下文自动写入 Challenge Set，作为进化数据源

**类比 3：人体免疫系统 → 双区架构 + 反熵代谢**

先天免疫/适应性免疫 = System 1/System 2；胸腺阳性/阴性选择 = 探索区晋升/淘汰。可借鉴：
- **亲和力成熟（Affinity Maturation）**：对一个 Pattern 生成多个编译变体（不同参数化方式、不同流程简化策略），通过 Challenge Set 竞争选优
- **治理特权区（Immune Privilege）**：显式列出享有进化豁免权的组件（Governance Kernel、Maintainer），明确豁免条件和边界
- **进化强度连续调节**：不是"暂停 48h"的二元开关，而是动态调整编译阈值（如连续回滚时从 ≥3 次提高到 ≥5 次），降低频率但不完全停止

**类比 4：Linux 内核发布流程 → 进化治理与灰度发布**

-rc 发布候选 + Git bisect + Signed-off-by 责任链。可借鉴：
- **收敛判据替代固定观察期**：监控编译产物错误率曲线，收敛到 0 即可提前结束观察期；持续波动则延长
- **进化 bisect**：系统性能下降时，通过 Event Log 二分回溯进化历史，快速定位退化源头
- **签名链**：Compiler 提出→Evolver 审查→Meta Judge 盲测→Governance 发布，每步在 Event Log 留不可变签名记录

**类比 5：TCP/IP 协议栈 → 混合协议分层**

QoS 分级 + TLS 渐进握手 + BBR 动态带宽估计。可借鉴：
- **QoS 与验证级别绑定**：fire-and-forget→无验证（心跳/日志）/ at-least-once→语义验证（Layer II）/ exactly-once→完整混合验证（Layer II + III）
- **TLS 式渐进形式化握手**：两个 Agent 首次通信从 FL-0 开始，随约束模板积累自动升级，升级过程双向协商（类似 cipher suite 协商）
- **BBR 式动态 context 预算**：不设固定 200 tokens 上限，根据任务复杂度和 LLM 响应质量反馈动态调节（S/M/L 密度自适应）

**类比 6：Amazon 双披萨团队 + 细胞分裂 → 组织复杂度控制**

团队大小限制 + API 强制 + 细胞凋亡/端粒机制。可借鉴：
- **模块"端粒"机制**：创建时分配生命预算，每次调用消耗一点，每次通过 Challenge Set 测试恢复一点，耗尽自动进入 Deprecated（比固定"30 天使用率 <5%"更动态）
- **模块分裂规则**：当模块 capability_profile 中 primary_intents 超过 3 个，或 Capability Matrix 评分方差过大时，触发分裂审查——Compiler 分析是否应拆分为更专业的子模块

## Starter Template Evaluation

### Primary Technology Domain

C++23 多进程 AI Agent 系统（非典型 Web 项目），不适用标准 Web starter template。

### 评估方法

通过 4 轮 Advanced Elicitation 推导和验证：
1. First Principles Analysis — 从基本事实推导出"按部署单元组织"的结构
2. Comparative Analysis Matrix — 4 种方案加权评分，Current 方案 3.90 分胜出
3. Critique and Refine — 发现并修复 6 个问题（命名/接口/Bot 归属/测试粒度/数据目录/启动脚本）
4. Cross-Functional War Room — 解决 4 个实操问题（libzmq 依赖/httplib 归属/一键启动/watchdog）

### Selected Starter：全新项目骨架（按部署单元组织）

**组织原则：** 目录结构和进程边界 1:1 对齐。"代码在哪个目录"直接回答"运行在哪个进程"。

**项目骨架：**

```
EvoClaw/
├── CMakeLists.txt                  # 顶层 CMake, C++23, FetchContent
├── cmake/                          # CMake 模块（FetchContent 配置）
├── lib/                            # 共享库 libevoclaw（OBJECT libs 聚合）
│   ├── CMakeLists.txt              #   聚合所有 OBJECT lib → libevoclaw.a
│   ├── event_log/                  #   EventStore 接口 + JSON Lines 实现
│   ├── memory/                     #   MemoryStore 接口 + 文件系统实现
│   ├── llm/                        #   LLM 客户端接口 + OpenAI 实现
│   ├── bus/                        #   MessageBus 接口 + ZeroMQ 实现
│   ├── protocol/                   #   消息类型定义 + 序列化
│   └── config/                     #   配置加载
├── src/
│   ├── engine/                     #   evoclaw-engine (Facade+Router+Holon+DiscordBot)
│   │   └── CMakeLists.txt
│   ├── compiler/                   #   evoclaw-compiler (Observer+Evolver+Compiler)
│   │   └── CMakeLists.txt
│   ├── judge/                      #   evoclaw-judge (Meta Judge, Phase 2 启用)
│   │   └── CMakeLists.txt
│   └── dashboard/                  #   evoclaw-dashboard (REST+SSE+Web)
│       └── CMakeLists.txt
├── tests/
│   ├── CMakeLists.txt
│   ├── lib/                        #   单元测试（镜像 lib/ 结构）
│   │   ├── bus/
│   │   ├── memory/
│   │   ├── event_log/
│   │   ├── llm/
│   │   ├── protocol/
│   │   └── config/
│   └── integration/                #   跨进程集成测试
├── scripts/                        #   evoclaw.sh start/stop, watchdog
└── docs/                           #   开发文档
```

**构建产物：** 1 个静态库 `libevoclaw.a`（由 OBJECT libs 聚合）+ 4 个可执行文件

**Architectural Decisions Provided by Starter:**

| 类别 | 决策 |
|------|------|
| 语言 & 标准 | C++23, `-std=c++23` |
| 构建系统 | CMake 3.25+, FetchContent 管理 GTest/cppzmq/nlohmann-json/cpp-httplib |
| 系统依赖 | libzmq（系统包管理器安装：`apt install libzmq3-dev`） |
| 库组织 | lib/ 内子模块编译为 OBJECT library，聚合为 libevoclaw.a |
| 接口抽象 | 各子模块内部定义接口（如 `lib/memory/memory_store.h`），实现文件同目录，Phase 2 替换只需新增实现 |
| 测试框架 | GTest + CTest, 测试目录镜像 lib/ 结构 |
| 进程编排 | `scripts/evoclaw.sh start/stop`，按序启动 engine→compiler→dashboard，PID 文件追踪，watchdog 3s 自动重启 |
| 运行时数据 | 默认 `~/.evoclaw/data/`，可通过配置覆盖 |
| Discord Bot | 归入 engine 进程（Facade 通信通道），MVP 不独立进程 |
| httplib 归属 | 保留在 lib/ 层面（llm/ 用 client，dashboard 用 server），header-only 无链接开销 |

**Note:** 项目初始化应作为第一个实现 story。

## Core Architectural Decisions

### Decision Priority Analysis

**Critical Decisions (Block Implementation):**
- LLM 集成方式、ZeroMQ 拓扑、消息格式、Event Log 策略、Discord Bot 框架

**Important Decisions (Shape Architecture):**
- Prompt 管理、服务发现、记忆存储结构、风险分级实现、日志框架、健康检查

**Deferred Decisions (Post-MVP):**
- Token 预算精细控制（当前不限制）、多 LLM 后端切换、分布式部署

### LLM 集成架构

| 决策 | 选择 | 理由 |
|------|------|------|
| LLM 客户端设计 | 直接 HTTP 调用 OpenAI 兼容 API（httplib client） | MVP 最简，单一后端够用 |
| Prompt 管理 | 外部纯文本模板文件 + `{{variable}}` 占位符替换，运行时加载 | 可热更新，不需重编译；模板存放于 `~/.evoclaw/prompts/` |
| Token 预算控制 | 不限制，按需使用 | MVP 单用户，成本可控，避免过早优化 |

### 进程间通信

| 决策 | 选择 | 理由 |
|------|------|------|
| ZeroMQ 拓扑 | Pub-Sub（事件广播）+ DEALER-ROUTER（请求响应，异步不锁步） | 事件通知用 Pub-Sub 解耦，任务请求用 DEALER-ROUTER 避免 REQ-REP 锁步死锁 |
| 消息序列化 | JSON（nlohmann/json） | 已有依赖，可读性好，调试友好 |
| 服务发现 | 配置文件定义端点（`evoclaw.yaml`） | MVP 4 进程单机部署，不需要动态服务发现 |

### 数据架构

| 决策 | 选择 | 理由 |
|------|------|------|
| Event Log 格式 | 每进程独立写 jsonl（`~/.evoclaw/data/events/{process}/`），定期合并到 canonical log + SHA-256 校验 | 避免多进程写锁竞争，各进程独立性强 |
| 记忆存储结构 | 按类型分目录（`working_memory/`、`user_model/`、`episodic/`），每条记忆一个 JSON 文件 | 粒度最细，单条可独立读写/删除/归档 |
| 配置文件格式 | YAML（yaml-cpp，FetchContent 管理） | 可读性最好，适合复杂嵌套配置 |
| Prompt 模板格式 | Prompt Registry（YAML 元数据 + 纯文本模板 + `{{variable}}` 占位符） | 支持版本管理、依赖声明、输出 schema |

### 安全与权限

| 决策 | 选择 | 理由 |
|------|------|------|
| 风险分级实现（FR57 L0-L3） | 白名单定义操作分级 + Agent 按需申请权限 + L2+ 需用户实时确认 | 兼顾安全和灵活性 |
| 系统操作沙盒 | 子进程 + ulimit/cgroup 资源限制 | 中等隔离，实现成本合理，MVP 够用 |
| LLM API Key 管理 | 环境变量（`EVOCLAW_LLM_API_KEY`） | 标准做法，不入库 |

### 可观测性

| 决策 | 选择 | 理由 |
|------|------|------|
| 日志框架 | spdlog（FetchContent 管理） | C++ 最流行，异步日志，多 sink 支持 |
| 进程健康检查 | PID 文件（基础存活）+ ZeroMQ 心跳（应用级健康）混合 | 双重保障，scripts/ 用 PID 检查，engine 用心跳监控 |
| Dashboard 数据源 | ZeroMQ Pub-Sub 订阅实时事件 + 文件系统读取历史数据 | 实时性好，历史数据不走 IPC 避免回放开销 |

### Discord Bot 集成

| 决策 | 选择 | 理由 |
|------|------|------|
| Bot 实现方式 | D++ (DPP) C++ Discord 库（FetchContent 管理） | 活跃维护，原生 C++，API 覆盖完整 |
| 命令交互模式 | Slash Commands + 自然语言混合 | `/task`、`/status` 等快捷命令提高效率，自由文本保留灵活性 |

### 完整依赖清单

| 依赖 | 管理方式 | 用途 |
|------|---------|------|
| GTest | FetchContent | 测试框架 |
| nlohmann/json | FetchContent | JSON 序列化 |
| cpp-httplib | FetchContent | HTTP client（LLM API）+ server（Dashboard） |
| cppzmq | FetchContent | ZeroMQ C++ binding |
| libzmq | 系统包（`apt install libzmq3-dev`） | ZeroMQ 核心库 |
| yaml-cpp | FetchContent | YAML 配置解析 |
| spdlog | FetchContent | 日志框架 |
| D++ (DPP) | FetchContent | Discord Bot |

### Decision Impact Analysis

**实现顺序：**
1. 项目骨架初始化（CMake + FetchContent + 目录结构）
2. lib/bus/（ZeroMQ Pub-Sub + Req-Rep 封装）+ lib/protocol/（消息定义）
3. lib/config/（YAML 配置加载）+ lib/event_log/（per-process jsonl 写入）
4. lib/llm/（OpenAI HTTP 客户端）+ prompt 模板加载
5. lib/memory/（文件系统记忆存储）
6. src/engine/（Facade + Router + Holon + Discord Bot）
7. src/compiler/（Observer + Evolver + Compiler）
8. src/dashboard/（REST + SSE + Web 面板）
9. scripts/（启动/停止/watchdog）
10. src/judge/（Phase 2）

**跨组件依赖：**
- 所有进程依赖 lib/bus/ + lib/protocol/ + lib/config/ + lib/event_log/
- engine + compiler 依赖 lib/llm/ + lib/memory/
- dashboard 依赖 lib/bus/（订阅事件）+ 文件系统（读历史）
- engine 额外依赖 D++ (DPP)

## Implementation Patterns & Consistency Rules

### C++ 命名规范（Google C++ Style）

| 元素 | 规范 | 示例 |
|------|------|------|
| 类/结构体 | `PascalCase` | `EventStore`, `MessageBus` |
| 函数/方法 | `PascalCase` | `GetEvents()`, `SendMessage()` |
| 变量 | `snake_case` | `event_count`, `user_name` |
| 成员变量 | `trailing_underscore_` | `name_`, `event_count_` |
| 常量 | `kPascalCase` | `kMaxRetry`, `kDefaultTimeout` |
| 枚举值 | `kPascalCase` | `kActive`, `kDeprecated` |
| 命名空间 | `snake_case` | `evoclaw::memory`, `evoclaw::bus` |
| 文件名 | `snake_case.cpp/.h` | `event_store.cpp`, `message_bus.h` |
| 头文件保护 | `#ifndef EVOCLAW_MODULE_FILE_H_` | `#ifndef EVOCLAW_MEMORY_EVENT_STORE_H_` |

### JSON 与消息格式

**JSON 字段命名：** `snake_case`（与 C++ 变量名一致）

**ZeroMQ 消息信封：**
```json
{
  "msg_id": "uuid",
  "msg_type": "task.request",
  "source": "engine",
  "target": "compiler",
  "timestamp": "2026-02-26T10:30:00Z",
  "payload": { ... }
}
```

**Event Log 事件格式：**
```json
{
  "event_id": "uuid",
  "event_type": "agent.task.completed",
  "source_process": "engine",
  "timestamp": "2026-02-26T10:30:00Z",
  "data": { ... },
  "checksum": "sha256:..."
}
```

**事件类型命名：** 点分层级（`agent.task.completed`, `evolution.compile.started`）

### 错误处理模式

**错误传播：** `std::expected<T, Error>`（C++23），编译期强制处理错误路径

**错误类型：** 统一基类 + 模块级错误码
```cpp
namespace evoclaw {
enum class ErrorCode { kOk, kNotFound, kTimeout, kInvalidArg, kInternal };

struct Error {
  ErrorCode code;
  std::string message;
  std::string context;  // 模块名/函数名

  // 从模块级错误码构造
  template <typename ModuleError>
  static Error From(ModuleError e, std::string_view ctx) {
    return {ErrorCode::kInternal, ModuleErrorToString(e), std::string(ctx)};
  }
};
}  // namespace evoclaw

// 模块级错误码
namespace evoclaw::bus {
enum class BusError { kConnectionFailed, kTimeout, kSerializationError };
std::string ModuleErrorToString(BusError e);
}
```

**日志级别规范：**
- `TRACE`: 函数进出、变量值（仅 debug 构建）
- `DEBUG`: 内部状态变化、消息收发详情
- `INFO`: 进程启动/停止、任务开始/完成、进化事件
- `WARN`: 可恢复错误、性能退化、接近阈值
- `ERROR`: 不可恢复错误、外部服务失败
- `CRITICAL`: 进程即将崩溃、数据完整性受损

### 代码组织模式

**头文件分离：** 小型类/模板允许 header-only，复杂类严格 `.h`/`.cpp` 分离

**接口定义：** 核心替换点用纯虚基类（运行时多态），内部热路径用 Concepts（编译期多态）
```cpp
// 核心替换点 — 纯虚基类（Phase 2 可替换实现）
class MemoryStore {
 public:
  virtual ~MemoryStore() = default;
  virtual std::expected<Memory, Error> Load(std::string_view id) = 0;
  virtual std::expected<void, Error> Save(const Memory& mem) = 0;
};

// 内部热路径 — Concepts
template <typename T>
concept Serializable = requires(T t, nlohmann::json& j) {
  { t.ToJson() } -> std::convertible_to<nlohmann::json>;
  { T::FromJson(j) } -> std::same_as<std::expected<T, Error>>;
};
```

**依赖注入：** 工厂创建 + 构造函数注入
```cpp
// 工厂创建
auto store = CreateMemoryStore(config);  // 返回 unique_ptr<MemoryStore>

// 构造函数注入
class Engine {
 public:
  Engine(std::unique_ptr<MemoryStore> store, std::unique_ptr<MessageBus> bus);
};
```

**智能指针：** `std::unique_ptr` 为主，`std::shared_ptr` 仅在确实需要共享所有权时使用

### 并发模式

**线程模型：** Router 用线程池处理请求分发，每个 Holon 组队用独立 `std::jthread` + `std::stop_token` 支持优雅停止

**线程间通信：** 消息传递优先（线程安全队列），最小化共享状态

**异步 LLM 调用：** 自建线程池（`lib/common/thread_pool.h`）
```cpp
auto future = thread_pool.Submit([&] {
  return llm_client.Complete(prompt);
});
// 非阻塞检查或等待结果
auto result = future.get();
```

### 测试模式

**测试命名：** `TEST(ModuleName, MethodName_Scenario_Expected)`
```cpp
TEST(EventStore, Append_ValidEvent_ReturnsOk)
TEST(EventStore, Append_DuplicateId_ReturnsError)
TEST(MessageBus, Subscribe_InvalidTopic_ReturnsError)
```

**Mock 策略：** 核心接口用 GMock，简单依赖用手写 fake
```cpp
// GMock — 核心接口
class MockMemoryStore : public MemoryStore {
 public:
  MOCK_METHOD(std::expected<Memory, Error>, Load, (std::string_view id), (override));
  MOCK_METHOD(std::expected<void, Error>, Save, (const Memory& mem), (override));
};

// 手写 fake — 简单依赖
class FakeConfig : public Config {
 public:
  std::string Get(std::string_view key) override { return values_[key]; }
  std::unordered_map<std::string, std::string> values_;
};
```

**测试文件命名：** `{source_name}_test.cpp`（`event_store_test.cpp`）

### Enforcement Guidelines

**All AI Agents MUST:**
- 遵循 Google C++ Style Guide 命名规范
- 所有可失败函数返回 `std::expected<T, Error>`，禁止抛异常
- JSON 字段一律 `snake_case`，消息信封和事件格式严格遵循上述模板
- 新增接口（Phase 2 替换点）使用纯虚基类，通过构造函数注入
- 测试文件与源文件同名加 `_test` 后缀，测试用例遵循 `Method_Scenario_Expected` 命名

## Project Structure & Boundaries

### FR 到架构组件映射

| FR 范围 | 功能域 | 目标目录 |
|---------|--------|---------|
| FR1-8 | 任务交互（Facade + 统一人格 + 主动服务） | `src/engine/facade/` |
| FR9-11 | 动态组队（Router + Holon 工作池） | `src/engine/router/`, `src/engine/holon/` |
| FR12-16 | 组织通信（消息总线 + 混合协议） | `lib/bus/`, `lib/protocol/` |
| FR17-21 | 进化编译（Compiler + Observer + Evolver） | `src/compiler/` |
| FR22-26 | 治理（Governance Gate + 宪法 + 晋升） | `src/engine/governance/` |
| FR27-33 | 记忆（Event Log + 工作记忆 + 用户模型 + 情景记忆） | `lib/event_log/`, `lib/memory/` |
| FR34-39 | 通信协议（渐进式形式化 + 规则引擎） | `lib/protocol/` |
| FR40-48 | 监控治理（Dashboard + 审计 + 熵监控） | `src/dashboard/`, `src/engine/governance/` |
| FR49-56 | 系统控制（文件/shell/进程/网络/代码/cron） | `src/engine/holon/capabilities/` |
| FR57-58 | 安全（风险分级 + 沙盒） | `src/engine/safety/` |
| FR59-63 | 集成接口（LLM + Discord + REST + SDK） | `lib/llm/`, `src/engine/facade/`, `src/dashboard/` |

### 完整项目目录树

```
EvoClaw/
├── CMakeLists.txt
├── cmake/
│   ├── dependencies.cmake
│   └── compiler_options.cmake
├── .gitignore
├── .clang-format
├── .clang-tidy
│
├── lib/                                    # ═══ 共享库 libevoclaw ═══
│   ├── CMakeLists.txt
│   ├── bus/                                # MessageBus（FR12-13）
│   │   ├── CMakeLists.txt
│   │   ├── message_bus.h
│   │   └── zmq_message_bus.h/.cpp
│   ├── protocol/                           # 消息协议（FR34-39）
│   │   ├── CMakeLists.txt
│   │   ├── message.h
│   │   ├── message_types.h
│   │   ├── serializer.h/.cpp
│   │   ├── rule_engine.h
│   │   ├── basic_rule_engine.h/.cpp
│   │   ├── cerebellum.h/.cpp              # 小脑：消息预处理管线（打包/验证/冲突检测）
│   ├── event_log/                          # Event Log（FR27-28）
│   │   ├── CMakeLists.txt
│   │   ├── event_store.h
│   │   ├── jsonl_event_store.h/.cpp
│   │   └── event_merger.h/.cpp
│   ├── memory/                             # 记忆存储（FR29-33）
│   │   ├── CMakeLists.txt
│   │   ├── memory_store.h
│   │   ├── fs_memory_store.h/.cpp
│   │   ├── memory_types.h
│   │   └── memory_index.h
│   ├── llm/                                # LLM 客户端（FR59）
│   │   ├── CMakeLists.txt
│   │   ├── llm_client.h
│   │   ├── openai_client.h/.cpp
│   │   ├── prompt_registry.h               # Prompt Registry（ID/版本/依赖/schema）
│   │   └── prompt_registry.cpp
│   ├── config/                             # 配置管理
│   │   ├── CMakeLists.txt
│   │   ├── config.h
│   │   └── yaml_config.h/.cpp
│   │
│   └── common/                             # 通用工具
│       ├── CMakeLists.txt
│       └── thread_pool.h                   # 自建线程池（替代 std::async）
│
├── src/                                    # ═══ 可执行文件 ═══
│   ├── engine/                             # evoclaw-engine（FR1-11, 22-26, 49-58）
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp
│   │   ├── engine.h/.cpp
│   │   ├── facade/
│   │   │   ├── facade.h/.cpp
│   │   │   └── discord_bot.h/.cpp
│   │   ├── router/
│   │   │   ├── router.h/.cpp
│   │   │   ├── capability_matrix.h
│   │   │   └── epsilon_greedy.h
│   │   ├── holon/
│   │   │   ├── holon_pool.h/.cpp
│   │   │   ├── holon.h/.cpp
│   │   │   └── capabilities/
│   │   │       ├── file_ops.h
│   │   │       ├── shell_ops.h
│   │   │       ├── process_ops.h
│   │   │       └── network_ops.h
│   │   ├── governance/
│   │   │   ├── governance_gate.h
│   │   │   ├── constitution.h
│   │   │   └── entropy_monitor.h          # Phase 3 stub — MVP 仅定义接口
│   │   └── safety/
│   │       ├── risk_classifier.h
│   │       ├── sandbox.h
│   │       └── permission_manager.h
│   ├── compiler/                           # evoclaw-compiler（FR17-21）
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp
│   │   ├── observer.h/.cpp
│   │   ├── evolver.h/.cpp
│   │   ├── compiler.h/.cpp
│   │   └── pattern_registry.h
│   ├── judge/                              # evoclaw-judge（Phase 2）
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp
│   │   └── meta_judge.h
│   └── dashboard/                          # evoclaw-dashboard（FR40-48, FR61）
│       ├── CMakeLists.txt
│       ├── main.cpp
│       ├── rest_api.h/.cpp
│       ├── sse_handler.h/.cpp
│       └── static/index.html
│
├── tests/                                  # ═══ 测试 ═══
│   ├── CMakeLists.txt
│   ├── lib/
│   │   ├── bus/zmq_message_bus_test.cpp
│   │   ├── protocol/serializer_test.cpp
│   │   ├── protocol/basic_rule_engine_test.cpp
│   │   ├── protocol/cerebellum_test.cpp
│   │   ├── event_log/jsonl_event_store_test.cpp
│   │   ├── event_log/event_merger_test.cpp
│   │   ├── memory/fs_memory_store_test.cpp
│   │   ├── llm/openai_client_test.cpp
│   │   ├── llm/prompt_registry_test.cpp
│   │   └── config/yaml_config_test.cpp
│   ├── src/                                # 可执行文件组件单元测试
│   │   ├── engine/
│   │   │   ├── facade_test.cpp
│   │   │   ├── discord_bot_test.cpp
│   │   │   ├── router_test.cpp
│   │   │   ├── epsilon_greedy_test.cpp
│   │   │   ├── holon_pool_test.cpp
│   │   │   ├── holon_test.cpp
│   │   │   ├── governance_gate_test.cpp
│   │   │   ├── risk_classifier_test.cpp
│   │   │   └── sandbox_test.cpp
│   │   ├── compiler/
│   │   │   ├── observer_test.cpp
│   │   │   ├── evolver_test.cpp
│   │   │   ├── compiler_test.cpp
│   │   │   └── pattern_registry_test.cpp
│   │   └── dashboard/
│   │       ├── rest_api_test.cpp
│   │       └── sse_handler_test.cpp
│   └── integration/
│       ├── bus_integration_test.cpp
│       ├── engine_compiler_test.cpp
│       └── event_log_merge_test.cpp
│
├── scripts/
│   ├── evoclaw.sh
│   └── watchdog.sh
│
├── prompts/                                # ═══ Prompt Registry ═══
│   ├── facade/
│   │   ├── system_persona.yaml             # 元数据（id/version/depends/schema）
│   │   ├── system_persona.txt              # 模板正文
│   │   ├── task_analysis.yaml
│   │   └── task_analysis.txt
│   ├── router/
│   │   ├── intent_classification.yaml
│   │   └── intent_classification.txt
│   ├── compiler/
│   │   ├── pattern_recognition.yaml
│   │   ├── pattern_recognition.txt
│   │   ├── compilation.yaml
│   │   └── compilation.txt
│   └── holon/
│       ├── planner.yaml, planner.txt
│       ├── executor.yaml, executor.txt
│       └── critic.yaml, critic.txt
│
└── config/
    ├── evoclaw.yaml.example
    ├── risk_levels.yaml
    └── constitution.yaml
```

### 集成边界

**进程间通信（ZeroMQ）：**

```
engine ←──DEALER-ROUTER──→ compiler    (编译请求/结果)
engine ←──Pub-Sub──────→ dashboard   (事件广播)
engine ←──DEALER-ROUTER──→ judge       (盲测请求/结果, Phase 2)
compiler ──Pub-Sub──────→ dashboard  (编译进度事件)
```

**lib/ 内部依赖（单向）：**

```
config ← (无依赖)
protocol ← config
event_log ← protocol, config
bus ← protocol, config
memory ← event_log, config    # 记忆从事件日志派生视图（工作记忆提取活跃上下文、情景记忆构建任务叙事、Challenge Set 收集失败事件）
llm ← config
```

**可执行文件对 lib/ 的依赖：**

| 可执行文件 | lib 模块 | 额外依赖 |
|-----------|---------|---------|
| engine | bus, protocol, event_log, memory, llm, config | D++ (DPP) |
| compiler | bus, protocol, event_log, memory, llm, config | — |
| judge | bus, protocol, event_log, config | — |
| dashboard | bus, protocol, event_log, config | cpp-httplib (server) |

**运行时数据目录（`~/.evoclaw/`）：**

```
~/.evoclaw/
├── data/
│   ├── events/
│   │   ├── engine/                 # engine 进程写入
│   │   ├── compiler/               # compiler 进程写入
│   │   └── canonical/              # 合并后的 canonical log
│   └── memory/
│       ├── working_memory/
│       ├── episodic/
│       ├── user_model/
│       └── challenge_set/
├── prompts/                        # prompt 模板
├── config/evoclaw.yaml             # 运行时配置
└── logs/                           # spdlog 日志
```

## Architecture Validation

### 验证结果总览

| 维度 | 评分 | 说明 |
|------|------|------|
| 一致性 | 4.5/5 | 技术选型兼容，PRD↔架构不一致已标注 |
| 需求覆盖 | 4.5/5 | 58/63 FR 完全覆盖，5 个补充了实现细节 |
| 实现就绪度 | 4.5/5 | 8 个 gap 全部修复 |
| 总体 | **4.5/5 — 良好，可进入实现阶段** |

### PRD 回溯更新标注

> **Note [PRD-SYNC-1]:** PRD 中 DDS 相关描述（6.2, 7.2, 7.5 节）应回溯更新为"消息中间件（MVP: ZeroMQ，Phase 2 评估 DDS）"。ADR-3 为最终决策。

> **Note [PRD-SYNC-2]:** PRD FR61（向量数据库）和 FR62（事件存储引擎）应标注为 Phase 2。MVP 阶段由 ADR-4 文件系统方案覆盖。

### MVP 最终进程模型（ADR-2 + 约束松弛合并）

ADR-2 原文的 6 类进程是分析阶段初始方案。经约束松弛 #4 和 #9 分析，MVP 最终采纳 4 进程模型：

| 进程 | 包含组件 | 状态 |
|------|---------|------|
| evoclaw-engine | Facade + Router + Holon 工作池 + Discord Bot + Governance | MVP 核心 |
| evoclaw-compiler | Observer + Evolver + Compiler + Pattern Registry | MVP 核心 |
| evoclaw-dashboard | REST API + SSE + Web 面板 | MVP 核心 |
| evoclaw-judge | Meta Judge | Phase 2 stub（目录保留，代码最小化） |

### 补充设计：插件热替换协议（FR14）

类比 Erlang 双版本共存机制：

1. Compiler 通过 Req-Rep 向 engine 发送 `plugin.register` 消息，携带新版本插件
2. Engine Router 将新版本标记为 `current`，旧版本标记为 `old`
3. 新请求路由到 `current` 版本
4. 进行中的旧请求继续使用 `old` 版本
5. 旧版本所有进行中请求完成后（或超时 30s），卸载 `old` 版本
6. 观察期：新版本 ≥10 次调用内失败率 >20% 时，自动回滚到 `old`

### 补充设计：主动服务触发机制（FR8, Phase 2）

engine 进程内的主动服务触发器：

- **定时扫描**：每 30 分钟扫描用户模型，检测重复行为模式（≥3 次同类操作）
- **事件驱动**：订阅 event_log 的 `agent.task.completed` 事件，实时统计同类任务频率
- **触发条件**：同类操作 ≥3 次 且 最近一次在 24h 内
- **触发动作**：通过 Discord Bot 发送建议消息（非阻塞，用户可忽略）

### 补充设计：小脑消息预处理管线（FR37）

小脑（Cerebellum）位于 `lib/protocol/cerebellum.h/.cpp`：

```
raw message → Cerebellum::Pack()     # 压缩 context（S/M/L 密度）
            → Cerebellum::Validate()  # 调用 RuleEngine 验证约束
            → Cerebellum::DetectConflict()  # 冲突检测
            → dispatched message
```

- 调用时机：MessageBus 收到消息后、分发给目标 Agent 前
- Pack() 根据消息级别（L0/L1/L2）决定压缩程度
- Validate() 对 L2 消息调用 RuleEngine，L0/L1 跳过
- 异步模式：L0/L1 消息不阻塞，L2 验证失败后追溯处理

### Validation Checklist

| # | 检查项 | 状态 |
|---|--------|------|
| V1 | 技术选型兼容性（8 个依赖无冲突） | ✅ PASS |
| V2 | lib/ 依赖 DAG（无循环） | ✅ PASS |
| V3 | 命名规范一致性 | ✅ PASS（C4 已修复） |
| V4 | 错误类型转换关系 | ✅ PASS（C5 已修复） |
| V5 | PRD↔架构一致性 | ⚠️ 已标注回溯（PRD-SYNC-1/2） |
| V6 | 进程模型确定性 | ✅ PASS（C3 已合并） |
| V7 | FR 覆盖率（63 FR） | ✅ PASS（58 直接覆盖 + 5 补充设计） |
| V8 | NFR 覆盖率（24 NFR） | ✅ PASS（20 直接覆盖 + 4 可接受） |
| V9 | 热替换协议（FR14） | ✅ PASS（R1 已补充） |
| V10 | 主动服务触发（FR8） | ✅ PASS（R2 已补充） |
| V11 | 小脑管线（FR37） | ✅ PASS（R4 已补充） |
| V12 | 项目目录树完整性 | ✅ PASS（cerebellum 已补充） |
| V13 | 实现模式示例充分性 | ✅ PASS |
| V14 | AI Agent 实现一致性保障 | ✅ PASS（Enforcement Guidelines 完整） |

### Expert Panel Review 修订

基于三位领域专家（分布式系统/AI Agent/C++ 系统工程）评审，采纳以下修订：

**修订 1：ZeroMQ 拓扑从 REQ-REP 改为 DEALER-ROUTER**

原决策 2.1 选择 Pub-Sub + Req-Rep 混合。专家指出 REQ-REP 的锁步协议在对端崩溃时会导致 socket 死锁（无法恢复）。

修订后：
- 事件广播：Pub-Sub（不变）
- 请求响应：DEALER-ROUTER（替代 REQ-REP）
  - engine 运行 ROUTER socket，compiler/judge 运行 DEALER socket
  - 异步双向通信，不锁步，对端崩溃不影响 socket 状态
  - 代价：需手动管理消息信封（routing ID），实现稍复杂

**修订 2：MVP 砍掉 exactly-once QoS**

原设计声称支持三级 QoS（fire-and-forget / at-least-once / exactly-once）。专家指出 exactly-once 在 ZeroMQ + 文件系统架构下不可实现（需要 WAL + 分布式事务）。

修订后 MVP 只支持两级：
- L0：fire-and-forget（心跳、日志、通知）
- L1：at-least-once（任务请求、编译结果）——通过 Event Log msg_id 幂等检查去重
- L2 exactly-once：明确标注 Phase 2，需引入 WAL 机制

**修订 3：砍掉自研注册中心，改用配置文件**

原决策 2.3 选择"轻量自研注册中心"。专家指出 4 进程 MVP 不需要服务发现。

修订后：
- MVP：端点定义在 `evoclaw.yaml` 配置文件中（`ipc:///tmp/evoclaw-{process}.sock`）
- Phase 2：如需动态发现，用 ZeroMQ `zmq_proxy` 做 broker，不自研注册中心

**修订 4：Prompt 管理升级为 Prompt Registry**

原决策 1.2 选择"纯文本模板 + 占位符替换"。专家指出多 Agent 系统的 prompt 之间有依赖关系，需要版本管理和输出约束。

修订后 prompt 模板结构：
```yaml
# prompts/compiler/pattern_recognition.yaml
id: compiler.pattern_recognition
version: 1
depends_on: [holon.executor]  # 需理解 executor 输出格式
output_schema: json            # 期望 LLM 返回 JSON
template_file: pattern_recognition.txt
```
- 每个 prompt 有 YAML 元数据 + 纯文本模板
- PromptLoader 升级为 PromptRegistry，加载时验证依赖关系
- 支持版本回滚（保留历史版本文件）

**修订 5：锁定编译器最低版本**

专家指出 `std::expected` 在 GCC 12 中支持不完整。

修订后：
- 最低编译器要求：GCC 13+ 或 Clang 16+
- `cmake/compiler_options.cmake` 中添加版本检查

**修订 6：std::async 替换为自建线程池**

专家指出 `std::async` 的线程池行为不可控（实现定义，可能每次创建新线程）。

修订后异步 LLM 调用模式：
```cpp
// lib/ 新增 common/thread_pool.h
class ThreadPool {
 public:
  explicit ThreadPool(size_t num_threads);
  template <typename F>
  std::future<std::invoke_result_t<F>> Submit(F&& task);
};

// 使用
auto future = thread_pool.Submit([&] { return llm_client.Complete(prompt); });
```
- engine 进程：1 个线程池（Router 分发 + Holon 任务 + LLM 调用共享）
- compiler 进程：1 个独立线程池

### Graph of Thoughts 分析

**隐藏依赖链：**

除致命链 T1→O2→U1→D1 外，发现两条隐藏链：
- 形式验证链（5 层）：C5(Z3)→T2(≤100ms)→ADR-5(Cortex)→D2(混合协议)→D4(小脑 context)——规则引擎只覆盖 FL-0~FL-2，Phase 2 的 FL-3/FL-4 需要重新评估技术路径
- 进化质量链（6 层）：D1(Loop)→FR17(模式识别)→C7(≥3 次)→FR20(沙盒测试)→D3(双评估)→C10(不可变日志)——MVP 移除 Meta Judge 意味着 D3 假设不被验证

**隐藏冲突：**

1. ADR-2（Facade+Router+Holon 合并为单进程）vs NFR-可靠性（崩溃隔离）：Holon 执行系统操作（FR51-58）崩溃会拖垮整个 engine。**缓解：** Holon 的系统操作必须在 sandbox 子进程中执行（`src/engine/safety/sandbox.h`），崩溃不影响 engine 主线程
2. Token 预算不限制（决策 1.3）vs D4（小脑 context 管理）：不限制 token 会让小脑压缩机制失去意义。**缓解：** 保持"不限制总量"但引入"单次 context 窗口占比 ≤15%"软上限

**瓶颈节点：**

Event Log 是全架构连接度最高的节点（12 条边）——所有进程写入、记忆派生、审计链、Challenge Set、进化观察都依赖它。**决策：** Event Log（`lib/event_log/`）必须第一个达到生产质量，是整个系统的基石。

**孤儿节点修复：**

- D5（ε-greedy 路由）：补充与 Compiler 的联动——Compiler 编译产物注册后，Router 的 ε 值应动态调整（新插件上线初期提高探索率，观察期结束后降低）
- C8（探索区预算低 30%）：映射到实现——`src/engine/holon/holon_pool.h` 中 Holon 创建时根据所属区域（稳定/探索）设置 token 预算系数（1.0 / 0.7）

**涌现模式——渐进式复杂度原则：**

架构中隐含的核心原则：每个维度都支持独立升级，不需要一步到位。提升为显式架构原则：

| 维度 | MVP | 升级触发条件 | Phase 2 目标 |
|------|-----|-------------|-------------|
| 通信形式化 | FL-0~FL-2（规则引擎） | 规则引擎误判率 >5% | FL-3~FL-4（Z3） |
| 存储 | 文件系统 JSON/JSONL | 记忆检索延迟 >200ms | 向量库 + ES |
| 进程模型 | 4 类进程 | engine CPU 占用 >80% | 6 类进程 |
| 编译产物 | L0 路径缓存 + L1 模板 | L1 模板覆盖率 >60% | L2 完整插件 |
| QoS | fire-and-forget + at-least-once | 消息丢失率 >0.01% | exactly-once（WAL） |
| 评估 | 用户确认（单门控） | 编译产物 >10 个/月 | Meta Judge 逻辑隔离 |

**观察者悖论预警：**

Compiler 编译产物改变 Router 路由决策 → 某些协作路径不再被执行 → Observer 观察到的模式分布偏移 → 可能触发错误编译决策（类似强化学习 distribution shift）。

缓解措施：
- 编译产物上线观察期内，Observer 同时记录"如果不使用插件，Holons 会怎么处理"的反事实数据
- 定义进化稳定性指标：连续 3 个编译周期内 Router 路由分布变化 >30% 时，触发进化减速（提高编译阈值）

### PRD 回溯更新补充

> **Note [PRD-SYNC-3]:** PRD FR13 措辞应从"每个 Agent 作为独立进程运行"修订为"Agent 间通过消息传递通信，故障隔离通过进程级或沙盒级机制保证"。当前 MVP 的单进程多线程 + sandbox 方案满足此修订后的要求。

### MVP 里程碑定义

| 里程碑 | 范围 | 验证标准 |
|--------|------|---------|
| **M1 — 基础设施可通信** | lib/config + lib/protocol + lib/bus + lib/event_log + lib/common | 两个进程通过 ZeroMQ 互发 JSON 消息；`bus_integration_test.cpp` 通过 |
| **M2 — 单任务端到端** | lib/llm + lib/memory + src/engine（Facade+Router+Holon 基础版）| 用户通过 Discord 发任务 → engine 调 LLM → 返回结果；手动验证完整任务流 |
| **M3 — 进化闭环验证** | src/compiler + src/dashboard（基础版）| 重复同类任务 ≥3 次 → Compiler 识别模式 → 生成 L0/L1 产物；Toolization Loop 至少闭合一次（D1 假设验证） |

**硬检查点：** M1 是 T1→O2 风险链的验证点。如果 M1 未按预期完成，需立即评估是否调整范围或技术方案。

## Quick Start for Implementation

**给 AI Agent 的 1 页纸指南：**

**第一步：项目初始化**
- 创建目录结构（参考"完整项目目录树"章节）
- 配置 CMakeLists.txt + cmake/dependencies.cmake（FetchContent: GTest, nlohmann/json, cpp-httplib, cppzmq, yaml-cpp, spdlog, DPP）
- 系统依赖：`apt install libzmq3-dev`
- 编译器要求：GCC 13+ 或 Clang 16+
- 验证：空项目能编译通过

**第二步：按 M1 里程碑实现 lib/**
- 实现顺序：config → protocol → bus → event_log → common
- 每个模块：接口（纯虚基类）→ 实现 → 测试
- 关键模式：`std::expected<T, Error>` 返回值、Google C++ Style、OBJECT library

**第三步：按 M2 里程碑实现 engine**
- 实现顺序：llm → memory → engine(facade → router → holon)
- Discord Bot 用 D++ (DPP)
- 线程模型：Router 线程池 + Holon jthread

**第四步：按 M3 里程碑实现 compiler**
- Observer 监听 Event Log → 模式识别 → Compiler 生成 L0/L1 产物

**必读章节：**
- "Implementation Patterns & Consistency Rules" — 命名/错误处理/并发/测试的所有规范
- "Core Architectural Decisions" — 18 项技术决策
- "完整项目目录树" — 文件级实现指南
- "集成边界" — 进程间通信和 lib 依赖关系

### Party Mode 审视补充

**P1 — M1 验收标准细化：**

M1 验收定义为以下 3 项全部通过：
1. `evoclaw-engine` 和 `evoclaw-compiler` 两个进程独立启动，PID 文件正确写入
2. engine 通过 Pub-Sub 发送心跳事件，compiler 成功接收并打印日志
3. engine 通过 DEALER-ROUTER 发送 echo 请求，compiler 返回 echo 响应；`bus_integration_test.cpp` 通过

**P5 — PRD 回溯时机约束：**

> **约束：** 架构文档定稿后、Epic/Story 拆分前，必须完成 PRD-SYNC-1/2/3 的回溯写入 prd.md。Epic 拆分以回溯后的 PRD 为准。

**P6 — NFR 验证方法：**

| NFR | 验证方法 |
|-----|---------|
| NFR-1 响应延迟 < 2s | engine 集成测试：Discord 消息到达 → LLM mock 返回 → 响应发出，计时断言 < 2s |
| NFR-3 Token 预算精度 | BudgetTracker 单元测试：累计 token 与 LLM 返回 usage 字段对比，误差 < 1% |
| NFR-5 事件日志持久化 | event_log 单元测试：写入 → kill -9 → 重启 → 读取验证最后一条完整 |
| NFR-8 进程自动重启 | watchdog 集成测试：kill 进程 → 验证 3s 内 PID 文件更新 |

**P8 — ZeroMQ 测试 Mock 策略：**

- 单元测试：Mock `MessageBus` 接口（纯虚基类），hand-written fake 实现，不依赖真实 ZeroMQ
- 集成测试（`tests/integration/`）：使用 `inproc://` transport 做进程内真实 ZeroMQ 通信，无需网络
- 端到端测试（手动）：`tcp://` transport，多进程真实部署
