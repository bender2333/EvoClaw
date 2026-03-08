#pragma once

namespace evoclaw::server::dashboard {

inline constexpr const char* kDashboardHtml = R"html(
<!doctype html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>EvoClaw Dashboard</title>
  <style>
    :root {
      --bg: #1a1a2e;
      --card: #16213e;
      --highlight: #0f3460;
      --accent: #e94560;
      --text: #eaeaea;
      --muted: #9aa0b4;
      --ok: #44d18f;
      --warn: #ffbe55;
      --border: #243a60;
    }

    * {
      box-sizing: border-box;
    }

    body {
      margin: 0;
      background: radial-gradient(circle at top right, #243b68 0%, var(--bg) 55%);
      color: var(--text);
      font-family: "Segoe UI", "Noto Sans", sans-serif;
      min-height: 100vh;
    }

    .container {
      width: min(1200px, 96vw);
      margin: 0 auto;
      padding: 1rem;
      display: grid;
      gap: 1rem;
    }

    .header {
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 1rem 1.2rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 0.8rem;
    }

    .header h1 {
      margin: 0;
      font-size: 1.3rem;
      letter-spacing: 0.04em;
    }

    .status-pill {
      border-radius: 999px;
      padding: 0.3rem 0.8rem;
      border: 1px solid var(--border);
      background: var(--highlight);
      color: var(--ok);
      font-size: 0.85rem;
      white-space: nowrap;
    }

    .lang-switch {
      display: flex;
      gap: 0.4rem;
      align-items: center;
    }

    .lang-btn {
      background: var(--highlight);
      border: 1px solid var(--border);
      color: var(--text);
      padding: 0.25rem 0.6rem;
      border-radius: 6px;
      font-size: 0.8rem;
      cursor: pointer;
      transition: background 120ms ease, border-color 120ms ease;
    }

    .lang-btn:hover {
      background: #165086;
      border-color: #3a5a8a;
    }

    .lang-btn.active {
      background: var(--accent);
      border-color: #ff758b;
    }

    .grid {
      display: grid;
      gap: 1rem;
      grid-template-columns: 1fr 1.3fr;
      grid-template-areas:
        "overview events"
        "agents actions"
        "agents runtime"
        "agents evolution";
    }

    .card {
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 1rem;
    }

    .card h2 {
      margin: 0 0 0.7rem;
      font-size: 1rem;
      color: #f5f7ff;
    }

    .overview { grid-area: overview; }
    .events { grid-area: events; }
    .agents { grid-area: agents; }
    .actions { grid-area: actions; }
    .runtime { grid-area: runtime; }
    .rollback { grid-area: evolution; }
    .evolution { grid-area: evolution; }

    .stats {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 0.6rem;
    }

    .stat {
      background: rgba(15, 52, 96, 0.45);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 0.7rem;
    }

    .stat .label {
      color: var(--muted);
      font-size: 0.8rem;
      margin-bottom: 0.2rem;
    }

    .stat .value {
      font-size: 1.15rem;
      font-weight: 700;
    }

    .integrity-ok { color: var(--ok); }
    .integrity-bad { color: var(--accent); }

    .event-feed {
      height: 320px;
      overflow-y: auto;
      background: #101c34;
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 0.5rem;
      font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
      font-size: 0.82rem;
      line-height: 1.4;
    }

    .event-item {
      border-bottom: 1px dashed rgba(154, 160, 180, 0.25);
      padding: 0.35rem 0.2rem;
      word-break: break-word;
    }

    .event-item:last-child {
      border-bottom: none;
    }

    .event-time {
      color: var(--muted);
      margin-right: 0.35rem;
    }

    .event-name {
      color: var(--accent);
      margin-right: 0.35rem;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.02em;
      font-size: 0.75rem;
    }

    .agent-grid {
      display: grid;
      gap: 0.7rem;
      grid-template-columns: repeat(auto-fill, minmax(190px, 1fr));
    }

    .agent-card {
      background: rgba(15, 52, 96, 0.45);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 0.7rem;
    }

    .agent-card h3 {
      margin: 0 0 0.5rem;
      font-size: 0.95rem;
    }

    .agent-line {
      color: var(--muted);
      font-size: 0.82rem;
      margin: 0.2rem 0;
    }

    .inspect-runtime-btn {
      margin-top: 0.55rem;
      background: #132d4f;
      border-color: #2b4f7b;
    }

    .inspect-runtime-btn.active {
      background: var(--accent);
      border-color: #ff758b;
    }

    .actions form {
      display: grid;
      gap: 0.6rem;
    }

    textarea,
    select,
    input,
    button {
      width: 100%;
      border-radius: 8px;
      border: 1px solid var(--border);
      background: #101c34;
      color: var(--text);
      padding: 0.6rem;
      font: inherit;
    }

    textarea {
      resize: vertical;
      min-height: 84px;
    }

    .ops-divider {
      border-top: 1px solid rgba(154, 160, 180, 0.25);
      margin: 0.7rem 0;
    }

    .ops-label {
      color: var(--muted);
      font-size: 0.82rem;
    }

    .button-row {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 0.6rem;
    }

    button {
      cursor: pointer;
      background: var(--highlight);
      transition: transform 120ms ease, background 120ms ease;
      font-weight: 600;
    }

    button:hover {
      transform: translateY(-1px);
      background: #165086;
    }

    .rollback-snapshot-item {
      background: rgba(15, 52, 96, 0.35);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 0.7rem;
      margin-bottom: 0.6rem;
    }

    .rollback-snapshot-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 0.4rem;
    }

    .rollback-snapshot-agent {
      font-weight: 600;
      color: #f5f7ff;
      font-size: 0.85rem;
    }

    .rollback-snapshot-time {
      color: var(--muted);
      font-size: 0.75rem;
    }

    .rollback-snapshot-details {
      color: var(--muted);
      font-size: 0.75rem;
      margin-bottom: 0.5rem;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }

    .rollback-btn {
      background: var(--accent);
      border-color: #ff758b;
      color: #fff;
      padding: 0.35rem 0.7rem;
      font-size: 0.78rem;
    }

    .rollback-btn:hover {
      background: #ff5f79;
    }

    .rollback-empty {
      color: var(--muted);
      font-size: 0.85rem;
      text-align: center;
      padding: 1.5rem;
    }

    button.accent {
      background: var(--accent);
      border-color: #ff758b;
    }

    button.accent:hover {
      background: #ff5f79;
    }

    .history-list {
      margin: 0;
      padding-left: 1rem;
      color: var(--muted);
      max-height: 180px;
      overflow-y: auto;
      font-size: 0.85rem;
    }

    .runtime-selected {
      margin: 0 0 0.6rem;
      color: var(--muted);
      font-size: 0.85rem;
    }

    .runtime-history-list {
      margin-bottom: 0.8rem;
      max-height: 160px;
    }

    .runtime-history-empty {
      margin: 0 0 0.8rem;
      color: var(--muted);
      font-size: 0.84rem;
    }

    .runtime-history-hint {
      margin: 0 0 0.6rem;
      color: var(--muted);
      font-size: 0.8rem;
    }

    .runtime-history-entry-btn {
      width: 100%;
      text-align: left;
      background: rgba(22, 49, 85, 0.72);
      color: #d6ddf1;
      border: 1px solid rgba(66, 96, 134, 0.8);
      border-radius: 8px;
      padding: 0.45rem 0.55rem;
      font-size: 0.8rem;
      line-height: 1.3;
    }

    .runtime-history-entry-btn:hover {
      background: rgba(35, 72, 122, 0.85);
    }

    .runtime-diff-form {
      display: grid;
      gap: 0.65rem;
    }

