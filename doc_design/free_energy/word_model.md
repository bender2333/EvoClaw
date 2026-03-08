这里边每个模块的工程化都挺复杂的，我需要一些学术上的支撑，先讨论世界模型和最小惊讶原则，找一下最新的关于自由能或者类似的论文，有没有能够完成这个模型的理论和数学算法00:00Searched the webSearched the web让我系统地搜索相关学术文献。Searched the webSearched the web现在我有足够的材料了。让我综合性地展开这个讨论。

这是一个非常值得深挖的话题。让我系统地把理论层次梳理清楚，然后告诉你哪些能直接指导 EvoClaw 的工程实现。
学术全景：从哲学到算法
一、理论基础层：自由能原理（FEP）
核心数学（Friston，2010-2024）
自由能原理基于这样一个思想：系统沿着最小惊讶的路径运行，或等价地，最小化基于其世界模型所做的预测与实际感知之间的差距，这个差距由变分自由能来量化。 Oxford Academic
数学上，变分自由能 F 可以写成：
F = KL[q(s) || p(s|o)] - log p(o)
  = 期望能量 - 熵
  = 重建误差 + KL散度（近似后验 vs 先验）
等价地：F = -ELBO（与机器学习的证据下界完全等价）
从随机动力系统的朗之万方程出发，经过对 Markov blanket 的特定划分，最终得到 Bayesian 力学——一种关于自我组织的物理学，可解读为自证（self-evidencing）、自组装、或主动推断。 arXiv
关键洞见：FEP 与你的世界模型的对应
自证（self-evidencing）这个概念意味着：感知、认知和行动都有同一个命令——最大化大脑生成模型对感知世界的证据。实践中，这通过因子图上的置信传播或变分消息传递来实现，它同时包含生成模型和 Bayesian 信念更新方案。 Wikipedia
这就是 EvoClaw 世界模型的理论锚点——组织的"自我"就是它关于自身的生成模型，进化就是这个模型的持续自证过程。

二、可计算的实现层：离散 POMDP + 主动推断
这是最关键的一层：FEP 在工程上如何落地？
标准形式化（Parr, Pezzulo, Friston，2022）
主动推断框架特别是其近期的 POMDP 表述，为感知、学习和决策提供了统一的数学框架。智能体被假设通过将先验信念与感知输入相结合，推断不同外部状态和事件的概率——包括其自身的行动。 arXiv
POMDP 的生成模型有四个矩阵：
A 矩阵（观察模型）：P(o_t | s_t)          — "看到 o，隐藏状态可能是什么"
B 矩阵（转移模型）：P(s_t+1 | s_t, u_t)   — "采取行动 u 后，状态如何变化"
C 矩阵（偏好/目标）：log P(o_t)            — "什么观察是被偏好的"
D 矩阵（先验信念）：P(s_0)                 — "初始状态的信念"
状态推断的更新规则（非常关键，直接可以编码）：
隐藏状态推断通过找到最优变分后验 q(s_t) 来最小化变分自由能，对于简单的 POMDP 生成模型，这个更新规则化简为一个极其简洁的形式，本质上与 Bayes 法则成比例：
q(s_t) = σ(ln A[o,:] + ln B[:,:,u] · q(s_{t-1})) arXiv
这个公式直接告诉你：当新观察 o 到来时，只需做一次矩阵乘法加 softmax，就能更新对世界状态的信念。
期望自由能（EFE）——行动选择的核心：
期望自由能（EFE）的分解揭示了行动选择如何被驱动。其中"实用项"驱动智能体寻求偏好的观察，而另一项——通常被称为认识论价值——驱动智能体寻求能解决不确定性的信息，即好奇心。 arXiv
G(u) = -期望信息增益（好奇心项）- 期望价值（实用项）
      = -(信息增益) - E[log P(o|C)]

