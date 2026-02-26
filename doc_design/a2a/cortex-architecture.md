# Cortex Architecture: 仿脑皮层多 Agent 通信架构

> 项目：A2A Research
> 作者：Fry & Bender
> 日期：2026-02-25
> 状态：设计草案 v0.1
> 灵感来源：Logitext (arXiv:2602.18095) + 大脑皮层六层结构

---

## 0. 核心思想

人脑大脑皮层（Neocortex）由六层神经元组成，每层有不同功能，信息在层间垂直流动、层内水平传播。我们借鉴这个结构，设计多 Agent 系统的通信与协调架构。

**核心类比：**

| 人脑 | Agent 系统 | 功能 |
|------|-----------|------|
| 大脑皮层（Neocortex） | LLM 大脑 | 高阶认知：理解、推理、决策、创造 |
| 小脑（Cerebellum） | NLTC 协调层 | 通信协调：打包、验证、冲突检测、时序控制 |
| 海马体（Hippocampus） | 情景记忆 | 记住发生了什么（任务历史） |
| 基底神经节（Basal Ganglia） | 约束模板库 | 习惯化行为模式（常用约束模板） |
| 丘脑（Thalamus） | 消息路由器 | 信息中继、过滤、优先级排序 |
| 前额叶工作记忆 | 工作记忆 | 当前活跃任务状态 |

**设计原则：LLM 大脑只处理需要"有意识思考"的部分，其余由皮层下结构自动处理，不占用 context 窗口。**

---

## 1. 六层架构总览

参照新皮层六层结构（Layer I-VI），我们定义 Agent 通信的六层：

```
┌─────────────────────────────────────────────────────────┐
│ Layer I   意图层（Molecular Layer）                       │
│ 最高层抽象，LLM 只需表达意图                              │
├─────────────────────────────────────────────────────────┤
│ Layer II  语义层（External Granular Layer）               │
│ 自然语言约束（NLTC 文本约束），不可形式化的语义            │
├─────────────────────────────────────────────────────────┤
│ Layer III 形式层（External Pyramidal Layer）              │
│ 形式化约束（SMT/Z3），可精确验证的逻辑和数值              │
├─────────────────────────────────────────────────────────┤
│ Layer IV  路由层（Internal Granular Layer）               │
│ 消息路由、Agent 寻址、负载均衡                            │
├─────────────────────────────────────────────────────────┤
│ Layer V   执行层（Internal Pyramidal Layer）              │
│ 任务执行、结果收集、超时控制                              │
├─────────────────────────────────────────────────────────┤
│ Layer VI  记忆层（Polymorphic Layer）                     │
│ 工作记忆、情景记忆、语义记忆、模板库                      │
└─────────────────────────────────────────────────────────┘
```

### 信息流向（对应皮层的垂直柱状结构）

```
下行流（任务下发）：
  Layer I → II → III → IV → V → VI
  意图 → 语义约束 → 形式约束 → 路由 → 执行 → 记忆存储

上行流（结果返回）：
  Layer VI → V → IV → III → II → I
  记忆查询 → 结果收集 → 路由回传 → 形式验证 → 语义验证 → 摘要给 LLM
```

---

## 2. 各层详细设计

### Layer I — 意图层（Molecular Layer）

**对应脑区：** 皮层第一层，接收广泛的反馈输入，整合顶层信息
**角色：** LLM 大脑与皮层下系统的接口

**职责：**
- 接收 LLM 的简短意图表达
- 返回给 LLM 的精炼摘要
- 是 LLM context 中唯一可见的层

**设计原则：** 极简。进出 LLM 的信息必须最小化。

```python
class LayerI_Intent:
    """意图层 — LLM 大脑的接口"""

    def interpret_intent(self, llm_output: str) -> TaskIntent:
        """
        LLM 说: "让博士改一下上次的 Z3 代码，Leela 说错误处理不够"
        解析为结构化意图（不需要 LLM，用规则/模式匹配）
        """
        return TaskIntent(
            action="modify",
            target_agent="doctor",
            reference="last_code_task",      # 指向情景记忆
            feedback_from="leela",
            focus="error_handling"
        )

    def summarize_for_llm(self, result: TaskResult) -> str:
        """
        将完整结果压缩为 LLM 需要的最小摘要
        """
        if result.all_constraints_met:
            return f"✓ {result.agent}完成{result.task_type}，" \
                   f"耗时{result.duration//60}分钟，质量{result.quality_score}"
        else:
            return f"✗ {result.agent}未达标：{result.failure_summary}"
```

