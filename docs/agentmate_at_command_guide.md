# AgentMate AT 指令指南

文档版本：**1.2**  
协议主版本：**1**  
状态：ESP32 基础版固件已实现；服务端对接待完成  
适用连接：首版为 USB 串口，`115200 8N1`

本文是 [AgentMate 服务端与 ESP32 通信技术方案](agentmate_communication_plan.md) 的指令参考。服务端发送命令，设备返回数据行及最终结果；串口终端也可用于手工调试。

## 1. 支持的命令总览

| 命令 | 功能 | 主要响应 |
|---|---|---|
| `AT+HELLO=<version>,<server_nonce>,<server_id>` | 发现设备、协商版本、发起身份挑战 | `+HELLO:...`、`OK` |
| `AT+PAIRBEGIN` | 开启四位码配对窗口 | `+PAIR:<pair_id>,<remaining_s>`、`OK` |
| `AT+PAIRCOMMIT=<pair_id>,<pin>,<key_hex>` | 校验屏幕四位码并保存新配对密钥 | `OK` |
| `AT+AUTH=<host_proof>` | 服务端完成双向认证 | `OK` |
| `AT+SYNCBEGIN=<epoch>,<sync_id>` | 开始逐条会话同步 | `OK` |
| `AT+SESSION=<epoch>,<key>,<rev>,<provider>,<state>,<last_task>,<finished_at>,<task_seconds>,<ctx_used>,<ctx_window>,<ctx_pct_bp>,<ctx_updated_at>` | 新增或更新一条会话 | `OK` |
| `AT+DELETE=<epoch>,<key>` | 删除一条会话 | `OK` |
| `AT+SYNCEND=<epoch>,<sync_id>` | 结束同步并删除未出现的旧会话 | `OK` |
| `AT+LIST?` | 查询设备上的全部会话键及版本 | `+LIST:...`、`+ITEM:...`、`OK` |
| `AT+GET=<key>` | 查询一条会话的完整协议字段 | `+SESSION:...`、`OK` |
| `AT+USAGE=<epoch>,<provider>,<metric_key>,<window_key>,<used_bp>,<reset_at>,<window_seconds>,<updated_at>` | 同步一个 provider 的用量窗口 | `OK` |
| `AT+USAGECLEAR=<epoch>,<provider>` | 清除指定 provider 的用量记录 | `OK` |
| `AT+EVENT=<epoch>,<event_id>,<key>,<provider>,<kind>,<occurred_at>` | 发送一次性任务提醒 | `OK` |
| `AT+PING=<nonce>` | 检查设备活动状态 | `+PONG:...`、`OK` |
| `AT+TIME=<unix_ms>` | 按服务端 UTC 时间校准设备时钟 | `+TIME:...`、`OK` |
| `AT+TIME?` | 查询设备 UTC 时间和校时状态 | `+TIME:...`、`OK` |
| `AT+INFO?` | 查询设备信息及会话容量 | `+INFO:...`、`OK` |
| `AT+TFTBRIGHT=<value>` | 设置并保存 TFT 背光亮度 | `OK` |
| `AT+TFTBRIGHT?` | 查询 TFT 背光亮度 | `+TFTBRIGHT:<value>`、`OK` |
| `AT+LEDBRIGHT=<value>` | 设置并保存 LED 灯环全局亮度 | `OK` |
| `AT+LEDBRIGHT?` | 查询 LED 灯环全局亮度 | `+LEDBRIGHT:<value>`、`OK` |
| `AT+TFTROTATE=<angle>` | 设置并保存 TFT 界面旋转角度 | `OK` |
| `AT+TFTROTATE?` | 查询 TFT 界面旋转角度 | `+TFTROTATE:<angle>`、`OK` |

所有命令都以单独的 `OK` 或 `ERROR` 行结束。失败时设备可先返回 `+ERR:<code>`。`+HELLO:`、`+PAIR:`、`+LIST:`、`+ITEM:`、`+SESSION:`、`+PONG:`、`+TIME:`、`+INFO:` 和亮度查询结果是**响应行**，不能由服务端当作命令发送。

## 2. 通用约定