    .runtime-compare-controls {
      display: grid;
      gap: 0.6rem;
      grid-template-columns: 1fr 1fr auto;
      align-items: end;
    }

    .runtime-compare-controls button {
      min-width: 110px;
    }

    .runtime-inline-message {
      margin: 0;
      min-height: 1.2rem;
      color: var(--warn);
      font-size: 0.82rem;
    }

    .runtime-inline-message.error {
      color: var(--accent);
    }

    .runtime-diff-output {
      margin: 0;
      max-height: 220px;
      overflow: auto;
      background: #101c34;
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 0.7rem;
      color: #d6ddf1;
      font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
      font-size: 0.8rem;
      line-height: 1.35;
      word-break: break-word;
    }

    .runtime-diff-text {
      margin: 0;
      white-space: pre-wrap;
      word-break: break-word;
      color: inherit;
      font: inherit;
    }

    .runtime-diff-structured {
      display: grid;
      gap: 0.55rem;
    }

    .runtime-diff-entry {
      border: 1px solid var(--border);
      border-radius: 8px;
      background: #0f192f;
      padding: 0.55rem;
    }

    .runtime-diff-field {
      margin-bottom: 0.45rem;
      font-size: 0.78rem;
      font-weight: 700;
      color: #ecf0fc;
    }

