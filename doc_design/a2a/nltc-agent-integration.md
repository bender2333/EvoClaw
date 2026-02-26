# NLTC Agent 集成方案：Agent 怎么支持 NLTC

> 项目：A2A Research / Cortex Architecture
> 作者：Fry & Bender
> 日期：2026-02-25
> 状态：草案 v0.1
> 核心问题：Agent 是 LLM，只会说自然语言。NLTC 是结构化协议。怎么桥接？

---

## 0. 核心原则

**Agent 不说 NLTC。Agent 说人话。小脑说 NLTC。**

Agent 通过工具调用与小脑交互，就像用 web_search 不需要懂 HTTP 一样。

```
错误思路：
  让 Agent 学会 NLTC 语法 → 烧 context，格式不可靠

正确思路：
  Agent 调用 cortex 工具 → 小脑处理 NLTC → Agent 只看摘要
```

---

## 1. 工具接口设计

### 1.1 Cortex 工具集

Agent 可用的工具（注册为 OpenClaw skill 的 tools）：

```yaml
tools:
  # ---- 任务管理 ----
  - name: cortex_dispatch
    description: "派发任务给其他 Agent"
    params:
      target: string       # 目标 Agent (doctor/leela)
      task: string         # 任务描述（自然语言，一句话）
      deadline: int?       # 可选，秒
      constraints: object? # 可选，额外约束 {key: "condition"}
    returns: string        # 摘要："已派给博士，预计30分钟"

  - name: cortex_status
    description: "查看任务状态"
    params:
      task_id: string?     # 可选，不填则返回所有活跃任务
    returns: string        # 摘要："博士正在改代码，已60%"

  - name: cortex_result
    description: "获取任务结果"
    params:
      task_id: string
      detail: bool?        # 是否要详细信息
    returns: string        # 摘要或详情

  # ---- 协商 ----
  - name: cortex_roundtable
    description: "发起多 Agent 讨论"
    params:
      agents: list[string] # 参与者
      topic: string        # 讨论主题
      deadline: int?
    returns: string        # 讨论结果摘要

  # ---- 记忆查询 ----
  - name: cortex_recall
    description: "查询历史任务或 Agent 信息"
    params:
      query: string        # 自然语言查询
    returns: string        # 结构化结果的摘要

  # ---- 约束管理 ----
  - name: cortex_add_constraint
    description: "为当前任务添加约束"
    params:
      task_id: string
      constraint: string   # "test_pass_rate >= 0.95" 或自然语言
    returns: string        # 确认
```

### 1.2 Agent 视角的使用方式

Agent 的 system prompt 里只需要加一段：

```markdown
## Cortex 工具

你有一个"小脑"帮你协调与其他 Agent 的通信。用以下工具：

- cortex_dispatch: 派任务给博士或 Leela
- cortex_status: 查任务进度
- cortex_result: 拿结果
- cortex_roundtable: 发起讨论
- cortex_recall: 查历史

你不需要关心消息格式、约束验证、记忆管理。小脑会处理。
你只需要：说清楚要做什么，看小脑返回的摘要，做决策。
```

**这段 prompt 只有 ~150 tokens。** 比把 NLTC 规范塞进去（~2000 tokens）省了 10 倍。

---

## 2. 小脑内部：从工具调用到 NLTC

### 2.1 dispatch 的完整流程

