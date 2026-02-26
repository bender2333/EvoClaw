---
project: EvoClaw
sprintDuration: 2 weeks
totalSprints: 6
mvpTimeline: 12 weeks (3 months)
developer: 1 (bender)
createdAt: '2026-02-26'
methodology: Scrum-solo (2-week sprints, no ceremonies overhead)
---

# EvoClaw Sprint Plan — Phase 1 MVP

## 总览

| Sprint | 周次 | 里程碑 | 目标 | Epic |
|--------|------|--------|------|------|
| S1 | W1-W2 | — | 项目骨架 + 核心 lib 模块 | Epic 1 (1.1-1.3) |
| S2 | W3-W4 | **M1** | 消息总线 + 事件日志 + 双进程通信验证 | Epic 1 (1.4-1.6) |
| S3 | W5-W6 | — | LLM 客户端 + 记忆 + Discord Bot + Facade | Epic 2 (2.1-2.4) |
| S4 | W7-W8 | **M2** | Router + 工作记忆集成 + 端到端验证 | Epic 2 (2.5-2.7), Epic 7 (7.1-7.2) |
| S5 | W9-W10 | — | Holons 协作 + 系统控制 + 通信协议 | Epic 3, Epic 4 (4.1-4.2, 4.6), Epic 7 (7.3-7.5) |
| S6 | W11-W12 | **M3** | Compiler 进化闭环 + Dashboard + 反馈学习 | Epic 5, Epic 6, Epic 8 |

## 风险检查点

| 时间 | 检查 | 不通过则 |
|------|------|----------|
| S2 结束（W4） | M1 达成？两进程 ZeroMQ 通信正常？ | 评估 C++ 可行性，考虑技术方案调整 |
| S4 结束（W8） | M2 达成？Discord→LLM→回复端到端跑通？ | 砍 Epic 4/7/8 非核心 Story，聚焦 M3 |
| S5 结束（W10） | Holons 3 角色协作跑通？ | 简化 Compiler 为半自动模式，降低 M3 门槛 |

---

## Sprint 1（W1-W2）：项目骨架与核心模块

**目标：** 从零搭建 C++23 项目，核心 lib 模块可编译可测试。

| Story | 描述 | 估时 | 验收 |
|-------|------|------|------|
| 1.1 | 项目骨架与构建系统初始化 | 3d | CMake + FetchContent 8 依赖 + 空测试通过 |
| 1.2 | 配置加载模块（lib/config） | 2d | YAML 加载 + 环境变量覆盖 + 3 场景测试 |
| 1.3 | 消息协议与序列化（lib/protocol） | 3d | 消息信封 JSON 序列化/反序列化 + RuleEngine 接口 |

**产出：** 项目可编译，lib/config + lib/protocol + lib/common 就绪，GTest 测试通过。

**Sprint 1 Definition of Done：**
- `cmake .. && make` 零错误零警告
- `ctest` 全部通过（config + protocol + common 测试）
- 目录结构与架构文档一致

---

## Sprint 2（W3-W4）：消息总线与事件日志 → M1

**目标：** 两个进程通过 ZeroMQ 通信，事件日志可持久化。**M1 里程碑达成。**

| Story | 描述 | 估时 | 验收 |
|-------|------|------|------|
| 1.4 | ZeroMQ 消息总线（lib/bus） | 3d | Pub-Sub + DEALER-ROUTER，inproc:// 测试通过 |
| 1.5 | 事件日志持久化（lib/event_log） | 3d | append-only JSONL + SHA-256 + 异步写入 + 查询 |
| 1.6 | M1 集成验证——双进程通信 | 2d | engine↔compiler 心跳 + echo + 启停脚本 |

**产出：** evoclaw-engine 和 evoclaw-compiler 最小进程可启动、通信、记录事件。

**M1 验收标准：**
1. 两进程独立启动，PID 文件写入 `~/.evoclaw/`
2. engine Pub-Sub 心跳 → compiler 接收
3. engine DEALER-ROUTER echo → compiler 响应
4. `bus_integration_test.cpp` 通过