行动选择：u* = argmin_u G(u)
这是 EvoClaw Surprise Score 的理论根基——惊讶不是随意定义的启发式，而是 EFE 中信息增益的另一面。

三、最新前沿：Active Inference + LLM 的融合
最接近 EvoClaw 的论文（arXiv:2412.10425，2024年12月）
这篇论文提出了一种将主动推断与 LLM 集成的新方法，以自由能原理为框架，构建认知层架设在 LLM 智能体之上，通过原则性的信息寻求行为动态调整提示和搜索策略。该框架用三个状态因子（提示状态、搜索状态、信息状态）和七种观察方式来建模环境。 arXiv
这与 EvoClaw 的架构几乎同构：LLM 是执行原语（神经元），AIF 框架是认知层（世界模型）。
2025年更激进的论文（arXiv:2508.05619）
在这个混合系统中，LLM 提供摊销推断机制（amortized inference machinery），而主动推断提供决策规则——通过让 LLM 建议候选世界状态和策略，用简短的思维链理由说明，然后选择最小化期望自由能（EFE）的选项，有可能产生可扩展、可检查的智能体，无需手工编码矩阵或设计奖励信号。 PubMed Central

四、因果结构学习：世界模型的骨架
LLM + 因果发现（2024-2025 最新综述）
将 LLM 集成到因果发现中代表了一种范式转变。LLM 可以（1）直接从自然语言描述中推断因果图结构，自动化专家假设生成阶段；（2）作为后验校正机制，验证和精炼统计因果发现方法找到的关系；（3）作为传统统计算法的先验信息来源。 arXiv
这直接对应 EvoClaw 的 Modeler 角色：LLM 作为因果假设生成器，Event Log 的统计数据作为验证器。
动态贝叶斯网络（DBN）——因果结构随时间演化
对于具有多种配置的时间数据，存在多种时间模型：动态贝叶斯网络（DBN）、连续时间贝叶斯网络（CTBN）、Granger 因果性等，它们分别在特定假设下提供因果解释。 PubMed Central
EvoClaw 的 CausalStructure 本质上是一个随时间演化的 DBN，每次 Observer 观察到惊讶，都在更新因果强度权重。

对 EvoClaw 的直接映射
现在把理论精确地映射到你的三个世界模型组件：
TaskSpace → 变分后验 q(s)
理论对应：
  隐藏状态 s = {任务类型, 难度层级, 所需能力集}
  观察 o = {实际执行轮次, 成功/失败, 用时}
  A 矩阵 = P(o | s)，即"给定任务类型，观察分布是什么"

当前实现（V5.0）：
  expected_rounds ≈ E[o_rounds | s = task_type]（A矩阵的期望值）
  confidence ≈ 对 A 矩阵参数估计的 Dirichlet 浓度参数

理论升级：
  expected_rounds 应存储为 Beta/Gamma 分布参数，而非点估计
  这样 confidence 有正式的概率解释，而非启发式数值
具体算法：贝叶斯参数学习
每次任务完成后，对 A 矩阵做在线 Bayesian 更新（Dirichlet-Categorical 共轭）：
α_new = α_old + Δ_observed
其中 α 是 Dirichlet 浓度参数

Surprise = -log P(o | q(s), A)  ← 这是真正的对数惊讶（log surprisal）
         = KL[q(s) || p(s|o,A)]的近似

实现复杂度：O(|观察类别|)，极轻量
CausalStructure → B 矩阵
理论对应：
  B[s_t+1 | s_t, u_t] = 转移矩阵
  u_t = 组织采取的行动（选择哪个模块/路径）
  
  当前实现（condition→action→outcome）就是在学习稀疏 B 矩阵

因果强度 strength ≈ B矩阵中对应条目的 Dirichlet 参数浓度
算法：从 Event Log 归纳 B 矩阵
python# 每次观察 (s_t, u_t, s_{t+1})：
B_count[s_next][s_curr][action] += 1