- 命令与响应均为 ASCII，一行以 `\r\n` 结束；接收端兼容 `\n`。每行最多 256 字节，**包括行尾**。示例为方便阅读省略行尾字节。
- 服务端同一时刻只发送一条尚未收到最终结果的命令。设备可以先返回一条或多条 `+...:` 数据行，最后返回 `OK` 或 `ERROR`。调试日志不得写进协议串口。
- 参数严格按表中顺序以英文逗号分隔；不接受多余字段、空格、引号或自动转义。只有明确允许的字段可为空或写 `?`。解析器必须保留连续逗号形成的空字段。
- `device_id`、`server_id`、`epoch`、握手用 nonce、`pair_id` 和 `event_id` 为 32 个小写十六进制字符；`key`、`metric_key` 和 `window_key` 为 1～8 位小写字母或数字；`provider` 为 1～16 位小写字母或数字。其他短标识只能使用 `[A-Za-z0-9_.-]`。非负整数用十进制且没有前导 `+`；四位 `pin` 是例外，必须保留前导零。
- `server_id` 在同一台电脑的 AgentMate 安装期间持久不变，与每次服务端启动生成的 `epoch` 不同。设备只保存一台电脑的 `server_id` 和随机长期密钥；新电脑完成四位码配对后替换旧配对。
- `epoch` 是服务端本次启动生成的随机 ID；新的 epoch 使旧会话 key 和旧用量失效。`rev` 是同一 epoch 内、同一会话的递增 64 位版本。`sync_id` 是同一 epoch 内递增的正整数。
- `HELLO`、`INFO?`、`PAIRBEGIN`、`PAIRCOMMIT` 和 `AUTH` 可在认证前使用；配对命令只能在有效 `HELLO` 后、按下文的配对窗口规则执行。其他命令均须在认证后使用。显式启用的未配对开发调试模式可跳过正式密钥认证；该模式不提供可信身份保证。手工调试业务命令时，应先完成握手与认证，或启动此调试模式。
- 正常命令建议在 2 秒内取得最终结果，`PING` 可等待 5 秒。设备在 30 秒内没有收到有效命令时清除远端会话和用量；亮度配置为本地持久设置，不因断线而清除。

## 3. `AT+HELLO`：发现与设备能力

**格式**：`AT+HELLO=<version>,<server_nonce>,<server_id>`

| 参数 | 说明 |
|---|---|
| `version` | 协议主版本，本文固定为 `1` |
| `server_nonce` | 服务端为此次握手生成的随机 128 位值，32 位小写十六进制 |
| `server_id` | 本机 AgentMate 安装标识，32 位小写十六进制，跨 daemon 重启保持不变 |

**响应**：`+HELLO:<version>,agentmate,<server_nonce>,<device_id>,<device_nonce>,<model>,<firmware>,<max_sessions>,<max_usage_entries>,<proof>`，随后 `OK`。设备回显 `server_nonce`，另生成新的 `device_nonce`。`model` 与 `firmware` 是不含逗号的短标识。`max_sessions` 为设备的**总会话槽数**，取值 `1–16`；`max_usage_entries` 为所有 provider 合计可存储的用量窗口槽数，可以是 `0`。服务端按这两个实际容量决定下发数量。

`proof` 是 64 位小写十六进制 HMAC：`HMAC-SHA256(device_key, "agentmate-v1/device/" || server_nonce || device_nonce || device_id)`。拼接的是各字段的 ASCII 原文。**只有请求中的 `server_id` 与设备保存的配对 ID 相同**，设备才用对应密钥生成 proof；尚未配对或请求来自另一台电脑时返回 `-`。`-` 表示“尚未与本服务端认证”，不表示已经证明设备身份；服务端可据此进入四位码配对。nonce 回显、协议标识、版本或字段格式错误时，服务端关闭该候选串口。设备遇到不支持的主版本返回 `UNSUPPORTED_VERSION`。

```text
AT+HELLO=1,fe6d7e13d354eb1f8d45968587589c65,1337a242cd1845299b397e8222293984
+HELLO:1,agentmate,fe6d7e13d354eb1f8d45968587589c65,8d2b47e0a99347c7a976d386dc194fa2,d8c4b78b9c63195dd337cbcb3f88b914,agentmate_esp32c3,fw1.0,4,8,<proof>
OK
```

示例中的 `<proof>` 是占位符，实际线路上必须替换为 64 位十六进制值或未与本服务端配对时的 `-`。

## 4. `AT+PAIRBEGIN`：开启四位码配对

