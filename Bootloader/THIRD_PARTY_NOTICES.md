# Bootloader 第三方组件说明

Bootloader 中只引入了完成签名验证与摘要计算所需的最小源码文件：

- **micro-ecc**，提交 `541b3a78026420a3e369c4c9281c396b5e531113`，作者 Kenneth MacKay，BSD-2-Clause。原始项目：<https://github.com/kmackay/micro-ecc>。完整许可见 `ThirdParty/micro-ecc/LICENSE.txt`。
- **TinyCrypt**，提交 `cf24c907a61c01bc2c4e1ee0ae24457c27a840a2`，Copyright Intel Corporation，BSD-3-Clause。原始项目：<https://github.com/intel/tinycrypt>。完整许可见 `ThirdParty/tinycrypt/LICENSE.txt`。

这两个组件的许可文本必须随源码和二进制再发布材料一并保留。项目自己的修改不改变这些上游组件的版权归属。
