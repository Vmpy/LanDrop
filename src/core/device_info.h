/**
 * @file device_info.h
 * @brief 设备信息结构体 — 存储局域网内发现的设备元数据
 * @author Mupengyi
 * @date 2026-05-02
 */
#pragma once

#include <QString>
#include <QHostAddress>
#include <QDateTime>

namespace landrop
{
namespace core
{

// 设备信息结构体: 描述一个在线设备的完整信息
struct device_info
{
    QString uuid;               // 设备唯一标识, 首次启动生成并持久化到 QSettings
    QString device_name;        // 用户自定义的设备显示名, 默认取计算机名
    QHostAddress ip_address;    // 设备的 IPv4 地址, 从 UDP 数据报中获取
    quint16 tcp_port = 0;       // 设备的 TCP 文件接收监听端口, 由系统动态分配
    QString platform;           // 操作系统标识: "win" / "mac" / "linux"
    QDateTime last_seen;        // 最后一次收到该设备广播的时间, 用于离线检测
};

} // namespace core
} // namespace landrop