**格式**：`AT+PAIRBEGIN`。只有在本串口已完成有效 `HELLO`、尚未通过 `AUTH` 时可调用。设备使用加密随机数生成 32 位十六进制 `pair_id` 与均匀分布的四位十进制 `pin`（`0000–9999`），屏幕同时显示 `pin`、设备 ID 后八位和剩余时间。设备回复 `+PAIR:<pair_id>,<remaining_s>`、`OK`，**绝不通过串口或 Web API 返回 pin**。窗口从生成时起持续 120 秒，按单调时钟计时；窗口内重复 `PAIRBEGIN` 返回同一 `pair_id` 和剩余秒数，不更换屏幕数字。

设备允许旧配对存在时开启新配对窗口；只有新配对真正成功才替换旧密钥。窗口超时后隐藏 pin；服务端不会自动反复开启窗口，用户可在本机网页中请求重试。连续三次错误输入立即关闭窗口，并从第三次失败起冷却 300 秒；冷却期间 `PAIRBEGIN` 返回 `PAIR_LOCKED`。USB 断开或新 `HELLO` 使尚未完成的窗口失效，不改变旧配对。

```text
AT+PAIRBEGIN
+PAIR:6ac9d42b7b984ce7a33a328f0df13b51,120
OK
```

## 5. `AT+PAIRCOMMIT`：校验四位码并保存配对

**格式**：`AT+PAIRCOMMIT=<pair_id>,<pin>,<key_hex>`。`pair_id` 必须是当前窗口的 ID；`pin` 必须是恰好四个 ASCII 数字，包含可能的前导零；`key_hex` 是服务端用加密随机数生成的 256 位长期密钥，写成 64 个小写十六进制字符。设备常量时间比较 pin，并把当前 `HELLO` 的 `server_id` 与新密钥作为一个完整配对记录写入 NVS。保存成功才返回 `OK`、清除 pin 和旧配对；保存失败返回 `STORAGE_ERROR`，旧记录仍有效。错误 pin 返回 `PAIR_MISMATCH`，过期窗口返回 `PAIR_EXPIRED`，第三次错误及冷却期间返回 `PAIR_LOCKED`，错误 pair ID 返回 `BAD_STATE`。

服务端在发送命令**之前**以仅本机用户可读的权限持久保存待确认密钥；收到 `OK` 后重新执行 `HELLO → AUTH`，验证新密钥后才标记为已配对并同步业务数据。若 `OK` 丢失或服务端在提交后退出，重启时先尝试以待确认密钥完成 HMAC 认证；失败再重新打开配对窗口。设备不会在后续认证中显示 pin。四位码只确认用户正在看设备，不作为长期密钥；首次密钥经本机 USB 串口传输。

```text
AT+PAIRCOMMIT=6ac9d42b7b984ce7a33a328f0df13b51,0042,<64位key_hex>
OK
```

示例中的 `<64位key_hex>` 是占位符，实际线路上必须是完整的 64 位小写十六进制密钥。

## 6. `AT+AUTH`：服务端认证

**格式**：`AT+AUTH=<host_proof>`。`host_proof` 是 64 位小写十六进制值：`HMAC-SHA256(device_key, "agentmate-v1/host/" || server_nonce || device_nonce || server_id)`。设备校验通过后返回 `OK`，并允许后续状态和配置命令。没有先成功执行 `HELLO`、`server_id` 不是当前配对对象、proof 不匹配或 nonce 已失效时返回 `UNAUTHORIZED`。完成 `PAIRCOMMIT` 后须重新 `HELLO`，不能沿用配对前的挑战值。

在显式启用的未配对开发调试模式下，可使用 `AT+AUTH=-` 进入调试会话；正式模式必须拒绝 `-`。每次重新连接都要重新握手，不能复用上次的 proof。

```text
AT+AUTH=<host_proof>
OK
```

## 7. `AT+SYNCBEGIN`：开始会话同步

**格式**：`AT+SYNCBEGIN=<epoch>,<sync_id>`。`sync_id` 是本次同步的正整数编号，与 `SYNCEND` 完全一致。设备返回 `OK` 后进入同步过程。

新 epoch 开始时，设备清除旧 epoch 的会话和所有 provider 用量。同一 epoch 重同步时，设备先将已有会话标记为“本轮未看到”；每条后续 `SESSION` 会更新该标记。若设备仍在另一场未结束的同步中，返回 `BAD_STATE`。本命令本身不删除同一 epoch 中未出现的旧会话。

