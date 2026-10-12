> 协议 2 更新：SESSION 和 GET 新增主机标识与 Session 短名；设备页标题为 LOCAL·S1 / WSL·S1（中间为圆点），provider 使用图标。必须配套升级主机服务；不接受旧版 13 字段 SESSION。

# AgentMate AT 指令指南

文档版本：**1.7**
协议主版本：**1**  
状态：ESP32 固件及电脑端服务已实现
适用连接：ESP32-C3 UART0 经 CH340 USB-UART 桥，`115200 8N1`

## 1. 命令与模式

正常模式通过 `AT+HELLO=2,<nonce>` 发现设备后直接同步业务数据；一期不进行配对和身份认证。产测模式只接受发现、信息、模式、回显、日志和产测命令。`PAIRBEGIN`、`PAIRCOMMIT`、`AUTH` 在本版返回 `UNSUPPORTED_FEATURE`，配对认证规划为二期。

| 命令 | 功能 | 主要响应 | 产测 | 正常 |
|---|---|---|:---:|:---:|
| `AT+HELLO=2,<nonce>` | 发现设备、查询容量和当前模式 | `+HELLO:...`、`OK` | ✅ | ✅ |
| `AT+INFO?` | 查询设备信息及会话容量 | `+INFO:...`、`OK` | ✅ | ✅ |
| `AT+MODE?` | 查询当前模式 | `+MODE:<FACTORY或NORMAL>`、`OK` | ✅ | ✅ |
| `AT+MODE=FACTORY` / `AT+MODE=NORMAL` | 保存模式并重启 | `OK` | ✅ | ✅ |
| `AT+ECHO?` / `AT+ECHO=<0或1>` | 查询或控制输入回显 | `+ECHO:<0或1>`、`OK`；设置返回 `OK` | ✅ | ✅ |
| `AT+LOG?` / `AT+LOG=<0或1>` | 查询或控制应用日志 | `+LOG:<0或1>`、`OK`；设置返回 `OK` | ✅ | ✅ |
| `AT+TESTLED=<index或ALL>,<r>,<g>,<b>,<brightness>` | 指定单颗或全部 LED 的颜色与亮度 | `OK` | ✅ | |
| `AT+TESTTFT=<BLACK或WHITE或RED或GREEN或BLUE或GRAY>` | TFT 纯色显示 | `OK` | ✅ | |
| `AT+TESTBACKLIGHT=<0..255>` | TFT 背光测试 | `OK` | ✅ | |
| `AT+TESTFONT=<8x8或8x16或10x18或12x22或16x32>` | ASCII 字体样张 | `OK` | ✅ | |
| `AT+TESTAUTO=<0或1>` | 停止或启动自动轮播 | `OK` | ✅ | |
| `AT+SYNCBEGIN=<epoch>,<sync_id>` | 开始逐条会话同步 | `OK` | | ✅ |
| `AT+SESSION=<epoch>,<key>,<rev>,<provider>,<state>,<last_task>,<finished_at>,<task_seconds>,<ctx_used>,<ctx_window>,<ctx_pct_bp>,<ctx_updated_at>,<project_name>,<host_label>,<session_short_name>` | 新增或更新一条会话 | `OK` | | ✅ |
| `AT+DELETE=<epoch>,<key>` | 删除一条会话 | `OK` | | ✅ |
| `AT+SYNCEND=<epoch>,<sync_id>` | 结束同步并删除未出现的旧会话 | `OK` | | ✅ |
| `AT+LIST?` | 查询全部会话键及版本 | `+LIST:...`、`+ITEM:...`、`OK` | | ✅ |
| `AT+GET=<key>` | 查询一条会话的完整字段 | `+SESSION:...`、`OK` | | ✅ |
| `AT+USAGE=<epoch>,<provider>,<metric_key>,<window_key>,<used_bp>,<reset_at>,<window_seconds>,<updated_at>,<reset_utc_offset_minutes>` | 同步一条 provider 用量窗口 | `OK` | | ✅ |
| `AT+USAGEBEGIN=<epoch>,<provider>,<sync_id>` / `AT+USAGEEND=<epoch>,<provider>,<sync_id>` | 暂存／原子提交整批 provider 用量 | `OK` | | ✅ |
| `AT+USAGECLEAR=<epoch>,<provider>` | 清除指定 provider 的用量记录 | `OK` | | ✅ |
| `AT+EVENT=<epoch>,<event_id>,<key>,<provider>,<kind>,<occurred_at>` | 发送一次性任务提醒 | `OK` | | ✅ |
| `AT+PING=<nonce>` | 检查设备活动状态 | `+PONG:...`、`OK` | | ✅ |
| `AT+TIME=<unix_ms>,<utc_offset_minutes>` | 按服务端 UTC 时间校准设备时钟 | `+TIME:...`、`OK` | | ✅ |
| `AT+SHOWINFO=<0..60>` / `AT+SHOWINFO?` | 拉起、关闭或查询设备信息页 | `OK` / `+SHOWINFO:...`、`OK` | | ✅ |
| `AT+TIME?` | 查询设备 UTC 时间和校时状态 | `+TIME:...`、`OK` | | ✅ |
| `AT+TFTBRIGHT=<value>` | 设置并保存 TFT 背光亮度 | `OK` | | ✅ |
| `AT+TFTBRIGHT?` | 查询 TFT 背光亮度 | `+TFTBRIGHT:<value>`、`OK` | | ✅ |
| `AT+LEDBRIGHT=<value>` | 设置并保存 LED 灯环亮度 | `OK` | | ✅ |
| `AT+LEDBRIGHT?` | 查询 LED 灯环亮度 | `+LEDBRIGHT:<value>`、`OK` | | ✅ |
| `AT+TFTROTATE=<angle>` | 设置并保存 TFT 界面旋转角度 | `OK` | | ✅ |
| `AT+TFTROTATE?` | 查询 TFT 界面旋转角度 | `+TFTROTATE:<angle>`、`OK` | | ✅ |
| `AT+CLOCKTHEME=<theme>` | 设置并保存待机时间页配色 | `OK` | | ✅ |
| `AT+CLOCKTHEME?` | 查询当前时间配色 | `+CLOCKTHEME:<theme>`、`OK` | | ✅ |

