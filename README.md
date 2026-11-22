# AgentMate ESP32-C3

AgentMate 的设备端固件，使用 ESP-IDF 构建。电脑端 AgentMate 服务采集并整理 Codex、Claude Code 等工具的数据，通过 CH340 桥接的 UART0 发送 AT 指令；ESP32-C3 在 240×240 GC9A01 圆形 TFT 和 16 颗 RGB LED 灯环上展示会话状态、用量与提醒。

设备本身不直接采集 Agent 数据，也不负责联网获取用量或蓝牙外设电量。电脑端服务需要单独部署。

## 当前功能

- 会话状态、工作时长、Context 使用情况、项目名及主机／会话标识显示，最多保存 8 条会话。
- Codex、Claude provider 用量同步，最多保存 4 条用量记录，支持整批原子更新。
- 工作、思考、等待输入、需要批准、状态未更新及任务完成／失败／取消的屏幕与灯环效果。
- 日期时间待机页、用量轮播、蓝牙外设电量展示，最多接收主机选择的 4 个外设。
- TFT 和灯环亮度、屏幕旋转、待机时钟主题及自定义 RGB 配色，配置保存到 NVS。
- 产测模式：屏幕纯色、单灯／全灯、背光、字体与自动轮播测试。
- 串口 OTA：AT 控制、XMODEM-1K/CRC 传输、镜像校验及启动自检回滚。

当前软件版本由根目录 `CMakeLists.txt` 的 `project(... VERSION ...)` 定义，为 **v3.0**；AT 协议主版本为 **2**。配对与身份认证属于二期规划，当前返回 `UNSUPPORTED_FEATURE`。

页面预览见 [UI 实现与验收记录](docs/agentmate_ui_implementation.md)及[固件渲染预览](docs/design/firmware_preview.png)。设计文档保留了历史记录和规划，当前实现以源码及构建配置为准。

## 硬件与接线

默认硬件为 ESP32-C3、4 MB Flash、GC9A01 SPI 圆屏和 16 灯 RGB 灯环。

| 功能 | GPIO / 连接 |
|---|---|
| RGB 灯环 DIN | GPIO1 |
| TFT SCLK | GPIO6 |
| TFT MOSI | GPIO7 |
| TFT CS | GPIO10 |
| TFT DC | GPIO3 |
| TFT RST | GPIO4 |
| TFT 背光控制 | GPIO5 |
| UART0 TX / RX | GPIO21 / GPIO20，经 CH340 连接电脑 |
| 原生 USB | GPIO18 / GPIO19，保留 |

TFT 使用支持 3.3V 供电与逻辑的模块，灯环使用 USB 5V，所有模块必须共地。灯环的电平转换、串联电阻、电容和背光驱动接法见[硬件连接说明](docs/hardware_connection.md)。默认灯环亮度为 25%，屏幕背光为 75%。

## 快速开始

### 1. 准备环境和子模块

需要已安装并初始化的 ESP-IDF 环境、Git 和 Python 3。以下命令均在项目根目录执行。

```sh
git submodule update --init --recursive

# 按本机 ESP-IDF 安装位置调整路径
. "$HOME/esp/esp-idf/export.sh"
idf.py --version
```

`components/` 是独立 Git 子模块，通过 SSH 从 GitHub 仓库 `git@github.com:eehut/esp32_components.git` 获取；首次检出需要配置 GitHub SSH 访问。ESP-IDF Component Manager 依据 `main/idf_component.yml` 解析依赖，首次构建可能需要下载组件。

常规固件构建使用已提交的图标和字形资源，无需安装图像生成工具。

### 2. 构建

```sh
# 首次配置时设置目标芯片
idf.py set-target esp32c3
idf.py build

# 查看固件大小
idf.py size
```

应用固件输出为 `build/agentmate_esp32.bin`。`set-target` 会重新生成本地构建配置，后续日常构建执行 `idf.py build` 即可。

`sdkconfig.defaults` 和 `partitions.csv` 是受版本控制的配置；`sdkconfig`、`build/` 和 `managed_components/` 是本地产物。需要保留的配置修改应写入受版本控制的文件。

### 3. 烧录

先退出占用设备串口的电脑端服务或串口工具，再烧录：

```sh
# 将示例替换为系统实际枚举的 CH340 串口
PORT=/dev/cu.usbserial-XXXX
idf.py -p "$PORT" flash
```