```text
AT+SYNCBEGIN=629d8a7097154fb0a3e5d74ea00665c1,1
OK
```

## 8. `AT+SESSION`：新增或更新一条会话

**格式**：`AT+SESSION=<epoch>,<key>,<rev>,<provider>,<state>,<last_task>,<finished_at>,<task_seconds>,<ctx_used>,<ctx_window>,<ctx_pct_bp>,<ctx_updated_at>`。

| 参数 | 格式和含义 |
|---|---|
| `epoch` | 当前服务端 epoch，必须与设备当前 epoch 相同 |
| `key` | 1～8 位会话短键；服务端启动期间指向同一原始会话 |
| `rev` | 该会话在当前 epoch 内递增的正整数版本 |
| `provider` | `codex` 或 `claude` |
| `state` | `initializing`、`ready`、`working`、`waiting_for_input`、`permission_required`、`stale`、`ended` 或 `offline` |
| `last_task` | `completed`、`failed`、`cancelled` 或 `-` |
| `finished_at` | 最近任务结束的 Unix 秒；`last_task=-` 时为 `0` |
| `task_seconds` | 当次任务到本命令生成时已持续的秒数，范围 `0–4294967295`；没有当次任务时为 `0` |
| `ctx_used` | Claude 已使用 token 数；见下文的空值规则 |
| `ctx_window` | Claude 窗口 token 数；已知读数时大于 `0` |
| `ctx_pct_bp` | Claude context 占用百分数 × 100，范围 `0–10000`；例如 `600` 是 6.00% |
| `ctx_updated_at` | Claude context 读数的 Unix 秒 |

`task_seconds` 只描述当次任务，不表示会话从创建至今的寿命。进行中的 `working`、`waiting_for_input`、`permission_required` 填从该次任务开始累计的秒数；设备用单调时钟在这个基准上继续累加，服务端不必按秒重发。`stale` 填进入该状态前的累计秒数，设备不再累加。`last_task` 为 `completed`、`failed` 或 `cancelled` 时，填该次任务的总秒数，并保持到下一次任务替换它。状态、任务结果或 context 变化而发送本命令时，带上当时的秒数。时长的自然增长不是新的会话变化。

Claude 有读数时，四个 context 字段均填数字；当前读数未知时，四个字段都写 `?`。**Codex 会话的四个字段全部留空**，表示该设备会话协议不支持 Codex context，而非数值为零。例如末尾的 `0,15,,,,` 中，`0` 是 `finished_at`，`15` 是 `task_seconds`，后面是四个空字段。字段不能混用空值、`?` 和数字。

设备以 `(epoch,key,rev)` 幂等处理重试：相同版本返回 `OK` 且不重复产生变化，也不重置 `task_seconds` 的本地累加基准；较小版本返回 `STALE_REV`。服务端若要校正时长，必须递增 `rev`。新 key 占用一个会话槽，容量已满时返回 `CAPACITY`；服务端应先 `DELETE` 不再入选的会话。同步期间成功写入的 key 同时标记为“本轮已看到”。进程退出后服务端使用 `DELETE` 移除设备会话，服务端内存中的 `ended` 历史可继续保留。

```text
AT+SESSION=629d8a7097154fb0a3e5d74ea00665c1,s1,7,claude,working,-,0,42,12000,200000,600,1790769600
OK
AT+SESSION=629d8a7097154fb0a3e5d74ea00665c1,s2,3,codex,permission_required,-,0,15,,,,
OK
```

## 9. `AT+DELETE`：删除一条会话

**格式**：`AT+DELETE=<epoch>,<key>`。成功后设备移除该 key 并返回 `OK`；key 本来不存在也返回 `OK`，以便安全重试。epoch 不匹配时返回 `BAD_STATE`。设备容量满时，服务端先删除不再显示的 key，再新增入选会话。

```text
AT+DELETE=629d8a7097154fb0a3e5d74ea00665c1,s2
OK
```

## 10. `AT+SYNCEND`：完成会话同步

**格式**：`AT+SYNCEND=<epoch>,<sync_id>`。epoch、sync ID 必须与最近成功的 `SYNCBEGIN` 相同；设备删除本轮未见到的旧 key，结束同步并返回 `OK`。同一轮的 END 重试仍返回 `OK`。没有对应 BEGIN 时返回 `BAD_STATE`。

