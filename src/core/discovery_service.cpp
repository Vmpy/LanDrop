/**
 * @file discovery_service.cpp
 * @brief 局域网设备发现服务实现 — UDP 组播收发, 设备表维护, 离线检测
 * @author Mupengyi
 * @date 2026-05-02
 */
#include "discovery_service.h"
#include "protocol.h"
#include "protocol_constants.h"

#include <QUdpSocket>
#include <QTimer>
#include <QNetworkInterface>
#include <QJsonObject>
#include <QSettings>
#include <QUuid>
#include <QHostInfo>

namespace landrop
{
namespace core
{

discovery_service::discovery_service(QObject *parent)
    : QObject(parent)
{
    load_or_generate_uuid();                                    // 初始化: 加载或生成 UUID

    // 从 QSettings 读取设备名, 默认使用计算机名
    device_name_ = QSettings().value("device_name", QHostInfo::localHostName()).toString();

    socket_ = new QUdpSocket(this);                             // 创建 UDP 套接字

    announce_timer_ = new QTimer(this);                         // 创建广播定时器
    announce_timer_->setInterval(broadcast_interval_ms);        // 设置间隔 30000ms
    connect(announce_timer_, &QTimer::timeout, this, &discovery_service::send_announce);

    offline_check_timer_ = new QTimer(this);                    // 创建离线检测定时器
    offline_check_timer_->setInterval(offline_timeout_ms / 3);  // 每 30 秒检查一次
    connect(offline_check_timer_, &QTimer::timeout,
            this, &discovery_service::check_offline_devices);
}

discovery_service::~discovery_service()
{
    stop();                                                     // 析构前发送 goodbye 并清理资源
}

void discovery_service::start()
{
    // 绑定 UDP 组播端口, ShareAddress 允许多个实例共享同一端口
    socket_->bind(QHostAddress::AnyIPv4, udp_discovery_port, QUdpSocket::ShareAddress);

    // 加入组播组, 接收发往 239.255.255.250 的数据包
    socket_->joinMulticastGroup(QHostAddress(multicast_group_address));

    // 当有数据到达时, 触发 read_pending_datagrams
    connect(socket_, &QUdpSocket::readyRead,
            this, &discovery_service::read_pending_datagrams);

    announce_timer_->start();                                   // 启动 30 秒广播定时器
    offline_check_timer_->start();                              // 启动离线检测定时器

    send_announce();                                            // 立即发送首次在线广播
}

void discovery_service::stop()
{
    if (socket_->state() == QUdpSocket::BoundState)             // 仅在已绑定时执行
    {
        QByteArray goodbye = protocol::build_goodbye(uuid_);    // 构建离线通知包
        socket_->writeDatagram(goodbye, QHostAddress(multicast_group_address),
                               udp_discovery_port);             // 广播 goodbye

        socket_->leaveMulticastGroup(QHostAddress(multicast_group_address)); // 离开组播组
        socket_->close();                                       // 关闭套接字
    }

    announce_timer_->stop();                                    // 停止广播定时器
    offline_check_timer_->stop();                               // 停止离线检测定时器

    // 断开信号连接, 防止已关闭 socket 触发 read_pending_datagrams
    disconnect(socket_, &QUdpSocket::readyRead,
               this, &discovery_service::read_pending_datagrams);
}

void discovery_service::update_device_name(const QString &name)
{
    device_name_ = name;                                        // 更新内存中的设备名
    QSettings().setValue("device_name", name);                  // 持久化到配置文件
    send_announce();                                            // 立即发送新名称的广播
}

void discovery_service::update_tcp_port(quint16 port)
{
    tcp_port_ = port;                                           // 更新 TCP 端口 (由 transfer_server 通知)
}

QString discovery_service::device_name() const
{
    return device_name_;
}

QString discovery_service::uuid() const
{
    return uuid_;
}

quint16 discovery_service::tcp_port() const
{
    return tcp_port_;
}

void discovery_service::send_announce()
{
    // 构建包含本机信息的 JSON 广播包
    QByteArray announce = protocol::build_announce(
        device_name_, uuid_, tcp_port_, detect_platform());

    // 通过 UDP 组播发送到 239.255.255.250:10262
    socket_->writeDatagram(announce, QHostAddress(multicast_group_address), udp_discovery_port);
}

void discovery_service::read_pending_datagrams()
{
    while (socket_->hasPendingDatagrams())                      // 处理所有待处理的数据报
    {
        QByteArray datagram;
        datagram.resize(static_cast<int>(socket_->pendingDatagramSize())); // 预分配缓冲区

        QHostAddress sender;                                    // 发送方 IP 地址
        quint16 sender_port = 0;                                // 发送方端口

        socket_->readDatagram(datagram.data(), datagram.size(),
                              &sender, &sender_port);           // 读取数据报

        QJsonObject json = protocol::parse_json(datagram.trimmed()); // 去除空白并解析 JSON
        if (json.isEmpty())                                     // 解析失败则跳过
        {
            continue;
        }

        QString type = json["type"].toString();                 // 报文类型
        QString sender_uuid = json["uuid"].toString();          // 发送方 UUID

        if (sender_uuid == uuid_)                               // 忽略来自自己的报文
        {
            continue;
        }

        if (type == "announce")                                 // 在线广播
        {
            device_info info;
            info.uuid = sender_uuid;
            info.device_name = json["device_name"].toString();
            info.ip_address = sender;                           // 从数据报中获取 IP
            info.tcp_port = static_cast<quint16>(json["tcp_port"].toInt());
            info.platform = json["platform"].toString();
            info.last_seen = QDateTime::currentDateTime();      // 记录收到时间

            bool is_new = !device_table_.contains(sender_uuid); // 判断是否为新设备
            device_table_[sender_uuid] = info;                  // 更新设备表

            if (is_new)                                         // 仅在新发现时发射信号
            {
                emit device_discovered(info);
            }
        }
        else if (type == "goodbye")                             // 离线通知
        {
            if (device_table_.contains(sender_uuid))
            {
                device_table_.remove(sender_uuid);              // 从设备表移除
                emit device_offline(sender_uuid);               // 通知 UI 更新
            }
        }
    }
}

void discovery_service::check_offline_devices()
{
    QDateTime now = QDateTime::currentDateTime();
    QList<QString> to_remove;                                   // 收集待移除的设备 UUID

    for (auto it = device_table_.begin(); it != device_table_.end(); ++it)
    {
        qint64 elapsed_ms = it.value().last_seen.msecsTo(now); // 距上次收到广播的毫秒数
        if (elapsed_ms > offline_timeout_ms)                    // 超过 90 秒
        {
            to_remove.append(it.key());                         // 标记为离线
        }
    }

    for (const QString &uuid : to_remove)                       // 逐个移除并通知
    {
        device_table_.remove(uuid);
        emit device_offline(uuid);
    }
}

void discovery_service::load_or_generate_uuid()
{
    QSettings settings;
    uuid_ = settings.value("device_uuid").toString();           // 尝试加载已有 UUID
    if (uuid_.isEmpty())                                        // 首次运行
    {
        uuid_ = QUuid::createUuid().toString(QUuid::WithoutBraces); // 生成新 UUID
        settings.setValue("device_uuid", uuid_);                // 持久化保存
    }
}

QString discovery_service::detect_platform() const
{
#ifdef Q_OS_WIN
    return QStringLiteral("win");                               // Windows 平台
#elif defined(Q_OS_MACOS)
    return QStringLiteral("mac");                               // macOS 平台
#else
    return QStringLiteral("linux");                             // Linux 平台
#endif
}

} // namespace core
} // namespace landrop
