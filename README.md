# OV-Watch Bare-Metal Refactor

> 面向智能手表交互、显示性能与低功耗设计的 STM32F411 + LVGL 嵌入式作品集；基于 [No-Chicken/OV-Watch](https://github.com/No-Chicken/OV-Watch) 进行独立重构、性能优化和功能扩展。

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
![MCU](https://img.shields.io/badge/MCU-STM32F411CEU6-32C5D2)
![GUI](https://img.shields.io/badge/GUI-LVGL-5C6BC0)
![Build](https://img.shields.io/badge/Keil-build%20passing-brightgreen)

本项目运行于 STM32F411CEU6 智能手表硬件，围绕 LVGL 显示性能、低功耗状态管理、可靠持久化和可穿戴交互进行了重构与扩展。本仓库同时作为嵌入式软件求职作品集：功能取舍、性能数据和技术结论均尽量提供可定位的源码与实机依据。它不是原项目的官方版本，也不代表原作者立场。

原项目作者保留原始代码及素材的相应权利；本仓库维护者对 2026 年完成的修改和新增部分负责。完整来源与修改声明见 [ATTRIBUTION.md](ATTRIBUTION.md)。本修改版本继续按照 GNU GPL v3.0 发布。

## 当前 Bootloader 分支说明

你正在查看的 `feature/recovery-bootloader` 分支加入了独立恢复 Bootloader、ECDSA-P256 固件签名、SHA-256 完整性校验和断电恢复入口。第一次使用请从 [Bootloader 中文新手指南](Bootloader/GETTING_STARTED_CN.md) 开始。默认分支 `main` 保持不带安全 Bootloader 的直接编译、直接下载流程，适合先学习手表应用本身。

## 我的主要工程贡献

- 三套表盘，可水平滑动、滚动吸附，并持久化最后选择。
- 经典机械表盘采用平滑秒针与局部脏区刷新，避免整个 `lv_meter` 重绘。
- 菜单减少 LVGL 对象层级，使用自定义绘制并消除重复背景填充。
- 正常显示、AOD、STOP 三态低功耗状态机，支持按键、触摸、RTC 和 MPU 抬腕唤醒。
- 24C02 多槽循环持久化：地址分区、序号回绕、校验、掉电安全和旧格式兼容。
- 运动目标闭环：目标设置、EEPROM 保存、进度条、跨页面达标事件和唤醒后庆祝提示。
- 控制中心、手电筒、亮度调节、软件关机、通知、历史数据和统一触摸返回。
- LVGL 自带 FPS/CPU 性能监视，用于实机迭代而不是只凭肉眼判断。

## 实机性能结果

以下数字来自当前硬件上的阶段性实测，不应理解为跨平台基准：

| 场景 | 初始版本 | 当前版本 | 参考版本 |
|---|---:|---:|---:|
| 菜单连续滑动最低 FPS | 约 14 | 约 40 | 约 21 |

主要优化位于：

- `User/App/menu_page.c`：单 panel 菜单项、自定义 `LV_EVENT_DRAW_MAIN`、透明背景。
- `User/WatchFace/watch_face.c`：秒针独立绘制、角度插值、旧位置与新位置局部失效。
- `MDK-ARM/My_OV_Watch.uvprojx`：面向速度的编译优化。

这里的 FPS 是 LVGL 在当前脏区刷新负载下的帧率，并不表示每帧都通过 SPI 传输了完整的 240×280 画面。完整测试方法与后续复测要求见 [docs/PERFORMANCE.md](docs/PERFORMANCE.md)。

## 软件架构

```text
main loop
├─ DeviceManager_Process()   传感器采样、计步、告警和持久化调度
├─ BatteryManager_Process()  电池状态与低电量事件
├─ Key / Touch               输入事件
└─ AppUI_Process()           页面切换、AOD、STOP、唤醒和全局弹层
   ├─ WatchFace
   ├─ Menu / Pages
   ├─ StatusBar
   └─ ControlCenter
```

页面只读取设备层缓存，设备层不直接操作某个页面。页面切换通过请求标志延迟到 `AppUI_Process()`，避免在 LVGL 事件回调栈中删除当前对象。

更完整的设计说明见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。

## 硬件

- MCU：STM32F411CEU6
- LCD：ST7789，240×280，SPI
- Touch：CST816
- Motion：MPU6050
- Heart rate：EM7028
- Environment：AHT21
- Compass：LSM303DLH
- EEPROM：BL24C02 / 24C02
- BLE：KT6328

具体连线应以原理图、CubeMX 工程和 `Core/Inc/main.h` 为准。

## 编译

已验证环境：

- Keil MDK-ARM
- Arm Compiler 5.06 update 7 (build 960)
- 工程：`MDK-ARM/My_OV_Watch.uvprojx`

步骤：

1. 使用 Keil 打开 `MDK-ARM/My_OV_Watch.uvprojx`。
2. 选择 `My_OV_Watch` target。
3. Build/Rebuild。
4. 使用 ST-Link 将生成的 HEX 下载到 STM32F411CEU6。

最近一次完整构建结果：

```text
Program Size: Code=186276 RO-data=223896 RW-data=1408 ZI-data=110184
0 Error(s), 29 Warning(s)
```

现有警告来自工程内保留的 STM32 HAL 与 LVGL 第三方源码；本项目自编写的应用层和本次字体、图标替换未引入新的编译警告。

## 目录

```text
BSP/            板级驱动
Core/           CubeMX/HAL 初始化与主循环
Drivers/        CMSIS 与 STM32 HAL
ThirdParty/     LVGL 等第三方组件
User/App/       UI 状态机、菜单、状态栏和设置页
User/Device/    传感器调度、EEPROM、历史与电池管理
User/Pages/     功能页面
User/Power/     STOP、唤醒与关机逻辑
User/WatchFace/ 三套表盘及刷新优化
docs/           架构和性能说明
```

## 已知限制

- FPS 数据仍需补充固定手势、重复次数、SPI 配置和视频证据后才能作为严格 benchmark。
- 使用 ST-Link 或外部 3.3 V 供电时，软件关机不会让 MCU 真正掉电，设备会黑屏并停在 WFI；真实电池供电时由 `POWER_EN` 保持电路断电。
- 字体和图标已替换为具有明确再分发许可的 Fredoka、Material Icons 与公共领域 font8x8 字形子集，来源和许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
- 当前版本定位为可在实机复现和验证的嵌入式作品集；性能结论只适用于已记录的硬件与测试场景，不把局部优化扩大为对上游版本的全面评价。

## AI 使用说明

本项目开发过程中使用了 AI 辅助进行代码生成、重构、注释和审查。维护者负责需求定义、硬件搭建、实机测量、方案取舍、编译烧录和最终验收。详细说明见 [AI_USAGE.md](AI_USAGE.md)。

## 许可证与致谢

本项目是 GPL-3.0 项目的修改版本，继续以 GNU GPL v3.0 发布。发布固件二进制时，应同时提供对应版本的完整源代码和构建说明。

- 上游项目：[No-Chicken/OV-Watch](https://github.com/No-Chicken/OV-Watch)
- 上游作者：No-Chicken / 不吃油炸鸡
- 许可证：[GNU General Public License v3.0](LICENSE)

感谢原作者公开硬件与软件工程，为学习和二次开发提供了基础。
