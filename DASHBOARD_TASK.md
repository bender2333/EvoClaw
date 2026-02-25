# Web Dashboard Implementation Task

## 目标
为 EvoClaw 添加 Web Dashboard，包含 REST API + SSE 实时推送 + 静态 HTML 仪表盘。

## 技术方案
- HTTP Server: `cpp-httplib`（单头文件，零依赖）
- 实时推送: Server-Sent Events (SSE)（比 WebSocket 简单，cpp-httplib 原生支持流式响应）
- 前端: 纯 HTML/CSS/JS（内嵌在 C++ 中作为字符串，或从文件加载）

## 需要创建的文件

### 1. third_party/httplib.h
下载 cpp-httplib 单头文件：
```bash
curl -L https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -o third_party/httplib.h
```

### 2. src/server/server.hpp + server.cpp — EvoClawServer

```cpp
class EvoClawServer {
public:
    EvoClawServer(facade::EvoClawFacade& facade, int port = 8080);
    
    void start();      // 启动 HTTP 服务（阻塞）
    void start_async(); // 后台启动
    void stop();
    
private:
    void setup_routes();
    void broadcast_event(const nlohmann::json& event); // SSE 推送
    
    facade::EvoClawFacade& facade_;
    httplib::Server server_;
    int port_;
    
    // SSE 客户端管理
    std::mutex sse_mutex_;
    std::vector<std::function<void(const std::string&)>> sse_clients_;
};
```

### 3. REST API 端点

| Method | Path | 说明 |
|--------|------|------|
| GET | `/` | 仪表盘 HTML 页面 |
| GET | `/api/status` | 系统状态 JSON |
| GET | `/api/agents` | Agent 列表 + 能力矩阵 |
| POST | `/api/task` | 提交任务 `{"description": "...", "type": "execute"}` |
| POST | `/api/evolve` | 触发进化循环 |
| GET | `/api/events` | 事件日志（最近 100 条） |
| GET | `/api/events/stream` | SSE 实时事件流 |

### 4. SSE 实时推送

当以下事件发生时，通过 SSE 推送给所有连接的客户端：
- 任务提交/完成/失败
- Agent 注册
- 进化循环开始/结束
- 张力检测

SSE 格式：
```
event: task_complete
data: {"task_id": "t1", "agent_id": "executor-1", "success": true, "score": 0.85}

event: evolution
data: {"tensions": 2, "proposals": 1, "applied": 1}
```

### 5. src/server/dashboard.hpp — 仪表盘 HTML

将 HTML/CSS/JS 作为 C++ 字符串常量嵌入（避免外部文件依赖）。

仪表盘布局：

```
┌─────────────────────────────────────────────────────┐
│  EvoClaw Dashboard                    [状态: 运行中]  │
├──────────────────┬──────────────────────────────────┤
│  系统概览         │  实时事件流                       │
│  ┌──────────┐    │  ┌──────────────────────────────┐│
│  │ Agents: 3│    │  │ 14:32:01 task-5 → executor  ││
│  │ Tasks: 47│    │  │ 14:32:02 task-5 SUCCESS 0.85││
│  │ Events:89│    │  │ 14:32:05 evolution started   ││
│  │ Integrity│    │  │ 14:32:06 tension: KPI decline││
│  │   ✅     │    │  │ 14:32:07 proposal applied    ││
│  └──────────┘    │  │ ...（自动滚动）               ││
│                  │  └──────────────────────────────┘│
│  Agent 卡片       │                                  │
│  ┌──────────┐    │  操作面板                         │
│  │ Planner  │    │  ┌──────────────────────────────┐│
│  │ intent:  │    │  │ 任务描述: [____________]     ││
│  │  plan    │    │  │ [提交任务]  [触发进化]       ││
│  │ score:75%│    │  └──────────────────────────────┘│
│  └──────────┘    │                                  │
│  ┌──────────┐    │  进化历史                         │
│  │ Executor │    │  ┌──────────────────────────────┐│
│  │ intent:  │    │  │ Cycle 1: 2 tensions, 1 apply││
│  │  execute │    │  │ Cycle 2: 0 tensions          ││
│  │ score:75%│    │  └──────────────────────────────┘│
│  └──────────┘    │                                  │
├──────────────────┴──────────────────────────────────┤
│  Event Log Integrity: ✅ PASS                        │
└─────────────────────────────────────────────────────┘
```

前端技术要求：
- 纯 HTML/CSS/JS，不依赖任何框架
- 使用 CSS Grid 布局
- 配色：深色主题（#1a1a2e 背景，#16213e 卡片，#0f3460 高亮，#e94560 强调）
- 使用 EventSource API 接收 SSE
- 自动刷新状态（每 5 秒轮询 /api/status）
- 实时事件流自动滚动
- 提交任务和触发进化用 fetch POST
- 响应式布局（支持移动端）

### 6. examples/server_demo.cpp — 服务器启动示例

```cpp
int main() {
    // 初始化 Facade
    EvoClawFacade facade(config);
    facade.initialize();
    
    // 注册默认 Agents
    facade.register_agent(planner);
    facade.register_agent(executor);
    facade.register_agent(critic);
    
    // 启动 Web Server
    EvoClawServer server(facade, 8080);
    std::cout << "Dashboard: http://localhost:8080" << std::endl;
    server.start(); // 阻塞
}
```

### 7. 更新 CMakeLists.txt
- 添加 src/server/server.cpp 到 EVOLCLAW_SOURCES
- 添加 examples/server_demo.cpp 到 examples

### 8. 更新 Facade
在 EvoClawFacade 中添加事件回调机制，让 Server 能监听事件：

```cpp
// facade.hpp 新增
using EventCallback = std::function<void(const nlohmann::json&)>;
void set_event_callback(EventCallback callback);

// 在 submit_task, trigger_evolution 等方法中调用 callback
```

## 验收标准
1. `cd build && cmake .. && make -j4` 编译成功
2. `./examples/evoclaw_server_demo` 启动后访问 http://localhost:8080 看到仪表盘
3. 仪表盘显示系统状态、Agent 列表
4. 通过仪表盘提交任务，实时看到执行结果
5. 通过仪表盘触发进化，实时看到进化过程
6. SSE 事件流正常推送
7. 所有现有测试仍然通过

## 注意
- cpp-httplib 需要 pthread，CMakeLists.txt 已有 Threads::Threads
- SSE 连接用 chunked transfer encoding
- 仪表盘 HTML 嵌入为 C++ raw string literal（R"html(...)html"）
- 深色主题，现代感，不要太花哨

---
开始实现。
