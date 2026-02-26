# OpenClaw 集成 Google A2A 方案（原型）

## 1. 目标

将 OpenClaw 的 3 个 Agent（Bender / 博士 / Leela）升级为可通过 Google A2A 协议互通的多 Agent 系统，实现：

- Agent 独立部署与发现（`agent.json`）
- Router 智能选择单 Agent
- Orchestrator 支持圆桌工作流（依次发言）

## 2. 当前原型结构

- `agents/bender_agent.py`：管家 Agent（5001）
- `agents/doctor_agent.py`：架构研究 Agent（5002）
- `agents/leela_agent.py`：质疑者 Agent（5003）
- `network/router.py`：关键词策略路由
- `network/orchestrator.py`：A2A 客户端编排层（`ask_single` / `ask_roundtable` / `relay`）
- `main.py`：启动 3 个 Agent 并演示协作
- `test_a2a.py`：自动化验证脚本

## 3. 与 OpenClaw 的接入建议

1. **Agent Runtime 层接入**
- 将 OpenClaw 的每个 Agent 进程改为 A2A server 入口（保留原有能力实现）。
- 将现有 prompt/工具能力映射到 `AgentSkill`，对外在 `agent.json` 暴露。

2. **消息总线层接入**
- 在 OpenClaw 内部新增 `A2ATransport`（封装 `python_a2a.A2AClient`）。
- 所有 Agent 间调用走 `orchestrator.ask_single()` 或 `orchestrator.ask_roundtable()`，避免直接互调。

3. **路由层接入**
- 先使用 `SmartRouter` 关键词路由上线（可解释、低风险）。
- 第二阶段替换为 LLM Router（保留关键词策略作为 fallback）。

4. **观测与治理**
- 在 `orchestrator` 记录每次路由决策：`query / selected_agent / reason / latency`。
- 增加失败重试和超时策略：单 Agent 超时后回退 Bender。

## 4. 运行方式

```bash
# 演示启动 + 自动路由 + 圆桌
venv/bin/python main.py --mode both

# 仅圆桌
venv/bin/python main.py --mode roundtable --question "请大家讨论 A2A 集成风险"

# 测试脚本
venv/bin/python test_a2a.py
```

## 5. 集成到现有 OpenClaw 的最小改动

1. 把 `agents/*.py` 中的 `handle_message` 改为调用你们现有 Agent 核心推理函数。
2. 在 OpenClaw 的“任务分发入口”替换为 `network/router.py + network/orchestrator.py`。
3. 保留端口约定（5001/5002/5003），并在配置中心声明 endpoint。

## 6. 后续增强

- 把 roundtable 从串行改为并行 + 汇总（缩短耗时）。
- 对话上下文持久化（conversation_id -> storage）。
- 接入服务注册中心（python-a2a discovery）以支持动态扩缩容。