```
Agent 调用:
  cortex_dispatch(
    target="doctor",
    task="改 Z3 封装的错误处理，覆盖 Leela 的反馈",
    deadline=1800,
    constraints={"test_pass_rate": ">= 0.95"}
  )

小脑内部处理（不需要 LLM）：

Step 1: 意图分类
  task 文本 → 关键词匹配 → task_type = "code_modify"
  规则：包含"改/修改/重构" + "代码/封装/模块" → code_modify

Step 2: 模板匹配
  task_type = "code_modify" → 模板库查找 → code_task 模板

Step 3: 记忆查询
  "Z3 封装" → 情景记忆搜索 → task_041 (最近的 Z3 相关任务)
  "Leela 的反馈" → 情景记忆搜索 → task_041_review

Step 4: 约束组装
  模板默认约束:
    test_pass_rate >= 0.95  (模板默认)
    lint_errors <= 0        (模板默认)
  Agent 指定约束:
    test_pass_rate >= 0.95  (Agent 显式指定，覆盖默认)
    deadline <= 1800        (Agent 指定)
  语义记忆推断:
    博士改代码平均 1500s → deadline 合理

Step 5: 语义约束（从任务描述提取）
  "错误处理" → 模板中有预定义语义约束:
    {{let error_handling = [[代码包含完善的错误处理]]}}
  "覆盖 Leela 的反馈" → 附加上下文:
    bindings: {"反馈内容": task_041_review.feedback}

Step 6: 打包 NLTC 消息
  → 完整的 NLTC-A2A 消息（Agent 看不到这个）

Step 7: 发送 + 注册工作记忆

Step 8: 返回摘要给 Agent
  "已派给博士修改 Z3 封装错误处理，附上 Leela 反馈，
   deadline 30分钟，约束：测试≥95%、无 lint 错误"
```

### 2.2 关键：Step 1 意图分类不需要 LLM

```python
class IntentClassifier:
    """基于规则的意图分类器 — 不需要 LLM"""

    PATTERNS = {
        "code_create": {
            "keywords": ["写", "实现", "创建", "开发", "搭建"],
            "context": ["代码", "模块", "函数", "类", "API", "服务"]
        },
        "code_modify": {
            "keywords": ["改", "修改", "修复", "重构", "优化", "更新"],
            "context": ["代码", "模块", "函数", "bug", "错误", "封装"]
        },
        "code_review": {
            "keywords": ["审查", "review", "检查", "看看"],
            "context": ["代码", "PR", "提交", "改动"]
        },
        "research": {
            "keywords": ["调研", "研究", "分析", "查", "找", "搜"],
            "context": ["论文", "方案", "技术", "市场", "数据"]
        },
        "arbitrage": {
            "keywords": ["套利", "分析", "监控", "扫描"],
            "context": ["市场", "Polymarket", "DEX", "价格", "价差"]
        },
        "discuss": {
            "keywords": ["讨论", "商量", "评估", "辩论"],
            "context": ["方案", "设计", "架构", "策略"]
        }
    }

    def classify(self, task_text: str) -> str:
        """
        规则匹配，O(1) 复杂度，零 LLM 开销
        """
        scores = {}
        for task_type, pattern in self.PATTERNS.items():
            score = 0
            for kw in pattern["keywords"]:
                if kw in task_text:
                    score += 2
            for ctx in pattern["context"]:
                if ctx in task_text:
                    score += 1
            scores[task_type] = score

        best = max(scores, key=scores.get)
        if scores[best] >= 3:  # 至少匹配一个关键词+一个上下文
            return best
        return "generic"  # 无法分类，用通用模板
```

**覆盖率：** 日常 80%+ 的任务可以被规则分类。剩下的用 "generic" 模板兜底。

### 2.3 如果规则分类失败怎么办？

```
方案 A: 用 generic 模板（只有基本约束）
  → 能用，但约束不够精确

方案 B: 问 Agent 一个选择题（不是开放问题）
  小脑 → Agent: "这个任务最接近哪类？
    1. 写代码  2. 改代码  3. 代码审查
    4. 调研    5. 套利分析  6. 讨论"
  Agent: "2"
  → 一次交互，几个 token，解决分类

方案 C: 用小模型分类（本地跑，不占 API 额度）
  task_text → 本地 classifier → task_type
  → 可以用 sentence-transformers 做语义匹配
```

**推荐：A 为默认，B 为 fallback。** 不引入额外模型依赖。

---

## 3. 接收端：博士/Leela 怎么处理 NLTC 消息

### 3.1 两种模式

```
模式 A: 透明模式（推荐）
  博士的 LLM 看不到 NLTC 格式
  小脑把 NLTC 消息转换为自然语言 prompt 给博士
  博士用自然语言返回结果
  小脑把结果转换回 NLTC 格式

模式 B: 原生模式（高级）
  博士的 LLM 直接看到结构化任务描述
  博士返回结构化结果
  适合高频、模式固定的任务
```

