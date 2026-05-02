/**
 * @file file_sender.cpp
 * @brief 文件发送器实现 — 连接/握手/分块发送/ACK 处理/NACK 重传/超时检测
 * @author Mupengyi
 * @date 2026-05-02
 */
#include "file_sender.h"
#include "protocol.h"
#include "protocol_constants.h"

#include <QTcpSocket>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QHostAddress>

namespace landrop
{
namespace core
{

file_sender::file_sender(QObject *parent)
    : QObject(parent)
{
    socket_ = new QTcpSocket(this);                             // 创建 TCP 套接字

    // 连接成功 → 发送握手包
    connect(socket_, &QTcpSocket::connected,
            this, &file_sender::on_connected);

    // 有数据可读 → 处理 ACK/NACK
    connect(socket_, &QTcpSocket::readyRead,
            this, &file_sender::on_ready_read);

    // 被动断连 → 通知 UI 连接丢失
    connect(socket_, &QTcpSocket::disconnected, this, [this]()
    {
        if (sending_)                                           // 正在发送过程中
        {
            emit error("Connection lost");
        }
        emit disconnected();
    });

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(socket_, &QTcpSocket::errorOccurred, this, [this]()
    {
        if (sending_)
        {
            emit error(socket_->errorString());                 // 套接字级别的错误描述
        }
    });
#else
    connect(socket_, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, [this]()
    {
        if (sending_)
        {
            emit error(socket_->errorString());
        }
    });
#endif

    send_timer_ = new QTimer(this);
    send_timer_->setInterval(0);                                // 间隔 0ms: 每当事件循环空闲就发送
    connect(send_timer_, &QTimer::timeout, this, &file_sender::send_next_chunk);

    timeout_timer_ = new QTimer(this);
    timeout_timer_->setInterval(heartbeat_timeout_ms);          // 45 秒超时
    connect(timeout_timer_, &QTimer::timeout, this, &file_sender::check_timeout);
}

file_sender::~file_sender()
{
    cancel();                                                   // 确保资源清理
}

void file_sender::connect_to_host(const QHostAddress &address, quint16 port,
                                   const QString &device_name, const QString &uuid)
{
    device_name_ = device_name;                                 // 保存本机信息
    uuid_ = uuid;
    socket_->connectToHost(address, port);                      // 发起 TCP 连接
}

void file_sender::send_file(const QString &file_path)
{
    if (!handshake_done_)                                       // 握手尚未完成
    {
        current_file_path_ = file_path;                         // 暂存路径, 等待握手完成后发送
        return;
    }

    current_file_path_ = file_path;

    QFileInfo info(file_path);
    if (!info.exists())                                         // 文件不存在
    {
        emit error(QStringLiteral("文件不存在: %1").arg(file_path));
        return;
    }

    file_ = new QFile(file_path, this);                         // 打开源文件
    if (!file_->open(QIODevice::ReadOnly))                      // 只读模式
    {
        emit error(QStringLiteral("无法打开文件: %1").arg(file_path));
        return;
    }

    file_size_ = info.size();                                   // 获取文件大小
    total_chunks_ = (file_size_ + file_chunk_size - 1) / file_chunk_size; // 计算总块数

    // 先发送文件元数据, 告知接收方文件名/大小/块大小
    QByteArray meta = protocol::build_file_meta(info.fileName(), file_size_);
    socket_->write(meta);

    sending_ = true;                                            // 标记发送状态
    timeout_timer_->start();                                    // 启动超时监控
    send_timer_->start();                                       // 开始分块发送
}

void file_sender::cancel()
{
    sending_ = false;                                           // 取消发送状态
    send_timer_->stop();                                        // 停止发送定时器
    timeout_timer_->stop();                                     // 停止超时定时器

    if (file_)                                                  // 关闭文件
    {
        file_->close();
        file_ = nullptr;
    }

    if (socket_->state() != QAbstractSocket::UnconnectedState)
    {
        socket_->disconnectFromHost();                          // 断开 TCP
    }
}

void file_sender::on_connected()
{
    QByteArray handshake = protocol::build_handshake(device_name_, uuid_); // 构建握手包
    socket_->write(handshake);                                  // 立即发送
}

void file_sender::on_ready_read()
{
    while (socket_->canReadLine())                              // 逐行读取 (JSON 以 \n 结尾)
    {
        QByteArray data = socket_->readLine();
        QJsonObject json = protocol::parse_json(data.trimmed());

        if (json.isEmpty())                                     // 解析失败
        {
            continue;
        }

        QString type = json["type"].toString();

        if (type == "handshake_ack")                            // 握手确认
        {
            handshake_done_ = true;                             // 标记握手完成
            emit connected();                                   // 通知 UI 连接就绪

            if (!current_file_path_.isEmpty())                  // 有等待发送的文件
            {
                send_file(current_file_path_);                  // 自动开始发送
            }
        }
        else if (type == "ack")                                 // 接收确认
        {
            qint64 last_seq = json["last_seq"].toVariant().toLongLong();
            handle_ack(last_seq);
        }
        else if (type == "nack")                                // 重传请求
        {
            qint64 missing_seq = json["seq"].toVariant().toLongLong();
            handle_nack(missing_seq);
        }
        else if (type == "reject")                              // 接收方拒绝
        {
            sending_ = false;
            emit error(QStringLiteral("接收方拒绝: %1").arg(json["reason"].toString()));
            cancel();
        }
        else if (type == "ping")                                // 心跳请求
        {
            socket_->write(protocol::build_pong());             // 回复 Pong
        }
    }
}

void file_sender::send_next_chunk()
{
    if (!file_)                                                 // 文件未打开
    {
        send_timer_->stop();
        return;
    }

    // 清理已确认的块 (序号 <= last_acked_seq 的块可以释放)
    for (auto it = unacked_chunks_.begin(); it != unacked_chunks_.end();)
    {
        if (it.key() <= last_acked_seq_)
        {
            it = unacked_chunks_.erase(it);                     // 移除已确认块
        }
        else
        {
            ++it;
        }
    }

    if (unacked_chunks_.size() > 64)                            // 未确认块过多, 暂停发送 (滑动窗口)
    {
        return;
    }

    if (current_seq_ >= total_chunks_)                          // 所有块已发送
    {
        if (unacked_chunks_.isEmpty())                          // 所有块均已确认
        {
            send_timer_->stop();
            timeout_timer_->stop();
            sending_ = false;
            file_->close();
            emit finished();                                    // 通知 UI 发送完成
        }
        return;                                                 // 还有块未确认, 等待 ACK
    }

    // 计算当前块的实际大小 (最后一块可能不足 8KB)
    qint64 remaining = file_size_ - (current_seq_ * file_chunk_size);
    int chunk_len = static_cast<int>(qMin(static_cast<qint64>(file_chunk_size), remaining));

    QByteArray payload(chunk_len, '\0');                        // 预分配缓冲区
    qint64 read = file_->read(payload.data(), chunk_len);       // 读取文件数据
    if (read <= 0)
    {
        emit error("Read error");
        cancel();
        return;
    }
    payload.resize(static_cast<int>(read));                     // 调整为实际读取大小

    send_chunk(current_seq_, payload);                          // 发送该块
    current_seq_++;                                             // 序号递增
}

void file_sender::send_chunk(qint64 seq, const QByteArray &data)
{
    QByteArray chunk = protocol::build_chunk(seq, data);        // 构建二进制块
    socket_->write(chunk);                                      // 写入套接字
    unacked_chunks_[seq] = data;                                // 保存原始数据到未确认列表 (用于重传)
    timeout_timer_->start();                                    // 重置超时定时器
}

void file_sender::handle_ack(qint64 last_seq)
{
    if (last_seq > last_acked_seq_)                             // 只接受更大的确认序号
    {
        last_acked_seq_ = last_seq;                             // 更新已确认序号
        retry_count_ = 0;                                       // 重置重传计数
    }
}

void file_sender::handle_nack(qint64 missing_seq)
{
    if (retry_count_ >= max_retries)                            // 超过最大重传次数 (3 次)
    {
        emit error("Max retries exceeded");
        cancel();
        return;
    }

    if (unacked_chunks_.contains(missing_seq))                  // 该块在未确认列表中
    {
        QByteArray data = unacked_chunks_[missing_seq];         // 获取原始数据
        send_chunk(missing_seq, data);                          // 重传该块
        retry_count_++;                                         // 重传计数 +1
    }
}

void file_sender::check_timeout()
{
    if (unacked_chunks_.isEmpty())                              // 无未确认块, 不算超时
    {
        return;
    }

    emit error("Transfer timeout");                             // 通知 UI 超时
    cancel();
}

} // namespace core
} // namespace landrop