如果同步途中断线，不执行 END；重新连接后从 `HELLO` 开始，再做一次完整同步。这个过程只要求一个固定会话表及每条一位“已看到”标记。

```text
AT+SYNCEND=629d8a7097154fb0a3e5d74ea00665c1,1
OK
```

## 11. `AT+LIST?`：查询会话清单

**格式**：`AT+LIST?`。先返回 `+LIST:<epoch>,<count>,<max_sessions>`，再为每条已存会话返回 `+ITEM:<key>,<rev>`，最后返回 `OK`。`count` 是**设备当前条目数**，不是服务端全部会话数；应等于 `+ITEM` 行数，且不大于 `max_sessions`。设备尚未接收 epoch 时以 `-` 表示 epoch，`count=0`。

服务端首次同步前可查询旧 key，在同一 epoch 且容量已满时先删除不再入选的条目；同步后查询并核对 key/rev。运行中每 60 秒可以查询一次，发现缺失、多余或版本不一致时只修复差异。

```text
AT+LIST?
+LIST:629d8a7097154fb0a3e5d74ea00665c1,2,4
+ITEM:s1,7
+ITEM:s2,3
OK
```

## 12. `AT+GET`：查询一条会话

**格式**：`AT+GET=<key>`。设备返回 `+SESSION:<epoch>,<key>,<rev>,<provider>,<state>,<last_task>,<finished_at>,<task_seconds>,<ctx_used>,<ctx_window>,<ctx_pct_bp>,<ctx_updated_at>`，然后 `OK`；字段顺序、空值语义与 `AT+SESSION` 完全相同。`task_seconds` 返回最近一次成功写入的基准秒数，不包含设备为显示而在本地累加的时间。key 不存在时返回 `NOT_FOUND`。此命令供内容核对和人工诊断，不需频繁轮询。

```text
AT+GET=s1
+SESSION:629d8a7097154fb0a3e5d74ea00665c1,s1,7,claude,working,-,0,42,12000,200000,600,1790769600
OK
```

## 13. `AT+USAGE`：同步 provider 用量

**格式**：`AT+USAGE=<epoch>,<provider>,<metric_key>,<window_key>,<used_bp>,<reset_at>,<window_seconds>,<updated_at>`。每条命令更新一项 provider 指标的一个时间窗口。

| 参数 | 格式和含义 |
|---|---|
| `epoch` | 当前服务端 epoch |
| `provider` | 用量来源，例如 `codex`、`claude`；最多 16 位小写字母或数字 |
| `metric_key` | 指标或限额短键，1～8 位；同一 provider 内标识一项用量 |
| `window_key` | 时间窗口短键，1～8 位；常用值为 `primary`、`secondary` |
| `used_bp` | 已用比例的基点数，范围 `0–10000`；窗口缺失时填 `-` |
| `reset_at` | 窗口重置的 Unix 秒；窗口缺失时填 `0` |
| `window_seconds` | 窗口长度秒数；缺失或未知时填 `0` |
| `updated_at` | 服务端读取该用量的 Unix 秒 |

`used_bp=4200` 表示已用 42.00%。当前 Codex 配额可按每个 limit ID 分配一个 `metric_key`，再将 `primary` 和 `secondary` 两个窗口分别发送。每条记录由 `(provider, metric_key, window_key)` 唯一标识。设备总记录数不能超过 `HELLO` 报告的 `max_usage_entries`；超出时返回 `CAPACITY`。服务端用 `USAGECLEAR=<epoch>,<provider>` 清理一个 provider 的旧记录，再发送该 provider 当前完整记录集。采集失败或 `available=false` 时只清除该 provider 的记录。当前服务端可以先将 Codex 配额映射到此格式；其他 provider 有可用用量来源后复用相同命令。Session context 与此用量表相互独立。

```text
AT+USAGE=629d8a7097154fb0a3e5d74ea00665c1,codex,l1,primary,4200,1790773200,18000,1790769600
OK
AT+USAGE=629d8a7097154fb0a3e5d74ea00665c1,codex,l1,secondary,1800,1790859600,604800,1790769600
OK
```

## 14. `AT+USAGECLEAR`：清除 provider 用量

**格式**：`AT+USAGECLEAR=<epoch>,<provider>`。设备清空指定 provider 的全部用量窗口记录并返回 `OK`；原来为空时也返回 `OK`。首次同步、该 provider 的采集失败、`available=false` 或准备下发该 provider 的新记录集时使用。它不改变会话表、其他 provider 的用量或亮度配置。