### 3.2 透明模式详解

```
NLTC 消息到达博士的小脑：

Step 1: 小脑解包 NLTC 消息

Step 2: 生成自然语言 prompt（模板化，不需要 LLM）

  prompt_template = """
  任务：{task}
  
  背景：
  - 上次任务：{previous_task_summary}
  - 相关反馈：{feedback}
  
  要求：
  - 测试通过率 ≥ 95%
  - 无 lint 错误
  - 30 分钟内完成
  - 错误处理要覆盖：空输入、超时、类型错误
  
  请完成任务并返回修改后的代码。
  """

  → 这个 prompt 是从 NLTC 消息机械生成的，不需要 LLM

Step 3: 博士 LLM 执行任务，返回自然语言结果

Step 4: 小脑从结果中提取结构化数据

  博士说: "已修改完成，所有测试通过（98%），
          用了 20 分钟，代码在 ~/codes/z3_wrapper.py"

  小脑提取（规则 + 正则）:
    test_pass_rate = 0.98    # 正则: \d+% → float
    time_spent = 1200        # 正则: \d+ 分钟 → 秒
    artifacts = ["~/codes/z3_wrapper.py"]

Step 5: 打包 NLTC 响应，发回 Bender 的小脑
```

### 3.3 结果提取器

```python
import re

class ResultExtractor:
    """从 Agent 自然语言输出中提取结构化数据"""

    EXTRACTORS = {
        "test_pass_rate": [
            r"通过率[：:\s]*(\d+(?:\.\d+)?)[%％]",
            r"(\d+(?:\.\d+)?)[%％]\s*通过",
            r"pass[_ ]rate[：:\s]*(\d+(?:\.\d+)?)",
        ],
        "time_spent": [
            r"用了[：:\s]*(\d+)\s*分钟",
            r"耗时[：:\s]*(\d+)\s*分钟",
            r"(\d+)\s*分钟.*完成",
            r"(\d+)\s*min",
        ],
        "lint_errors": [
            r"(\d+)\s*个?\s*lint\s*错误",
            r"lint[：:\s]*(\d+)",
            r"无\s*lint\s*错误",  # → 0
        ],
        "file_path": [
            r"(~/[\w/\-\.]+\.\w+)",
            r"(/home/[\w/\-\.]+\.\w+)",
        ]
    }

    def extract(self, text: str, variables: dict) -> dict:
        """从文本中提取变量值"""
        assignment = {}

        for var_name, var_type in variables.items():
            if var_name not in self.EXTRACTORS:
                continue

            for pattern in self.EXTRACTORS[var_name]:
                match = re.search(pattern, text)
                if match:
                    raw = match.group(1) if match.lastindex else "0"
                    if var_type == "float":
                        assignment[var_name] = float(raw) / 100 \
                            if "%" in match.group(0) else float(raw)
                    elif var_type == "int":
                        val = int(raw)
                        if "分钟" in match.group(0) or "min" in match.group(0):
                            val *= 60  # 转换为秒
                        assignment[var_name] = val
                    elif var_type == "str":
                        assignment[var_name] = raw
                    break

            # 特殊处理："无 lint 错误" → 0
            if var_name == "lint_errors" and var_name not in assignment:
                if re.search(r"无\s*lint|0\s*个?\s*lint|lint.*clean", text):
                    assignment[var_name] = 0

        return assignment
```

### 3.4 提取失败怎么办？

```
如果正则提取不到某个变量：

方案 A: 问 Agent 一个精确问题
  小脑 → 博士: "测试通过率具体是多少？回答一个数字。"
  博士: "0.98"
  → 一次交互，几个 token

方案 B: 标记为 unknown，让 Bender 决定是否接受
  小脑 → Bender: "✓ 博士完成，但测试通过率未报告，其他指标正常"
  Bender 决定是否追问

方案 C: 要求 Agent 用固定格式返回关键指标
  在 prompt 末尾加:
  "完成后请附上：通过率=X%，耗时=X分钟，文件=路径"
  → 大部分 LLM 会遵循这个格式
```

**推荐：C 为默认（在 prompt 中引导格式），A 为 fallback。**

