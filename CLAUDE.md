# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

EvoClaw 是一个以组织进化为核心的多 Agent AI 管家系统。核心理念：进化被制度化，组织被约束化，复杂度被预算化。

当前仓库状态：C++ 实现代码已在 `574ab18` 中删除，仅保留设计文档。完整实现代码可从 `8b09b98`（P1 阶段）回溯。

## 构建与测试（基于历史实现）

```bash
# 构建
mkdir build && cd build
cmake ..
make -j$(nproc)

# 运行全部测试（16 suites, 59 tests）
cd build && ctest --output-on-failure

# 运行单个测试
./build/tests/test_budget
./build/tests/test_slp

# 运行示例
./build/examples/evoclaw_basic_usage
./build/examples/evoclaw_evolution_demo
./build/examples/server_demo    # Web Dashboard on localhost:3000
```

技术栈：C++23, CMake 3.25+, GTest (FetchContent), nlohmann/json + cpp-httplib (header-only, third_party/)。

## 架构

### 多层架构（严格单向依赖：上层读取下层，下层不感知上层）

```
Facade（用户感知层）— 统一人格，内部组织对用户透明
    ↓
Governance Kernel（治理内核）— 不可自动替换，管发布/回滚/冻结/权限
    ↓
进化健康层 — Meta Judge（盲测A/B）、Challenge Curator、Dormancy Controller、Entropy Monitor
    ↓
进化层 — Observer + Evolver（Maintainer，不可自替）、Constitution、Compiler
    ↓
记忆层 — Canonical Event Log（不可变事件账本）+ 派生视图（Working Memory / User Model / Module Memory / Organization Log / Challenge Set）
    ↓
执行层（双区）— 稳定区（UMI 合约插件）+ 探索区（临时 Holons，强预算上限）
    ↓
工具化横切层 — UMI 接口标准、SLP 语义原语、Safety Airbag
```

### 源码模块映射（commit 8b09b98）

| 目录 | 对应架构层 |
|------|-----------|
| `src/facade/` | Facade 用户感知层 |
| `src/governance/` | Governance Kernel 治理内核 |
| `src/evolution/` | 进化层（Evolver + EvolutionBudget） |
| `src/memory/`, `src/event_log/` | 记忆层 |
| `src/router/`, `src/agent/` | 执行层（ε-greedy 路由 + Planner/Executor/Critic） |
| `src/umi/`, `src/slp/`, `src/protocol/` | 工具化横切层 |
| `src/budget/` | 资源预算（BudgetTracker per-agent token 追踪） |
| `src/llm/` | LLM 客户端（OpenAI 兼容接口） |
| `src/server/` | REST API + SSE 实时推送 |

### 关键设计约束

- Operational Critic 与 Meta Judge 物理隔离，防止评估坍缩
- Maintainer（Observer + Evolver）不可自动替换，避免自指悖论
- 探索区产物必须经 Governance Gate 晋升才能进入稳定区
- SLP 原语有完整生命周期：Draft → Active → Deprecated → Merged → Retired
- 反熵机制：5 个熵指标超阈值时自动触发压缩/合并/退役

## 设计文档结构

- `doc_design/EvoClaw_Unified_Design.md` — 统一设计文档（v1-v5 综合，最权威）
- `doc_design/evoclaw_v1-v4.md` — 各版本独立设计演进
- `doc_design/gptv5.md` — v5.0 自稳定进化 OS 设计
- `doc_design/a2a/` — A2A 协议研究：仿脑皮层六层架构（Cortex）+ NLTC 协议 + Google A2A 集成方案

## 语言

项目文档和注释使用中文。
