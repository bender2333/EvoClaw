# A2A Research — 文档索引

## 设计文档

| 文档 | 内容 | 关注点 | 状态 |
|------|------|--------|------|
| [cortex-architecture.md](cortex-architecture.md) | 仿脑皮层六层架构 | WHAT — 系统是什么 | v0.1 |
| [nltc-protocol-spec.md](nltc-protocol-spec.md) | NLTC-A2A 消息协议 | FORMAT — 消息长什么样 | v0.1 |
| [nltc-agent-integration.md](nltc-agent-integration.md) | Agent 集成方案 | HOW — Agent 怎么用 | v0.1 |
| [integration-plan.md](integration-plan.md) | OpenClaw 集成方案 | WHERE — 部署在哪 | v1.0 |

## 核心论文

- **Logitext**: `~/papers/logitext-neurosymbolic-smt.pdf` ([arXiv:2602.18095](https://arxiv.org/abs/2602.18095))
  - NLTC 概念来源，将 LLM 推理作为 SMT 理论

## 核心思想

LLM 是大脑，NLTC 小脑负责通信协调。Agent 通过工具调用与小脑交互，不需要懂 NLTC 协议。小脑处理约束验证、记忆管理、冲突检测，只返回精炼摘要给 LLM，最大化释放 context 窗口。