**Context 预算：** 每次交互 ≤ 200 tokens 进出 LLM

---

### Layer II — 语义层（External Granular Layer）

**对应脑区：** 皮层第二层，小型锥体神经元，处理局部连接
**角色：** 处理不可形式化的自然语言约束（NLTC 的文本部分）

**职责：**
- 管理 NLTC 文本约束（{{let C = [[自然语言描述]]}}）
- 语义约束的 LLM 评估（仅在必要时调用）
- 语义缓存（相同约束不重复评估）

```python
class LayerII_Semantic:
    """语义层 — 自然语言约束处理"""

    def __init__(self):
        self.semantic_cache = {}  # 语义评估缓存

    def create_nltc(self, description: str,
                    bindings: dict) -> NLTC:
        """创建文本约束"""
        return NLTC(
            clause=description,
            bindings=bindings,
            constraint_type=ConstraintType.TEXTUAL
        )

    def evaluate(self, nltc: NLTC, value: str) -> bool:
        """
        评估文本约束是否满足
        优先查缓存，缓存未命中才调 LLM
        """
        cache_key = (nltc.clause, value[:200])  # 截断避免 key 过长
        if cache_key in self.semantic_cache:
            return self.semantic_cache[cache_key]

        # 必须调 LLM 的情况 — 这是唯一需要 LLM 的地方
        result = self._llm_evaluate(nltc, value)
        self.semantic_cache[cache_key] = result
        return result

    def _llm_evaluate(self, nltc: NLTC, value: str) -> bool:
        """LLM 评估（开销大，尽量少调）"""
        prompt = f"判断以下内容是否满足约束。\n" \
                 f"约束：{nltc.clause}\n" \
                 f"内容：{value[:500]}\n" \
                 f"回答 True 或 False。"
        return llm_call(prompt)
```

**关键：语义层是整个架构中唯一可能调用 LLM 的非大脑组件。通过缓存和模板化，调用频率应趋近于零。**

---

### Layer III — 形式层（External Pyramidal Layer）

**对应脑区：** 皮层第三层，中型锥体神经元，负责皮层间长距离连接
**角色：** 处理可形式化的约束（SMT/Z3）

**职责：**
- 形式约束的定义、组合、求解
- 多 Agent 约束合并与冲突检测
- 结果的形式化验证

```python
from z3 import *

class LayerIII_Formal:
    """形式层 — SMT 约束处理"""

    def __init__(self):
        self.solver = Solver()

    def verify(self, constraints: list, assignment: dict) -> VerifyResult:
        """
        验证赋值是否满足所有形式约束
        纯计算，零 LLM 开销，毫秒级
        """
        s = Solver()
        variables = {}

        for name, value in assignment.items():
            if isinstance(value, (int, float)):
                variables[name] = Real(name)
                s.add(variables[name] == value)
            elif isinstance(value, bool):
                variables[name] = Bool(name)
                s.add(variables[name] == value)

        for constraint in constraints:
            s.add(eval(constraint, {"__builtins__": {}}, variables))

        result = s.check()
        return VerifyResult(
            satisfied=(result == sat),
            failures=self._extract_failures(s) if result == unsat else []
        )

    def merge_and_check(self, agent_constraints: dict) -> MergeResult:
        """
        合并多个 Agent 的约束，检查是否可同时满足
        用于 roundtable 协商
        """
        s = Solver()
        all_constraints = []

        for agent, constraints in agent_constraints.items():
            for c in constraints:
                s.add(c)
                all_constraints.append((agent, c))

        if s.check() == sat:
            return MergeResult(satisfiable=True, model=s.model())
        else:
            core = s.unsat_core()
            conflicting = [(a, c) for a, c in all_constraints if c in core]
            return MergeResult(
                satisfiable=False,
                conflicts=conflicting,
                summary=f"冲突：{[a for a,_ in conflicting]} 的约束不兼容"
            )
```

