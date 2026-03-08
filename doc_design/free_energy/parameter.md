好，我来系统研究一下，搜索最相关的论文和实现方案。现在我有足够的材料来给出严谨的答案。这是一个需要认真推导的问题，让我直接写出结论。

---

# EvoClaw 认知控制面技术规范 v1.0

这份规范直接回答三个问题，每个答案都建立在明确的学术来源上。

---

## 问题一：四通道惊讶向量的数学定义

### 为什么不能是单值

神经科学的 fMRI 元分析明确表明，惊讶信号具有至少两个独立的神经表征：结果的方向（比预期好还是差，即 signed RPE）和结果的惊讶程度（偏离期望有多远，即 unsigned RPE）——两者激活不同的脑区网络。 这说明大脑本身就没有把惊讶压缩成一个数，而是维护独立的通道。

关于惊讶定义的分类学研究整理了 18 种不同的惊讶定义，将其归入四个主要类别：预测惊讶（puzzlement）、变化点检测、置信度修正、以及启示惊讶（enlightenment）——后者度量的是新观察改变了智能体多少信念，与信息增益紧密相关。

对 EvoClaw 的直接含义：**单值惊讶会把这四类语义混在一起，导致路由和进化判断错误**。

### 四通道定义（正式规范）

**通道 C1：执行惊讶（Execution Surprise）**

度量任务执行结果相对于世界模型预测的偏差。

```
定义：使用对数惊讶（Shannon surprisal）

S_exec(o_t) = -log P(o_t | task_type, world_model)

其中 P(o_t | task_type) 由 TaskSpace 的 Dirichlet 后验给出：
  P(success | task_type) = α_success / (α_success + α_failure)
  α 参数通过贝叶斯在线更新维护（见问题三）

完整形式：
  S_exec = -log P(rounds_actual | μ_rounds, σ_rounds)
            -log P(success_actual | p_success)

学术来源：Shannon surprisal，即自由能原理中的 -log P(o|model) 项
```

**通道 C2：成本惊讶（Cost Surprise）**

度量资源消耗偏离预期的程度，使用置信度修正惊讶（Confidence-Corrected Surprise）。

```
定义：
  S_cost = (tokens_actual - μ_tokens) / σ_tokens  × confidence_weight

置信度权重：
  confidence_weight = α_total / (α_total + κ)
  其中 α_total = Σα_i（Dirichlet浓度之和，样本越多越接近1）
  κ 是平滑常数（建议κ=10）

解释：刚冷启动时 confidence_weight 趋近0，成本惊讶几乎不触发；
      样本积累后 confidence_weight 趋近1，成本偏差才会被认真对待。

方向编码：
  S_cost > 0 → 比预期贵（负方向）
  S_cost < 0 → 比预期便宜（正方向，可能发现了高效路径）

学术来源：Confidence-Corrected Surprise（arXiv:2209.01034）
```

**通道 C3：用户惊讶（User Surprise）**

度量用户反馈与系统预期的偏差。这是离散信号，适合贝叶斯惊讶。

```
定义：贝叶斯惊讶（Bayesian Surprise）

S_user = KL[ P(θ_user | feedback_t) || P(θ_user) ]
        = KL[ Dir(α + Δ) || Dir(α) ]

其中：
  θ_user = 用户对此类任务的满意度概率分布
  α = 当前 Dirichlet 参数（先验）
  Δ = 本次反馈更新量（正反馈+1，负反馈在对应维度+1）

闭合形式近似：
  S_user ≈ Σ_k (Δα_k / α_0) × log(Δα_k / α_0 × α_0/α_k)
  其中 α_0 = Σ α_k

解释：用户惊讶度量的是"这次反馈改变了我对用户偏好的信念多少"
      而不是"用户给了好评还是差评"——这正是GPT建议的语义

学术来源：Bayesian Surprise = KL[posterior || prior]（Baldi & Itti 2010，被 arXiv:2209.01034 收录）
```

**通道 C4：发现惊讶（Discovery Surprise）**

度量意外成功路径的出现——这是专门触发 Compiler 的信号。

