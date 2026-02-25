# EvoClaw

> C++23 自进化多智能体组织框架

[![Tests](https://img.shields.io/badge/tests-71%20passed-brightgreen)]()
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue)]()
[![License](https://img.shields.io/badge/license-MIT-green)]()

## 特性

- **UMI 契约** — 统一模块接口，定义 Agent 能力边界和安全气囊
- **智能路由** — ε-greedy 算法，平衡利用与探索
- **双区架构** — 稳定区保障可靠性，探索区孵化创新
- **模式编译** — 自动检测重复协作模式，编译为可复用 Pattern
- **反熵机制** — 防止路由固化，保持系统适应性
- **SLP 压缩** — 多粒度语义原语，智能文本压缩
- **实时 Dashboard** — REST API + SSE 推送

## 快速开始

```bash
# 克隆
git clone https://github.com/your-org/EvoClaw.git
cd EvoClaw

# 构建
mkdir -p build && cd build
cmake ..
make -j4

# 测试
./tests/evoclaw_test

# 运行 Demo
export EVOCLAW_API_KEY="your-openai-compatible-key"
./examples/evoclaw_server_demo
# 访问 http://localhost:8080
```

## 架构

```
┌─────────────────────────────────────────┐
│            Web Dashboard                │
├─────────────────────────────────────────┤
│            EvoClawFacade                │
├─────────┬─────────┬─────────┬───────────┤
│ Router  │ Evolver │Compiler │ Entropy   │
├─────────┴─────────┴─────────┴───────────┤
│     Planner │ Executor │ Critic        │
├─────────────────────────────────────────┤
│  MessageBus │ Memory │ Governance      │
├─────────────────────────────────────────┤
│    LLMClient │ BudgetTracker │ SLP     │
└─────────────────────────────────────────┘
```

## API 示例

```bash
# 系统状态
curl http://localhost:8080/api/status

# 提交任务
curl -X POST http://localhost:8080/api/task \
  -H "Content-Type: application/json" \
  -d '{"description": "分析这段代码的性能问题"}'

# 触发进化
curl -X POST http://localhost:8080/api/evolve

# 查看预算
curl http://localhost:8080/api/budget

# 查看双区状态
curl http://localhost:8080/api/zones

# 查看熵度量
curl http://localhost:8080/api/entropy
```

## 配置

### LLM

API Key 解析顺序：
1. `EVOCLAW_API_KEY` 环境变量
2. `~/.openclaw/openclaw.json` → `models.providers.mynewapi.apiKey`
3. Mock 模式降级

### Router

```cpp
RoutingConfig config;
config.epsilon = 0.1;           // 10% 探索
config.cold_start_quota = 0.15; // 新 Agent 15% 流量保障
config.cold_start_days = 14;    // 冷启动期 14 天
```

## 文档

- [ARCHITECTURE.md](ARCHITECTURE.md) — 详细架构设计
- [doc_design/](doc_design/) — 设计文档 v1-v4

## 依赖

- C++23 (GCC 13+ / Clang 16+)
- CMake 3.25+
- nlohmann/json
- cpp-httplib
- Google Test

## License

MIT