**性能：** 典型约束集（<50 个变量）求解时间 < 10ms

---

### Layer IV — 路由层（Internal Granular Layer）

**对应脑区：** 皮层第四层，接收丘脑输入的主要层，星形神经元密集
**角色：** 消息路由、Agent 寻址、负载感知

**职责：**
- 根据任务类型选择目标 Agent
- 消息序列化/反序列化
- 超时和重试控制
- 负载感知（哪个 Agent 当前空闲）

```python
class LayerIV_Router:
    """路由层 — 消息路由与 Agent 寻址"""

    def __init__(self, agents: dict):
        self.agents = agents  # {"doctor": AgentEndpoint, "leela": ...}
        self.load_state = {}  # Agent 当前负载

    def route(self, intent: TaskIntent) -> str:
        """
        根据意图选择目标 Agent
        优先用意图中指定的 Agent，否则根据能力匹配
        """
        if intent.target_agent:
            return intent.target_agent

        # 从语义记忆查 Agent 能力画像
        best = self._match_by_capability(intent.task_type)
        return best

    def send(self, target: str, message: NLTCMessage,
             timeout: int = 3600) -> AsyncTask:
        """发送消息并注册超时"""
        endpoint = self.agents[target]
        task = AsyncTask(
            target=target,
            message=message,
            timeout=timeout,
            sent_at=time.time()
        )
        endpoint.deliver(message)
        return task

    def _match_by_capability(self, task_type: str) -> str:
        """根据语义记忆中的能力画像匹配 Agent"""
        # 查 Layer VI 语义记忆
        profiles = memory.semantic.get_agent_profiles()
        scores = {}
        for agent, profile in profiles.items():
            if task_type in profile.get("擅长", []):
                scores[agent] = profile.get("平均质量", 0.5)
        return max(scores, key=scores.get) if scores else "doctor"
```

---

### Layer V — 执行层（Internal Pyramidal Layer）

**对应脑区：** 皮层第五层，大型锥体神经元，主要输出层，投射到皮层下结构
**角色：** 任务生命周期管理

**职责：**
- 任务状态机（created → sent → running → completed/failed）
- 结果收集与验证编排
- 重试与降级策略

```python
from enum import Enum

class TaskState(Enum):
    CREATED = "created"
    SENT = "sent"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    TIMEOUT = "timeout"
    RETRYING = "retrying"

class LayerV_Execution:
    """执行层 — 任务生命周期管理"""

    def __init__(self, layer_iii, layer_ii, layer_vi):
        self.formal = layer_iii
        self.semantic = layer_ii
        self.memory = layer_vi
        self.active_tasks = {}

    def execute_task(self, task: Task) -> TaskResult:
        """完整的任务执行流程"""
        # 1. 注册到工作记忆
        self.memory.working.register(task)
        task.state = TaskState.SENT

        # 2. 等待结果（异步）
        response = yield task.wait_for_response()
        task.state = TaskState.RUNNING

        # 3. 形式验证（Layer III，零 LLM 开销）
        formal_result = self.formal.verify(
            task.formal_constraints,
            response.assignment
        )

        # 4. 语义验证（Layer II，仅在有文本约束时调 LLM）
        semantic_result = None
        if task.textual_constraints:
            semantic_result = self.semantic.evaluate_all(
                task.textual_constraints,
                response.payload
            )

        # 5. 综合判定
        all_pass = formal_result.satisfied and \
                   (semantic_result is None or semantic_result.satisfied)

        if all_pass:
            task.state = TaskState.COMPLETED
            # 6. 归档到情景记忆
            self.memory.episodic.archive(task, response)
        else:
            task.state = TaskState.FAILED
            # 可选：自动重试
            if task.retries < task.max_retries:
                task.state = TaskState.RETRYING
                task.retries += 1
                yield self.execute_task(task)  # 递归重试

        return TaskResult(
            task=task,
            response=response,
            formal_check=formal_result,
            semantic_check=semantic_result,
            state=task.state
        )
```

---

### Layer VI — 记忆层（Polymorphic Layer）

**对应脑区：** 皮层第六层，多形态神经元，连接丘脑，反馈调节
**角色：** 三层记忆系统 + 约束模板库