**⚠️ 风险检查点：** M1 是致命风险链 T1→O2 的验证点。如果 ZeroMQ + C++23 集成遇到严重阻碍，此处决策是否调整技术方案。

---

## Sprint 3（W5-W6）：LLM 客户端与 Discord Bot

**目标：** 系统能调用 LLM、记住用户偏好、通过 Discord 对话。

| Story | 描述 | 估时 | 验收 |
|-------|------|------|------|
| 2.1 | LLM 客户端与 Prompt Registry（lib/llm） | 3d | OpenAI HTTP 客户端 + 重试 + Prompt 模板加载 |
| 2.2 | 记忆存储模块（lib/memory） | 2d | 文件系统存储 + 工作记忆 + 用户模型 |
| 2.3 | Discord Bot 基础连接 | 2d | DPP Bot 收发消息 + Slash Commands 注册 |
| 2.4 | Facade 统一人格层 | 2d | 人格 prompt 注入 + 进度摘要 + 失败恢复选项 |

**产出：** Discord 发消息 → Facade 组装 prompt → LLM 回复 → Discord 返回。记忆模块可读写。

---

## Sprint 4（W7-W8）：路由与端到端验证 → M2

**目标：** 完整的单任务端到端流程。**M2 里程碑达成。** 同时启动通信协议基础。

| Story | 描述 | 估时 | 验收 |
|-------|------|------|------|
| 2.5 | Fast Router 基础路由 | 2d | 插件匹配 + 未命中转发 Holons + Event Log 记录 |
| 2.6 | 工作记忆与用户模型集成 | 2d | 会话上下文注入 + 偏好适配 + token 追踪 |
| 2.7 | M2 端到端验证 | 1d | Discord 完整任务流 + Event Log 全链路 |
| 7.1 | 混合协议消息格式 | 2d | formal + semantic 约束字段 + 序列化 |
| 7.2 | 规则引擎形式约束验证 | 2d | BasicRuleEngine 数值/范围/布尔验证 ≤100ms |

**M2 验收标准：**
1. Discord 发"帮我总结 Rust vs C++"→ 管家回复结构化对比
2. 回复格式符合用户模型偏好
3. Event Log 记录完整轨迹（接收→路由→LLM→回复）
4. token 使用量正确追踪

**产出：** 单任务端到端可用，混合协议基础就绪。

---

## Sprint 5（W9-W10）：多 Agent 协作与系统控制

**目标：** 复杂任务由 Holons 团队协作完成，管家具备系统操作能力，通信协议完善。

| Story | 描述 | 估时 | 验收 |
|-------|------|------|------|
| 3.1 | Dynamic Holons 组队框架 | 2d | Planner/Executor/Critic 组队 + jthread |
| 3.2 | Holon 内部协作流程 | 2d | 三阶段协作 + Critic 重试 + Event Log |
| 3.3 | 并行子任务执行 | 1d | 线程池并行 + 依赖排序 |
| 4.1 | 文件系统操作能力 | 1d | FileOps + 风险分级 + Event Log |
| 4.2 | Shell 命令执行能力 | 1d | ShellOps + 沙盒 + 超时 |
| 4.6 | 风险分级与权限管理 | 1d | RiskClassifier L0-L3 + 确认流程 |
| 7.3 | 语义约束 LLM 评估 | 1d | L2 消息语义评估 + 批量化 |
| 7.4 | 小脑消息预处理管线 | 1d | Pack + Validate + DetectConflict |

**产出：** 复杂任务 Holons 协作完成，文件/Shell 操作可用，通信协议管线就绪。

**注意：** Epic 4 的 Story 4.3-4.5（进程管理/网络/代码执行/定时任务）推迟到 M3 后或 Phase 2，MVP 聚焦文件+Shell 两个最核心能力。

---

## Sprint 6（W11-W12）：进化闭环与仪表盘 → M3