---

## 4. 渐进式支持：从零到完整

### Phase 0: 纯 NL（现状）

```
Bender LLM ←→ 自然语言 ←→ 博士 LLM
完全没有小脑，所有通信在 LLM context 中
```

### Phase 1: 加入小脑路由（最小改动）

```
Bender LLM → cortex_dispatch() → 小脑 → NL prompt → 博士 LLM
                                  ↓
                              记忆存储
                                  ↓
Bender LLM ← 摘要 ← 小脑 ← NL 结果 ← 博士 LLM

改动：
  + Bender 的 system prompt 加 cortex 工具描述（~150 tokens）
  + 小脑进程（Python，无 GPU）
  + 记忆目录结构
  
不改：
  博士和 Leela 完全不变
  通信内容还是自然语言
  
收益：
  Bender 的 context 中不再有完整的任务描述和结果
  只有摘要（~50 tokens vs ~3000 tokens）
```

### Phase 2: 加入形式约束验证

```
在 Phase 1 基础上：
  + 模板库（JSON 文件）
  + Z3 验证
  + 结果提取器（正则）

改动：
  小脑在发送前自动附加形式约束
  小脑在收到结果后自动 Z3 验证
  博士的 prompt 末尾加格式引导

收益：
  数值类约束自动验证，不需要 LLM 判断
  自动发现数学错误（如套利分析的例子）
```

### Phase 3: 加入语义约束

```
在 Phase 2 基础上：
  + 语义约束定义
  + LLM 评估（带缓存）
  + 渐进式形式化

改动：
  模板中增加语义约束字段
  小脑在验证时按需调 LLM 评估语义约束

收益：
  "代码质量好不好"这类主观判断也能自动化
  随着缓存积累，LLM 调用越来越少
```

### Phase 4: 加入多 Agent 协商

```
在 Phase 3 基础上：
  + roundtable 工具
  + 约束合并与冲突检测
  + 协商协议

改动：
  新增 cortex_roundtable 工具
  小脑支持多 Agent 约束合并

收益：
  自动发现 Agent 间的分歧
  LLM 只需要做最终决策
```

---

## 5. 对现有 Agent 的改动量

### Bender（主 Agent / 路由器）

```diff
  改动：
+ system prompt 加 ~150 tokens 的 cortex 工具描述
+ 可用工具列表加 5 个 cortex_* 工具
  
  不改：
  SOUL.md, IDENTITY.md, USER.md 不变
  现有工具（read, write, exec 等）不变
  Telegram 通道不变
```

### 博士 / Leela（执行 Agent）

```diff
  Phase 1-2 改动：零
  （小脑在外部处理，博士看到的还是自然语言 prompt）

  Phase 3 可选改动：
+ prompt 末尾加格式引导（~30 tokens）
  "完成后请附上：通过率=X%，耗时=X分钟，文件=路径"
```

**这是最关键的设计决策：执行 Agent 不需要任何改动。** 所有 NLTC 逻辑都在小脑中，对 Agent 透明。

---

## 6. 小脑的部署形态

### 方案 A: OpenClaw Skill（推荐）

```
~/.openclaw/skills/cortex/
├── SKILL.md           # Skill 描述
├── cortex.py          # 小脑主进程
├── intent.py          # 意图分类器
├── extractor.py       # 结果提取器
├── memory.py          # 记忆层
├── formal.py          # Z3 形式验证
├── semantic.py        # 语义约束评估
├── templates/         # 约束模板库
│   ├── code_task.json
│   └── ...
└── requirements.txt   # z3-solver
```

Agent 通过 OpenClaw 的 tool 机制调用小脑功能。

### 方案 B: 独立进程 + HTTP API

```
小脑作为独立服务运行在 localhost:5010
Agent 通过 HTTP 调用

优点：可以被多个 Agent 共享
缺点：多一个进程要管理
```

### 方案 C: 嵌入 OpenClaw Gateway

```
小脑逻辑嵌入 OpenClaw 的 Gateway 进程
作为消息中间件自动处理

优点：最透明，Agent 完全无感
缺点：需要改 OpenClaw 源码
```

