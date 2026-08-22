# OV-Watch Bootloader 中文新手指南

这份指南面向第一次接触 Bootloader、Flash 分区、数字签名和固件打包的读者。请按顺序操作，不要直接把普通 `My_OV_Watch.hex` 烧到已经安装安全 Bootloader 的设备中。

## 先选择你需要的版本

- 默认分支 `main`：不带安全 Bootloader，克隆后打开 `MDK-ARM/My_OV_Watch.uvprojx`，可以按普通 STM32 工程直接编译下载。
- 分支 `feature/recovery-bootloader`：包含独立 Bootloader、ECDSA-P256 签名校验、SHA-256 完整性校验、串口升级协议和断电恢复入口。

本分支不是双应用 A/B。STM32F411CEU6 只有 512 KiB Flash，而当前 APP 约 410 KiB，无法同时保存两个完整 APP。本方案保证 Bootloader 恢复入口不会被 APP 升级擦除，但升级中断后需要重新发送固件，不能自动切回旧 APP。

## 先理解四个文件

| 文件 | 作用 | 是否公开 |
| --- | --- | --- |
| `ota_signing_private.pem` | 给固件签名，相当于发布者印章 | 不提交；每位开发者自行生成 |
| `Bootloader/Inc/ota_public_key.h` | 验证签名，编译进 Bootloader | 可以公开 |
| `OV_Watch_SignedApp_latest.hex` | 签名清单和完整 APP，供 ST-Link 更新 | 可以作为 Release 附件发布 |
| `OV_Watch_latest.ovota` | 签名清单和完整 APP，供串口 OTA | 可以作为 Release 附件发布 |

私钥不包含 Windows 用户名，也不是登录密码。学习签名算法不需要共享同一把私钥：工具可以立即生成新的 P-256 密钥对。私钥一旦公开，任何人都能制作“验证通过”的固件，这时签名只能作为格式演示，不能再证明发布者身份。

## 1. 准备环境

需要：

1. Keil MDK-ARM 和 STM32F4 Device Pack。
2. Python 3。
3. ST-Link，以及 SWDIO、SWCLK、GND、3.3 V 电平参考；空板连接困难时建议再接 NRST。
4. 如果需要串口 OTA，准备 3.3 V USB-TTL。不要把 5 V 串口电平直接接到 MCU。

在仓库根目录安装 Python 依赖：

```powershell
py -3 -m pip install -r Bootloader\Tools\requirements.txt
```

`cryptography` 用于生成密钥和签名，`pyserial` 用于串口发送 OTA 包。

## 2. 每位开发者生成自己的密钥对

先在仓库外建立一个只属于自己的目录，例如 `D:\OVWatchKeys`，然后运行：

```powershell
py -3 Bootloader\Tools\ota_tool.py keygen `
  --private-key D:\OVWatchKeys\ota_signing_private.pem `
  --public-header Bootloader\Inc\ota_public_key.h `
  --no-password
```

结果：

- 私钥写入 `D:\OVWatchKeys\ota_signing_private.pem`。
- 对应公钥写入 `Bootloader\Inc\ota_public_key.h`。
- 工具拒绝覆盖已经存在的私钥，避免误删原密钥。

`--no-password` 只适合教学和本机开发，它表示 PEM 文件未加密，并不表示没有数字签名。希望给私钥增加文件口令时，去掉 `--no-password`，然后按提示输入口令。

公钥变化后必须重新编译并烧录 Bootloader。使用 A 私钥生成的签名，只能由包含 A 公钥的 Bootloader 验证。

## 3. 编译 Bootloader

打开：

```text
Bootloader/MDK-ARM/OV_Bootloader.uvprojx
```

点击 `Build`。生成文件应位于：

```text
Bootloader/MDK-ARM/Objects/OV_Bootloader.hex
```

Bootloader 固定占用：

```text
0x08000000 ~ 0x0800FFFF
```

## 4. 编译真正的 My_OV_Watch APP

打开：

```text
MDK-ARM/My_OV_Watch.uvprojx
```

点击 `Build`。这个工程仍然是真正的手表 APP，只是通过 `My_OV_Watch_OTA.sct` 把起始地址重定位到了：

```text
0x08011000
```

不要把地址改回 `0x08000000`，否则会与 Bootloader 重叠。

## 5. 从 AXF 导出 APP 的 BIN

在仓库根目录运行：

```powershell
fromelf.exe --bin `
  --output Bootloader\Output\My_OV_Watch_latest.bin `
  MDK-ARM\My_OV_Watch\My_OV_Watch.axf
