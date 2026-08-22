# OV-Watch 单分区安全恢复 Bootloader

本目录实现的是适合当前 `STM32F411CEU6（512 KiB Flash）` 的“单应用区 + 不可覆盖恢复区”方案。
它不是双分区 A/B：现有应用约 410 KiB，内部 Flash 无法同时容纳两份应用。升级断电后，旧应用可能已被擦除，但 Bootloader 始终保留，可以重新传输固件，因此不会因应用区损坏而失去恢复入口。

第一次接触 Bootloader、数字签名或 Keil 分区烧录，请先阅读 [中文新手指南](GETTING_STARTED_CN.md)。指南从生成自己的密钥对开始，完整解释空板首次烧录、日常 APP 更新、串口 OTA 和常见故障。默认分支 `main` 仍是不带此 Bootloader、可直接编译下载的版本。

## Flash 布局

| 地址范围 | 用途 |
| --- | --- |
| `0x08000000 ~ 0x0800FFFF` | Bootloader，64 KiB，扇区 0~3 |
| `0x08010000 ~ 0x08010FFF` | 128 字节签名清单及保留空间 |
| `0x08011000 ~ 0x0807FFFF` | 主应用，最大 454,656 字节 |

应用工程已经链接到 `0x08011000`，并通过 `USER_VECT_TAB_ADDRESS` 和 `VECT_TAB_OFFSET=0x11000` 重定位中断向量表。

## 安全与断电策略

1. Bootloader 先验证固件头的目标芯片、长度、CRC32 和 ECDSA-P256 签名，签名不正确时绝不擦除应用。
2. 通过验证后才擦除扇区 4~7，并按 256 字节块接收；每块都有 CRC32。
3. 全部写完后重新计算应用 SHA-256，并检查 MSP、Thumb Reset Handler 和地址范围。
4. 只有上述检查全部通过，才最后写入有效标记。任何更早时刻断电，下一次启动都会留在恢复模式，不会跳进半写入应用。
5. 正常启动也会检查清单签名、向量表和完整应用 SHA-256。

这里 CRC32 用于发现传输或存储中的偶然损坏；它不具备防伪能力。ECDSA 签名才负责证明固件由持有私钥的人发布，SHA-256 则把签名清单与完整固件内容绑定。

## 构建

- Bootloader：打开 `MDK-ARM/OV_Bootloader.uvprojx` 并构建。生成的 HEX 位于本地 `MDK-ARM/Objects/OV_Bootloader.hex`。
- 主应用：构建仓库根目录的 `MDK-ARM/My_OV_Watch.uvprojx`。
- 用 Keil `fromelf` 导出应用裸 BIN，例如：

```powershell
fromelf.exe --bin --output My_OV_Watch.bin My_OV_Watch.axf
```

## 制作签名升级包

每位开发者都应生成自己的密钥对。私钥放在仓库外并做好备份；公钥头文件可以公开并编译进 Bootloader。丢失私钥后，包含原公钥的 Bootloader 不会接受新密钥签发的固件，只能通过 ST-Link 重刷包含新公钥的 Bootloader。生成方法见 [中文新手指南](GETTING_STARTED_CN.md)。

```powershell
py -3 Bootloader/Tools/ota_tool.py pack `
  --bin Bootloader/Output/My_OV_Watch_latest.bin `
  --version 1 `
  --private-key D:\OVWatchKeys\ota_signing_private.pem `
  --no-password `
  --output Bootloader/Output/OV_Watch_latest.ovota

py -3 Bootloader/Tools/ota_tool.py inspect Bootloader/Output/OV_Watch_latest.ovota
```

正式使用时建议把私钥改成口令加密版本，并去掉 `--no-password`。

## 进入恢复模式与发送

每次上电或复位后的 3 秒窗口内，连续按下两次 KEY1（PA5，低电平有效），设备进入恢复模式；应用或清单无效时会无条件进入恢复模式。未触发且应用验证通过时，Bootloader 跳转到当前应用。

恢复协议运行在 USART1（PA9/PA10，9600-8-N-1）。可以使用 3.3 V USB-TTL，或使用能把 KT6328 透明通道映射成串口的上位机链路：

```powershell
py -3 Bootloader/Tools/ota_tool.py send --port COM7 Bootloader/Output/OV_Watch_latest.ovota
```

不要把 5 V 串口电平直接接到 MCU。当前脚本面向串口；若电脑端只能看到 BLE GATT 而没有虚拟 COM，后续还需增加对应的 BLE GATT 上位机传输层，设备端签名、写入和恢复逻辑无需改变。

## 首次安装与重要限制

首次安装推荐使用工具的 `factory` 命令，生成一个同时包含 Bootloader、已提交签名清单和主应用的组合 HEX，再用 ST-Link 一次烧录：

```powershell
py -3 Bootloader/Tools/ota_tool.py factory `
  --boot-hex Bootloader/MDK-ARM/Objects/OV_Bootloader.hex `
  --bin Bootloader/Output/My_OV_Watch_latest.bin `
  --version 1 `
  --private-key D:\OVWatchKeys\ota_signing_private.pem `
  --no-password `
  --output Bootloader/Output/OV_Watch_Factory_latest.hex