```text
AT+USAGECLEAR=629d8a7097154fb0a3e5d74ea00665c1,codex
OK
```

## 15. `AT+EVENT`：即时提醒

**格式**：`AT+EVENT=<epoch>,<event_id>,<key>,<provider>,<kind>,<occurred_at>`。`kind` 为 `permission_required`、`completed`、`failed` 或 `cancelled`；`occurred_at` 是 Unix 秒。设备缓存最近至少 32 个 event ID；重复发送同一 event ID 仍返回 `OK`，但只处理一次。

事件只负责一次性提醒。会话当前状态以 `SESSION` / `DELETE` 为准；断线重连时服务端不会重放历史事件，也不会因重同步再次制造提醒。

```text
AT+EVENT=629d8a7097154fb0a3e5d74ea00665c1,9c7775eaf6254acaa9ef9473510c3da2,s2,codex,permission_required,1790769600
OK
```

## 16. `AT+PING`：连接保活

**格式**：`AT+PING=<nonce>`。心跳 nonce 为 1～32 位小写十六进制字符；设备回复 `+PONG:<nonce>,<uptime_ms>`、`OK`。必须原样回显 nonce，`uptime_ms` 是设备启动后的非负毫秒数。服务端每 10 秒发一次，单次等待 5 秒；连续 3 次失败或串口读写失败时，将该设备标记离线并重连。心跳与会话 `stale` 状态相互独立。

```text
AT+PING=74a8
+PONG:74a8,123456
OK
```

## 17. `AT+TIME`：设置或查询设备时间

服务端是 UTC 时间源。设备重启后必须重新校时；串口链路建立并完成认证后，服务端立即发送一次 `TIME`，随后每 30 分钟校准一次。session 和用量中的时间戳仍使用 Unix 秒；本命令使用 Unix 毫秒，以减少同步误差。

**设置格式**：`AT+TIME=<unix_ms>`。参数为自 `1970-01-01T00:00:00Z` 起的 UTC 毫秒数，正整数。设备设置系统 UTC 时钟后，返回 `+TIME:<device_unix_ms>,<uptime_ms>`、`OK`。`device_unix_ms` 是设备生成响应时读取的当前时间；`uptime_ms` 是设备启动后的单调运行时间。设备设置失败返回 `TIME_ERROR`。

服务端在发送前、接收响应时分别记录本机 UTC 时间和单调时间，用请求/响应时间中点估算设备偏差。偏差绝对值超过 100 毫秒时重新发送校准命令。正常串口传输下二者在校准时刻应相差不超过该阈值；之后会因设备晶振误差逐渐产生偏差，因此定期校准。时间同步统一系统 UTC，不调整用户时区。

**查询格式**：`AT+TIME?`。设备返回 `+TIME:<device_unix_ms>,<uptime_ms>,<sync_age_ms>`、`OK`。`sync_age_ms` 是距最近一次成功校准的单调时间；设备尚未校时则返回 `+TIME:0,<uptime_ms>,-`。查询值供人工诊断，不改变设备时钟。

```text
AT+TIME=1790769600123
+TIME:1790769600125,123456
OK
AT+TIME?
+TIME:1790769601125,124456,1000
OK
```

## 18. `AT+INFO?`：设备信息

**格式**：`AT+INFO?`。回复 `+INFO:<device_id>,<model>,<firmware>,<version>,<max_sessions>`、`OK`。该只读命令允许在认证前使用，便于人工确认设备，但不能代替 `HELLO` 的 nonce 检查与 HMAC 身份认证。设备的 `max_usage_entries` 仍以 `HELLO` 的响应为准。

```text
AT+INFO?
+INFO:8d2b47e0a99347c7a976d386dc194fa2,agentmate_esp32c3,fw1.0,1,4
OK
```

## 19. `AT+TFTBRIGHT`：TFT 背光亮度

**设置**：`AT+TFTBRIGHT=<value>`，其中 `value` 为十进制整数 `0–255`。`0` 关闭背光，`255` 为驱动允许的最高占空比。设备在背光控制成功且配置保存到 NVS 后返回 `OK`；非法值返回 `INVALID_ARG`，NVS 写入失败返回 `STORAGE_ERROR`。配置跨设备重启保留；没有已保存值时使用当前工程的默认亮度 `192`。