```
定义：信息增益（Expected Information Gain）的实现值

S_disc = -log P(path_signature | known_patterns)

其中 path_signature 是协作轨迹的语义摘要（SLP原语序列的哈希）
P(path_signature | known_patterns) 用最近邻核密度估计：
  P(sig) = (1/|Patterns|) × Σ_p K(sig, p)
  K(sig, p) = exp(-edit_distance(sig, p) / τ)

简化实现（MVP）：
  P(sig) = (匹配数 + 1) / (历史轨迹总数 + |Vocabulary|)
  使用加1平滑的频率估计

只有 S_disc 高（新路径）且 S_exec 为正惊讶（比预期好）时
才触发 Compiler 注意队列

学术来源：信息增益 = KL[posterior || prior]，
          是期望自由能（EFE）中"认识论价值"项的实现值
```

### 四通道路由规则（完整表格）

| 通道 | 阈值 | 负方向触发 | 正方向触发 |
|------|------|-----------|-----------|
| C1 执行惊讶 | \|S_exec\| > 0.4 | P0 模块修复 | P1 Compiler队列 |
| C2 成本惊讶 | \|S_cost\| > 2.0σ | P1 成本审查 | 静默记录 |
| C3 用户惊讶 | S_user > 0.1 nats | P0 用户体验修复 | P2 用户模型更新 |
| C4 发现惊讶 | S_disc > 2.0 且 C1正 | - | P1 强制触发Compiler |

连续触发升级：
- C1 负惊讶连续5次（同task_type） → P1 组织级审查
- 任意通道系统性偏差（跨30天趋势发散） → P2 Modeler 触发

---

## 问题二：Modeler 三类提案的 Schema

### 为什么要类型约束

AGM 信念修订理论区分了三种操作：扩展（增加新信念，无冲突时自由进行）、收缩（移除信念）、修订（用新信息替换冲突的旧信念，需最小化信念损失）。当新信息与旧信念冲突时，智能体应该放弃"解释能力和整体信息价值尽可能小的信念"。

这为 Modeler 的三类操作提供了严格的理论依据：Modeler 做的正是 AGM 意义上的"修订"，而不是自由的"扩展"。允许 Modeler 随意扩展世界模型（等价于无约束地增加新信念），会在没有冲突证据的情况下膨胀认知底座。

### 三类提案的正式 Schema

**类型 A：TaskSpace 分类树修订**

```json
{
  "proposal_type": "TASK_TAXONOMY",
  "operation": "SPLIT" | "MERGE" | "RECLASSIFY",
  "evidence": {
    "surprise_pattern": "C1通道在过去30天的惊讶分布",
    "n_observations": 45,
    "divergence_from_prior": 0.34
  },
  "payload": {
    "SPLIT": {
      "source_task_type": "research_report",
      "proposed_subtypes": ["literature_review", "competitive_analysis"],
      "split_criterion": "路由到不同模块且结果质量差异显著"
    },
    "MERGE": {
      "source_task_types": ["summarize_doc", "extract_key_points"],
      "proposed_unified_type": "document_processing",
      "merge_criterion": "共享>80%模块路径且成功率差异<5%"
    }
  },
  "validation_protocol": {
    "method": "探索区A/B实验",
    "required_n": 30,
    "success_criterion": "C1惊讶幅度降低≥20%",
    "rollback_condition": "C1负惊讶增加或C3用户惊讶增加"
  }
}
```

**类型 B：路由先验更新**

```json
{
  "proposal_type": "ROUTING_PRIOR",
  "operation": "STRENGTHEN" | "WEAKEN" | "REDIRECT",
  "evidence": {
    "affected_task_types": ["data_analysis"],
    "current_capability_matrix_entry": {
      "module": "module_A",
      "score": 0.72
    },
    "observed_surprise_channel": "C1",
    "observed_magnitude": 0.51,
    "direction": "NEGATIVE"
  },
  "payload": {
    "WEAKEN": {
      "module": "module_A",
      "task_type": "data_analysis",
      "proposed_score_delta": -0.15,
      "rationale": "持续负执行惊讶，预期成功率高估"
    },
    "REDIRECT": {
      "from_module": "module_A",
      "to_module": "module_B",
      "task_type": "data_analysis",
      "proposed_routing_weight_shift": 0.3
    }
  },
  "validation_protocol": {
    "method": "Router影子模式（shadow routing）",
    "required_n": 20,
    "success_criterion": "C1惊讶幅度降低≥15%",
    "rollback_condition": "C1或C3任意通道恶化"
  }
}
```

**类型 C：因果图结构修订**