**推荐 Phase 1-3 用方案 A（Skill），Phase 4+ 考虑方案 B。**

---

## 7. 完整示例：从用户请求到任务完成

```
用户 Fry 在 Telegram 说：
  "帮我让博士分析下 Polymarket 的套利机会"

=== Bender LLM 处理 ===

Bender 看到用户消息，决定用 cortex 派发：

  cortex_dispatch(
    target="doctor",
    task="分析 Polymarket 上 BTC 相关市场的套利机会",
    deadline=3600
  )

=== 小脑处理（Bender 侧）===

1. 意图分类: "分析" + "套利" + "市场" → arbitrage
2. 模板匹配: arbitrage_analysis
3. 记忆查询: 上次套利分析 task_038，博士耗时 45 分钟
4. 约束组装:
   formal:
     - fee_rate >= 0
     - slippage >= 0
     - net_profit > fee_rate + slippage
     - min_profit_rate >= 0.01
     - time_spent <= 3600
5. 打包 NLTC 消息
6. 发送给博士的小脑

=== 小脑处理（博士侧）===

7. 解包 NLTC 消息
8. 生成自然语言 prompt:
   "分析 Polymarket 上 BTC 相关市场的套利机会。
    上次分析参考：task_038。
    要求：
    - 覆盖市场内套利和跨市场组合套利
    - 考虑手续费和滑点
    - 1小时内完成
    完成后请附上：发现机会数=X，最低利润率=X%，
    手续费率=X%，滑点=X%，净利润=X%，耗时=X分钟"

=== 博士 LLM 执行 ===

9. 博士正常工作，用 web_fetch 抓数据，分析，返回结果:
   "分析完成。发现 3 个套利机会。
    发现机会数=3，最低利润率=2.3%，
    手续费率=2%，滑点=0.5%，净利润=2.3%，耗时=45分钟
    [详细分析...]"

=== 小脑处理（博士侧）===

10. 提取结构化数据:
    opportunities_found=3, min_profit_rate=0.023,
    fee_rate=0.02, slippage=0.005, net_profit=0.023,
    time_spent=2700

11. 打包 NLTC 响应，发回 Bender 小脑

=== 小脑处理（Bender 侧）===

12. Z3 形式验证:
    fee_rate=0.02 >= 0 ✓
    slippage=0.005 >= 0 ✓
    net_profit=0.023 > fee_rate+slippage=0.025 ✗ ← 失败！

13. 返回摘要给 Bender LLM:
    "✗ 博士套利分析有数学错误：声称净利润2.3%，
     但费率2%+滑点0.5%=2.5%，净利润不够覆盖成本。"

=== Bender LLM 决策 ===

14. Bender 回复 Fry:
    "博士分析了 Polymarket，找到 3 个机会，
     但我验证发现他的利润计算有误——
     声称净赚 2.3%，但成本就要 2.5%。
     让他重新算一下？"

=== 整个过程中 Bender 的 context 使用 ===

  用户消息:          ~30 tokens
  cortex_dispatch:   ~50 tokens (工具调用)
  小脑返回摘要:      ~60 tokens
  Bender 回复:       ~80 tokens
  总计:              ~220 tokens

  对比纯 NL 方式:    ~3000+ tokens
  压缩比:            13x
```

---

## 8. 开放问题

1. **意图分类的覆盖率** — 规则能覆盖多少？需要实际数据验证
2. **结果提取的准确率** — 正则能提取多少？格式引导的遵循率？
3. **小脑的启动开销** — Z3 import 时间、模板加载时间
4. **多语言支持** — 当前规则是中文，英文任务怎么办？
5. **错误恢复** — 小脑自身出错（crash/bug）时的降级策略
6. **博士/Leela 的自主性** — 透明模式下，Agent 不知道约束的存在，会不会导致它们无法主动优化？

---

## 9. 与 cortex-architecture.md 的关系

```
cortex-architecture.md:  六层架构是什么（WHAT）
nltc-protocol-spec.md:   消息协议长什么样（FORMAT）
nltc-agent-integration.md: Agent 怎么用（HOW）← 本文档
```

三份文档共同构成 Cortex 系统的完整设计。