`AT+PAIRBEGIN`、`AT+PAIRCOMMIT=...` 和 `AT+AUTH=...` 属于二期规划；当前固件收到这些命令会返回 `+ERR:UNSUPPORTED_FEATURE`、`ERROR`。

主机标识与 Session 短名扩展使用协议主版本 **2**，不协商旧格式。SESSION、USAGE、TIME 的完整字段必须下发，旧字段数量返回 `INVALID_ARG`。服务端须同步适配。

## 2. 行格式与日志

- 命令与响应均为 ASCII，每行最多 512 字节（包括行尾）。设备接收 CR、LF 或 CRLF，响应以 CRLF 结束；退格和 DEL 删除当前行末字符。打开回显时，设备回显输入字符，回车后先输出 CRLF 再输出响应。空行不执行命令。
- 每条命令最终以独立的 `OK` 或 `ERROR` 行结束；错误前可有 `+ERR:<code>`。服务端同一时刻只发送一条未完成的命令。参数以逗号分隔，保留空字段，不接受额外字段。
- 应用日志开启后以 `+LOG:<seq>,<INFO|ERROR>,agentmate,<message>` 单独上报，位于完整命令响应之后。服务端须将 `+LOG:` 与命令响应分开处理；运行期普通 ESP-IDF 日志和 bootloader 日志已关闭。ROM 在复位瞬间仍可能输出启动字节，服务端打开端口后应等待设备启动并清空旧输入再发送 `HELLO`。固件只记录命令名称和状态，不记录参数原文。
- 产测模式的回显与应用日志默认开启，正常模式默认关闭。`ECHO`、`LOG` 开关只对本次运行有效，重启后恢复模式默认值。模式切换先保存 NVS、回复 `OK`，然后重启。
- 新 NVS 首次启动默认为产测；已有 `device_id` 且无模式标志的设备默认为正常，以兼容固件升级。产测测试亮度不保存，停止自动轮播或切换测试项目后恢复普通亮度。产测完成由工位判定，`AT+MODE=NORMAL` 不记录通过/失败结论。

## 3. 设备发现

`AT+HELLO=2,<nonce>` 中的 nonce 为 32 位小写十六进制字符。响应格式为 `+HELLO:<protocol_version>,<protocol>,<nonce>,<device_id>,<model>,<software_version>,<max_sessions>,<max_usage_entries>,<mode>`，随后 `OK`。服务端检查协议标识、nonce 回显、设备 ID、容量和模式；只有 `NORMAL` 才开始同步。这些字段用于识别协议兼容设备，不提供加密身份保证。`AT+INFO?` 的字段定义见第 18 节。