```

不要把 Keil 主应用工程生成的普通 HEX 直接当成可启动升级包：它不包含 `0x08010000` 的签名清单。单独更新应用时，应使用 `apphex` 命令生成“签名清单 + 应用”的组合 HEX，或者通过 OTA 发送 `.ovota` 包。

### 在 Keil µVision 中烧录组合 HEX

直接打开 `FactoryFlash/OV_Watch_Factory_Flash.uvprojx`。这是一个只负责下载现成 HEX 的工程，不需要也不应该执行 Build：

1. 打开 `Options for Target -> Utilities`，选择 `Use Debug Driver`，下拉框选择 `ST-Link Debugger`。
2. 点击 `Settings`，在 `Flash Download` 页确认存在 `STM32F4xx 512kB Flash` 算法，起始地址应为 `0x08000000`、大小为 `0x00080000`。
3. 首次修复时选择 `Erase Full Chip`，勾选 `Program`、`Verify` 和 `Reset and Run`。
4. 退出设置，点击菜单 `Flash -> Download`。Keil 应显示正在加载 `../Output/OV_Watch_Factory_latest.hex`，最后出现 `Programming Done` 和 `Verify OK`。
5. 复位后不要按 KEY1；Bootloader 会等待 3 秒并校验固件，约 4~5 秒后进入手表界面。

如果 Keil 报无法连接，可在 ST-Link 的 Debug 设置中把连接模式临时设为 `Connect under Reset`，按住板上复位键开始连接，连接成功后松开。

### 在 Keil 中单独烧 Bootloader

打开 `MDK-ARM/OV_Bootloader.uvprojx`，先 Build，再把 `Utilities -> Settings -> Erase` 设为 `Erase Sectors`，最后点击 `Flash -> Download`。不要选择 `Erase Full Chip`，否则现有主应用和签名清单也会被擦除。Bootloader 只占用 `0x08000000~0x0800FFFF`。

### 在 Keil 中单独烧签名主应用

先构建根目录的 `MDK-ARM/My_OV_Watch.uvprojx`，再由 `fromelf` 导出 BIN，并用工具生成签名应用 HEX：

```powershell
py -3 Bootloader/Tools/ota_tool.py apphex `
  --bin Bootloader/Output/My_OV_Watch_latest.bin `
  --version 1 `
  --private-key D:\OVWatchKeys\ota_signing_private.pem `
  --no-password `
  --output Bootloader/Output/OV_Watch_SignedApp_latest.hex
```

随后打开 `AppFlash/OV_Watch_SignedApp_Flash.uvprojx`，选择 `Erase Sectors`，直接执行 `Flash -> Download`，不要 Build。该 HEX 只写签名清单和 `0x08011000` 开始的应用区，不覆盖 Bootloader。

### Bootloader 屏幕状态

正常上电会显示启动页、3 秒进度条和 `DOUBLE-CLICK KEY1 FOR OTA UPDATE`。双击 KEY1 后进入 `OTA RECOVERY MODE`；清单、向量表或镜像摘要异常时也会显示对应原因并等待签名固件。升级传输期间显示接收进度，完成后提示重启。应用发生 HardFault 或初始化 `Error_Handler` 时，背光仍分别以 6 次或 7 次慢闪循环报码。

本方案能防止“升级中断导致恢复入口也损坏”，但不能在内部 Flash 保存上一版完整应用，也不能在新应用运行异常时自动切回上一版。真正的 A/B 自动回滚需要更大容量 MCU 或外部 Flash。第一次上板前应保留 ST-Link，并依次实测：正常升级、错误签名、篡改数据、传输中断、擦除中断和写入中断。
