/**
 * @file protocol_constants.h
 * @brief 协议常量定义
 * @author Mupengyi
 * @date 2026-05-02
 */
#pragma once

#include <QtGlobal>

namespace landrop
{
namespace core
{

// ========== UDP 组播配置 ==========

// 组播地址: 239.255.255.250 (IANA 未分配, 仅限局域网)
constexpr const char *multicast_group_address = "239.255.255.250";
// UDP 发现端口: 10262 (未被 IANA 分配给已知服务)
constexpr quint16 udp_discovery_port = 10262;
// 广播间隔: 启动时立即发送, 之后每隔 30 秒发送一次在线声明
constexpr int broadcast_interval_ms = 30000;
// 离线超时: 若 90 秒内未收到某设备的再次广播, 将其从设备列表移除
constexpr int offline_timeout_ms = 90000;

// 协议版本: 当前为 v1
constexpr int protocol_version = 1;

// ========== TCP 控制通道配置 ==========

// 心跳间隔: TCP 连接建立后每隔 15 秒互发 ping/pong
constexpr int heartbeat_interval_ms = 15000;
// 心跳超时: 超过 45 秒未收到任何数据则判定超时, 主动断开连接
constexpr int heartbeat_timeout_ms = 45000;
// 握手超时: 连接建立后 10 秒内未完成握手则断开
constexpr int handshake_timeout_ms = 10000;

// ========== 文件传输配置 ==========

// 文件分块大小: 8KB (8192 字节), 平衡传输速度与内存占用
constexpr int file_chunk_size = 8192;
// ACK 确认频率: 接收方每收到 64 个块发送一次确认
constexpr int ack_frequency = 64;
// 最大重传次数: 单个块重传超过 3 次则终止传输
constexpr int max_retries = 3;
// ACK 定时器间隔: 每秒至少发送一次 ACK
constexpr int ack_timeout_ms = 1000;

// 接收确认弹窗自动关闭超时: 30 秒内未操作自动拒绝
constexpr int receive_dialog_timeout_ms = 30000;

} // namespace core
} // namespace landrop