| 字段 | 格式和含义 |
|---|---|
| `protocol_version` | 协议主版本，一位十进制数字，当前固定为 `2`；其他值返回 `UNSUPPORTED_VERSION` |
| `protocol` | 协议标识，固定为 `agentmate` |
| `nonce` | 原样回显请求中的 32 位小写十六进制字符 |
| `device_id` | 设备 ID，32 位小写十六进制字符；首次启动生成并写入 NVS，之后保持不变 |
| `model` | 设备型号，当前为 `AGM-C3` |
| `software_version` | 软件版本，格式为 `v<主版本>.<次版本>`，取自根目录 `CMakeLists.txt` 的 `project()` `VERSION` 并在编译时写入；当前为 `v2.0` |
| `max_sessions` | 会话槽上限，当前为 `8`；`SESSION` 超出时返回 `CAPACITY` |
| `max_usage_entries` | 用量记录上限，当前为 `4`；`USAGE` 超出时返回 `CAPACITY` |
| `mode` | 当前模式，`FACTORY` 或 `NORMAL` |

```text
AT+HELLO=2,fe6d7e13d354eb1f8d45968587589c65
+HELLO:2,agentmate,fe6d7e13d354eb1f8d45968587589c65,8d2b47e0a99347c7a976d386dc194fa2,AGM-C3,v2.0,8,4,NORMAL
OK
```

## 4. 产测命令

`TESTLED` 的 index 为 `0–15` 或 `ALL`，RGB 与 brightness 均为 `0–255`。`TESTTFT` 显示整屏纯色；`TESTBACKLIGHT` 设置临时背光亮度；`TESTFONT` 按大小绘制可打印 ASCII 样张，多页每两秒切换。每条单项测试命令停止自动轮播。`TESTAUTO=1` 每两秒切换测试阶段，依次覆盖屏幕纯色、灯环整体颜色、逐灯点亮、背光三个档位和五种字体；`TESTAUTO=0` 返回产测首页并恢复持久亮度。产测命令通过校验并更新测试状态后回复 `OK`，实际绘制由 UI 任务随后执行。

## 5. 正常模式业务命令

原有业务命令的参数、响应和状态同步语义见下文。正常模式不以 `HELLO` 或认证作为逐条命令的访问门槛，但服务端应先通过 `HELLO` 发现设备并按返回容量同步。连续 30 秒没有有效命令时，设备清空远端会话和用量并显示等待主机状态；本地亮度配置保留。

## 6. 一期连接顺序

`HELLO → TIME → LIST? → SYNCBEGIN → 每条会话一条 SESSION → SYNCEND → LIST? → USAGECLEAR → USAGE`。若已有会话表满，先使用 `DELETE` 腾出槽位。服务端进程和固件重启后重新发现并完整同步。

可使用 `python tools/test_at_serial.py /dev/cu.usbserial-1420` 执行实机 AT 交互检查。脚本验证行结束、退格、回显、日志、模式及产测命令响应；画面和灯珠仍需目视确认。端口名称以实际系统枚举结果为准。

## 7. `AT+SYNCBEGIN`：开始会话同步

**格式**：`AT+SYNCBEGIN=<epoch>,<sync_id>`。`sync_id` 是本次同步的正整数编号，与 `SYNCEND` 完全一致。设备返回 `OK` 后进入同步过程。

新 epoch 开始时，设备清除旧 epoch 的会话和所有 provider 用量。同一 epoch 重同步时，设备先将已有会话标记为“本轮未看到”；每条后续 `SESSION` 会更新该标记。若设备仍在另一场未结束的同步中，返回 `BAD_STATE`。本命令本身不删除同一 epoch 中未出现的旧会话。

```text
AT+SYNCBEGIN=629d8a7097154fb0a3e5d74ea00665c1,1
OK
```

## 8. `AT+SESSION`：新增或更新一条会话

**格式**：`AT+SESSION=<epoch>,<key>,<rev>,<provider>,<state>,<last_task>,<finished_at>,<task_seconds>,<ctx_used>,<ctx_window>,<ctx_pct_bp>,<ctx_updated_at>,<project_name>,<host_label>,<session_short_name>`。

