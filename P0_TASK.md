# P0 Implementation Task — Mock 替换为真实 LLM

## 目标
将 Agent 的 mock 实现替换为真实 LLM 调用，让系统有真实行为。

## LLM API 配置
使用本地 OpenAI 兼容代理（HTTP，不需要 OpenSSL）：
- Base URL: `http://localhost:3000/v1`
- API Key: 从环境变量 `EVOCLAW_API_KEY` 读取，或从 `~/.openclaw/openclaw.json` 的 `models.providers.mynewapi.apiKey` 读取
- Model: `claude-opus-4-6`
- 格式: OpenAI Chat Completions（POST /v1/chat/completions）
- 纯 HTTP，不需要 OpenSSL

## 需要创建/修改的文件

### 1. 新建 `src/llm/llm_client.hpp` + `llm_client.cpp` — LLM HTTP 客户端

```cpp
namespace evoclaw::llm {

struct LLMConfig {
    std::string base_url = "https://api.moonshot.cn/v1";
    std::string api_key;           // 从环境变量读取
    std::string model = "kimi-k2.5";
    double temperature = 0.3;
    int max_tokens = 2048;
};

struct ChatMessage {
    std::string role;    // "system", "user", "assistant"
    std::string content;
};

struct LLMResponse {
    bool success;
    std::string content;       // assistant 回复内容
    int prompt_tokens = 0;
    int completion_tokens = 0;
    std::string error;         // 失败时的错误信息
};

class LLMClient {
public:
    explicit LLMClient(LLMConfig config);
    
    // 同步调用 LLM
    [[nodiscard]] LLMResponse chat(const std::vector<ChatMessage>& messages) const;
    
    // 便捷方法
    [[nodiscard]] LLMResponse ask(const std::string& system_prompt, const std::string& user_message) const;
    
private:
    LLMConfig config_;
    // 使用 cpp-httplib 发 HTTPS 请求
};

// 工厂函数：从环境变量创建
[[nodiscard]] LLMClient create_from_env();

} // namespace evoclaw::llm
```

实现要点：
- 使用 cpp-httplib 的 Client（HTTP，不是 SSLClient）发请求到 localhost:3000
- 请求体：`{"model": "claude-opus-4-6", "messages": [...], "temperature": 0.3, "max_tokens": 2048}`
- 解析响应：`response.choices[0].message.content`
- 超时 30 秒
- 失败时返回 LLMResponse{success=false, error="..."}
- 如果没有 API key，fallback 到 mock 模式（返回固定字符串，打印警告）

### 2. 修改 `src/agent/agent.hpp` — 添加 LLM 支持

```cpp
class Agent {
public:
    // 新增：设置 LLM 客户端
    void set_llm_client(std::shared_ptr<llm::LLMClient> client);
    
protected:
    std::shared_ptr<llm::LLMClient> llm_client_;
    
    // 便捷方法：调用 LLM（如果没有 client 则 fallback 到 mock）
    [[nodiscard]] llm::LLMResponse call_llm(const std::string& system_prompt, 
                                             const std::string& user_message) const;
};
```

### 3. 修改 `src/agent/planner.cpp` — 真实 LLM 规划

```cpp
TaskResult Planner::execute(const Task& task) {
    // 如果有 LLM client，用 LLM 生成计划
    if (llm_client_) {
        auto response = call_llm(
            "You are a task planner. Break down the given task into clear, actionable steps. "
            "Output a numbered list of steps. Be concise.",
            "Task: " + task.description
        );
        if (response.success) {
            // 返回 LLM 生成的计划
        }
    }
    // fallback 到原有 mock 逻辑
}
```

### 4. 修改 `src/agent/executor.cpp` — 真实 LLM 执行

```cpp
TaskResult Executor::execute(const Task& task) {
    if (llm_client_) {
        auto response = call_llm(
            "You are a task executor. Given a task description, produce the output. "
            "Be thorough and accurate.",
            "Task: " + task.description + "\nContext: " + task.context.dump()
        );
        // ...
    }
    // fallback
}
```

### 5. 修改 `src/agent/critic.cpp` — 真实 LLM 评估

```cpp
TaskResult Critic::execute(const Task& task) {
    if (llm_client_) {
        auto response = call_llm(
            "You are a quality critic. Evaluate the given work and provide:\n"
            "1. A score from 0.0 to 1.0\n"
            "2. A verdict: accept or reject\n"
            "3. Brief feedback\n"
            "Output as JSON: {\"score\": 0.85, \"verdict\": \"accept\", \"feedback\": \"...\"}",
            "Work to evaluate: " + task.description + "\nContext: " + task.context.dump()
        );
        // 解析 JSON 响应
    }
    // fallback
}
```

### 6. 修改 `src/facade/facade.cpp` — 初始化时创建 LLM client

```cpp
void EvoClawFacade::initialize() {
    // ... 现有初始化 ...
    
    // 创建 LLM client（如果有 API key）
    llm_client_ = std::make_shared<llm::LLMClient>(llm::create_from_env());
}

void EvoClawFacade::register_agent(std::shared_ptr<agent::Agent> agent) {
    // ... 现有逻辑 ...
    
    // 注入 LLM client
    if (llm_client_) {
        agent->set_llm_client(llm_client_);
    }
}
```

### 7. 修改 `src/facade/facade.hpp` — 添加 LLM client 成员

```cpp
#include "llm/llm_client.hpp"
// ...
std::shared_ptr<llm::LLMClient> llm_client_;
```

### 8. 实现 Safety Airbag 运行时检查

在 Router 路由前检查 AirbagSpec：
```cpp
// router.cpp route() 方法中
// 检查选中 Agent 的 airbag level
auto& airbag = agent->contract().airbag;
if (airbag.level == AirbagLevel::MAXIMUM) {
    // 完全隔离模式：检查权限白名单
}
```

在 Agent execute 前检查：
```cpp
// agent.cpp execute() 前
// 检查 budget 限制
if (task.budget.max_tokens > 0 && /* 已用 tokens */ > task.budget.max_tokens) {
    return TaskResult{.success = false, .output = "Budget exceeded"};
}
```

### 9. 更新 CMakeLists.txt
添加 `src/llm/llm_client.cpp` 到 EVOLCLAW_SOURCES。
不需要 OpenSSL（纯 HTTP 调用本地代理）。

### 10. 新建 `tests/test_llm.cpp`
- 测试 LLMConfig 默认值
- 测试 create_from_env（无 key 时 fallback）
- 测试 ChatMessage 构造
- 测试 mock fallback 模式

### 11. 更新 `examples/server_demo.cpp`
- 启动时打印 LLM 状态（是否有 API key，使用哪个模型）

## 回测要求
完成后必须验证：
1. `./tests/evoclaw_test` — 所有现有测试仍然通过（mock fallback）
2. 启动 server_demo，通过 Dashboard 提交任务，验证 LLM 返回真实内容
3. SSE 事件流正常推送 LLM 执行结果
4. 无 API key 时 graceful fallback 到 mock

## 注意
- API key 从环境变量 EVOCLAW_API_KEY 读取，不要硬编码
- 如果环境变量没有，尝试从 ~/.openclaw/openclaw.json 读取 models.providers.mynewapi.apiKey
- 不需要 OpenSSL，纯 HTTP 调用 localhost:3000
- LLM 调用失败时必须 fallback 到 mock，不能崩溃

---
开始实现。
