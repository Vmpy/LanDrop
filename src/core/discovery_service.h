/**
 * @file discovery_service.h
 * @brief 局域网设备发现服务 — 通过 UDP 组播实现设备上线/下线感知
 * @author Mupengyi
 * @date 2026-05-02
 */
#pragma once

#include <QObject>
#include <QHash>

#include "device_info.h"

class QUdpSocket;  // UDP 套接字, 用于组播收发
class QTimer;      // 定时器, 控制广播间隔和离线检测周期

namespace landrop
{
namespace core
{

// 设备发现服务: 发送在线广播 + 监听其他设备广播 + 维护在线设备表
class discovery_service : public QObject
{
    Q_OBJECT

public:
    explicit discovery_service(QObject *parent = nullptr);
    ~discovery_service() override;

    // 启动发现服务: 绑定 UDP 端口, 加入组播组, 启动定时器, 立即发送首次广播
    void start();

    // 停止发现服务: 发送 goodbye, 离开组播组, 停止定时器
    void stop();

    // 更新设备显示名 (用户修改设置后调用, 会触发重新广播)
    void update_device_name(const QString &name);

    // 更新 TCP 文件接收端口 (transfer_server 启动后回调)
    void update_tcp_port(quint16 port);

    // === 属性访问 ===
    QString device_name() const; // 获取当前设备显示名
    QString uuid() const;        // 获取本机 UUID
    quint16 tcp_port() const;    // 获取当前 TCP 监听端口

public slots:
    // 发送一次在线广播 (定时器触发 + 手动调用)
    void send_announce();

signals:
    // 发现新设备时发射 (首次收到该设备的 announce 或信息更新)
    void device_discovered(const landrop::core::device_info &info);

    // 设备离线时发射 (收到 goodbye 或超过 90 秒未收到广播)
    void device_offline(const QString &uuid);

private slots:
    // 读取并处理收到的 UDP 数据报
    void read_pending_datagrams();

    // 定时检查设备表, 移除超时未活动的设备
    void check_offline_devices();

private:
    // 从 QSettings 加载 UUID, 不存在则生成新的并持久化
    void load_or_generate_uuid();

    // 检测当前操作系统平台, 返回 "win" / "mac" / "linux"
    QString detect_platform() const;

    QUdpSocket *socket_ = nullptr;            // UDP 组播套接字
    QTimer *announce_timer_ = nullptr;        // 周期性广播定时器 (30 秒)
    QTimer *offline_check_timer_ = nullptr;   // 离线检测定时器 (30 秒)

    QString device_name_;                     // 本机设备显示名
    QString uuid_;                            // 本机唯一标识 (首次启动生成)
    quint16 tcp_port_ = 0;                    // 本机 TCP 文件接收端口

    // 在线设备表: key=UUID, value=设备完整信息
    QHash<QString, device_info> device_table_;
};

} // namespace core
} // namespace landrop