| 参数 | 格式和含义 |
|---|---|
| `epoch` | 当前服务端 epoch，必须与设备当前 epoch 相同 |
| `key` | 1～8 位会话短键；服务端启动期间指向同一原始会话 |
| `rev` | 该会话在当前 epoch 内递增的正整数版本 |
| `provider` | `codex` 或 `claude` |
| `state` | `initializing`、`ready`、`working`、`thinking`、`waiting_for_input`、`permission_required`、`task_success`、`task_canceled`、`task_failed`、`stale`、`ended` 或 `offline` |
| `last_task` | `completed`、`failed`、`cancelled` 或 `-` |
| `finished_at` | 最近任务结束的 Unix 秒；`last_task=-` 时为 `0` |
| `task_seconds` | 当次任务到本命令生成时已持续的秒数，范围 `0–4294967295`；没有当次任务时为 `0` |
| `ctx_used` | 会话已使用 token 数；见下文的空值规则 |
| `ctx_window` | 会话窗口 token 数；已知读数时大于 `0` |
| `ctx_pct_bp` | 会话 context 占用百分数 × 100，范围 `0–10000`；例如 `600` 是 6.00% |
| `ctx_updated_at` | 会话 context 读数的 Unix 秒 |
| `project_name` | 工程 basename 的百分号编码 UTF-8；解码后最多 64 字节，未知为空字段 |
| `host_label` | LOCAL 或用户配置的 3–8 位大写字母、数字、下划线 |
| `session_short_name` | S 加 1–4294967295 的十进制编号，无前导零；按来源独立分配 |

`task_seconds` 只描述当次任务，不表示会话从创建至今的寿命。进行中的 `working`、`thinking`、`waiting_for_input`、`permission_required` 填从该次任务开始累计的秒数；设备用单调时钟在这个基准上继续累加，服务端不必按秒重发。`stale` 填进入该状态前的累计秒数，设备不再累加。`task_success`、`task_canceled`、`task_failed` 是暂时展示的任务状态，服务端通常在 60 秒后下发 `ready`，新任务或其他有效状态可提前覆盖；设备不自行计时回退。`last_task` 为 `completed`、`failed` 或 `cancelled` 时，填该次任务的总秒数，并保持到下一次任务替换它。状态、任务结果或 context 变化而发送本命令时，带上当时的秒数。时长的自然增长不是新的会话变化。

Claude 和 Codex 有读数时，四个 context 字段均填数字；当前读数未知时，四个字段都写 `?`。Codex 的四字段也可全空，表示不支持上下文，此时设备仅显示配额。字段不能混用空值、`?` 和数字。协议仍为 2，服务端与固件需同时升级到本实现；旧固件不接受 Codex 的数字或未知字段。

设备以 `(epoch,key,rev)` 幂等处理重试：相同版本返回 `OK` 且不重复产生变化，也不重置 `task_seconds` 的本地累加基准；较小版本返回 `STALE_REV`。服务端若要校正时长，必须递增 `rev`。新 key 占用一个会话槽，容量已满时返回 `CAPACITY`；服务端应先 `DELETE` 不再入选的会话。同步期间成功写入的 key 同时标记为“本轮已看到”。进程退出后服务端使用 `DELETE` 移除设备会话，服务端内存中的 `ended` 历史可继续保留。

```text
AT+SESSION=629d8a7097154fb0a3e5d74ea00665c1,s1,7,claude,working,-,0,42,12000,200000,600,1790769600,agentmate_esp32,LOCAL,S1
OK
AT+SESSION=629d8a7097154fb0a3e5d74ea00665c1,s2,3,codex,permission_required,-,0,15,,,,,agentmate_service,LOCAL,S2
OK
```

