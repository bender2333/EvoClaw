# P2 Task: 双区架构 + Compiler + 反熵机制

## 概述
基于 v4.0 设计文档，实现三个核心特性。

## Feature 1: 双区架构 (Dual Zone)

### 文件
- `src/zone/zone.hpp` — Zone 枚举 (STABLE / EXPLORATION) + ZonePolicy 结构体
- `src/zone/zone_manager.hpp` + `zone_manager.cpp` — 管理 agent 所属区域、晋升流程、资源预算差异

### 设计
```cpp
namespace evoclaw::zone {

enum class Zone { STABLE, EXPLORATION };

struct ZonePolicy {
    Zone zone = Zone::STABLE;
    double token_budget_ratio = 1.0;      // 探索区 = 0.7 (比稳定区低30%)
    int min_success_count = 20;           // 晋升最低成功次数
    double min_success_rate = 0.75;       // 晋升最低成功率
    int retirement_days = 7;             // 晋升后旧实例保留天数
    bool can_face_user = true;           // 探索区 = false
};

class ZoneManager {
public:
    void assign_zone(const AgentId& agent, Zone zone);
    Zone get_zone(const AgentId& agent) const;
    ZonePolicy get_policy(const AgentId& agent) const;
    
    // 晋升检查：成功次数 >= 20 且成功率 >= 0.75
    bool is_promotion_eligible(const AgentId& agent) const;
    void promote_to_stable(const AgentId& agent);
    
    // 记录执行结果
    void record_result(const AgentId& agent, bool success);
    
    nlohmann::json status() const;
};

} // namespace evoclaw::zone
```

### 集成
- Router: 路由时检查 zone，探索区 agent 不能直接接收用户任务（除非被稳定区 agent 调用）
- Facade: 创建 ZoneManager，注入 Router，暴露 `/api/zones` 端点
- Agent 注册时可指定 zone

## Feature 2: Compiler 角色 (Pattern Detection → Compilation)

### 文件
- `src/compiler/pattern.hpp` — Pattern 结构体（SLP 原语序列 + UMI 调用序列模板）
- `src/compiler/compiler.hpp` + `compiler.cpp` — Compiler 角色：监听 OrgLog → 识别重复模式 → 生成 Pattern

### 设计
```cpp
namespace evoclaw::compiler {

struct Pattern {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> step_sequence;  // agent_id 或 primitive_id 序列
    int observation_count = 0;               // 被观察到的次数
    double avg_success_rate = 0.0;
    Timestamp created_at;
    Timestamp last_seen;
};

class Compiler {
public:
    explicit Compiler(const memory::OrgLog& org_log);
    
    // 扫描 OrgLog，识别重复出现的任务序列模式
    std::vector<Pattern> detect_patterns(int min_occurrences = 3) const;
    
    // 将 Pattern 编译为可直接路由的快捷方式
    // 返回编译后的 Pattern（带 UMI 契约草稿）
    Pattern compile(const Pattern& pattern) const;
    
    // 获取所有已编译的 Pattern
    const std::vector<Pattern>& compiled_patterns() const;
    
    nlohmann::json status() const;

private:
    const memory::OrgLog& org_log_;
    std::vector<Pattern> compiled_patterns_;
};

} // namespace evoclaw::compiler
```

## Feature 3: 反熵机制 (Anti-Entropy)

### 文件
- `src/entropy/entropy_monitor.hpp` + `entropy_monitor.cpp` — 熵度量 + 反熵触发

### 设计
```cpp
namespace evoclaw::entropy {

struct EntropyMetrics {
    int active_agent_count = 0;
    int active_primitive_count = 0;
    double routing_concentration = 0.0;  // 路由集中度 (0-1, 1=全部路由到同一agent)
    double avg_task_rounds = 0.0;        // 平均任务轮次
    double entropy_score = 0.0;          // 综合熵值 (0-1)
};

struct AntiEntropyAction {
    std::string type;       // "primitive_merge", "module_consolidation", "cold_start_perturbation"
    std::string description;
    nlohmann::json details;
};

class EntropyMonitor {
public:
    explicit EntropyMonitor(const router::Router& router);
    
    // 计算当前熵度量
    EntropyMetrics measure() const;
    
    // 当熵值超过阈值时，生成反熵操作建议
    std::vector<AntiEntropyAction> suggest_actions(double threshold = 0.7) const;
    
    // 执行冷启动扰动：对长期未被路由的 agent 分配流量
    void apply_cold_start_perturbation(router::Router& router, double traffic_ratio = 0.05);
    
    nlohmann::json status() const;

private:
    const router::Router& router_;
    double warning_threshold_ = 0.7;
};

} // namespace evoclaw::entropy
```

## 集成要求

### Facade 更新
- 创建 ZoneManager、Compiler、EntropyMonitor
- 注入到 Router（zone 检查）和 Evolver（反熵建议）
- `get_status()` 增加 zones、patterns、entropy 字段

### Server 端点
- `GET /api/zones` — 区域状态
- `GET /api/patterns` — 已识别/编译的 Pattern
- `GET /api/entropy` — 熵度量 + 反熵建议

### Dashboard
- 在 dashboard.hpp 中增加 Zones、Patterns、Entropy 面板

### 测试
- `tests/test_zone.cpp` — ZoneManager 测试（分配、晋升、策略）
- `tests/test_compiler.cpp` — Pattern 检测和编译测试
- `tests/test_entropy.cpp` — 熵度量和反熵建议测试

### CMakeLists.txt
- 添加新 .cpp 到 EVOLCLAW_SOURCES
- 添加新 test .cpp 到 tests/CMakeLists.txt

## 验收标准
1. `make -j4` 编译通过
2. 所有测试通过（包括新增测试）
3. server_demo 启动后 `/api/zones`、`/api/patterns`、`/api/entropy` 返回有效 JSON
4. 探索区 agent 的 token 预算自动降低 30%
5. EntropyMonitor 能计算熵值并生成反熵建议

## 完成后
```bash
openclaw system event --text "Done: P2 dual-zone + compiler + anti-entropy" --mode now
```