macOS 端口通常形如 `/dev/cu.usbserial-*`，Linux 通常形如 `/dev/ttyUSB*`，Windows 使用实际 `COM` 端口。后续示例中的 `PORT` 均需替换为实际设备端口。

`idf.py flash` 用于基础部署，包含 bootloader、分区表和应用。首次为旧设备部署 OTA 能力时，也需要更新支持回滚的 bootloader；只上传应用无法补充该能力。

### 4. 首次启动与串口检查

串口参数为 **115200、8N1、无硬件流控**。使用支持发送文本命令的串口终端，逐条发送下面的命令；每条以 CR、LF 或 CRLF 结束，等待 `OK` 或 `ERROR` 后再发送下一条。

```text
AT+HELLO=2,fe6d7e13d354eb1f8d45968587589c65
AT+INFO?
AT+MODE?
```

`HELLO` 返回设备 ID、型号、软件版本、容量及模式。nonce 必须为 32 位小写十六进制字符，设备会原样回显；服务端应为每次发现生成新的 nonce。

**新 NVS 首次启动默认为 `FACTORY` 产测模式。** 完成接线和显示测试后发送：

```text
AT+MODE=NORMAL
```

设备保存模式、返回 `OK` 后重启。等待启动完成、清空旧串口输入，再重新发送 `HELLO`，确认模式为 `NORMAL`，即可交给电脑端服务同步数据。已有设备保留 NVS 中的模式配置。

正常模式可用以下命令检查配置和信息页：

```text
AT+TFTBRIGHT?
AT+LEDBRIGHT?
AT+SHOWINFO=10
AT+PING=74a8
```

正常模式默认关闭回显和应用日志，产测模式默认开启。需要诊断时可发送 `AT+LOG=1`，应用日志以独立的 `+LOG:` 行上报；使用 `AT+LOG=0` 关闭。开关重启后恢复模式默认值。

UART0 专用于 AT 通信，常规运行期日志和 bootloader 日志已关闭。`idf.py -p "$PORT" monitor` 可用于观察串口，但没有普通日志输出是正常现象；ROM 在复位瞬间仍可能输出启动字节。

## 电脑端通信

典型同步流程为：

```text
HELLO → TIME → SYNCBEGIN → SESSION（逐条）→ SYNCEND
      → USAGEBEGIN → USAGE（逐条）→ USAGEEND
      → BATTERY（按需）→ PING（持续保活）
```

- 服务端根据 `HELLO` 返回的容量选择下发内容；当前 `SESSION` 使用包含主机标识和会话短名的 15 字段格式。
- 命令与响应使用 ASCII，文本参数按协议进行百分号编码，每行最多 512 字节（含行尾）。响应以 CRLF 结束。
- 每次仅保留一条未完成命令，最终响应为独立的 `OK` 或 `ERROR` 行；错误原因见前面的 `+ERR:<code>`。
- 时间和时区偏移由电脑端下发。建议每 10 秒发送心跳；连续 30 秒没有有效命令时，设备清空远端状态并回到等待主机页面。

完整字段、同步语义、产测命令和错误码见 [AT 指令指南](docs/agentmate_at_command_guide.md)。连接与恢复策略见[通信方案](docs/agentmate_communication_plan.md)。

## 串口 OTA

OTA 仅在正常模式可用，接收本工程的**应用 `.bin`**，不接收合并 Flash 镜像、bootloader、分区表或字库。开始前先释放串口。

```sh
python3 -m pip install pyserial

# 只查询状态
python3 tools/test_ota_serial.py "$PORT"

# 上传并校验，成功后保留 READY 状态
python3 tools/test_ota_serial.py "$PORT" --firmware build/agentmate_esp32.bin
```

上传成功后可在串口终端发送 `AT+OTACOMMIT` 激活并重启；也可在上传命令后添加 `--commit`，由脚本完成提交。新版首次启动会进行硬件、显示和任务健康自检，通过后确认镜像，失败时由支持回滚的 bootloader 退回旧版。