`project_name` 是目录最后一段，不传全路径。安全字符 `A-Z a-z 0-9 - _ . ~` 可直接发送，其他 UTF-8 字节编码为 `%HH`；例如 `工程,a` 为 `%E5%B7%A5%E7%A8%8B%2Ca`。接受大小写十六进制；空字段表示未知。拒绝非法 UTF-8、控制字符、路径分隔符与超长值。GET 使用同一编码。目录变化必须递增 rev。

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
+LIST:629d8a7097154fb0a3e5d74ea00665c1,2,8
+ITEM:s1,7
+ITEM:s2,3
OK
```

## 12. `AT+GET`：查询一条会话

**格式**：`AT+GET=<key>`。设备返回 `+SESSION:<epoch>,<key>,<rev>,<provider>,<state>,<last_task>,<finished_at>,<task_seconds>,<ctx_used>,<ctx_window>,<ctx_pct_bp>,<ctx_updated_at>,<project_name>,<host_label>,<session_short_name>`，然后 `OK`；字段顺序、空值语义与 `AT+SESSION` 完全相同。`task_seconds` 返回最近一次成功写入的基准秒数，不包含设备为显示而在本地累加的时间。key 不存在时返回 `NOT_FOUND`。此命令供内容核对和人工诊断，不需频繁轮询。

```text
AT+GET=s1
+SESSION:629d8a7097154fb0a3e5d74ea00665c1,s1,7,claude,working,-,0,42,12000,200000,600,1790769600,agentmate_esp32,LOCAL,S1
OK
```

## 13. `AT+USAGE`：同步 provider 用量

**格式**：`AT+USAGE=<epoch>,<provider>,<metric_key>,<window_key>,<used_bp>,<reset_at>,<window_seconds>,<updated_at>,<reset_utc_offset_minutes>`。每条命令更新一项 provider 指标的一个时间窗口。

| 参数 | 格式和含义 |
|---|---|
| `epoch` | 当前服务端 epoch |
| `provider` | 用量来源，例如 `codex`、`claude`；最多 16 位小写字母或数字 |
| `metric_key` | 指标或限额短键，1～8 位；同一 provider 内标识一项用量 |
| `window_key` | 时间窗口短键，1～16 位；常用值为 `primary`、`secondary` |
| `used_bp` | 已用比例的基点数，范围 `0–10000`；读数未知时填 `-` |
| `reset_at` | 窗口重置的 Unix 秒；窗口缺失时填 `0` |
| `window_seconds` | 窗口长度秒数；缺失或未知时填 `0` |
| `updated_at` | 服务端读取该用量的 Unix 秒 |
| `reset_utc_offset_minutes` | 重置时刻在主机时区的 UTC 偏移，`-840～+840` 分钟；未知为 `-` |

`used_bp=4200` 表示已用 42.00%。当前 Codex 配额可按每个 limit ID 分配一个 `metric_key`，再将 `primary` 和 `secondary` 两个窗口分别发送。每条记录由 `(provider, metric_key, window_key)` 唯一标识。设备总记录数不能超过 `HELLO` 报告的 `max_usage_entries`；超出时返回 `CAPACITY`。服务端用 `USAGEBEGIN`、完整记录集、`USAGEEND` 原子替换一个 provider 的用量；刷新过程中继续显示上一批，不先 USAGECLEAR。采集失败或 `available=false` 时只清除该 provider 的记录。当前服务端可以先将 Codex 配额映射到此格式；其他 provider 有可用用量来源后复用相同命令。Session context 与此用量表相互独立。

```text
AT+USAGE=629d8a7097154fb0a3e5d74ea00665c1,codex,l1,primary,4200,1790773200,18000,1790769600,480
OK
AT+USAGE=629d8a7097154fb0a3e5d74ea00665c1,codex,l1,secondary,1800,1790859600,604800,1790769600,480
OK
```

未知百分比不代表窗口消失：若窗口时长已知，即使 `used_bp=-` 仍保留 `window_seconds` 和可获得的 `reset_at`／偏移。设备只展示 Codex 的 18000／604800 秒窗口，同一 metric 的两档配对；按 metric 字典序选择第一组，同档多条按 window_key 字典序选择第一条。显示剩余值 `10000-used_bp`，一位小数，未知显示「用量未知」。成功发布任一 Codex 目标窗口后，本次连接支持待机用量显示，无任务或结果时与日期时间每 10 秒轮播；不依赖 Session 或最近活动。

### 用量批次：USAGEBEGIN／USAGEEND

格式均为 `<epoch>,<provider>,<sync_id>`。epoch 必须匹配当前同步 epoch；provider 沿用 USAGE 校验；sync_id 为非零 uint64，在一次握手期间单调递增，且不与 Session 的同步序号混用。正常模式返回 OK；产测不支持。

BEGIN 建立最多 4 条总记录的暂存表：复制其他 provider，清除暂存表中的目标 provider。后续 USAGE 只改暂存表，重复记录按键覆盖。END 与活跃批次匹配后，在 model_lock 内一次替换用量表，未在本批发送的目标 provider 窗口自动移除；其他 provider 保留。无批次时 USAGE 仍可直接更新，供诊断使用。

活跃批次的重复 BEGIN 为幂等，不清空已暂存数据或续期；未处于其他活跃或失败批次时，最近已提交批次的重复 END 为幂等，不重写模型。过期或不匹配的序号返回 BAD_STATE。USAGE 格式错误、容量错误或 provider 不匹配会使整批失败，END 不发布部分数据；失败后必须新 BEGIN、USAGECLEAR 或 HELLO 才能继续写入。

批次从 BEGIN 起 30 秒超时，丢弃暂存内容并拒绝迟到的 USAGE／END，保留旧用量。新 HELLO、epoch 重建、断线及 USAGECLEAR 会丢弃未提交批次。服务端只有 END 成功后才记录同步指纹。

```text
AT+USAGEBEGIN=629d8a7097154fb0a3e5d74ea00665c1,codex,1
OK
AT+USAGE=629d8a7097154fb0a3e5d74ea00665c1,codex,l1,primary,4200,1790773200,18000,1790769600,480
OK
AT+USAGEEND=629d8a7097154fb0a3e5d74ea00665c1,codex,1
OK
```

## 14. `AT+USAGECLEAR`：清除 provider 用量

**格式**：`AT+USAGECLEAR=<epoch>,<provider>`。设备清空指定 provider 的全部用量窗口记录并返回 `OK`；原来为空时也返回 `OK`。该 provider 的采集失败、`available=false` 或确实没有可发送记录时使用；正常刷新使用批次替换。它会丢弃当前未提交批次。它不改变会话表、其他 provider 的用量或亮度配置。本次连接已支持用量时保留支持标记，清除后继续轮播未知用量与日期时间；新 HELLO、新 epoch、断线或重启重置标记。

```text
AT+USAGECLEAR=629d8a7097154fb0a3e5d74ea00665c1,codex
OK
```

## 15. `AT+EVENT`：即时提醒

**格式**：`AT+EVENT=<epoch>,<event_id>,<key>,<provider>,<kind>,<occurred_at>`。`kind` 为 `permission_required`、`completed`、`failed` 或 `cancelled`；`occurred_at` 是 Unix 秒。设备缓存最近至少 32 个 event ID；重复发送同一 event ID 仍返回 `OK`，但只处理一次。

事件保留去重；指向当前最高组有效会话时可关闭信息页。事件不抢切同组轮播、不延长 10 秒驻留、不叠加独立 LED 闪光。会话当前状态以 `SESSION` / `DELETE` 为准；断线重连时服务端不会重放历史事件，也不会因重同步再次制造提醒。

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

服务端是 UTC 时间源。设备重启后必须重新校时；串口链路建立后，服务端立即发送一次 `TIME`，随后每 30 分钟校准一次。session 和用量中的时间戳仍使用 Unix 秒；本命令使用 Unix 毫秒，以减少同步误差。

**设置格式**：`AT+TIME=<unix_ms>,<utc_offset_minutes>`。参数为自 `1970-01-01T00:00:00Z` 起的 UTC 毫秒数，正整数。偏移为 `-840～+840` 的有符号分钟数，`0` 有效。设备设置系统 UTC 时钟及当前偏移后，返回 `+TIME:<device_unix_ms>,<uptime_ms>,<utc_offset_minutes>`、`OK`。`device_unix_ms` 是设备执行校时时采用的 UTC 时间；`uptime_ms` 是设备启动后的单调运行时间。设备设置失败返回 `TIME_ERROR`。

服务端在发送前、接收响应时分别记录本机 UTC 时间和单调时间，用请求/响应时间中点估算设备偏差。偏差绝对值超过 100 毫秒时重新发送校准命令。正常串口传输下二者在校准时刻应相差不超过该阈值；之后会因设备晶振误差逐渐产生偏差，因此定期校准。时间同步统一系统 UTC，不调整用户时区。

**查询格式**：`AT+TIME?`。设备返回 `+TIME:<device_unix_ms>,<uptime_ms>,<sync_age_ms>,<utc_offset_minutes>`、`OK`。`sync_age_ms` 是距最近一次成功校准的单调时间；设备尚未校时则返回 `+TIME:0,<uptime_ms>,-,-`。查询值供人工诊断，不改变设备时钟。

```text
AT+TIME=1790769600123,480
+TIME:1790769600125,123456,480
OK
AT+TIME?
+TIME:1790769601125,124456,1000,480
OK
```

## 18. `AT+INFO?`：设备信息

**格式**：`AT+INFO?`。回复 `+INFO:<device_id>,<model>,<software_version>,<protocol_version>,<max_sessions>`、`OK`。该只读命令可在两种模式使用；服务端仍应使用 `HELLO` 检查 nonce 回显和模式。设备的 `max_usage_entries` 仍以 `HELLO` 的响应为准。

| 字段 | 格式和含义 |
|---|---|
| `device_id` | 设备 ID，32 位小写十六进制字符；与 `HELLO` 返回的同一值 |
| `model` | 设备型号，当前为 `AGM-C3` |
| `software_version` | 软件版本，格式为 `v<主版本>.<次版本>`，与 `HELLO` 返回的同一值；当前为 `v2.0` |
| `protocol_version` | 协议主版本，一位十进制数字，当前固定为 `2` |
| `max_sessions` | 会话槽上限，当前为 `8` |

```text
AT+INFO?
+INFO:8d2b47e0a99347c7a976d386dc194fa2,AGM-C3,v2.0,2,8
OK
```

## 19. `AT+TFTBRIGHT`：TFT 背光亮度

**设置**：`AT+TFTBRIGHT=<value>`，其中 `value` 为十进制整数百分比 `0–100`。`0` 关闭背光，`100` 为驱动允许的最高占空比。设备在背光控制成功且配置保存到 NVS 后返回 `OK`；非法值返回 `INVALID_ARG`，NVS 写入失败返回 `STORAGE_ERROR`。配置跨设备重启保留；没有已保存值时使用默认亮度 **75%**（驱动值 192）。

**查询**：`AT+TFTBRIGHT?` 返回 `+TFTBRIGHT:<value>`、`OK`，数值为当前生效亮度百分比，范围 0–100。若屏幕背光硬连供电、固件没有可控背光 GPIO，设置和查询均返回 `UNSUPPORTED_FEATURE`。

```text
AT+TFTBRIGHT=50
OK
AT+TFTBRIGHT?
+TFTBRIGHT:50
OK
```

## 20. `AT+LEDBRIGHT`：LED 灯环亮度

**设置**：`AT+LEDBRIGHT=<value>`，其中 `value` 为十进制整数百分比 `0–100`。它是整个 LED 灯环的全局亮度系数：`0` 熄灭，`100` 保持原始颜色强度。设备应用该系数、刷新当前灯环帧并保存到 NVS 后返回 `OK`；非法值返回 `INVALID_ARG`，保存失败返回 `STORAGE_ERROR`。配置跨设备重启保留；没有已保存值时使用默认亮度 **25%**（驱动值 64）。

**查询**：`AT+LEDBRIGHT?` 返回 `+LEDBRIGHT:<value>`、`OK`。返回值为 0–100 的整数百分比。16 灯全白且亮度 100% 时电流可能较高；USB 供电的原型建议保留默认值或根据实际电源能力设置。

```text
AT+LEDBRIGHT=25
OK
AT+LEDBRIGHT?
+LEDBRIGHT:25
OK
```

常规亮度配置在服务端 API、面板、AT 设置／查询及设备信息页中统一为整数百分比。驱动与 NVS 保留原来的 0–255 值，升级不会重新解释已保存的驱动值或改变实际亮度。设置换算为 `(percent * 255 + 50) / 100`，查询／显示换算为 `(raw * 100 + 127) / 255`；所有 0～100 设置可精确回读对应百分比。产测 TESTLED／TESTBACKLIGHT 使用的临时驱动值仍为 0–255。

## 21. AT+TFTROTATE：TFT 界面旋转

设置格式：`AT+TFTROTATE=<angle>`，角度只能是 0、90、180 或 270。设备旋转整个界面的绘制方向，保存到 NVS 后返回 `OK`。设置跨重启保留，默认 0；非法角度返回 `INVALID_ARG`，屏幕驱动失败返回 `INTERNAL_ERROR`，保存失败返回 `STORAGE_ERROR`。改变角度后界面完整重绘。此命令仅在正常模式可用。

查询格式：`AT+TFTROTATE?` 返回 `+TFTROTATE:<angle>`、`OK`，其中角度是当前生效值。此命令仅在正常模式可用。

~~~text
AT+TFTROTATE=90
OK
AT+TFTROTATE?
+TFTROTATE:90
OK
~~~

## 22. 错误响应与典型同步顺序

| 错误码 | 含义 |
|---|---|
| `UNKNOWN_COMMAND` | 未知命令 |
| `INVALID_ARG` | 参数格式或范围错误 |
| `UNSUPPORTED_VERSION` | 不支持的协议主版本 |
| `UNSUPPORTED_FEATURE` | 命令在当前模式不可用或留待二期 |
| `BAD_STATE` | 命令与当前同步状态冲突 |
| `LINE_TOO_LONG` | 命令行超出长度限制 |
| `STORAGE_ERROR` | NVS 持久化失败 |
| `INTERNAL_ERROR` | 驱动或内部处理失败 |
| `CAPACITY`、`STALE_REV` | 会话表容量或版本冲突 |

连接顺序见第 6 节。普通业务命令及查询仅在正常模式使用；产测设备先以 `AT+MODE=NORMAL` 完成产测并重启。

## 一期 UI 信息页与本地时间

`AT+SHOWINFO=<seconds>`：正常模式下 `1～60` 拉起或续期信息页，`0` 关闭，返回 `OK`。必须显式给参数；常用驻留为 10 秒。`AT+SHOWINFO?` 返回 `+SHOWINFO:<remaining_seconds>`（向上取整，未显示为 0）、`OK`。产测返回 `UNSUPPORTED_FEATURE`。INFO? 仍只查询串口，不拉起画面。

信息页不显示倒计时。新出现的需要批准／等待输入状态，或指向当前最高 Session 组的非重复有效 EVENT，会关闭信息页；结束后不自动恢复。已有告警不阻止人工拉起。断线也关闭本页。

TIME 只设置 UTC 与当前显示偏移，不将链路变为 ONLINE；SYNCEND 完成才在线。新 HELLO 使时钟与当前偏移无效，必须重新 TIME；同步开始不会清除刚设置的偏移。重启后无效，超时后当前偏移失效。缺当前偏移时 TIME? 最后一项为 `-`。当前时间、日期只在待机和信息页显示主机本地时间；缺时钟或偏移时显示占位。未来重置时间使用 USAGE 自带偏移，不使用 TIME 的当前偏移。主机每 30 分钟以及时区／夏令时变化后重新 TIME，并刷新各 USAGE 的重置偏移。

开启 LOG 后，INFO? 的结构化 +LOG 额外提供 heap、min_heap、at_stack、ui_stack 和 render_max_us，供真机内存／刷新检查；任务栈余量单位为字节（ESP-IDF），刷新耗时单位为微秒。这些诊断只走串口，不进入 UI。

正常模式开启 LOG 后，INFO? 的诊断行另含 `page`、`page_seq`、`usage_supported`：page 为连接 0、任务 1、结果 2、用量 3、日期时间 4、信息 5；page_seq 为成功绘制的页面切换计数。仅用于测试诊断，不改变 INFO 响应字段，LOG=0 时不输出。

会话与灯环诊断另含 `group`（0 连接、1 等待、2 完成、3 工作、4 待机）、`slot`（1～8，非会话页为 0）、`led`（0 熄灭、1 未连接、2 连接中、3 授权、4 工作、5 思考、6 stale、7 成功、8 输入、9 失败、10 取消）、`mask`（16 位亮灯掩码）与 `rgb`（模式的 24 位基准颜色）。`led_sum` 是全部灯珠 RGB 通道总和，`p0`／`p8` 为对应灯珠的 24 位 RGB；这些像素值在空间档位缩放之后、用户全局亮度缩放之前，反映成功提交到驱动的帧。`status`／`frame`／`status_seq` 对应成功绘制的状态边框动画，非会话页 status／frame 为 255。诊断字段仅供测试，不作为服务端协议依赖。


## 23. `AT+CLOCKTHEME`：待机时间页配色

设置：`AT+CLOCKTHEME=<theme>`，仅正常模式可用，名称区分大小写。合法值如下：

| 名称 | 方案 | 时间 | 日期 | 年份 |
|---|---|---|---|---|
| `AMBER` | B 版暖琥珀，默认 | `#F1BC74` | `#D3DFDD` | `#938573` |
| `WHITE` | 原版白色 | `#F6F5FF` | `#E2E8F6` | `#92A4C9` |
| `MINT` | A 版薄荷绿 | `#77E6BC` | `#CEEBDD` | `#6D9D8B` |
| `CODEX` | Codex 紫色系，时间提亮以便黑底阅读 | `#B6A0F2` | `#D3C8EA` | `#8975B4` |

