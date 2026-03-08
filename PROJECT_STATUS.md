# EvoClaw 项目状态

**最后更新:** 2026-03-08  
**当前版本:** main 分支

---

## 📊 完成概览

| 模块 | 功能 | 状态 |
|------|------|------|
| **P9** | Runtime Config Versioning | ✅ |
| **P9** | Runtime Config History | ✅ |
| **P9** | Runtime Config Diff | ✅ |
| **P9** | History Prune | ✅ |
| **P11** | Auto-Prune Governance | ✅ |
| **P11** | Server/API 暴露 | ✅ |
| **P11** | Dashboard Controls | ✅ |
| **P11** | 审计事件化 | ✅ |
| **P11** | Governance 持久化 | ✅ |
| **P12** | Rollback UI 闭环 | ✅ |
| **P13** | Dashboard i18n (EN/ZH) | ✅ |
| **P14** | Dashboard 性能优化 | ✅ |

---

## 🎯 核心功能

### 1. Runtime Config 管理

**功能描述:**
- 每个 Agent 的 runtime config 都有版本历史
- 支持查看任意两个版本之间的 diff
- 支持按阈值自动裁剪历史
- 支持手动裁剪历史

**API:**
- `GET /api/runtime-config/version?agent_id=...`
- `GET /api/runtime-config/history?agent_id=...`
- `GET /api/runtime-config/diff?agent_id=...&from_version=...&to_version=...`
- `POST /api/runtime-config/history/prune`

**Dashboard:**
- Runtime Config Inspector 面板
- 历史条目列表（显示 version/proposal_id/changed_at）
- 版本比较工具

---

### 2. Rollback 功能

**功能描述:**
- 每次 runtime config 变更都会创建 snapshot
- 可以从 Dashboard 一键回滚到任意 snapshot
- 回滚后自动记录新版本

**API:**
- `GET /api/status` (包含 `rollback_snapshots` 数组)
- `POST /api/runtime-config/rollback`

**Dashboard:**
- Rollback Snapshots 卡片
- 每个 snapshot 显示 agent_id/proposal_id/applied_at
- 一键 Rollback 按钮

---

### 3. Governance 自动化

**功能描述:**
- 可配置每个 agent 保留的 history 条数
- 自动裁剪超出阈值的历史
- 配置持久化到 `governance_config.json`
- 裁剪时发送审计事件

**配置:**
```json
{
  "runtime_history_keep_last_per_agent": 3
}
```

**Dashboard:**
- Operations 面板中的治理配置表单
- 实时更新 auto_prune_enabled 状态

---

### 4. Dashboard 国际化

**支持语言:**
- English (默认)
- 简体中文

**切换方式:**
- Dashboard header 右上角 EN/中文 按钮
- 语言偏好保存到 localStorage

**翻译范围:**
- 所有卡片标题
- 统计标签
- 按钮文本
- 表单标签
- 状态文本
- 提示文案

---

### 5. Dashboard 性能优化

**优化点:**
- refreshStatus 间隔：5s → 10s
- SSE 连接状态指示 (🟢/🟡/🔴)
- Rollback Snapshots 懒加载
- Event Feed 最多保留 200 条
- 自动重连 SSE

---

## 📁 关键文件

### 后端
- `src/facade/facade.hpp` — Runtime config 管理接口
- `src/facade/facade.cpp` — Runtime config 实现
- `src/server/server.cpp` — Server API 路由
- `src/server/server.hpp` — Server handler 声明

### 前端
- `src/server/dashboard.hpp` — Dashboard HTML/CSS/JS (单文件)

### 测试
- `tests/test_integration.cpp` — 集成测试
- `tests/test_server_runtime_config_api.cpp` — Server API 测试

### 文档
- `DESIGN.md` — 详细设计文档 (P9-P14)
- `PROJECT_STATUS.md` — 本文件
- `README.md` — 项目介绍

---

## 🧪 测试状态

**最新测试结果:**
```
106/106 测试通过
3 个 live 测试按预期 skip
```

**运行测试:**
```bash
cd build
ctest --output-on-failure
```

---

## 🚀 快速启动

```bash
# 编译
cd /home/bender/projects/EvoClaw
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# 启动 server
./examples/evoclaw_server_demo --port 8080

# 访问 dashboard
# http://localhost:8080
```

---

## 📈 下一步建议

1. **新功能需求** — 等待用户提出新需求
2. **代码清理** — 移除未使用代码，统一命名
3. **文档完善** — 补充 API 文档、用户指南
4. **监控告警** — 增加系统监控和告警功能

---

## 📝 版本历史

| 日期 | 版本 | 内容 |
|------|------|------|
| 2026-03-08 | main | P9/P11/P12/P13/P14 完成 |
| 2026-03-07 | main | P9/P11 基础功能完成 |
| 2026-03-01 | main | P4 真实 LLM 接入 |

---

**维护者:** Fry  
**贡献者:** Doctor (AI Assistant)