**职责：**
- 工作记忆：当前活跃任务
- 情景记忆：历史任务记录
- 语义记忆：提炼的知识（Agent 能力画像、协作模式）
- 约束模板库：从经验中积累的可复用约束模式

```python
import json
import time
from pathlib import Path

class LayerVI_Memory:
    """记忆层 — 三层记忆 + 模板库"""

    def __init__(self, base_path: str):
        self.base = Path(base_path)
        self.working = WorkingMemory()
        self.episodic = EpisodicMemory(self.base / "episodic")
        self.semantic = SemanticMemory(self.base / "semantic")
        self.templates = TemplateLibrary(self.base / "templates")


class WorkingMemory:
    """工作记忆 — 当前活跃任务（内存中，任务结束即清）"""

    def __init__(self):
        self._tasks = {}  # task_id → Task

    def register(self, task):
        self._tasks[task.id] = task

    def get_active(self, agent: str = None) -> list:
        tasks = self._tasks.values()
        if agent:
            tasks = [t for t in tasks if t.target_agent == agent]
        return [t for t in tasks if t.state not in
                (TaskState.COMPLETED, TaskState.FAILED)]

    def clear(self, task_id: str):
        self._tasks.pop(task_id, None)


class EpisodicMemory:
    """
    情景记忆 — 完成的任务记录（持久化到磁盘）
    
    存储结构：
      episodic/
        2026-02-25/
          task_042.json
          task_043.json
        2026-02-24/
          task_041.json
    """

    def __init__(self, path: Path):
        self.path = path
        self.path.mkdir(parents=True, exist_ok=True)

    def archive(self, task, response):
        """任务完成后归档"""
        date_dir = self.path / time.strftime("%Y-%m-%d")
        date_dir.mkdir(exist_ok=True)

        record = {
            "task_id": task.id,
            "agent": task.target_agent,
            "type": task.task_type,
            "intent": task.intent_text,
            "constraints": {
                "formal": task.formal_constraints,
                "textual": [c.clause for c in task.textual_constraints]
            },
            "result_summary": response.summary,
            "artifacts": response.artifact_paths,
            "duration": response.duration,
            "quality_score": response.quality_score,
            "timestamp": time.time(),
            "state": task.state.value
        }

        with open(date_dir / f"{task.id}.json", "w") as f:
            json.dump(record, f, ensure_ascii=False, indent=2)

    def query(self, agent: str = None, task_type: str = None,
              latest: bool = False, limit: int = 10) -> list:
        """查询历史任务"""
        results = []
        for date_dir in sorted(self.path.iterdir(), reverse=True):
            if not date_dir.is_dir():
                continue
            for task_file in sorted(date_dir.iterdir(), reverse=True):
                with open(task_file) as f:
                    record = json.load(f)
                if agent and record["agent"] != agent:
                    continue
                if task_type and record["type"] != task_type:
                    continue
                results.append(record)
                if latest:
                    return results
                if len(results) >= limit:
                    return results
        return results


class SemanticMemory:
    """
    语义记忆 — 从经验中提炼的持久知识
    
    存储结构：
      semantic/
        agent_profiles.json    # Agent 能力画像
        collaboration.json     # 协作模式
        domain_knowledge.json  # 领域知识
    """

    def __init__(self, path: Path):
        self.path = path
        self.path.mkdir(parents=True, exist_ok=True)
        self._profiles = self._load("agent_profiles.json", {})
        self._collab = self._load("collaboration.json", {})

    def _load(self, filename, default):
        fp = self.path / filename
        if fp.exists():
            with open(fp) as f:
                return json.load(f)
        return default

    def _save(self, filename, data):
        with open(self.path / filename, "w") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)

    def update_agent_profile(self, agent: str, episodic_records: list):
        """从情景记忆中提炼 Agent 能力画像"""
        if agent not in self._profiles:
            self._profiles[agent] = {
                "task_count": 0,
                "avg_duration": {},
                "avg_quality": 0,
                "strengths": [],
                "failure_modes": []
            }

        profile = self._profiles[agent]
        profile["task_count"] = len(episodic_records)

        # 按任务类型统计平均耗时
        by_type = {}
        qualities = []
        for r in episodic_records:
            t = r["type"]
            if t not in by_type:
                by_type[t] = []
            by_type[t].append(r["duration"])
            if r.get("quality_score"):
                qualities.append(r["quality_score"])

        profile["avg_duration"] = {
            t: sum(ds) / len(ds) for t, ds in by_type.items()
        }
        if qualities:
            profile["avg_quality"] = sum(qualities) / len(qualities)

        self._save("agent_profiles.json", self._profiles)

    def update_collaboration(self, agent_pair: str,
                             conflict: bool, topic: str = None):
        """更新协作模式"""
        if agent_pair not in self._collab:
            self._collab[agent_pair] = {
                "total_interactions": 0,
                "conflicts": 0,
                "conflict_topics": []
            }

        c = self._collab[agent_pair]
        c["total_interactions"] += 1
        if conflict:
            c["conflicts"] += 1
            if topic:
                c["conflict_topics"].append(topic)
        c["conflict_rate"] = c["conflicts"] / c["total_interactions"]

        self._save("collaboration.json", self._collab)

    def get_agent_profiles(self):
        return self._profiles


class TemplateLibrary:
    """
    约束模板库 — 可复用的约束模式
    
    对应脑区：基底神经节（习惯化行为）
    
    存储结构：
      templates/
        code_task.json
        review_task.json
        research_task.json
        arbitrage_analysis.json
    """

    def __init__(self, path: Path):
        self.path = path
        self.path.mkdir(parents=True, exist_ok=True)
        self._templates = {}
        self._load_all()

    def _load_all(self):
        for fp in self.path.glob("*.json"):
            with open(fp) as f:
                self._templates[fp.stem] = json.load(f)

    def match(self, task_type: str) -> dict:
        """匹配模板"""
        return self._templates.get(task_type)

    def register(self, name: str, template: dict):
        """注册新模板"""
        self._templates[name] = template
        with open(self.path / f"{name}.json", "w") as f:
            json.dump(template, f, ensure_ascii=False, indent=2)

    def fill(self, name: str, **params) -> dict:
        """填充模板参数，生成具体约束"""
        template = self._templates.get(name)
        if not template:
            return None

        filled = {
            "variables": {},
            "formal_constraints": [],
            "textual_constraints": []
        }

        for var_name, var_type in template["variables"].items():
            if var_name in params:
                filled["variables"][var_name] = params[var_name]

        for constraint in template.get("formal_constraints", []):
            # 替换参数占位符
            c = constraint
            for k, v in params.items():
                c = c.replace(f"{{{k}}}", str(v))
            filled["formal_constraints"].append(c)

        for tc in template.get("textual_constraints", []):
            filled["textual_constraints"].append(tc)

        return filled
```