```json
{
  "proposal_type": "CAUSAL_EDGE",
  "operation": "ADD" | "REMOVE" | "REVERSE",
  "evidence": {
    "statistical_basis": {
      "co_occurrence_count": 23,
      "consistency_ratio": 0.87,
      "confound_check": "已控制task_type变量"
    },
    "llm_prior": {
      "confidence": 0.75,
      "rationale": "语义上合理：工具调用失败通常导致轮次增加"
    }
  },
  "payload": {
    "ADD": {
      "cause": "tool_call_failure_rate > 0.3",
      "effect": "rounds_actual > expected_rounds × 1.5",
      "proposed_strength": 0.7,
      "proposed_confidence": 0.6
    }
  },
  "validation_protocol": {
    "method": "反事实验证：隔离此因果边，观察C1预测误差变化",
    "required_n": 30,
    "success_criterion": "移除此边后C1幅度升高，保留此边后C1幅度降低",
    "rollback_condition": "验证失败或因果图引入环路"
  }
}
```

**Modeler 的约束规则（硬约束）：**
1. 每90天内类型A提案不超过3个（防止分类树爆炸）
2. 每30天内类型B提案不超过5个（防止路由抖动）
3. 类型C提案必须有 `n_observations ≥ 10`（防止小样本伪因果）
4. 任何提案必须指定 `rollback_condition`——没有回滚条件的提案自动拒绝
5. 正在验证中的提案不超过3个（防止并发实验干扰）

---

## 问题三：世界模型从 Event Log 派生的更新协议

### 理论基础：贝叶斯更新 vs 滑动平均

**滑动平均的根本缺陷**：它把所有观察等权处理，且无法表达"我对这个估计有多确定"。随着数据增加，它的估计会不断振荡，无法收敛到稳定信念。

**贝叶斯更新的优势**：在 Dirichlet-Multinomial 共轭模型中，后验均值可以理解为将最大似然估计（经验频率）与先验平滑的结合，参数 α_i 充当"虚拟计数"。随着观察增加，后验被数据主导，先验影响逐渐消退——这是理论上收敛的保证。

这直接说明：用 Dirichlet 参数存储 TaskSpace，而非点估计均值。

### TaskSpace 更新协议

**存储格式**：每个 task_type 存储 Dirichlet 参数向量，而非点估计。

```
// 任务成功/失败（二值）使用 Beta 分布（Dirichlet 的特例）
TaskTypeRecord {
  task_type_id: string
  
  // 成功率：Beta(α_success, α_failure)
  alpha_success: float  // 初始值：2.0（弱先验：相信会成功）
  alpha_failure: float  // 初始值：1.0
  
  // 执行轮次：用 Gamma 分布参数（或离散化后用 Dirichlet）
  // 简化方案：Welford 在线均值+方差，但附置信度权重
  rounds_mean: float     // Welford 在线均值
  rounds_variance: float // Welford 在线方差
  rounds_n: int          // 样本数（置信度的分母）
  
  // v5.0 惊讶通道存储
  last_c1_surprise: float
  rolling_c1_mean_30d: float  // 30天滑动窗口均值（用于趋势检测）
}
```

**触发更新的事件**：Event Log 中的 `agent.task.completed`

**更新算法（每次任务完成后执行，O(1) 时间复杂度）：**

```python
def update_task_space(task_type: str, outcome: TaskOutcome, event_log_entry: dict):
    record = world_model.get_task_type(task_type)
    
    # Step 1: 更新成功率（贝叶斯更新，闭合形式）
    if outcome.success:
        record.alpha_success += 1.0
    else:
        record.alpha_failure += 1.0
    
    # 后验均值（用于预测）
    p_success = record.alpha_success / (record.alpha_success + record.alpha_failure)
    
    # Step 2: 更新轮次估计（Welford 在线算法）
    n = record.rounds_n + 1
    delta = outcome.actual_rounds - record.rounds_mean
    record.rounds_mean += delta / n
    delta2 = outcome.actual_rounds - record.rounds_mean
    record.rounds_variance += (delta * delta2 - record.rounds_variance) / n
    record.rounds_n = n
    
    # Step 3: 计算 C1 执行惊讶（对数惊讶）
    # 成功率惊讶
    p_outcome = p_success if outcome.success else (1 - p_success)
    s_success = -log(max(p_outcome, 1e-6))
    
    # 轮次惊讶（标准化残差的绝对值）
    sigma = sqrt(record.rounds_variance + 1e-6)
    s_rounds = abs(outcome.actual_rounds - record.rounds_mean) / sigma
    
    # C1 综合：取两者最大值（保守估计）
    confidence_weight = (record.alpha_success + record.alpha_failure) / \
                       (record.alpha_success + record.alpha_failure + 10.0)
    c1_surprise = max(s_success, s_rounds) * confidence_weight
    
    # Step 4: 更新滚动均值（用于 Modeler 趋势检测）
    record.last_c1_surprise = c1_surprise
    # 指数加权移动平均（λ=0.95，约等价于30天窗口）
    record.rolling_c1_mean_30d = 0.95 * record.rolling_c1_mean_30d + 0.05 * c1_surprise
    
    world_model.save_task_type(record)
    return c1_surprise
```

