/**
 * @file transfer_server.h
 * @brief TCP 文件传输服务端 — 监听连接, 握手验证, 创建 file_receiver 处理接收
 * @author Mupengyi
 * @date 2026-05-02
 */
#pragma once

#include "protocol.h"  // 需要 file_meta_info 的完整定义

#include <QObject>
#include <QSet>

class QTcpServer;   // TCP 监听服务器
class QTcpSocket;   // TCP 套接字

namespace landrop
{
namespace core
{

struct device_info;
class file_receiver; // 文件接收器, 每个传入传输创建一个实例

// TCP 文件传输服务端: 监听系统分配的端口, 接受连接后握手, 通知 UI 有文件传输请求
class transfer_server : public QObject
{
    Q_OBJECT

public:
    explicit transfer_server(QObject *parent = nullptr);
    ~transfer_server() override;

    // 启动服务: 监听任意可用端口 (系统动态分配)
    bool start();

    // 停止服务: 关闭所有连接和监听
    void stop();

    // 获取当前监听端口号
    quint16 listen_port() const;

public slots:
    // 用户接受文件传输, 传入保存路径
    void accept_reception(const QString &save_path);

    // 用户拒绝文件传输
    void reject_reception();

signals:
    // 监听端口变更 (服务启动成功后发射)
    void listen_port_changed(quint16 port);

    // 收到文件传输请求 (发送方名称, 文件名, 文件大小)
    void file_proposal(const QString &sender_name, const QString &file_name, qint64 file_size);

    // 接收进度更新 (已接收字节数, 总字节数)
    void receive_progress(qint64 received, qint64 total);

    // 接收完成 (文件保存路径)
    void receive_finished(const QString &file_path);

    // 接收错误 (错误原因描述)
    void receive_error(const QString &reason);

private slots:
    // 处理新的 TCP 连接
    void on_new_connection();

    // 处理握手数据 (handshake / file_meta)
    void on_handshake_data();

private:
    QTcpServer *server_ = nullptr;              // TCP 监听服务器
    QSet<QTcpSocket *> pending_sockets_;        // 等待握手的待处理连接
    file_receiver *current_receiver_ = nullptr; // 当前正在处理的接收器 (同时只处理一个)
    file_meta_info pending_meta_;               // 暂存的待确认文件元数据
    QString pending_sender_name_;               // 暂存的发送方设备名
};

} // namespace core
} // namespace landrop