```

如果系统提示找不到 `fromelf.exe`，请使用 Keil 安装目录中 `fromelf.exe` 的完整路径。它一般位于 ARM 编译器的 `bin` 目录中。

BIN 是打包过程的中间文件，不包含烧录地址；不要让新手直接把它烧到 `0x08000000`。

## 6. 生成三种签名固件

下面示例使用未加密的教学私钥，所以包含 `--no-password`。如果你的私钥设置了口令，去掉该参数。

### 6.1 空板首次安装用 Factory HEX

```powershell
py -3 Bootloader\Tools\ota_tool.py factory `
  --boot-hex Bootloader\MDK-ARM\Objects\OV_Bootloader.hex `
  --bin Bootloader\Output\My_OV_Watch_latest.bin `
  --version 1 `
  --private-key D:\OVWatchKeys\ota_signing_private.pem `
  --no-password `
  --output Bootloader\Output\OV_Watch_Factory_latest.hex
```

它同时包含：

```text
Bootloader + 已提交的签名清单 + 完整 APP
```

### 6.2 ST-Link 日常更新用 SignedApp HEX

```powershell
py -3 Bootloader\Tools\ota_tool.py apphex `
  --bin Bootloader\Output\My_OV_Watch_latest.bin `
  --version 1 `
  --private-key D:\OVWatchKeys\ota_signing_private.pem `
  --no-password `
  --output Bootloader\Output\OV_Watch_SignedApp_latest.hex
```

它包含：

```text
0x08010000：签名清单
0x08011000：完整 My_OV_Watch APP
```

所以烧完 `SignedApp.hex` 后，不要再烧普通 `My_OV_Watch.hex`。SignedApp 已经包含完整 APP。

### 6.3 串口 OTA 用 OVOTA 包

```powershell
py -3 Bootloader\Tools\ota_tool.py pack `
  --bin Bootloader\Output\My_OV_Watch_latest.bin `
  --version 1 `
  --private-key D:\OVWatchKeys\ota_signing_private.pem `
  --no-password `
  --output Bootloader\Output\OV_Watch_latest.ovota
```

检查包的签名、SHA-256、向量表和镜像大小：

```powershell
py -3 Bootloader\Tools\ota_tool.py inspect `
  Bootloader\Output\OV_Watch_latest.ovota
```

每次发布建议增加 `--version`。当前版本号会参与签名，但 Bootloader 暂未实现禁止安装旧的合法签名固件，因此它还不是严格的 anti-rollback 计数器。

## 7. 空板首次烧录：推荐一次完成

打开：

```text
Bootloader/FactoryFlash/OV_Watch_Factory_Flash.uvprojx
```

这个工程只下载已经生成的 `OV_Watch_Factory_latest.hex`：不要 Build。

1. `Options for Target -> Utilities` 选择 `ST-Link Debugger`。
2. `Settings -> Flash Download` 确认存在 `STM32F4xx 512kB Flash` 算法。
3. 空板首次安装选择 `Erase Full Chip`。
4. 勾选 `Program`、`Verify`、`Reset and Run`。
5. 执行 `Flash -> Download`。
6. 重新上电后不要按 KEY1；等待 3 秒校验，然后进入手表 APP。

这一次下载已经完成 Bootloader、签名清单和 APP 的全部安装。

## 8. 空板分两次烧录

如果想学习分区，也可以这样操作。

第一次，打开 `Bootloader/MDK-ARM/OV_Bootloader.uvprojx`：

```text
Build -> Erase Full Chip -> Flash -> Download
```