### CausalStructure 更新协议

**学术基础**：LLM 生成因果先验 + 统计数据精炼的混合方案（NOTEARS + LLM prior）在低数据量场景（N<20）显著优于纯统计方法，而在 N>20 时两者结合仍有提升。 这是 EvoClaw 冷启动策略的直接理论支撑。

**两阶段策略：**

```
阶段1（冷启动，N < 20）：LLM 先验主导
  - Modeler 调用 LLM，输入任务描述和历史失败案例
  - LLM 输出候选因果边列表（A → B 的方向和强度）
  - 写入 CausalStructure，confidence = 0.4（低信任）
  - 这是"假设注入"，不是"事实写入"

阶段2（数据积累，N ≥ 20）：统计验证覆盖先验
  - 每当 (condition, action) 对出现 ≥ 5 次，计算结果一致性
  - 一致性 = 相同结果出现次数 / 总观察次数
  - 更新公式（贝叶斯更新）：
      alpha_consistent += observed_consistent
      alpha_total += 1
      strength = alpha_consistent / alpha_total
      confidence = 1 - 1/sqrt(alpha_total)  # 样本数的置信度函数
  
  - 与 LLM 先验冲突处理：
      if 统计 confidence > 0.7 且方向与LLM先验相反：
          发送 CAUSAL_EDGE REVERSE 提案给 Modeler 审批
      else：
          线性插值：strength = λ×statistical + (1-λ)×llm_prior
          λ = min(1.0, alpha_total / 20)  # 随样本数线性过渡
```

### Event Log → 世界模型的单向流动协议（解决GPT提出的所有权问题）

这是解决"世界模型 vs Event Log 谁是真相"争论的正式答案：

```
不变性：Event Log 是只写的观察流（append-only）
         世界模型是只读的预测层（可更新，但不能反向写入Event Log）

数据流方向（单向，不可逆）：
  Event Log → [Observer/更新协议] → 世界模型
  
世界模型 → [Router/Planner] → 行动决策
                                    ↓
                              Event Log（记录行动结果）

世界模型的内容 = f(Event Log 的统计聚合)
               ≠ Event Log 的内容本身

具体协议：
  1. 每次 task.completed 事件 → 立即触发 TaskSpace 更新（在线，<1ms）
  2. 每50次任务 → 批量重新归纳 CausalStructure（离线，可延迟）
  3. 每30天趋势分析 → Modeler 触发条件检测（定时）
  4. 任何 Modeler 提案写入世界模型前必须有对应的验证事件写入 Event Log
     即：世界模型的每一次修订，在 Event Log 中都有可追溯的事件链

Debug 原则：
  如果世界模型和实际行为不一致，通过 Event Log 可以完整重建世界模型状态
  世界模型是可从 Event Log 重建的"衍生物"，不是独立真相
```

---

## 总结：这三个问题的数学骨架

| 问题 | 核心数学 | 关键论文 | 实现复杂度 |
|------|----------|----------|-----------|
| 四通道惊讶向量 | Shannon surprisal + Bayesian Surprise（KL散度） | arXiv:2209.01034（惊讶分类学） | 低，纯数学计算 |
| Modeler 三类提案 | AGM 信念修订三操作（扩展/收缩/修订） + 类型化 Schema | AGM 1985，现代贝叶斯形式化 | 中等，主要是 schema 设计 |
| 世界模型更新协议 | Dirichlet-Categorical 共轭贝叶斯更新 + Welford 在线算法 + LLM先验混合 | Dirichlet 共轭先验（教材级），NOTEARS + LLM（arXiv:2402.11068） | 低，每次更新 O(1) |

这三个问题合在一起，覆盖了从"如何量化惊讶"到"如何修改信念"到"信念如何从数据流中产生"的完整认识论闭环——而且每一步都有可直接实现的闭合形式公式，不需要引入任何神经网络或梯度优化。