查询：`AT+CLOCKTHEME?` 返回 `+CLOCKTHEME:<theme>` 和 `OK`。设置成功先保存 NVS，再发布到显示模型；待机页下次刷新即生效。重复设置相同主题直接返回 `OK`，不重复写 NVS。主题仅影响时间日期待机页，连接状态顶栏、Session、用量页和灯环保持原配色；无效时钟／日期仍使用灰色占位且冒号不闪烁。

新增 NVS 键 `clock_theme`，升级时没有保存值或值超出范围则使用 `AMBER`。正常断线、HELLO／同步、模式切换和设备重启均保留设置。空值、未知名称、大小写错误及多余字段返回 `INVALID_ARG`；持久化失败返回 `STORAGE_ERROR` 且不发布新主题；产测模式设置／查询返回 `UNSUPPORTED_FEATURE`。

```text
AT+CLOCKTHEME=MINT
OK
AT+CLOCKTHEME?
+CLOCKTHEME:MINT
OK
AT+CLOCKTHEME=AMBER
OK
```

此为协议 2 的可选新增命令，不修改 HELLO／INFO 或现有字段；旧固件返回 `UNKNOWN_COMMAND` 时，服务端可禁用配色设置能力。本次仅提供设备接口，服务端设置界面及保存／下发逻辑暂未实现。