此时没有 APP，屏幕显示 `FIRMWARE INVALID` 或恢复模式是正常现象。

第二次，先按照前文生成 `OV_Watch_SignedApp_latest.hex`，再打开：

```text
Bootloader/AppFlash/OV_Watch_SignedApp_Flash.uvprojx
```

选择 `Erase Sectors`，然后直接执行 `Flash -> Download`，不要 Build。

到这里已经结束，不存在“第三次再烧普通 My_OV_Watch”。第三次烧普通 APP 会擦掉或破坏签名清单，使 Bootloader 报 `FIRMWARE INVALID` 或 `HASH MISMATCH`。

## 9. 以后不用蓝牙，直接用 Keil 更新 APP

以后修改表盘、菜单或传感器代码时，不需要重烧 Bootloader。重复：

```text
Build My_OV_Watch
-> fromelf 导出最新 BIN
-> ota_tool.py apphex 重新签名
-> AppFlash 工程 Erase Sectors
-> Flash -> Download
```

只要 APP 内容改变一个字节，SHA-256 就会改变，因此必须重新生成 SignedApp HEX。普通 APP HEX 不包含位于 `0x08010000` 的签名清单。

## 10. 使用串口 OTA

上电或复位后的 3 秒内双击 KEY1，进入 `OTA RECOVERY MODE`。USART1 使用 PA9/PA10、9600-8-N-1。

```powershell
py -3 Bootloader\Tools\ota_tool.py send `
  --port COM7 `
  Bootloader\Output\OV_Watch_latest.ovota
```

每个最大 256 字节的数据块使用 CRC32 检查传输错误；全部接收后再检查完整 APP 的 SHA-256、向量表和签名，最后才写入有效标记并重启。

当前发送脚本要求电脑端能看到串口 COM。只有 BLE GATT、没有虚拟串口时，还需要另做 BLE GATT 上位机传输层。

## 11. 数字签名到底验证什么

打包工具先计算完整 APP 的 SHA-256，再把芯片型号、版本、APP 大小、APP 地址和 SHA-256 放入 56 字节清单前缀，最后用 ECDSA-P256 私钥签名。

Bootloader 使用公开的 P-256 公钥验证签名：

- CRC32 检测偶然传输错误，不能防伪。
- SHA-256 是完整 APP 的内容指纹，单独不能证明发布者。
- ECDSA-P256 证明清单由对应私钥签发，并通过清单中的 SHA-256 把签名绑定到完整 APP。

修改 APP 后重新计算 SHA-256 并不能伪造固件，因为没有对应私钥就无法给新的清单生成合法 ECDSA 签名。

## 12. 常见故障

### `FIRMWARE INVALID`

签名清单不存在、格式错误、签名错误、CRC 错误或有效标记没有提交。确认 Bootloader 公钥与签名私钥是一对，并烧录 SignedApp 而不是普通 APP。

### `HASH MISMATCH`

Flash 中 APP 的 SHA-256 与签名清单不一致。常见原因是签名后又单独烧了普通 `My_OV_Watch.hex`，或者 SignedApp 与 APP 不是同一次构建。

### `VECTOR INVALID`

APP 没有正确链接到 `0x08011000`，初始 MSP 不在 SRAM，或者 Reset Handler 不在 APP 地址范围。检查 `MDK-ARM/My_OV_Watch_OTA.sct` 是否仍启用。

### 一直停在恢复模式

没有有效 APP、启动校验失败，或上电 3 秒内检测到了 KEY1 双击。屏幕上的原因文字应指出具体阶段。

### ST-Link 无法连接

在 ST-Link 设置中使用 `Connect under Reset`：按住复位键开始连接，连接成功后松开。确认 GND 共地、SWDIO/SWCLK 没接反，并使用 3.3 V 电平。

### 找不到 `OV_Watch_*_latest.hex`

`Bootloader/Output` 是本地生成目录，不提交到 Git。先完成 Bootloader、APP 编译和第 6 节的打包命令。

### Python 提示缺少模块

重新执行：

```powershell
py -3 -m pip install -r Bootloader\Tools\requirements.txt
```
