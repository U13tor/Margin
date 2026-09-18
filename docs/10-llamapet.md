# 10 — LlamaPet 本地大模型遥测与显卡桌宠

LlamaPet 是 Margin 专为本地大模型（LLM）推理和端侧开发者打造的实时遥测与桌面宠物伴侣插件。它将深度的硬件遥测、推理状态跟踪与生动的情绪点阵桌宠深度融合，且全过程遵守严格的本地回环隐私护栏。

---

## 核心特性

- **多推理后端无缝接入**：原生对接 `llama.cpp`（支持原生 `/slots`、`/health` 及 `/props` 协议）、`Ollama` 与各类兼容 OpenAI API 规范的端侧推理端点。
- **NVIDIA GPU 驱动级遥测**：实时捕获显存占用（VRAM）、核心利用率（GPU %）、实时功耗（W）与核心温度（°C）。优先采用 `nvml.dll` 驱动层直连，在无驱动或轻量化环境下自适应降级至 `nvidia-smi` 轮询。
- **情绪驱动桌宠伴侣**：
  - **双形态切换**：MiniPet 极简点阵宠物（96×96）与 Dock 高密仪表盘（260×50）；
  - **7 态点阵动画**：羊驼点阵根据推理状态（就绪、预填充 Prefill、解码中 Decoding、显存警告 Warning、高负载 Critical、离线 Disconnected 等）生动变幻；
  - **交互体验**：支持屏幕边缘吸附（≤20px 磁吸）、贴边自动半隐藏（Peek 3s 唤出）、Win32 鼠标穿透（Click-through，不遮挡工作区）与全局召唤快捷键 `Ctrl+Alt+P`。
- **GeekHUD 高密度仪表盘与按天 Token 统计**：
  - **双子视图自由切换**：顶部常驻分段控制器，在「实时监控（Live Monitor）」与「活跃度与统计（Activity & Stats）」之间无缝切换；
  - **实时走势大屏**：60 个采样周期的 VRAM / TPS 双轨实时走势 Canvas 曲线，槽位矩阵（SLOT ID、生成状态、Prompt 处理、KV 命中率、生成 Token 数）与一键释放空闲 KV 缓存（`erase idle slots`）；
  - **GitHub 风格 52 周热力图**：以 Catppuccin 5 级色阶呈现过去 52 周的每日 Token 生成分布，自适应横向平滑滚动，悬停色块即显当日精确 Token 数与活跃时长；
  - **7d / 30d 用量走势图**：堆叠柱状图对比提示词（Prompt Eval）与生成（Decoded）消耗；
  - **5 核心指标看板**：累计 Token 数、单日峰值用量、累计活跃时长、当前连续活跃天数与历史最长连续天数；
  - **权威双通道采集与本地隐私持久化**：优先通过 Prometheus `--metrics` 采集单调计数器（高并发无漏计），无缝回退 `/slots` 采样；基于本地 SQLite `llamapet_token_daily` 表，5 秒批量落盘，零网络外泄。

---

## 快速入门

1. **启用插件**：在 Margin 首次启动时，授权 LlamaPet 的 `localhost-http` 与 `database-write` 权限。
2. **默认端点**：出厂默认连接 `http://127.0.0.1:1802`。如果运行在其他端口，打开「设置中心 → LlamaPet」调整端点 URL（如 llama.cpp 默认端口 `http://127.0.0.1:8080`）。
3. **呼出桌宠**：按下全局快捷键 `Ctrl+Alt+P`，或通过系统托盘菜单点击「Show Floating Window」，点阵桌宠即可浮现于桌面右下角。
4. **查看仪表盘与热力图**：点击托盘图标打开 Margin 主面板切换至「LlamaPet」，或右键悬浮窗点击「Usage Stats & Heatmap」，即可直达用量统计与热力图。可在顶部「Live Monitor」与「Activity & Stats」间自由切换。

---

## 推理服务启动推荐参数

为了让 LlamaPet 获得最完整的推理与槽位遥测数据，推荐使用以下参数启动本地推理后端：

### 1. llama.cpp (`llama-server`)

> [!IMPORTANT]
> 务必加上 `--slots` 参数以开启槽位明细接口。若未带 `--slots`，LlamaPet 仍可正常获取 GPU 硬件指标与基础连通状态，但槽位矩阵将优雅提示降级。

推荐启动命令示例：

```bash
llama-server \
  -m /path/to/models/your-model.gguf \
  --port 8080 \
  --host 127.0.0.1 \
  --slots \
  --metrics \
  -ngl 99 \
  -c 4096
```

