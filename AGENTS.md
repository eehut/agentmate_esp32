# AGENTS.md

## 项目概况

- 本仓库是 AgentMate 的 ESP32-C3 设备固件，使用 ESP-IDF 构建。当前原型通过 CH340 桥接 UART0 接收电脑端 AgentMate 服务发送的 AT 指令，在 240×240 GC9A01 圆形 TFT 和 16 颗 RGB LED 灯环上显示会话、用量与提醒。设备不直接采集 Codex 或 Claude Code 数据。
- 当前应用入口是 `main/app_main.c`；`main/agentmate_protocol.c` 负责 UART0 AT、产测模式、NVS 和状态更新，`main/agentmate_ui.c` 负责屏幕与灯环显示，`main/agentmate_app.h` 定义共享数据结构与模块接口。配对认证留待二期。修改并发访问时注意 `model_lock` 和 `display_lock` 的使用。
- `components/` 是独立 Git 子模块，提供 TFT、字体、LED 等驱动与通用组件；`main/CMakeLists.txt` 列出本应用实际依赖的组件。修改组件前先确认变更属于子模块，避免把应用逻辑放进通用驱动。

## 文档与协议依据

- `docs/agentmate_at_command_guide.md` 是 AT 指令的字段、顺序、响应和错误码依据；更改协议时同步检查该文档及 `main/agentmate_protocol.c`。
- `docs/agentmate_esp32_software_design.md` 说明设备侧状态模型、显示和异常恢复；`docs/agentmate_prd.md` 说明产品范围；`docs/agentmate_communication_plan.md` 说明电脑服务与设备的通信方案。设计文档包含规划与验收内容，判断当前已实现行为时以源码和构建配置为准。
- `docs/hardware_connection.md` 记录原型接线。当前 `app_main.c` 使用 GPIO1 驱动灯环，GPIO6/7/10/3/4/5 分别连接 TFT 的 SCLK/MOSI/CS/DC/RST/背光；GPIO18/19 留给原生 USB。改动引脚时同时核对实际硬件和该文档。
- UART0 AT 通道不能混入普通运行期日志。当前 `sdkconfig.defaults` 关闭控制台与 bootloader 常规日志；应用诊断通过可开关的 `+LOG:` AT 行上报。ROM 复位字节仍可能出现在串口，服务端发现前应清空旧输入。

## 构建与文件边界

- 在项目根目录使用已初始化的 ESP-IDF 环境运行 `idf.py build`；首次检出子模块时先运行 `git submodule update --init --recursive`。目标芯片为 `esp32c3`，默认 4 MB Flash。
- `sdkconfig.defaults` 和 `partitions.csv` 是受版本控制的构建配置。`sdkconfig`、`build/`、`managed_components/` 是本地配置或构建产物；需要持久化的配置改动应落实到受版本控制的文件中。
- `partitions.csv` 定义 NVS、OTA 槽和 `hzk16` 字库分区。调整分区时检查各分区容量、偏移及 4 MB Flash 边界；字库打包与烧录说明见 `components/HZK_PACKER_README.md`。
- `assets/provider_icons/*.svg` 是 provider 图标源文件；`tools/generate_provider_icons.py` 生成对应 BMP 和 `main/provider_icons.h`。改动图标时从源文件重新生成，保持生成结果一致。
- 修改固件后至少执行 `idf.py build`；涉及串口协议、NVS、引脚或显示效果的改动还应按相关文档在真实设备上验证。烧录和串口监视需指定实际设备端口。

## C 代码风格

- C 代码采用 Linux 内核风格的命名、括号和控制流程约定，并遵循下列项目缩进与换行规则。
- 使用 4 个空格缩进；禁止使用 Tab 字符缩进。
- `if`、`else`、`for`、`while` 等条件与循环语句必须使用大括号，即使语句体只有一行。
- 每行最多 160 个字符。代码行不得超过此限制。
- 不要为了缩短行而随意换行；只有超过 160 字符或确实能提升表达清晰度时才拆分语句。
- 函数名、变量名和类型名使用小写加下划线；宏和枚举常量使用大写加下划线。
- 具有外部链接的全局变量使用 `g_` 前缀；所有静态存储期变量（文件作用域 `static` 变量和函数内 `static` 变量）使用 `s_` 前缀。
- 头文件使用 include guard；头文件包含顺序保持清晰，优先包含对应模块头文件。
- 避免不必要的 typedef、复杂宏、全局变量和深层嵌套；优先使用清晰直接的控制流程。
- 修改后保持现有项目的构建约定，不引入与本项目不兼容的格式化规则。

## C 文件版本声明

新增或实质性修改的 C 源文件应在文件开头使用以下声明。根据文件实际内容更新 `Description`、作者和修订记录；未经作者许可，不得擅自更改版权和许可文字。

```c
/*
#####################################################################################
#
# Copyright (c) 2023-2026 Liu Chuansen <liuchuansen@outlook.com>. All Rights Reserved.
#
# This software is the personal work of Liu Chuansen. You shall not copy, use,
# distribute, or create derivative works of this software without the author's
# prior written permission.
#
# UNAUTHORIZED COPYING, USE, OR DISTRIBUTION OF THIS SOFTWARE, OR ANY PORTION OR
# DERIVATIVE THEREOF, IS STRICTLY PROHIBITED. IF YOU HAVE RECEIVED THIS SOFTWARE IN
# ERROR, PLEASE NOTIFY THE AUTHOR IMMEDIATELY AND DELETE IT FROM YOUR SYSTEM.
#
#####################################################################################
#
# Description: Foxglove Context implementation
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-03-02 1.0      Liu Chuansen          Created.
#
#####################################################################################
*/
```