**目标：** Toolization Loop 至少闭合一次。Dashboard 基础可用。反馈学习机制就绪。**M3 里程碑达成。**

| Story | 描述 | 估时 | 验收 |
|-------|------|------|------|
| 5.1 | Observer 协作轨迹监听 | 1.5d | 轨迹提取 + 规范化 + 候选池 |
| 5.2 | 模式识别与 Pattern 生成 | 1.5d | ≥3 次匹配 → Pattern 生成 |
| 5.3 | Compiler 编译 Pattern 为插件 | 1.5d | LLM 编译 + 产物格式 |
| 5.4 | 沙盒测试与用户确认 | 1d | 回放测试 + Discord 确认 |
| 5.5 | 插件替换与 M3 闭环验证 | 1d | Router 调用插件 + 端到端闭环 |
| 6.1 | 任务完成反馈征询 | 0.5d | thumbs up/down + 超时默认 |
| 8.1 | Dashboard REST API 与基础面板 | 1.5d | /api/status + SSE + 静态页面 |
| 8.4 | 结构化指令实现 | 0.5d | /status /health /plugins /evolution |

**M3 验收标准：**
1. 重复同类任务 ≥3 次 → Compiler 识别模式
2. 编译产物通过沙盒测试 + 用户确认
3. Router 成功调用插件，同类任务不再触发 Holons
4. Event Log 记录完整进化链路
5. Dashboard 可查看系统状态

**产出：** MVP 核心假设验证——Toolization Loop 闭合。

---

## Sprint 6 后：收尾与稳定化（W12+）

M3 达成后，剩余 Story 按优先级补充：

| 优先级 | Story | 说明 |
|--------|-------|------|
| P1 | 5.6 | Toolization Loop 9 步治理流程（灰度发布 + 回滚） |
| P1 | 8.5 | 组件热替换 |
| P1 | 8.6 | 进程管理脚本与 Watchdog |
| P2 | 6.2-6.4 | 失败上下文记录 + 智能重试 + 经验模板 |
| P2 | 7.5-7.6 | SLP 梯度压缩 + 约束模板复用 |
| P2 | 8.2-8.3 | 进化日志面板 + Agent 健康面板 |
| P3 | 4.3-4.5 | 进程管理 + 网络操作 + 代码执行/定时任务 |

---

## Story 统计

| 类别 | Sprint 内 | 推迟 | 总计 |
|------|-----------|------|------|
| Epic 1（基础设施） | 6 | 0 | 6 |
| Epic 2（管家对话） | 7 | 0 | 7 |
| Epic 3（多 Agent 协作） | 3 | 0 | 3 |
| Epic 4（系统控制） | 3 | 3 | 6 |
| Epic 5（进化闭环） | 5 | 1 | 6 |
| Epic 6（反馈学习） | 1 | 3 | 4 |
| Epic 7（通信协议） | 5 | 1 | 6 |
| Epic 8（仪表盘运维） | 2 | 4 | 6 |
| **合计** | **32** | **12** | **44** |

Phase 1 共 44 个 Story，Sprint 1-6 覆盖 32 个核心 Story（73%），剩余 12 个为 M3 后补充。

---

## 依赖关系图

```
S1: [1.1] → [1.2] → [1.3]
         ↓
S2: [1.4] → [1.5] → [1.6=M1]
         ↓
S3: [2.1] → [2.2] → [2.3] → [2.4]
         ↓
S4: [2.5] → [2.6] → [2.7=M2]    [7.1] → [7.2]
         ↓                          ↓
S5: [3.1] → [3.2] → [3.3]    [7.3] → [7.4]
    [4.1] [4.2] [4.6]
         ↓
S6: [5.1] → [5.2] → [5.3] → [5.4] → [5.5=M3]
    [6.1] [8.1] [8.4]
```

---

## 每日节奏建议

- 上午：编码实现（3-4h 深度工作）
- 下午：测试 + 调试 + 文档（2-3h）
- 每周五：回顾本周进度，调整下周计划
- Sprint 结束日：运行全量测试，更新 Sprint 状态