**查询**：`AT+TFTBRIGHT?` 返回 `+TFTBRIGHT:<value>`、`OK`，数值为当前生效亮度。若屏幕背光硬连供电、固件没有可控背光 GPIO，设置和查询均返回 `UNSUPPORTED_FEATURE`。

```text
AT+TFTBRIGHT=128
OK
AT+TFTBRIGHT?
+TFTBRIGHT:128
OK
```

## 20. `AT+LEDBRIGHT`：LED 灯环亮度

**设置**：`AT+LEDBRIGHT=<value>`，其中 `value` 为十进制整数 `0–255`。它是整个 LED 灯环的全局亮度系数：`0` 熄灭，`255` 保持原始颜色强度。设备应用该系数、刷新当前灯环帧并保存到 NVS 后返回 `OK`；非法值返回 `INVALID_ARG`，保存失败返回 `STORAGE_ERROR`。配置跨设备重启保留；没有已保存值时使用当前驱动默认亮度 `64`。

**查询**：`AT+LEDBRIGHT?` 返回 `+LEDBRIGHT:<value>`、`OK`。16 灯全白且亮度 255 时电流可能较高；USB 供电的原型建议保留默认值或根据实际电源能力设置。

```text
AT+LEDBRIGHT=64
OK
AT+LEDBRIGHT?
+LEDBRIGHT:64
OK
```

## 21. AT+TFTROTATE：TFT 界面旋转

设置格式：`AT+TFTROTATE=<angle>`，角度只能是 0、90、180 或 270。设备旋转整个界面的绘制方向，保存到 NVS 后返回 `OK`。设置跨重启保留，默认 0；非法角度返回 `INVALID_ARG`，屏幕驱动失败返回 `INTERNAL_ERROR`，保存失败返回 `STORAGE_ERROR`。改变角度后界面完整重绘。此命令需要认证。

查询格式：`AT+TFTROTATE?` 返回 `+TFTROTATE:<angle>`、`OK`，其中角度是当前生效值。此命令需要认证。

~~~text
AT+TFTROTATE=90
OK
AT+TFTROTATE?
+TFTROTATE:90
OK
~~~

## 22. 错误响应与典型同步顺序

错误格式固定为两行：

```text
+ERR:CAPACITY
ERROR
```

| 错误码 | 含义 |
|---|---|
| `INVALID_ARG` | 参数个数、字符、格式、范围或字段组合不正确 |
| `UNKNOWN_COMMAND` | 不认识的命令 |
| `UNSUPPORTED_VERSION` | 不支持所请求的协议主版本 |
| `UNAUTHORIZED` | 尚未认证、认证失败或调试模式未开启 |
| `CAPACITY` | 新会话或 provider 用量记录超出设备报告的容量 |
| `NOT_FOUND` | `GET` 指定的 key 不存在 |
| `BAD_STATE` | epoch、同步阶段或命令执行顺序不正确 |
| `STALE_REV` | 会话版本小于设备已保存版本 |
| `LINE_TOO_LONG` | 收到超过 256 字节的行 |
| `UNSUPPORTED_FEATURE` | 当前硬件或固件不具备请求的功能，例如不可调 TFT 背光 |
| `STORAGE_ERROR` | 配对记录或亮度配置写入 NVS 失败 |
| `INTERNAL_ERROR` | 硬件驱动或其他内部操作失败 |
| `TIME_ERROR` | 系统时钟设置或读取失败 |
| `PAIR_MISMATCH` | 四位码错误，且配对窗口还有剩余尝试次数 |
| `PAIR_EXPIRED` | 四位码配对窗口已过期 |
| `PAIR_LOCKED` | 错误次数达到上限，配对进入冷却期 |

已配对设备：`HELLO → AUTH → TIME → LIST? → SYNCBEGIN → 每条会话一条 SESSION → SYNCEND → LIST? → 对每个 provider 执行 USAGECLEAR → 逐条发送该 provider 的 USAGE`。未与本服务端配对时：`HELLO → PAIRBEGIN → 用户在本机网页输入 pin → PAIRCOMMIT → 重新 HELLO → AUTH → 上述同步流程`。如果 LIST 发现同一 epoch 的旧表已占满，先用 `DELETE` 腾出槽位。之后只同步变化的 session；某 provider 用量变化时，只清除并重发该 provider 的记录。时间每 30 分钟重校一次。