传输期间串口进入二进制接收模式，不能夹发普通 AT 命令。SHA-256 用于完整性校验；当前未启用固件来源认证。取消、超时、错误注入和回滚验收方法见 [AT 指令指南的 OTA 章节](docs/agentmate_at_command_guide.md#24-串口-otaat-控制--xmodem-1kcrc)。

## 分区布局

默认分区表位于 `0x8000`，Flash 容量为 4 MB：

| 分区 | 偏移 | 大小 | 用途 |
|---|---|---|---|
| `nvs` | `0x9000` | `0x6000` | 设备 ID、模式和持久配置 |
| `otadata` | `0xF000` | `0x2000` | OTA 启动选择 |
| `phy_init` | `0x11000` | `0x1000` | PHY 初始化数据 |
| `ota_0` | `0x20000` | `0x1A0000` | 应用槽 0 |
| `ota_1` | `0x1C0000` | `0x1A0000` | 应用槽 1 |
| `hzk16` | `0x380000` | `0x60000` | 保留的字库分区 |

单个应用槽容量为 1,703,936 字节，OTA 时应以 `AT+OTA?` 返回的最大镜像大小为准。当前 UI 使用编译进应用的字形子集，不依赖完整 GB2312 字库；需要使用保留分区时参阅[字库打包工具说明](components/HZK_PACKER_README.md)。

## 开发与验证

### 主机回归

主机测试使用系统 `cc` 编译器及 AddressSanitizer／UndefinedBehaviorSanitizer，无需连接设备：

```sh
python3 tools/test_ui_host.py
python3 tools/test_ota_host.py
```

UI 测试覆盖状态策略、文本、页面渲染和灯效，预览输出至 `build/ui_host/`；安装 Pillow 后还会生成 PNG／GIF。OTA 测试覆盖 XMODEM 和升级生命周期。

### 实机验证

串口脚本依赖 `pyserial`，运行前先释放端口。脚本会下发测试状态，部分检查会切换模式、重启或改写配置，应在测试设备上按需执行，并目视检查画面与灯环。

```sh
python3 tools/test_at_serial.py "$PORT"
python3 tools/test_ui_serial.py "$PORT" --demo
python3 tools/test_session_serial.py "$PORT"
python3 tools/test_battery_serial.py "$PORT"
```

`tools/` 中另有用量、待机、状态、灯效、时钟主题及 OTA 专项脚本，可通过 `--help` 查看选项。修改固件后至少执行 `idf.py build`；涉及协议、NVS、引脚或显示时，还需在真实设备上验证。

### 资源生成

修改 provider SVG 或 UI 图标／字形后，从源文件重新生成并一起提交结果：

```sh
python3 -m pip install Pillow CairoSVG
python3 tools/generate_provider_icons.py
python3 tools/generate_ui_assets.py
```

provider 图标生成对应 BMP 与 `main/provider_icons.h`；UI 工具生成 `main/ui_assets.c`、`main/ui_assets.h` 等资源。UI 工具默认使用 macOS 字体，其他环境需通过 `--cjk-font`、`--latin-font`、`--number-font` 指定兼容字体，具体见脚本 `--help`。

## 目录与文档

| 路径 | 内容 |
|---|---|
| `main/app_main.c`、`main/app.h` | 应用入口、硬件初始化、共享状态与接口 |
| `main/protocol.c` | UART0 AT、产测控制、NVS 及状态同步 |
| `main/ui.c`、`main/ui_state.c`、`main/ui_render.c` | 页面布局、选页调度与绘制 |
| `main/status.c`、`main/led.c`、`main/usage.c`、`main/text.c` | 状态效果、灯环动画、用量及文本处理 |
| `main/ota.c`、`main/xmodem.c` | OTA 生命周期及二进制传输 |
| `components/` | TFT、字体、LED 等通用组件，独立 Git 子模块 |
| `assets/` | provider 与 UI 资源源文件 |
| `tools/`、`tests/` | 生成工具、主机测试及串口验收脚本 |
| `sdkconfig.defaults`、`partitions.csv` | 默认构建配置与分区表 |

- [产品需求](docs/agentmate_prd.md)
- [设备软件设计](docs/agentmate_esp32_software_design.md)
- [AT 指令指南](docs/agentmate_at_command_guide.md)
- [通信方案](docs/agentmate_communication_plan.md)
- [硬件连接说明](docs/hardware_connection.md)
- [UI 设计要求](docs/agentmate_ui_design_requirements.md)
- [UI 实现与验收记录](docs/agentmate_ui_implementation.md)
- [开发约定](AGENTS.md)

## 版权

Copyright (c) 2023–2026 Liu Chuansen。保留所有权利。本项目源码包含专有许可声明，使用、复制、分发或创作衍生作品须事先获得作者书面许可，具体以各文件中的声明为准。
