/**
 * @file transfer_server.cpp
 * @brief TCP 文件传输服务端实现 — 监听/接受连接/握手/创建接收器
 * @author Mupengyi
 * @date 2026-05-02
 */
#include "transfer_server.h"
#include "file_receiver.h"
#include "protocol.h"
#include "protocol_constants.h"
#include "device_info.h"

#include <QTcpServer>
#include <QTcpSocket>

namespace landrop
{
namespace core
{

transfer_server::transfer_server(QObject *parent)
    : QObject(parent)
{
    server_ = new QTcpServer(this);                             // 创建 TCP 服务器
    connect(server_, &QTcpServer::newConnection,
            this, &transfer_server::on_new_connection);         // 有新连接时处理
}

transfer_server::~transfer_server()
{
    stop();
}

bool transfer_server::start()
{
    if (!server_->listen(QHostAddress::Any))                    // 监听所有网络接口的任意端口
    {
        return false;                                           // 监听失败
    }
    emit listen_port_changed(server_->serverPort());            // 通知 discovery_service 更新端口
    return true;
}

void transfer_server::stop()
{
    if (current_receiver_)                                      // 清理正在接收的文件
    {
        current_receiver_->abort();                             // 中止接收, 删除已写入的部分
        current_receiver_->deleteLater();                       // 延迟删除
        current_receiver_ = nullptr;
    }

    for (QTcpSocket *sock : pending_sockets_)                   // 清理所有待握手连接
    {
        sock->abort();                                          // 立即断开
        sock->deleteLater();
    }
    pending_sockets_.clear();

    server_->close();                                           // 停止监听新连接
}

quint16 transfer_server::listen_port() const
{
    return server_->serverPort();                               // 返回当前监听端口
}

void transfer_server::accept_reception(const QString &save_path)
{
    if (!current_receiver_)                                     // 无活动接收器
    {
        return;
    }

    current_receiver_->accept(save_path);                       // 指示接收器开始写入文件
}

void transfer_server::reject_reception()
{
    if (!current_receiver_)                                     // 无活动接收器
    {
        return;
    }

    current_receiver_->reject();                                // 发送拒绝包并断开连接
}

void transfer_server::on_new_connection()
{
    while (server_->hasPendingConnections())                    // 处理所有待处理的连接
    {
        QTcpSocket *socket = server_->nextPendingConnection();  // 接受下一个连接
        if (!socket)
        {
            continue;
        }

        pending_sockets_.insert(socket);                        // 加入待握手队列

        socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1); // 启用 TCP keepalive

        // 有数据可读时, 检查是否为 handshake 或 file_meta
        connect(socket, &QTcpSocket::readyRead,
                this, &transfer_server::on_handshake_data);

        // 连接断开时自动清理 (可能握手阶段就断开了)
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]()
        {
            pending_sockets_.remove(socket);                    // 从待握手集合移除
            socket->deleteLater();                              // 延迟删除套接字
        });
    }
}

void transfer_server::on_handshake_data()
{
    for (QTcpSocket *socket : pending_sockets_)                 // 遍历所有待握手连接
    {
        if (socket->bytesAvailable() == 0)                      // 无数据 (由其他 socket 触发)
        {
            continue;
        }

        QByteArray data = socket->readLine();                   // 读取一行 (JSON 以 \n 结尾)
        if (data.isEmpty())
        {
            continue;
        }

        QJsonObject json = protocol::parse_json(data.trimmed()); // 解析 JSON
        QString type = json["type"].toString();

        if (type == "handshake")                                // 握手包
        {
            pending_sender_name_ = json["device_name"].toString(); // 记录发送方名称
            socket->write(protocol::build_handshake_ack());     // 回复握手确认
        }
        else if (type == "file_meta")                           // 文件元数据包
        {
            pending_meta_.file_name = json["file_name"].toString();
            pending_meta_.file_size = json["file_size"].toVariant().toLongLong();

            if (current_receiver_)                              // 已有接收器在忙
            {
                socket->write(protocol::build_reject("Receiver busy"));
                socket->disconnectFromHost();
                pending_sockets_.remove(socket);
                return;
            }

            // 创建文件接收器, 由它接管该 TCP 连接的生命周期
            current_receiver_ = new file_receiver(socket, this);

            // 信号转发: 将 file_receiver 的信号连接到 transfer_server 的信号
            connect(current_receiver_, &file_receiver::progress,
                    this, &transfer_server::receive_progress);
            connect(current_receiver_, &file_receiver::finished,
                    this, &transfer_server::receive_finished);
            connect(current_receiver_, &file_receiver::error,
                    this, &transfer_server::receive_error);

            // 接收器销毁时清除指针
            connect(current_receiver_, &file_receiver::destroyed, this, [this]()
            {
                current_receiver_ = nullptr;
            });

            pending_sockets_.remove(socket);                    // 从待握手队列移除

            emit file_proposal(pending_sender_name_,            // 通知 UI: 有文件传输请求
                               pending_meta_.file_name,
                               pending_meta_.file_size);
        }
        else                                                    // 未知报文类型
        {
            socket->write(protocol::build_reject("Unexpected packet"));
            socket->disconnectFromHost();                       // 直接拒绝并断开
            pending_sockets_.remove(socket);
        }
    }
}

} // namespace core
} // namespace landrop