# 归一化得到转移概率
B[s_next][s_curr][action] = B_count[s_next][s_curr][action] / sum(...)

# 因果强度 = 对应条目的集中度（远离均匀分布）
strength = max(B[:,s,a]) - 1/|S|  # 越集中越确定
```

### Surprise Score → 对数惊讶 / 信息增益
```
当前 V5.0 设计：
  Magnitude = |实际 - 期望| / 期望  ← 启发式

理论正确版本：
  Surprise(o) = -log P(o | 当前模型)  ← 香农信息量（nats）
  
  或更精确的贝叶斯惊讶：
  BayesSurprise = KL[P(θ|o) || P(θ)]  ← 观察o对模型参数分布的更新幅度
这个区别非常重要：香农惊讶告诉你"这个结果有多罕见"；贝叶斯惊讶告诉你"这个结果改变了你多少关于世界的信念"。后者才是驱动 Modeler 更新深层假设的正确量。

工程落地的算法选择建议
基于以上分析，给出具体的算法选型：
组件当前设计理论支撑算法实现复杂度TaskSpace 更新滑动平均贝叶斯在线学习（Dirichlet-Categorical）极低，10行代码CausalStructure 归纳频率统计结构方程模型 + BIC 评分（NOTEARS轻量版）中等Surprise Score启发式公式对数惊讶 -log P(o|model)低，替换一行Modeler 触发惊讶持续发散贝叶斯惊讶 KL[P(θ|o)||P(θ)]中等行动选择（Router）ε-greedy期望自由能最小化（EFE）高（需建完整POMDP）
关键判断：对于 MVP，你不需要实现完整的离散 POMDP（那会引入大量矩阵维度设计的工程成本）。更务实的路径是：

立即可用：把 Surprise Score 换成 -log P(o|model)，把 TaskSpace 更新换成 Dirichlet 在线学习——这两步能在一周内完成，获得扎实的理论基础，同时代码改动极小。
Phase 3-4 可用：引入稀疏 B 矩阵（CausalStructure 的概率版），用 Event Log 做在线 EM 算法更新，这时候 Modeler 的贝叶斯惊讶就有了正式的计算方法。
长期（若有必要）：完整 POMDP + 变分消息传递（pymdp 库有 Python 参考实现），Router 的 EFE 最小化替代 ε-greedy——但只有当 ε-greedy 明显不够用时才值得这个代价。


最值得精读的五篇论文
按与 EvoClaw 的直接相关性排序：

arXiv:2412.10425（Prakki，2024）——"Active Inference for Self-Organizing Multi-LLM Systems"：结构与 EvoClaw 几乎同构，有具体的三状态因子实现，最值得精读。
Parr, Pezzulo, Friston（2022）——《Active Inference: The Free Energy Principle in Mind, Brain, and Behavior》MIT Press：POMDP 形式化的权威教材，ABCD 矩阵的工程实现直接可用。
Smith et al.（2022）——"A step-by-step tutorial on active inference"（PMC8956124）：手把手推导，有 MATLAB/Python 代码，Surprise Score 的正式数学就在这里。
arXiv:2402.11068——"Large Language Models for Causal Discovery"：LLM 作为因果图先验的具体方法，对应你的 CausalStructure 冷启动和 Modeler 的假设生成。
arXiv:2508.05619（2025）——"The Missing Reward: Active Inference in the Era of Experience"：最新的 LLM-AIF 融合框架，讨论了为什么 AIF 对大规模 LLM 智能体是"热力学必要性"，对理解你的架构哲学有很高价值。


一句话总结：EvoClaw 的世界模型在理论上是一个因子化 POMDP 的生成模型，惊讶信号是其对数模型证据的负梯度，进化就是变分推断对生成模型参数的在线更新。这套框架已经被 Friston 团队严格证明，你需要做的是把这个连续数学映射到你离散的、基于 Event Log 的工程结构。这个映射本身就是你的创新