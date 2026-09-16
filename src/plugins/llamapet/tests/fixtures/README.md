# 测试夹具说明 (fixtures/)

离线响应样例，供 `src-tauri/tests/` 的解析单测使用（T1.3a/T1.3b/T3.3a/T3.3b 各卡会复制到 `src-tauri/tests/fixtures/`）。**测试断言以本文件期望值为准；发现夹具与真实服务行为不符 → 改为登记任务卡"问题记录"升级，不得自行改夹具**。

| 文件 | 模拟端点 | 用途卡 |
| :--- | :--- | :--- |
| `llamacpp_health.json` | `GET /health` | T1.3a |
| `llamacpp_props.json` | `GET /props` | T1.3a |
| `llamacpp_slots.json` | `GET /slots` | T1.3b、T2.1a（mock 数据同源） |
| `ollama_ps.json` | `GET /api/ps` | T3.3a |
| `openai_models.json` | `GET /v1/models` | T3.3b |

## llamacpp_slots.json 期望值（断言基准）

字段形态假设（与真实 llama.cpp 不符时升级，见上）：`state`：`0=idle`、`2=Prefill 处理中`、`3=解码生成中`；`cache_tokens` 为**已缓存 token id 数组**（长度即命中数）；`decoded_tokens` 缺失时回退 `n_decoded`。

| 指标 | 期望值 | 计算依据 |
| :--- | :--- | :--- |
| 槽位 0 命中率 | 20/24 ≈ 83.33% | `len(cache_tokens)/prompt_tokens` |
| 槽位 1 命中率 | 0/20 = 0% | 空数组 |
| 槽位 2 命中率 | 28/32 = 87.5% | — |
| 槽位 3 | 不参与聚合 | `prompt_tokens=0` |
| 整体命中率 | 48/76 ≈ 63.16% | Σcached/Σprompt（仅 prompt>0 槽位） |
| active_slots / total_slots | 3 / 4 | state ≠ 0 的数量 / 全部 |
| any_prefill / any_decoding | true / true | 槽 2；槽 0、1 |
| Σdecoded_tokens（TPS 差分基数） | 18+2+0+0 = 20 | — |
| speculative_active | false | 全槽 false |

## ollama_ps.json 期望值

| 指标 | 期望值 |
| :--- | :--- |
| 模型数（active_slots 语义） | 2 |
| size_vram 合计 | 8954829312 + 271044608 = 9225873920 |
| 其余遥测字段（tps/KV/槽位/推测解码） | 全部 None（能力矩阵，DESIGN.md §2） |

## openai_models.json 期望值

| 指标 | 期望值 |
| :--- | :--- |
| 模型清单 | `["qwen2.5-14b-instruct", "embedding-model"]` |
| connected | true |
| 其余遥测字段 | 全部 None |
