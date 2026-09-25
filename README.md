# LambdaY

LambdaY | 一个简介易用的64位操作系统，从UEFI启动

## 开发指南

**第一步**：克隆本项目至本地。

```bash
git clone https://github.com/pyao-dev/LambdaY --recursive
```

**第二步**：安装开发用依赖。在Ubuntu 26上：

```bash
sudo apt update
sudo apt install build-essential clang-format qemu-system-x86 mtools
```

Build Essential包含make和gcc等工具，GNU C作为C/C++编译器、链接器使用，ClangFormat作为格式化工具，QEMU为开发环境模拟器，mtools为`.img`文件工具。

## 声明

1. 本项目在 [GPL-v3](./LICENSE)协议下开源；
2. `include/kernel/graphics/pf/fontdata/asc16.h`和`include/kernel/graphics/pf/fontdata/hzk16.h`来自[m5stack/m5-hzk16](https://github.com/m5stack/m5-hzk16)项目