---

## 3. 信息流详解

### 3.1 下行流：任务下发

```
用户: "让博士改一下上次的 Z3 代码，Leela 说错误处理不够"
                    │
                    ▼
┌─ Layer I ─────────────────────────────────────────────┐
│ 解析意图:                                              │
│   action=modify, agent=doctor, ref=last_code_task     │
│   feedback_from=leela, focus=error_handling            │
└───────────────────────┬───────────────────────────────┘
                        │
                        ▼
┌─ Layer VI (记忆查询) ─────────────────────────────────┐
│ 情景记忆: 找到 task_041 (Z3封装, 博士, 通过)           │
│ 情景记忆: 找到 task_041_review (Leela, 错误处理不足)   │
│ 语义记忆: 博士改代码平均 1800s                         │
│ 模板库: 匹配 "code_task" 模板                          │
└───────────────────────┬───────────────────────────────┘
                        │
                        ▼
┌─ Layer II (语义约束) ─────────────────────────────────┐
│ {{let error_handling = [[完善错误处理，                 │
│   覆盖 Leela 指出的边界情况]]                          │
│   where "边界情况" is leela_feedback}}                 │
└───────────────────────┬───────────────────────────────┘
                        │
                        ▼
┌─ Layer III (形式约束) ────────────────────────────────┐
│ test_pass_rate >= 0.95    (继承 task_041)              │
│ lint_errors <= 0                                      │
│ deadline <= 1800          (语义记忆推断)               │
└───────────────────────┬───────────────────────────────┘
                        │
                        ▼
┌─ Layer IV (路由) ─────────────────────────────────────┐
│ target: doctor                                        │
│ 打包 NLTC 消息，附上 task_041 上下文                   │
│ 发送                                                  │
└───────────────────────┬───────────────────────────────┘
                        │
                        ▼
┌─ Layer V (执行) ──────────────────────────────────────┐
│ 注册 task_042 到工作记忆                               │
│ 状态: SENT → 等待博士响应                              │
└───────────────────────────────────────────────────────┘
```

