# P4 Task: Real LLM 接入（Bailian Kimi2.5）+ 智能进化升级

## 目标
把 EvoClaw 从 mock 演示系统升级为可真实运行的自进化系统：
1. 接通真实 LLM（优先 Bailian `kimi-k2.5`）
2. 升级进化引擎（EWMA 波动检测 + LLM 生成提案）
3. 提供可验证的端到端测试

## 范围
- 包含：`src/llm/`, `src/agent/`, `src/evolution/`, `tests/`, `examples/`
- 不包含：Dashboard 大改版、跨机分布式调度

## 子任务

### P4.1 LLM 接入与配置
- 读取 OpenClaw 配置（`~/.openclaw/openclaw.json`）中的 provider（默认 `bailian`）
- 支持从配置自动读取：`baseUrl` / `apiKey` / 默认模型
- 环境变量覆盖优先级：
  - `EVOCLAW_API_KEY`
  - `EVOCLAW_BASE_URL`
  - `EVOCLAW_MODEL`
- 失败时回退 mock，并输出明确错误

验收：
- 本地可用 `bailian/kimi-k2.5` 成功完成一次 `ask()`
- 无 key 时仍可跑 mock（现有测试不退化）

### P4.2 Agent 真执行链路
- Planner / Executor / Critic 统一走 `LLMClient`
- 任务预算（token/round/tool）接入执行流程
- Critic 输出结构化评分（`score + rationale + risks`）

验收：
- `submit_task()` 完整跑通：Plan -> Execute -> Critique
- OrgLog 有完整落库记录

### P4.3 进化引擎升级
- 在 `monitor()` 引入 EWMA：
  - `ewma_score`
  - `ewma_volatility`
- 触发条件改为自适应阈值，而不是固定阈值
- `propose()` 支持 LLM 生成候选 prompt/config patch

验收：
- 能从同一批日志中产出更稳定的 tension 检测结果
- EvolutionProposal 含具体 `new_value`（不再只是描述）

### P4.4 A/B 与门禁增强
- A/B 结果加入置信度和最小样本门槛
- Governance 门禁新增：
  - 失败回滚
  - 风险动作二次确认

验收：
- 低样本或改进不显著时，自动拒绝发布
- 事件账本记录完整

### P4.5 测试与示例
- 新增：
  - `test_llm_client.cpp`
  - `test_evolution_ewma.cpp`
  - `test_evolution_proposal_llm.cpp`
- 更新 `examples/evolution_demo`，展示一次真实进化循环

验收：
- 测试全绿
- 示例输出包含：tension -> proposal -> ab -> apply/rollback

## 风险与缓解
- 风险：外部 API 抖动导致测试不稳定
- 缓解：单元测试使用 mock；网络调用放 integration 标记

- 风险：LLM 生成提案不可控
- 缓解：Governance + schema 校验 + 白名单字段 patch

## 里程碑
- M1：P4.1 + P4.2
- M2：P4.3
- M3：P4.4 + P4.5

## 完成定义（DoD）
- 可用 Bailian Kimi2.5 跑通端到端任务
- 进化提案由 LLM 生成且可被治理层审查
- 测试全通过，回归无破坏
