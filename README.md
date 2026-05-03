# LanDrop

局域网文件传输桌面应用 — 无需互联网、无需账号，同一局域网内的设备自动发现并通过 TCP 传输文件。

## 特性

- **零配置** — 启动即用，设备通过 UDP 组播自动发现
- **无中心服务器** — 纯 P2P 传输，数据不经过第三方
- **跨平台** — Windows / macOS / Linux
- **分块传输** — 8KB 分块 + CRC32 校验，支持断点续传（ACK/NACK）
- **系统托盘** — 关闭窗口后最小化到托盘，后台接收文件

## 构建

**依赖**: Qt 6.5+ (Core, Network, Widgets), CMake 3.19+, C++17 编译器

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=<Qt6安装路径>
cmake --build build
```

也可用 Qt Creator 打开 `CMakeLists.txt`，使用 Desktop 套件直接构建。

## 使用

1. 在两台设备上启动 LanDrop（同一局域网）
2. 左侧面板自动显示在线设备
3. 点击目标设备，再点击「发送文件」选择文件
4. 接收方弹出确认弹窗，选择保存路径后开始传输
5. 进度条显示传输进度，完成后托盘弹出通知

## 架构

```
src/
├── main.cpp
├── core/                     # 核心层 (无 GUI 依赖)
│   ├── protocol.h/cpp        # 协议序列化/反序列化
│   ├── protocol_constants.h  # 所有协议常量
│   ├── device_info.h         # 设备信息结构体
│   ├── discovery_service     # UDP 组播设备发现
│   ├── transfer_server       # TCP 服务端, 接受连接
│   ├── file_sender           # 文件发送 (分块/重传)
│   └── file_receiver         # 文件接收 (组包/校验)
└── ui/                       # 表现层 (纯 UI)
    ├── main_window           # 主窗口
    ├── device_list_widget    # 设备列表
    └── receive_dialog        # 接收确认弹窗
```

## 协议概要

| 通道 | 协议 | 地址 | 说明 |
|------|------|------|------|
| 设备发现 | UDP 组播 | `239.255.255.250:10262` | JSON announce/goodbye, 30s 间隔, 90s 超时 |
| 控制通道 | TCP | 动态端口 | JSON 握手/元数据/ACK/NACK, 15s 心跳, 45s 超时 |
| 数据通道 | TCP | 同控制通道 | 二进制分块: 4B seq + 4B len + payload + 4B CRC32 |

## 许可证

LGPLv3 — 仅动态链接 Qt 开源版模块。