### 3.2 上行流：结果返回

```
博士返回结果
        │
        ▼
┌─ Layer V (执行) ──────────────────────────────────────┐
│ 收到响应，task_042 状态: RUNNING                       │
└───────────────────────┬───────────────────────────────┘
                        │
                        ▼
┌─ Layer III (形式验证) ────────────────────────────────┐
│ Z3 验证:                                              │
│   test_pass_rate = 0.97 >= 0.95  ✓                    │
│   lint_errors = 0 <= 0           ✓                    │
│   time_spent = 1200 <= 1800      ✓                    │
│ 结果: 全部通过                                         │
└───────────────────────┬───────────────────────────────┘
                        │
                        ▼
┌─ Layer II (语义验证) ─────────────────────────────────┐
│ 检查缓存: 未命中                                       │
│ LLM 评估: error_handling 约束是否满足                  │
│ 结果: True (覆盖了 Leela 指出的边界情况)               │
│ 写入缓存                                              │
└───────────────────────┬───────────────────────────────┘
                        │
                        ▼
┌─ Layer VI (记忆更新) ─────────────────────────────────┐
│ 工作记忆: task_042 清除                                │
│ 情景记忆: task_042 归档                                │
│ 语义记忆: 更新博士能力画像                             │
│   (code_modify 平均耗时更新为 1500s)                   │
└───────────────────────┬───────────────────────────────┘
                        │
                        ▼
┌─ Layer I (摘要) ──────────────────────────────────────┐
│ 返回给 LLM:                                           │
│ "✓ 博士完成 Z3 代码修改，耗时20分钟，                  │
│  测试通过率 97%，错误处理已覆盖 Leela 的反馈"          │
│                                                       │
│ ← 这是 LLM context 中唯一新增的内容（~50 tokens）     │
└───────────────────────────────────────────────────────┘
```

---

## 4. 多 Agent 协商（Roundtable）

```
用户: "让博士和 Leela 讨论下新功能的技术方案"

Layer I: 解析为 roundtable 意图
Layer IV: 同时发给 doctor 和 leela

博士返回:
  approach = "用 WebSocket 实现实时推送"
  formal: latency <= 100ms, complexity <= "medium"

Leela 返回:
  approach = "用 SSE 实现，更简单安全"
  formal: latency <= 500ms, complexity <= "low"
  textual: {{let security = [[不引入新的攻击面]]}}

Layer III 合并约束:
  博士: latency <= 100
  Leela: latency <= 500
  → 兼容（取交集: latency <= 100）

  博士: complexity <= "medium"
  Leela: complexity <= "low"
  → 冲突！medium ≠ low

Layer I 返回给 LLM:
  "博士建议 WebSocket（低延迟），Leela 建议 SSE（更简单安全）。
   延迟要求兼容，但复杂度有分歧。需要你决定。"

→ LLM 只需要做一个决策，不需要读两人的完整论述
```

---

## 5. 存储布局

```
~/.openclaw/shared/cortex/
├── templates/                    # Layer VI: 约束模板库
│   ├── code_task.json
│   ├── review_task.json
│   ├── research_task.json
│   └── arbitrage_analysis.json
├── episodic/                     # Layer VI: 情景记忆
│   ├── 2026-02-25/
│   │   ├── task_042.json
│   │   └── task_043.json
│   └── 2026-02-24/
│       └── task_041.json
├── semantic/                     # Layer VI: 语义记忆
│   ├── agent_profiles.json
│   ├── collaboration.json
│   └── domain_knowledge.json
└── cache/                        # Layer II: 语义评估缓存
    └── semantic_cache.json
```