- `--slots`：开启 `/slots` API，暴露并发槽位明细与 KV 缓存占用率；
- `--host 127.0.0.1`：绑定本地回环地址（配合 Margin `LoopbackGuard` 护栏）；
- `--metrics`：开放 Prometheus 风格实时统计指标。

### 2. Ollama

Ollama 默认绑定 `http://127.0.0.1:11434`。在「设置中心 → LlamaPet」中将引擎类型切换为 **Ollama**，并将端点设置为：

```text
http://127.0.0.1:11434
```

LlamaPet 将通过 Ollama 的本地进程状态接口获取正在运行的模型名称与显存分配。

### 3. OpenAI 兼容端点 (Local vLLM / LocalAI)

若使用本地启动的 vLLM 或 LocalAI，端点填入类似 `http://127.0.0.1:8000/v1`，选择 **OpenAI Compatible** 引擎即可。

---

## 硬件与显存告警

在「设置中心 → LlamaPet」中，您可以根据显卡的物理容量定制两档显存阈值：

- **显存警告阈值（Warning Threshold）**：默认 80%。当 VRAM 占用超过该值时，桌宠眼神变得紧绷，仪表盘走势图渐变至琥珀黄提示。
- **显存危急阈值（Critical Threshold）**：默认 90%。当超过该值时，桌宠触发汗滴惊慌动画，走势图呈现霓虹红告警，提醒开发者可能发生显存溢出（OOM）或上下文截断。

---

## 密钥保护与安全机制

- **API Key 透明加密**：部分本地端点配置了访问密码或 Token。LlamaPet 收集的 API Key 会在存盘时自动调用 Windows DPAPI（绑定当前 Windows 用户凭据），以 AES-256-GCM 密文保存在配置文件中，内存中按需解密，零明文落地。
- **回环护栏（LoopbackGuard）**：LlamaPet 的底层网络模块受 `LoopbackGuard` 强校验。所有发出的 HTTP 请求必须严格指向 `127.0.0.1`、`localhost` 或 `::1`。任何试图访问局域网或公网地址的请求均会被代码底层直接拦截并抛出硬断言，确保推理提示词与硬件信息绝不泄漏到任何外部网络。

---

## 悬浮窗交互指南

| 交互操作 | 操作方式 | 说明 |
|---|---|---|
| **唤出 / 隐藏** | `Ctrl+Alt+P` 全局热键，或托盘菜单项 | 随时一键显隐，不占用任务栏位置 |
| **拖拽移动** | 鼠标左键按住拖动 | 松开鼠标后自动保存位置 |
| **形态切换** | 托盘菜单「Switch to Dock / MiniPet」，或双击桌宠 | MiniPet (96×96) 专注可爱陪伴；Dock (260×50) 专注数据流监控 |
| **边缘吸附与隐藏** | 将窗口拖近屏幕任一边缘（≤ 20px） | 释放时自动磁吸贴边；开启「贴边自动隐藏」后，鼠标移开 3 秒进入 Peek 半折叠状态，鼠标悬停即刻展出 |
| **鼠标穿透** | 托盘菜单勾选「Click-Through」 | 开启 Win32 穿透样式（`WS_EX_TRANSPARENT`），点击和滚动将直接穿透至底层代码编辑器，避免遮挡工作流 |
| **释放空闲 KV** | 托盘菜单点击「Clear Idle KV Cache」或 HUD 界面按钮 | 向后端发送清空空闲槽位指令，立即释放显存且不中断正在生成的任务 |
| **复制重启命令** | 托盘菜单点击「Copy Restart Command」 | 将带有当前最佳参数的启动命令行一键复制到剪贴板 |
| **用量统计与热力图** | 悬浮窗右键菜单点击「Usage Stats & Heatmap」 | 直达 GeekHUD 仪表盘并切换到「活跃度与统计」视图 |

---

## 常见问题 (FAQ)

### 1. 为什么槽位明细显示「Slots telemetry not available」？
通常是因为 `llama-server` 启动时未添加 `--slots` 参数。请参考上方推荐参数重启服务即可恢复。

### 2. 托盘显示 401/403 认证错误？
若本地服务开启了鉴权（如 `--api-key your-key`），请进入「设置中心 → LlamaPet」填入对应的 API Key。LlamaPet 会自动在请求头中附带 `Authorization: Bearer <key>`。

### 3. GPU 遥测指标显示 N/A？
- 确认是否为 NVIDIA 独立显卡且已安装官方显卡驱动；
- LlamaPet 会自动按顺序探测系统中的 `nvml.dll` 与 `nvidia-smi.exe`；若在核显设备或无独显环境下运行，GPU 模块将自动静默，仅保留大模型推理网络指标。