    .runtime-diff-change-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(170px, 1fr));
      gap: 0.45rem;
    }

    .runtime-diff-change {
      border: 1px solid var(--border);
      border-radius: 8px;
      background: #0c1529;
      padding: 0.45rem;
    }

    .runtime-diff-change.before {
      border-color: #4b5f89;
    }

    .runtime-diff-change.after {
      border-color: #2f6d58;
    }

    .runtime-diff-change-label {
      font-size: 0.68rem;
      color: var(--muted);
      text-transform: uppercase;
      margin-bottom: 0.25rem;
    }

    .runtime-diff-change-value {
      margin: 0;
      white-space: pre-wrap;
      word-break: break-word;
      color: #d6ddf1;
      font: inherit;
    }

    .runtime-diff-empty-state,
    .runtime-diff-unavailable-state {
      border: 1px dashed var(--border);
      border-radius: 8px;
      padding: 0.55rem;
      color: var(--muted);
      font-size: 0.78rem;
      background: #0f192f;
    }

    .footer {
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 0.8rem 1rem;
      color: var(--muted);
      font-size: 0.9rem;
    }

    @media (max-width: 960px) {
      .grid {
        grid-template-columns: 1fr;
        grid-template-areas:
          "overview"
          "actions"
          "agents"
          "runtime"
          "events"
          "evolution";
      }

      .event-feed {
        height: 260px;
      }

      .runtime-compare-controls {
        grid-template-columns: 1fr;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <header class="header">
      <h1>EvoClaw Dashboard</h1>
      <div style="display:flex;gap:0.8rem;align-items:center;">
        <div class="lang-switch">
          <button type="button" class="lang-btn active" data-lang="en">EN</button>
          <button type="button" class="lang-btn" data-lang="zh">中文</button>
        </div>
        <div class="status-pill" id="runtime-status">Status: Booting</div>
      </div>
    </header>

    <main class="grid">
      <section class="card overview">
        <h2>System Status</h2>
        <div class="stats">
          <div class="stat">
            <div class="label">Agents</div>
            <div class="value" id="stat-agents">0</div>
          </div>
          <div class="stat">
            <div class="label">Tasks</div>
            <div class="value" id="stat-tasks">0</div>
          </div>
          <div class="stat">
            <div class="label">Events</div>
            <div class="value" id="stat-events">0</div>
          </div>
          <div class="stat">
            <div class="label">Integrity</div>
            <div class="value" id="stat-integrity">-</div>
          </div>
          <div class="stat">
            <div class="label">Runtime Tracked Agents</div>
            <div class="value" id="stat-runtime-tracked-agents">0</div>
          </div>
          <div class="stat">
            <div class="label">Runtime History Entries</div>
            <div class="value" id="stat-runtime-history-entries">0</div>
          </div>
          <div class="stat">
            <div class="label">Runtime Auto Prune</div>
            <div class="value" id="stat-runtime-auto-prune">off</div>
          </div>
          <div class="stat">
            <div class="label">Runtime Keep Last</div>
            <div class="value" id="stat-runtime-keep-last">0</div>
          </div>
        </div>
      </section>

      <section class="card events">
        <h2>Real-time Event Feed</h2>
        <div class="event-feed" id="event-feed"></div>
      </section>

      <section class="card agents">
        <h2>Agents</h2>
        <div class="agent-grid" id="agent-grid"></div>
      </section>

      <section class="card actions">
        <h2>Operations</h2>
        <form id="task-form">
          <textarea id="task-description" placeholder="Describe a task..."></textarea>
          <select id="task-type">
            <option value="execute">Execute</option>
            <option value="route">Route</option>
            <option value="critique">Critique</option>
            <option value="synthesize">Synthesize</option>
            <option value="evolve">Evolve</option>
          </select>
          <div class="button-row">
            <button type="submit">Submit Task</button>
            <button type="button" class="accent" id="evolve-btn">Trigger Evolution</button>
          </div>
        </form>
        <div class="ops-divider"></div>
        <form id="runtime-prune-form">
          <label class="ops-label" for="keep-last-per-agent">Runtime history keep_last_per_agent</label>
          <input id="keep-last-per-agent" type="number" min="0" step="1" value="1" />
          <button type="submit">Prune Runtime History</button>
        </form>
        <div class="ops-divider"></div>
        <form id="runtime-governance-form">
          <label class="ops-label" for="runtime-governance-keep-last">Runtime governance keep_last_per_agent</label>
          <input id="runtime-governance-keep-last" type="number" min="0" step="1" value="0" />
          <button type="submit">Update Runtime Governance</button>
        </form>
      </section>

      <section class="card evolution">
        <h2>Evolution History</h2>
        <ol class="history-list" id="evolution-history"></ol>
      </section>

      <section class="card runtime">
        <h2>Runtime Config Inspector</h2>
        <p class="runtime-selected" id="runtime-inspector-selected">Select an agent and click Inspect Runtime.</p>
        <p class="runtime-history-hint" id="runtime-history-hint">Click a runtime history entry to stage compare versions.</p>
        <ol class="history-list runtime-history-list" id="runtime-history-list"></ol>
        <p class="runtime-history-empty" id="runtime-history-empty">Select an agent to load runtime history.</p>
        <form id="runtime-diff-form" class="runtime-diff-form">
          <div class="runtime-compare-controls">
            <div>
              <label class="ops-label" for="runtime-from-version">from_version</label>
              <input id="runtime-from-version" type="number" min="0" step="1" value="0" />
            </div>
            <div>
              <label class="ops-label" for="runtime-to-version">to_version</label>
              <input id="runtime-to-version" type="number" min="0" step="1" value="0" />
            </div>
            <button type="submit" id="runtime-compare-btn">Compare Versions</button>
          </div>
        </form>
        <p class="runtime-inline-message" id="runtime-diff-message"></p>
        <div class="runtime-diff-output" id="runtime-diff-output">
          <pre class="runtime-diff-text">Diff will appear here after compare.</pre>
        </div>
      </section>

      <section class="card rollback">
        <h2>Rollback Snapshots</h2>
        <div id="rollback-snapshots-list">
          <p class="rollback-empty">Loading snapshots...</p>
        </div>
      </section>
    </main>

    <footer class="footer">
      Event Log Integrity: <span id="integrity-footer">Unknown</span>
    </footer>
  </div>

  <script>
    // i18n: translation dictionary
    const translations = {
      en: {
        'page.title': 'EvoClaw Dashboard',
        'card.system_status': 'System Status',
        'card.events': 'Real-time Event Feed',
        'card.agents': 'Agents',
        'card.operations': 'Operations',
        'card.evolution': 'Evolution History',
        'card.runtime': 'Runtime Config Inspector',
        'stat.agents': 'Agents',
        'stat.tasks': 'Tasks',
        'stat.events': 'Events',
        'stat.integrity': 'Integrity',
        'stat.runtime_tracked': 'Runtime Tracked Agents',
        'stat.runtime_history': 'Runtime History Entries',
        'stat.runtime_auto_prune': 'Runtime Auto Prune',
        'stat.runtime_keep_last': 'Runtime Keep Last',
        'btn.submit_task': 'Submit Task',
        'btn.trigger_evolution': 'Trigger Evolution',
        'btn.prune_history': 'Prune Runtime History',
        'btn.update_governance': 'Update Runtime Governance',
        'btn.compare': 'Compare Versions',
        'btn.inspect': 'Inspect Runtime',
        'label.task_description': 'Describe a task...',
        'label.keep_last_per_agent': 'Runtime history keep_last_per_agent',
        'label.governance_keep_last': 'Runtime governance keep_last_per_agent',
        'label.from_version': 'from_version',
        'label.to_version': 'to_version',
        'status.running': 'Status: Running',
        'status.starting': 'Status: Starting',
        'status.reconnecting': 'Status: SSE reconnecting',
        'status.integrity_unknown': 'Unknown',
        'hint.runtime_history': 'Click a runtime history entry to stage compare versions.',
        'hint.select_agent': 'Select an agent and click Inspect Runtime.',
        'hint.load_history': 'Select an agent to load runtime history.',
        'diff.idle': 'Diff will appear here after compare.',
        'diff.no_changes': 'No runtime config differences between selected versions.',
        'diff.unavailable': 'Runtime diff unavailable: selected versions are missing or pruned from retained history.',
        'footer.integrity': 'Event Log Integrity: ',
        'event.loading_history': 'Loading History...',
        'event.inspecting': 'Inspecting Runtime',
        'card.rollback': 'Rollback Snapshots',
        'btn.rollback': 'Rollback',
        'rollback.empty': 'No rollback snapshots available.',
        'rollback.rolling_back': 'Rolling back...',
        'rollback.success': 'Rollback succeeded',
        'rollback.failed': 'Rollback failed'
      },
      zh: {
        'page.title': 'EvoClaw 仪表盘',
        'card.system_status': '系统状态',
        'card.events': '实时事件流',
        'card.agents': 'Agent',
        'card.operations': '操作',
        'card.evolution': '进化历史',
        'card.runtime': 'Runtime 配置检查器',
        'stat.agents': 'Agent 数量',
        'stat.tasks': '任务数',
        'stat.events': '事件数',
        'stat.integrity': '完整性',
        'stat.runtime_tracked': 'Runtime 跟踪 Agent',
        'stat.runtime_history': 'Runtime 历史条目',
        'stat.runtime_auto_prune': '自动裁剪',
        'stat.runtime_keep_last': '保留条数',
        'btn.submit_task': '提交任务',
        'btn.trigger_evolution': '触发进化',
        'btn.prune_history': '裁剪 Runtime 历史',
        'btn.update_governance': '更新 Governance',
        'btn.compare': '比较版本',
        'btn.inspect': '检查 Runtime',
        'label.task_description': '描述任务...',
        'label.keep_last_per_agent': 'Runtime history keep_last_per_agent',
        'label.governance_keep_last': 'Runtime governance keep_last_per_agent',
        'label.from_version': '起始版本',
        'label.to_version': '目标版本',
        'status.running': '状态：运行中',
        'status.starting': '状态：启动中',
        'status.reconnecting': '状态：SSE 重连中',
        'status.integrity_unknown': '未知',
        'hint.runtime_history': '点击历史条目以 staging 比较版本。',
        'hint.select_agent': '选择一个 Agent 并点击检查 Runtime。',
        'hint.load_history': '选择一个 Agent 以加载 runtime 历史。',
        'diff.idle': '比较后此处将显示 diff。',
        'diff.no_changes': '所选版本间无 runtime 配置差异。',
        'diff.unavailable': 'Runtime diff 不可用：所选版本已被裁剪或不存在。',
        'footer.integrity': '事件日志完整性：',
        'event.loading_history': '加载历史中...',
        'event.inspecting': '检查 Runtime 中',
        'card.rollback': '回滚快照',
        'btn.rollback': '回滚',
        'rollback.empty': '暂无回滚快照。',
        'rollback.rolling_back': '回滚中...',
        'rollback.success': '回滚成功',
        'rollback.failed': '回滚失败'
      }
    };

    // Current language (default: en, restore from localStorage)
    let currentLang = localStorage.getItem('evoclaw_dashboard_lang') || 'en';
    if (!translations[currentLang]) {
      currentLang = 'en';
    }

    // Translation function
    function t(key) {
      const dict = translations[currentLang];
      return dict[key] || translations.en[key] || key;
    }

    // Apply language to UI
    function applyLanguage() {
      document.documentElement.lang = currentLang;
      document.title = t('page.title');

      // Card titles
      document.querySelector('.card.overview h2').textContent = t('card.system_status');
      document.querySelector('.card.events h2').textContent = t('card.events');
      document.querySelector('.card.agents h2').textContent = t('card.agents');
      document.querySelector('.card.actions h2').textContent = t('card.operations');
      document.querySelector('.card.evolution h2').textContent = t('card.evolution');
      document.querySelector('.card.runtime h2').textContent = t('card.runtime');
      const rollbackTitle = document.querySelector('.card.rollback h2');
      if (rollbackTitle) rollbackTitle.textContent = t('card.rollback');

      // Stats
      document.querySelector('#stat-agents').previousElementSibling.textContent = t('stat.agents');
      document.querySelector('#stat-tasks').previousElementSibling.textContent = t('stat.tasks');
      document.querySelector('#stat-events').previousElementSibling.textContent = t('stat.events');
      document.querySelector('#stat-integrity').previousElementSibling.textContent = t('stat.integrity');
      document.querySelector('#stat-runtime-tracked-agents').previousElementSibling.textContent = t('stat.runtime_tracked');
      document.querySelector('#stat-runtime-history-entries').previousElementSibling.textContent = t('stat.runtime_history');
      document.querySelector('#stat-runtime-auto-prune').previousElementSibling.textContent = t('stat.runtime_auto_prune');
      document.querySelector('#stat-runtime-keep-last').previousElementSibling.textContent = t('stat.runtime_keep_last');

      // Buttons
      const taskFormBtn = document.querySelector('#task-form button[type="submit"]');
      if (taskFormBtn) taskFormBtn.textContent = t('btn.submit_task');
      document.querySelector('#evolve-btn').textContent = t('btn.trigger_evolution');
      const pruneBtn = document.querySelector('#runtime-prune-form button[type="submit"]');
      if (pruneBtn) pruneBtn.textContent = t('btn.prune_history');
      const govBtn = document.querySelector('#runtime-governance-form button[type="submit"]');
      if (govBtn) govBtn.textContent = t('btn.update_governance');
      document.querySelector('#runtime-compare-btn').textContent = t('btn.compare');

      // Labels
      const taskDesc = document.querySelector('#task-description');
      if (taskDesc) taskDesc.placeholder = t('label.task_description');
      const pruneLabel = document.querySelector('label[for="keep-last-per-agent"]');
      if (pruneLabel) pruneLabel.textContent = t('label.keep_last_per_agent');
      const govLabel = document.querySelector('label[for="runtime-governance-keep-last"]');
      if (govLabel) govLabel.textContent = t('label.governance_keep_last');
      document.querySelector('label[for="runtime-from-version"]').textContent = t('label.from_version');
      document.querySelector('label[for="runtime-to-version"]').textContent = t('label.to_version');

      // Hints
      document.querySelector('#runtime-history-hint').textContent = t('hint.runtime_history');
      document.querySelector('#runtime-inspector-selected').textContent = t('hint.select_agent');
      document.querySelector('#runtime-history-empty').textContent = t('hint.load_history');

      // Diff
      const diffPre = document.querySelector('#runtime-diff-output .runtime-diff-text');
      if (diffPre) diffPre.textContent = t('diff.idle');

      // Footer
      const footerPrefix = document.querySelector('#integrity-footer').previousSibling;
      if (footerPrefix && footerPrefix.nodeType === Node.TEXT_NODE) {
        footerPrefix.textContent = t('footer.integrity');
      }

      // Update lang buttons
      document.querySelectorAll('.lang-btn').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.lang === currentLang);
      });

      // Refresh status pill if needed
      const statusPill = document.getElementById('runtime-status');
      if (statusPill) {
        if (statusPill.textContent.includes('Running') || statusPill.textContent.includes('运行')) {
          statusPill.textContent = t('status.running');
          statusPill.style.color = 'var(--ok)';
        } else if (statusPill.textContent.includes('Starting') || statusPill.textContent.includes('启动') || statusPill.textContent.includes('Booting')) {
          statusPill.textContent = t('status.starting');
          statusPill.style.color = 'var(--warn)';
        }
      }
    }

    // Language switch handler
    function setupLanguageSwitch() {
      document.querySelectorAll('.lang-btn').forEach(btn => {
        btn.addEventListener('click', () => {
          const newLang = btn.dataset.lang;
          if (newLang !== currentLang) {
            currentLang = newLang;
            localStorage.setItem('evoclaw_dashboard_lang', currentLang);
            applyLanguage();
          }
        });
      });
    }

    const state = {
      events: [],
      evolution: [],
      agentsPayload: null,
      runtimeConfigByAgent: Object.create(null),
      runtimeInspector: {
        selectedAgentId: null,
        historyByAgent: Object.create(null),
        historyLoadingByAgent: Object.create(null),
        historyErrorByAgent: Object.create(null),
        fromVersion: '0',
        toVersion: '0',
        diffOutput: 'Diff will appear here after compare.',
        diffPayload: null,
        diffMessage: '',
        diffError: false,
        diffUnavailable: false,
        diffLoading: false
      }
    };

    const refs = {
      runtimeStatus: document.getElementById('runtime-status'),
      statAgents: document.getElementById('stat-agents'),
      statTasks: document.getElementById('stat-tasks'),
      statEvents: document.getElementById('stat-events'),
      statIntegrity: document.getElementById('stat-integrity'),
      statRuntimeTrackedAgents: document.getElementById('stat-runtime-tracked-agents'),
      statRuntimeHistoryEntries: document.getElementById('stat-runtime-history-entries'),
      statRuntimeAutoPrune: document.getElementById('stat-runtime-auto-prune'),
      statRuntimeKeepLast: document.getElementById('stat-runtime-keep-last'),
      integrityFooter: document.getElementById('integrity-footer'),
      eventFeed: document.getElementById('event-feed'),
      evolutionHistory: document.getElementById('evolution-history'),
      agentGrid: document.getElementById('agent-grid'),
      taskForm: document.getElementById('task-form'),
      taskDescription: document.getElementById('task-description'),
      taskType: document.getElementById('task-type'),
      evolveBtn: document.getElementById('evolve-btn'),
      runtimePruneForm: document.getElementById('runtime-prune-form'),
      keepLastPerAgent: document.getElementById('keep-last-per-agent'),
      runtimeGovernanceForm: document.getElementById('runtime-governance-form'),
      runtimeGovernanceKeepLast: document.getElementById('runtime-governance-keep-last'),
      runtimeInspectorSelected: document.getElementById('runtime-inspector-selected'),
      runtimeHistoryList: document.getElementById('runtime-history-list'),
      runtimeHistoryEmpty: document.getElementById('runtime-history-empty'),
      runtimeDiffForm: document.getElementById('runtime-diff-form'),
      runtimeFromVersion: document.getElementById('runtime-from-version'),
      runtimeToVersion: document.getElementById('runtime-to-version'),
      runtimeCompareBtn: document.getElementById('runtime-compare-btn'),
      runtimeDiffMessage: document.getElementById('runtime-diff-message'),
      runtimeDiffOutput: document.getElementById('runtime-diff-output')
    };

    const runtimeDiffIdleOutput = 'Diff will appear here after compare.';
    const runtimeDiffNoDifferencesOutput = 'No runtime config differences between selected versions.';
    const runtimeDiffUnavailableOutput = 'Runtime diff unavailable: selected versions are missing or pruned from retained history.';
    const runtimeDiffRootFieldPathMap = Object.freeze({
      module_id: 'contract.module_id',
      version: 'contract.version',
      success_rate_threshold: 'contract.success_rate_threshold',
      estimated_cost_token: 'contract.estimated_cost_token',
      intent_tags: 'contract.intent_tags',
      required_tools: 'contract.required_tools'
    });

    function errorDetail(err) {
      if (err && err.payload && typeof err.payload.error === 'string' && err.payload.error) {
        return err.payload.error;
      }
      if (err && typeof err.message === 'string' && err.message) {
        return err.message;
      }
      return 'request failed';
    }

    function fmtTime(raw) {
      if (!raw) return new Date().toLocaleTimeString();
      const date = new Date(raw);
      if (Number.isNaN(date.getTime())) return String(raw);
      return date.toLocaleTimeString();
    }

    function fmtDateTime(raw) {
      if (!raw) return '-';
      const date = new Date(raw);
      if (Number.isNaN(date.getTime())) return String(raw);
      return date.toLocaleString();
    }

    function setIntegrity(ok) {
      const text = ok ? 'PASS' : 'FAIL';
      refs.statIntegrity.textContent = text;
      refs.integrityFooter.textContent = text;
      refs.statIntegrity.className = ok ? 'value integrity-ok' : 'value integrity-bad';
      refs.integrityFooter.className = ok ? 'integrity-ok' : 'integrity-bad';
    }

    function normalizeRuntimeVersion(version) {
      if (typeof version === 'number' && Number.isInteger(version) && version >= 0) {
        return version;
      }
      if (typeof version === 'string' && version.trim() !== '') {
        const parsed = Number.parseInt(version, 10);
        if (Number.isInteger(parsed) && parsed >= 0) {
          return parsed;
        }
      }
      return 0;
    }

    function setCompareVersionsFromTo(toVersion) {
      const safeToVersion = normalizeRuntimeVersion(toVersion);
      const safeFromVersion = Math.max(0, safeToVersion - 1);
      state.runtimeInspector.toVersion = String(safeToVersion);
      state.runtimeInspector.fromVersion = String(safeFromVersion);
      return { fromVersion: safeFromVersion, toVersion: safeToVersion };
    }

    function formatJsonValue(value) {
      const rendered = JSON.stringify(value, null, 2);
      if (typeof rendered === 'string') {
        return rendered;
      }
      if (value === undefined) {
        return 'undefined';
      }
      return String(value);
    }

    function indentLines(text, indent) {
      return String(text).split('\n').map((line) => `${indent}${line}`).join('\n');
    }

    function isRuntimeDiffObject(value) {
      return Boolean(value && typeof value === 'object' && !Array.isArray(value));
    }

    function normalizeRuntimeDiffRootKey(key) {
      if (Object.prototype.hasOwnProperty.call(runtimeDiffRootFieldPathMap, key)) {
        return runtimeDiffRootFieldPathMap[key];
      }
      return key;
    }

    function collectStructuredRuntimeDiffEntries(diffNode, pathParts, entries) {
      if (!isRuntimeDiffObject(diffNode)) {
        return false;
      }

      const hasBefore = Object.prototype.hasOwnProperty.call(diffNode, 'before');
      const hasAfter = Object.prototype.hasOwnProperty.call(diffNode, 'after');
      if (hasBefore && hasAfter) {
        if (pathParts.length === 0) {
          return false;
        }
        entries.push({
          fieldPath: pathParts.join('.'),
          before: diffNode.before,
          after: diffNode.after
        });
        return true;
      }

      const keys = Object.keys(diffNode);
      if (keys.length === 0) {
        return false;
      }

      let structured = true;
      for (const key of keys) {
        const nextPath = pathParts.concat(pathParts.length === 0 ? normalizeRuntimeDiffRootKey(key) : key);
        if (!collectStructuredRuntimeDiffEntries(diffNode[key], nextPath, entries)) {
          structured = false;
        }
      }
      return structured;
    }

    function getStructuredRuntimeDiffEntries(diffPayload) {
      if (!isRuntimeDiffObject(diffPayload)) {
        return null;
      }

      if (Object.keys(diffPayload).length === 0) {
        return [];
      }

      const entries = [];
      const structured = collectStructuredRuntimeDiffEntries(diffPayload, [], entries);
      if (!structured || entries.length === 0) {
        return null;
      }

      entries.sort((left, right) => left.fieldPath.localeCompare(right.fieldPath));
      return entries;
    }

    function createRuntimeDiffValueBlock(label, value, modifierClass) {
      const block = document.createElement('div');
      block.className = `runtime-diff-change ${modifierClass}`;

      const valueLabel = document.createElement('div');
      valueLabel.className = 'runtime-diff-change-label';
      valueLabel.textContent = label;
      block.appendChild(valueLabel);

      const renderedValue = document.createElement('pre');
      renderedValue.className = 'runtime-diff-change-value';
      renderedValue.textContent = formatJsonValue(value);
      block.appendChild(renderedValue);

      return block;
    }

    function renderRuntimeDiffOutput() {
      const inspector = state.runtimeInspector;
      refs.runtimeDiffOutput.innerHTML = '';

      if (inspector.diffUnavailable) {
        const unavailable = document.createElement('div');
        unavailable.className = 'runtime-diff-unavailable-state';
        unavailable.textContent = runtimeDiffUnavailableOutput;
        refs.runtimeDiffOutput.appendChild(unavailable);
        return;
      }

      const structuredEntries = getStructuredRuntimeDiffEntries(inspector.diffPayload);
      if (Array.isArray(structuredEntries)) {
        if (structuredEntries.length === 0) {
          const empty = document.createElement('div');
          empty.className = 'runtime-diff-empty-state';
          empty.textContent = runtimeDiffNoDifferencesOutput;
          refs.runtimeDiffOutput.appendChild(empty);
          return;
        }

        const structured = document.createElement('div');
        structured.className = 'runtime-diff-structured';
        for (const entry of structuredEntries) {
          const entryNode = document.createElement('div');
          entryNode.className = 'runtime-diff-entry';

          const field = document.createElement('div');
          field.className = 'runtime-diff-field';
          field.textContent = entry.fieldPath;
          entryNode.appendChild(field);

          const valueGrid = document.createElement('div');
          valueGrid.className = 'runtime-diff-change-grid';
          valueGrid.appendChild(createRuntimeDiffValueBlock('Before', entry.before, 'before'));
          valueGrid.appendChild(createRuntimeDiffValueBlock('After', entry.after, 'after'));
          entryNode.appendChild(valueGrid);

          structured.appendChild(entryNode);
        }
        refs.runtimeDiffOutput.appendChild(structured);
        return;
      }

      const fallbackText = document.createElement('pre');
      fallbackText.className = 'runtime-diff-text';
      fallbackText.textContent = inspector.diffOutput;
      refs.runtimeDiffOutput.appendChild(fallbackText);
    }

    function formatRuntimeDiff(diffPayload) {
      if (isRuntimeDiffObject(diffPayload)) {
        const keys = Object.keys(diffPayload);
        if (keys.length === 0) {
          return runtimeDiffNoDifferencesOutput;
        }

        const sections = [];
        for (const key of keys.sort()) {
          const change = diffPayload[key];
          const isBeforeAfterObject = Boolean(
            change
            && typeof change === 'object'
            && !Array.isArray(change)
            && Object.prototype.hasOwnProperty.call(change, 'before')
            && Object.prototype.hasOwnProperty.call(change, 'after')
          );

          if (isBeforeAfterObject) {
            sections.push(`${key}
  before:
${indentLines(formatJsonValue(change.before), '    ')}
  after:
${indentLines(formatJsonValue(change.after), '    ')}`);
            continue;
          }

          sections.push(`${key}
${indentLines(formatJsonValue(change), '  ')}`);
        }
        return sections.join('\n\n');
      }

      return formatJsonValue(diffPayload);
    }

    function renderEvolution() {
      refs.evolutionHistory.innerHTML = '';
      const items = state.evolution.slice(-20).reverse();
      for (const entry of items) {
        const li = document.createElement('li');
        li.textContent = `Cycle ${entry.cycle || '?'}: ${entry.tension_count || 0} tensions, ${entry.applied_count || 0} applied`;
        refs.evolutionHistory.appendChild(li);
      }
    }

    function appendEvent(eventObj) {
      state.events.push(eventObj);
      if (state.events.length > 200) state.events.shift();
      refs.statEvents.textContent = state.events.length;

      const atBottom = refs.eventFeed.scrollTop + refs.eventFeed.clientHeight >= refs.eventFeed.scrollHeight - 20;
      const row = document.createElement('div');
      row.className = 'event-item';

      const time = document.createElement('span');
      time.className = 'event-time';
      time.textContent = `[${fmtTime(eventObj.timestamp)}]`;

      const name = document.createElement('span');
      name.className = 'event-name';
      name.textContent = eventObj.event || 'event';

      const msg = document.createElement('span');
      msg.textContent = eventObj.message || JSON.stringify(eventObj);

      row.appendChild(time);
      row.appendChild(name);
      row.appendChild(msg);
      refs.eventFeed.appendChild(row);

      if (refs.eventFeed.children.length > 300) {
        refs.eventFeed.removeChild(refs.eventFeed.firstChild);
      }

      if (atBottom) {
        refs.eventFeed.scrollTop = refs.eventFeed.scrollHeight;
      }

      if (eventObj.event === 'evolution_completed') {
        state.evolution.push(eventObj);
        renderEvolution();
      }
    }

    function renderAgents(payload) {
      refs.agentGrid.innerHTML = '';
      const agents = (payload && payload.agents) || [];
      for (const agent of agents) {
        const runtimeSummary = state.runtimeConfigByAgent[agent.agent_id] || {};
        const runtimeVersion = runtimeSummary.current_version ?? '-';
        const runtimeHistoryCount = runtimeSummary.history_count ?? 0;
        const runtimeLatestChangedAt = fmtDateTime(runtimeSummary.latest_changed_at);
        const card = document.createElement('article');
        card.className = 'agent-card';
        card.innerHTML = `
          <h3>${agent.role || 'agent'} (${agent.agent_id || 'n/a'})</h3>
          <p class="agent-line">Module: ${agent.module_id || '-'}</p>
          <p class="agent-line">Version: ${agent.version || '-'}</p>
          <p class="agent-line">Intents: ${(agent.intent_tags || []).join(', ') || '-'}</p>
          <p class="agent-line">Score: ${Math.round((agent.success_rate_threshold || 0) * 100)}%</p>
          <p class="agent-line">runtime_version: ${runtimeVersion}</p>
          <p class="agent-line">runtime_history_count: ${runtimeHistoryCount}</p>
          <p class="agent-line">runtime_latest_changed_at: ${runtimeLatestChangedAt}</p>
        `;
        const inspectButton = document.createElement('button');
        inspectButton.type = 'button';
        inspectButton.className = 'inspect-runtime-btn';
        const isSelectedAgent = state.runtimeInspector.selectedAgentId === agent.agent_id;
        const historyLoading = Boolean(state.runtimeInspector.historyLoadingByAgent[agent.agent_id]);
        if (isSelectedAgent) {
          inspectButton.className += ' active';
        }
        inspectButton.disabled = historyLoading;
        inspectButton.textContent = historyLoading
          ? 'Loading History...'
          : (isSelectedAgent ? 'Inspecting Runtime' : 'Inspect Runtime');
        inspectButton.addEventListener('click', () => {
          selectRuntimeAgent(agent.agent_id);
        });
        card.appendChild(inspectButton);
        refs.agentGrid.appendChild(card);
      }
    }

    async function getJson(url, init) {
      let res = null;
      try {
        res = await fetch(url, init);
      } catch (err) {
        throw new Error(`request failed: ${err && err.message ? err.message : 'network error'}`);
      }
      let payload = null;
      try {
        payload = await res.json();
      } catch (_) {
        payload = null;
      }
      if (!res.ok) {
        const error = new Error(payload && payload.error ? payload.error : `HTTP ${res.status}`);
        error.payload = payload;
        throw error;
      }
      if (payload === null) {
        throw new Error('Invalid JSON response');
      }
      return payload;
    }

    function renderRuntimeInspector() {
      const inspector = state.runtimeInspector;
      const selectedAgentId = inspector.selectedAgentId;

      refs.runtimeHistoryList.innerHTML = '';
      refs.runtimeHistoryEmpty.style.display = 'none';
      renderRuntimeDiffOutput();
      refs.runtimeFromVersion.value = inspector.fromVersion;
      refs.runtimeToVersion.value = inspector.toVersion;
      refs.runtimeCompareBtn.disabled = !selectedAgentId || inspector.diffLoading;
      refs.runtimeCompareBtn.textContent = inspector.diffLoading
        ? 'Comparing Versions...'
        : (!selectedAgentId ? 'Select Agent to Compare' : 'Compare Versions');
      refs.runtimeDiffMessage.className = inspector.diffError ? 'runtime-inline-message error' : 'runtime-inline-message';
      refs.runtimeDiffMessage.textContent = inspector.diffMessage || '';

      if (!selectedAgentId) {
        refs.runtimeInspectorSelected.textContent = 'Select an agent and click Inspect Runtime.';
        refs.runtimeHistoryEmpty.style.display = 'block';
        refs.runtimeHistoryEmpty.textContent = 'Select an agent to load runtime history.';
        return;
      }

      refs.runtimeInspectorSelected.textContent = `Inspecting agent: ${selectedAgentId}`;
      const history = inspector.historyByAgent[selectedAgentId];
      const isLoading = Boolean(inspector.historyLoadingByAgent[selectedAgentId]);
      const historyError = inspector.historyErrorByAgent[selectedAgentId];

      if (isLoading) {
        refs.runtimeHistoryEmpty.style.display = 'block';
        refs.runtimeHistoryEmpty.textContent = `Loading runtime history for ${selectedAgentId}...`;
        return;
      }

      if (historyError) {
        refs.runtimeHistoryEmpty.style.display = 'block';
        refs.runtimeHistoryEmpty.textContent = historyError;
        return;
      }

      if (!Array.isArray(history) || history.length === 0) {
        refs.runtimeHistoryEmpty.style.display = 'block';
        refs.runtimeHistoryEmpty.textContent = 'No runtime config history entries for this agent.';
        return;
      }

      for (const entry of history.slice().reverse()) {
        const li = document.createElement('li');
        const entryVersion = normalizeRuntimeVersion(entry.version);
        const stageButton = document.createElement('button');
        stageButton.type = 'button';
        stageButton.className = 'runtime-history-entry-btn';
        stageButton.title = 'Stage this entry for compare controls';
        stageButton.textContent = `version=${entry.version ?? '-'} | proposal_id=${entry.proposal_id || '-'} | changed_at=${fmtDateTime(entry.changed_at)}`;
        stageButton.addEventListener('click', () => {
          const versions = setCompareVersionsFromTo(entryVersion);
          inspector.diffMessage = `Staged compare ${versions.fromVersion} -> ${versions.toVersion} from history entry.`;
          inspector.diffError = false;
          renderRuntimeInspector();
        });
        li.appendChild(stageButton);
        refs.runtimeHistoryList.appendChild(li);
      }
    }

    async function ensureRuntimeHistory(agentId, options = {}) {
      if (!agentId) {
        return;
      }
      const inspector = state.runtimeInspector;
      const forceReload = Boolean(options.forceReload);
      const alignCompareToLatest = Boolean(options.alignCompareToLatest);
      if (inspector.historyLoadingByAgent[agentId]) {
        return;
      }
      if (!forceReload
        && Object.prototype.hasOwnProperty.call(inspector.historyByAgent, agentId)
        && !inspector.historyErrorByAgent[agentId]) {
        return;
      }

      inspector.historyLoadingByAgent[agentId] = true;
      inspector.historyErrorByAgent[agentId] = '';
      renderRuntimeInspector();

      try {
        const query = new URLSearchParams({ agent_id: agentId });
        const payload = await getJson(`/api/runtime-config/history?${query.toString()}`);
        inspector.historyByAgent[agentId] = Array.isArray(payload) ? payload : [];
        inspector.historyErrorByAgent[agentId] = '';

        if (alignCompareToLatest && inspector.selectedAgentId === agentId && inspector.historyByAgent[agentId].length > 0) {
          const latest = inspector.historyByAgent[agentId][inspector.historyByAgent[agentId].length - 1];
          setCompareVersionsFromTo(latest.version);
        }
      } catch (err) {
        delete inspector.historyByAgent[agentId];
        inspector.historyErrorByAgent[agentId] = `Runtime history load failed: ${errorDetail(err)}`;
      } finally {
        inspector.historyLoadingByAgent[agentId] = false;
        renderRuntimeInspector();
      }
    }

    function selectRuntimeAgent(agentId) {
      if (!agentId) {
        return;
      }

      const inspector = state.runtimeInspector;
      const wasSameAgent = inspector.selectedAgentId === agentId;
      inspector.selectedAgentId = agentId;
      if (!wasSameAgent) {
        inspector.diffOutput = runtimeDiffIdleOutput;
        inspector.diffPayload = null;
        inspector.diffMessage = '';
        inspector.diffError = false;
        inspector.diffUnavailable = false;

        const runtimeSummary = state.runtimeConfigByAgent[agentId] || {};
        setCompareVersionsFromTo(runtimeSummary.current_version);
      }

      if (state.agentsPayload) {
        renderAgents(state.agentsPayload);
      }
      renderRuntimeInspector();
      ensureRuntimeHistory(agentId, { alignCompareToLatest: !wasSameAgent }).catch(() => {});
    }

    async function refreshStatus() {
      const status = await getJson('/api/status');
      refs.runtimeStatus.textContent = status.initialized ? 'Status: Running' : 'Status: Starting';
      refs.runtimeStatus.style.color = status.initialized ? 'var(--ok)' : 'var(--warn)';
      refs.statAgents.textContent = status.agent_count || 0;
      refs.statTasks.textContent = status.org_log_entries || 0;
      setIntegrity(Boolean(status.event_log_integrity));

      const runtimeConfig = status.runtime_config || {};
      refs.statRuntimeTrackedAgents.textContent = runtimeConfig.tracked_agents || 0;
      refs.statRuntimeHistoryEntries.textContent = runtimeConfig.history_entries || 0;
      const runtimeGovernance = runtimeConfig.governance || {};
      const autoPruneEnabled = Boolean(runtimeGovernance.auto_prune_enabled);
      refs.statRuntimeAutoPrune.textContent = autoPruneEnabled ? 'on' : 'off';
      refs.statRuntimeKeepLast.textContent = runtimeGovernance.keep_last_per_agent || 0;
      refs.runtimeGovernanceKeepLast.value = String(runtimeGovernance.keep_last_per_agent || 0);
      state.runtimeConfigByAgent = Object.create(null);
      for (const item of runtimeConfig.agents || []) {
        if (item && item.agent_id) {
          state.runtimeConfigByAgent[item.agent_id] = item;
        }
      }
      // Lazy load rollback snapshots on first render
      if (!state.rollbackSnapshotsLoaded) {
        state.rollbackSnapshotsLoaded = true;
        renderRollbackSnapshots(status.rollback_snapshots || []);
      }
      if (state.agentsPayload) {
        renderAgents(state.agentsPayload);
      }

      if (status.evolution && status.evolution.last_cycle && status.evolution.last_cycle.status === 'completed') {
        const cycle = status.evolution.last_cycle;
        cycle.cycle = cycle.cycle || state.evolution.length + 1;
        const already = state.evolution.some((e) => e.cycle === cycle.cycle && e.timestamp === cycle.timestamp);
        if (!already) {
          state.evolution.push(cycle);
          renderEvolution();
        }
      }
    }

    async function refreshAgents() {
      const agents = await getJson('/api/agents');
      state.agentsPayload = agents;
      renderAgents(state.agentsPayload);

      const selectedAgentId = state.runtimeInspector.selectedAgentId;
      if (!selectedAgentId) {
        return;
      }
      const exists = (state.agentsPayload.agents || []).some((agent) => agent.agent_id === selectedAgentId);
      if (!exists) {
        state.runtimeInspector.selectedAgentId = null;
        state.runtimeInspector.diffOutput = runtimeDiffIdleOutput;
        state.runtimeInspector.diffPayload = null;
        state.runtimeInspector.diffMessage = '';
        state.runtimeInspector.diffError = false;
        state.runtimeInspector.diffUnavailable = false;
        state.runtimeInspector.diffLoading = false;
      }
      renderRuntimeInspector();
    }

    function renderRollbackSnapshots(snapshots) {
      const container = document.getElementById('rollback-snapshots-list');
      if (!container) return;

      if (!snapshots || snapshots.length === 0) {
        container.innerHTML = '<p class="rollback-empty">No rollback snapshots available.</p>';
        return;
      }

      container.innerHTML = '';
      for (const snapshot of snapshots) {
        if (!snapshot || !snapshot.proposal_id) continue;

        const item = document.createElement('div');
        item.className = 'rollback-snapshot-item';

        const header = document.createElement('div');
        header.className = 'rollback-snapshot-header';

        const agentSpan = document.createElement('span');
        agentSpan.className = 'rollback-snapshot-agent';
        agentSpan.textContent = snapshot.agent_id || 'unknown';

        const timeSpan = document.createElement('span');
        timeSpan.className = 'rollback-snapshot-time';
        timeSpan.textContent = fmtDateTime(snapshot.applied_at);

        header.appendChild(agentSpan);
        header.appendChild(timeSpan);

        const details = document.createElement('div');
        details.className = 'rollback-snapshot-details';
        const beforeSummary = snapshot.config_before && snapshot.config_before.system_prompt
          ? snapshot.config_before.system_prompt.substring(0, 50)
          : '-';
        details.textContent = `proposal: ${snapshot.proposal_id.substring(0, 8)}... | before: ${beforeSummary}...`;

        const rollbackBtn = document.createElement('button');
        rollbackBtn.type = 'button';
        rollbackBtn.className = 'rollback-btn';
        rollbackBtn.textContent = 'Rollback';
        rollbackBtn.dataset.proposalId = snapshot.proposal_id;

        item.appendChild(header);
        item.appendChild(details);
        item.appendChild(rollbackBtn);

        container.appendChild(item);
      }
    }

    document.getElementById('rollback-snapshots-list').addEventListener('click', async (event) => {
      if (!event.target.classList.contains('rollback-btn')) return;

      const btn = event.target;
      const proposalId = btn.dataset.proposalId;
      if (!proposalId) return;

      btn.disabled = true;
      btn.textContent = 'Rolling back...';

      try {
        const payload = await getJson('/api/runtime-config/rollback', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ proposal_id: proposalId })
        });

        await Promise.all([refreshStatus(), refreshAgents()]);
        if (state.runtimeInspector.selectedAgentId) {
          await ensureRuntimeHistory(state.runtimeInspector.selectedAgentId, {
            forceReload: true,
            alignCompareToLatest: true
          });
        }

        appendEvent({
          event: 'runtime_config_rollback_success',
          message: `Rollback succeeded: ${proposalId.substring(0, 8)}...`,
          timestamp: new Date().toISOString()
        });
      } catch (err) {
        appendEvent({
          event: 'runtime_config_rollback_failed',
          message: `Rollback failed: ${err.message}`,
          timestamp: new Date().toISOString()
        });
        btn.disabled = false;
        btn.textContent = 'Rollback';
      }
    });

    async function loadEventHistory() {
      const payload = await getJson('/api/events');
      refs.eventFeed.innerHTML = '';
      state.events = [];
      for (const eventObj of payload.events || []) {
        appendEvent(eventObj);
      }
    }

    function connectStream() {
      const source = new EventSource('/api/events/stream');
      const sseStatusEl = document.getElementById('sse-status');
      
      // Update SSE status
      source.onopen = () => {
        if (sseStatusEl) sseStatusEl.textContent = '🟢';
      };
      source.onerror = () => {
        if (sseStatusEl) sseStatusEl.textContent = '🔴';
        // Auto-reconnect after 3 seconds
        setTimeout(() => {
          if (sseStatusEl) sseStatusEl.textContent = '🟡';
        }, 3000);
      };

      const eventTypes = [
        'task_submitted', 'task_complete', 'task_failed', 'agent_registered',
        'evolution_started', 'evolution_completed', 'tension_detected',
        'proposal_applied', 'proposal_rejected', 'server_started',
        'runtime_config_rollback_success', 'runtime_config_rollback_failed',
        'runtime_config_history_pruned', 'runtime_governance_updated',
        'runtime_governance_update_failed'
      ];

      const onEvent = (evt) => {
        try {
          const data = JSON.parse(evt.data);
          appendEvent(data);
          if (data.event === 'agent_registered') {
            refreshAgents().catch(() => {});
          }
        } catch (_) {
          appendEvent({ event: evt.type || 'message', message: evt.data, timestamp: new Date().toISOString() });
        }
      };

      eventTypes.forEach((name) => source.addEventListener(name, onEvent));
      source.onmessage = onEvent;
      source.onerror = () => {
        refs.runtimeStatus.textContent = 'Status: SSE reconnecting';
        refs.runtimeStatus.style.color = 'var(--warn)';
      };
    }

    refs.taskForm.addEventListener('submit', async (event) => {
      event.preventDefault();
      const description = refs.taskDescription.value.trim();
      if (!description) return;

      try {
        await getJson('/api/task', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            description,
            type: refs.taskType.value
          })
        });
        refs.taskDescription.value = '';
      } catch (err) {
        appendEvent({
          event: 'task_failed',
          message: `Task submit failed: ${err.message}`,
          timestamp: new Date().toISOString()
        });
      }
    });

    refs.evolveBtn.addEventListener('click', async () => {
      try {
        await getJson('/api/evolve', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: '{}'
        });
      } catch (err) {
        appendEvent({
          event: 'evolution_failed',
          message: `Evolution trigger failed: ${err.message}`,
          timestamp: new Date().toISOString()
        });
      }
    });

    refs.runtimePruneForm.addEventListener('submit', async (event) => {
      event.preventDefault();
      const keepLastPerAgent = Number.parseInt(refs.keepLastPerAgent.value, 10);
      if (!Number.isInteger(keepLastPerAgent) || keepLastPerAgent < 0) {
        appendEvent({
          event: 'runtime_config_prune_failed',
          message: 'Runtime config prune failed: keep_last_per_agent must be a non-negative integer',
          timestamp: new Date().toISOString()
        });
        return;
      }

      try {
        const payload = await getJson('/api/runtime-config/history/prune', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            keep_last_per_agent: keepLastPerAgent
          })
        });

        state.runtimeInspector.historyByAgent = Object.create(null);
        state.runtimeInspector.historyLoadingByAgent = Object.create(null);
        state.runtimeInspector.historyErrorByAgent = Object.create(null);

        await Promise.all([refreshStatus(), refreshAgents()]);
        if (state.runtimeInspector.selectedAgentId) {
          await ensureRuntimeHistory(state.runtimeInspector.selectedAgentId, {
            forceReload: true,
            alignCompareToLatest: true
          });
        }

        const historyEntries = payload
          && payload.runtime_config
          && Number.isInteger(payload.runtime_config.history_entries)
          ? payload.runtime_config.history_entries
          : 'n/a';
        appendEvent({
          event: 'runtime_config_prune_success',
          message: `Runtime config history pruned (keep_last_per_agent=${keepLastPerAgent}, history_entries=${historyEntries})`,
          timestamp: new Date().toISOString()
        });
      } catch (err) {
        appendEvent({
          event: 'runtime_config_prune_failed',
          message: `Runtime config prune failed: ${err.message}`,
          timestamp: new Date().toISOString()
        });
      }
    });

    refs.runtimeGovernanceForm.addEventListener('submit', async (event) => {
      event.preventDefault();
      const keepLastPerAgent = Number.parseInt(refs.runtimeGovernanceKeepLast.value, 10);
      if (!Number.isInteger(keepLastPerAgent) || keepLastPerAgent < 0) {
        appendEvent({
          event: 'runtime_governance_update_failed',
          message: 'Runtime governance update failed: keep_last_per_agent must be a non-negative integer',
          timestamp: new Date().toISOString()
        });
        return;
      }

      try {
        const payload = await getJson('/api/runtime-config/governance', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            keep_last_per_agent: keepLastPerAgent
          })
        });

        state.runtimeInspector.historyByAgent = Object.create(null);
        state.runtimeInspector.historyLoadingByAgent = Object.create(null);
        state.runtimeInspector.historyErrorByAgent = Object.create(null);

        await Promise.all([refreshStatus(), refreshAgents()]);
        if (state.runtimeInspector.selectedAgentId) {
          await ensureRuntimeHistory(state.runtimeInspector.selectedAgentId, {
            forceReload: true,
            alignCompareToLatest: true
          });
        }

        const enabled = payload && payload.governance && payload.governance.auto_prune_enabled;
        appendEvent({
          event: 'runtime_governance_updated',
          message: `Runtime governance updated (auto_prune=${enabled ? 'on' : 'off'}, keep_last_per_agent=${keepLastPerAgent})`,
          timestamp: new Date().toISOString()
        });
      } catch (err) {
        appendEvent({
          event: 'runtime_governance_update_failed',
          message: `Runtime governance update failed: ${err.message}`,
          timestamp: new Date().toISOString()
        });
      }
    });

    refs.runtimeDiffForm.addEventListener('submit', async (event) => {
      event.preventDefault();
      const selectedAgentId = state.runtimeInspector.selectedAgentId;
      if (!selectedAgentId) {
        state.runtimeInspector.diffMessage = 'Select an agent before comparing runtime versions.';
        state.runtimeInspector.diffError = true;
        renderRuntimeInspector();
        return;
      }

      const fromVersion = Number.parseInt(refs.runtimeFromVersion.value, 10);
      const toVersion = Number.parseInt(refs.runtimeToVersion.value, 10);
      if (!Number.isInteger(fromVersion) || !Number.isInteger(toVersion) || fromVersion < 0 || toVersion < 0) {
        state.runtimeInspector.diffMessage = 'from_version and to_version must be non-negative integers.';
        state.runtimeInspector.diffError = true;
        renderRuntimeInspector();
        return;
      }

      state.runtimeInspector.fromVersion = String(fromVersion);
      state.runtimeInspector.toVersion = String(toVersion);
      state.runtimeInspector.diffLoading = true;
      state.runtimeInspector.diffMessage = `Comparing runtime config versions ${fromVersion} -> ${toVersion}...`;
      state.runtimeInspector.diffError = false;
      state.runtimeInspector.diffUnavailable = false;
      renderRuntimeInspector();

      try {
        const query = new URLSearchParams({
          agent_id: selectedAgentId,
          from_version: String(fromVersion),
          to_version: String(toVersion)
        });
        const payload = await getJson(`/api/runtime-config/diff?${query.toString()}`);
        const diffPayload = Object.prototype.hasOwnProperty.call(payload, 'diff') ? payload.diff : {};
        state.runtimeInspector.diffPayload = diffPayload;
        state.runtimeInspector.diffOutput = formatRuntimeDiff(diffPayload);
        state.runtimeInspector.diffMessage = `Compare succeeded: ${fromVersion} -> ${toVersion}.`;
        state.runtimeInspector.diffError = false;
        state.runtimeInspector.diffUnavailable = false;
      } catch (err) {
        let message = `Runtime diff request failed: ${errorDetail(err)}.`;
        if (err && err.payload && err.payload.error === 'version_not_found') {
          const requested = err.payload.requested_version;
          const current = err.payload.current_version;
          if (typeof requested === 'number' && typeof current === 'number') {
            message = `Runtime diff failed: version_not_found (requested=${requested}, current=${current}).`;
          } else if (typeof requested === 'number') {
            message = `Runtime diff failed: version_not_found (requested=${requested}).`;
          } else {
            message = 'Runtime diff failed: version_not_found.';
          }
        }
        state.runtimeInspector.diffPayload = null;
        state.runtimeInspector.diffOutput = runtimeDiffUnavailableOutput;
        state.runtimeInspector.diffMessage = message;
        state.runtimeInspector.diffError = true;
        state.runtimeInspector.diffUnavailable = true;
        appendEvent({
          event: 'runtime_config_diff_failed',
          message: `${message} [agent_id=${selectedAgentId}, from_version=${fromVersion}, to_version=${toVersion}]`,
          timestamp: new Date().toISOString()
        });
      } finally {
        state.runtimeInspector.diffLoading = false;
        renderRuntimeInspector();
      }
    });

    async function bootstrap() {
      // Initialize i18n
      setupLanguageSwitch();
      applyLanguage();

      try {
        await Promise.all([refreshStatus(), refreshAgents(), loadEventHistory()]);
      } catch (err) {
        appendEvent({
          event: 'dashboard_error',
          message: `Bootstrap failed: ${err.message}`,
          timestamp: new Date().toISOString()
        });
      }

      connectStream();
      renderRuntimeInspector();
      setInterval(() => refreshStatus().catch(() => {}), 10000);
      setInterval(() => refreshAgents().catch(() => {}), 15000);
    }

    bootstrap();
  </script>
</body>
</html>
)html";

} // namespace evoclaw::server::dashboard