---

## 6. 与现有系统的集成

### 6.1 与 OpenClaw 的关系

```
OpenClaw (现有):
  Agent 定义、Telegram 通道、会话管理、LLM 调用

Cortex (新增):
  通信协调、约束验证、记忆管理

集成方式:
  Cortex 作为 OpenClaw 的一个 skill 或中间件
  不替换 OpenClaw，而是增强它
```

### 6.2 与 A2A 协议的关系

```
Google A2A Protocol:
  定义了 Agent 发现、消息格式、任务管理

Cortex:
  在 A2A 消息格式之上增加 NLTC 约束层
  A2A 负责"怎么传"，Cortex 负责"传什么、怎么验"
```

### 6.3 与现有记忆系统的关系

```
现有:
  memory/YYYY-MM-DD.md  → 自然语言日记（LLM 用）
  MEMORY.md             → 长期记忆（LLM 用）

Cortex 新增:
  cortex/episodic/      → 结构化任务记录（小脑用）
  cortex/semantic/      → Agent 画像和协作模式（小脑用）
  cortex/templates/     → 约束模板（小脑用）

两者互补:
  LLM 记忆: "博士最近心情不错" → 用于高层决策
  小脑记忆: "博士最近10次任务平均耗时2200s" → 用于约束设定
```

---

## 7. 性能预估

| 操作 | 现有方式 | Cortex 方式 | 改善 |
|------|---------|------------|------|
| 任务下发 | ~2000 tokens context | ~100 tokens context | **20x** |
| 结果验证 | ~3000 tokens LLM 调用 | Z3 < 10ms + 可能 1 次 LLM | **10-50x** |
| 历史查询 | 读整个 memory 文件 | 结构化查询 < 1ms | **1000x** |
| 冲突检测 | LLM 读两份报告对比 | Z3 合并约束 < 10ms | **100x** |
| Context 占用 | 通信占 50%+ | 通信占 < 5% | **10x** |

---

## 8. 实现路线图

| 阶段 | 内容 | 依赖 | 预计时间 |
|------|------|------|---------|
| **P0** | Layer VI 记忆层（情景+语义+模板库） | 无 | 3 天 |
| **P1** | Layer III 形式层（Z3 约束验证） | Z3 | 2 天 |
| **P2** | Layer I 意图层（意图解析+摘要生成） | P0 | 2 天 |
| **P3** | Layer IV 路由层（Agent 寻址） | P0 | 1 天 |
| **P4** | Layer V 执行层（任务状态机） | P0-P3 | 3 天 |
| **P5** | Layer II 语义层（NLTC 文本约束+缓存） | P1 | 2 天 |
| **P6** | 集成到 OpenClaw（作为 skill） | P0-P5 | 3 天 |
| **P7** | Roundtable 协商 | P1, P4 | 2 天 |

总计：约 18 天

---

## 9. 开放问题

1. **意图解析的准确性** — Layer I 用规则匹配还是小模型？规则快但覆盖有限
2. **模板自动提取** — 任务完成后如何自动生成新模板？规则 vs LLM 离线提取
3. **语义缓存失效** — 同一文本约束在不同上下文下可能有不同判断，缓存 key 怎么设计
4. **跨 Agent 记忆一致性** — 三个 Agent 共享记忆层，并发写入怎么处理
5. **与 LLM 记忆的同步** — 小脑记忆和 LLM 的 MEMORY.md 如何保持一致

---

## 10. 参考文献

1. **Logitext** — Oh et al., "Neurosymbolic Language Reasoning as Satisfiability Modulo Theory", arXiv:2602.18095, 2026
2. **Z3 Solver** — de Moura & Bjørner, "Z3: An Efficient SMT Solver", TACAS 2008
3. **Neocortex Six-Layer Structure** — Mountcastle, "The columnar organization of the neocortex", Brain, 1997
4. **Cerebellum Function** — Ito, "Control of mental activities by internal models in the cerebellum", Nature Reviews Neuroscience, 2008
5. **Distributed AGI Safety** — Tomašev et al., arXiv 2025
6. **Google A2A Protocol** — Google, 2025
7. **Memory-Prediction Framework** — Hawkins, "On Intelligence", 2004
