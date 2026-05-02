/**
 * @file file_receiver.cpp
 * @brief 文件接收器实现 — 组包, CRC32 校验, ACK/NACK, 超时处理
 * @author Mupengyi
 * @date 2026-05-02
 */
#include "file_receiver.h"
#include "protocol.h"
#include "protocol_constants.h"

#include <QTcpSocket>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

namespace landrop
{
namespace core
{

file_receiver::file_receiver(QTcpSocket *socket, QObject *parent)
    : QObject(parent)
    , socket_(socket)
{
    socket_->setParent(this);                                   // 套接字所有权转移给本对象

    ack_timer_ = new QTimer(this);                              // 创建 ACK 定时器
    ack_timer_->setInterval(ack_timeout_ms);                    // 每秒触发
    connect(ack_timer_, &QTimer::timeout, this, &file_receiver::send_ack);

    timeout_timer_ = new QTimer(this);                          // 创建超时定时器
    timeout_timer_->setInterval(heartbeat_timeout_ms);          // 45 秒超时
    connect(timeout_timer_, &QTimer::timeout, this, &file_receiver::check_timeout);

    // 套接字有数据可读时触发
    connect(socket_, &QTcpSocket::readyRead, this, &file_receiver::read_data);
}

file_receiver::~file_receiver()
{
    abort();                                                    // 确保资源清理
}

void file_receiver::accept(const QString &save_path)
{
    if (file_)                                                  // 已创建文件, 防止重复 accept
    {
        return;
    }

    file_ = new QFile(save_path, this);                         // 创建目标文件
    if (!file_->open(QIODevice::WriteOnly))                     // 以只写模式打开
    {
        emit error(QStringLiteral("无法创建文件: %1").arg(save_path));
        socket_->write(protocol::build_reject("Cannot create file"));
        socket_->disconnectFromHost();                          // 断开连接
        return;
    }

    timeout_timer_->start();                                    // 启动超时监控
}

void file_receiver::reject()
{
    socket_->write(protocol::build_reject("User declined"));    // 发送拒绝包
    socket_->disconnectFromHost();                              // 断开连接
    deleteLater();                                              // 延迟删除本对象
}

void file_receiver::abort()
{
    if (file_)                                                  // 清理已写入的文件
    {
        QString path = file_->fileName();
        file_->close();                                         // 关闭文件句柄
        file_->remove();                                        // 删除部分写入的文件 (需求: FR-10)
    }

    if (socket_->state() == QAbstractSocket::ConnectedState)
    {
        socket_->disconnectFromHost();                          // 断开 TCP 连接
    }
}

void file_receiver::read_data()
{
    QByteArray data = socket_->readAll();                       // 读取所有可用数据
    buffer_.append(data);                                       // 追加到接收缓冲区

    constexpr int min_chunk_size = 12;                          // 块最小长度: 4 + 4 + 4

    // 循环解析缓冲区中的完整数据块
    while (buffer_.size() >= min_chunk_size)
    {
        QDataStream peek_stream(buffer_);                       // 创建只读流探查头部
        peek_stream.setByteOrder(QDataStream::BigEndian);

        qint32 seq32 = 0;
        qint32 len32 = 0;

        peek_stream >> seq32 >> len32;                          // 读取 4B 序号 + 4B 长度

        qint64 chunk_len = static_cast<qint64>(len32);
        qint64 total_needed = 12 + chunk_len;                   // 完整块所需总字节数

        if (chunk_len < 0 || chunk_len > file_chunk_size * 2)   // 长度异常 (负数或超过两倍块大小)
        {
            buffer_.clear();                                    // 清空整个缓冲区
            emit error("Invalid chunk size");                   // 报告非法块大小
            abort();                                            // 中止接收
            return;
        }

        if (buffer_.size() < total_needed)                      // 数据不完整, 等待更多数据
        {
            break;
        }

        QByteArray chunk_data = buffer_.left(static_cast<int>(total_needed)); // 取出一个完整块
        buffer_.remove(0, static_cast<int>(total_needed));      // 从缓冲区移除

        process_chunk(chunk_data);                              // 处理该块
    }
}

void file_receiver::process_chunk(const QByteArray &chunk_data)
{
    protocol::chunk_info info = protocol::parse_chunk(chunk_data); // 解析二进制块

    if (!info.valid)                                            // CRC32 校验未通过
    {
        request_nack(expected_seq_);                            // 请求重传当前期望的块
        return;
    }

    if (info.seq != expected_seq_)                              // 块序号不连续 (发生丢包或乱序)
    {
        for (qint64 s = expected_seq_; s < info.seq; ++s)       // 将中间缺失的序号全部记录
        {
            if (!missing_seqs_.contains(s))
            {
                missing_seqs_.append(s);
            }
        }
        expected_seq_ = info.seq;                               // 更新期望序号
    }

    if (file_)                                                  // 用户已接受, 写入文件
    {
        file_->write(info.payload);                             // 写入载荷数据
        file_->flush();                                         // 立即刷盘
    }

    bytes_received_ += info.len;                                // 累加已接收字节
    expected_seq_ = info.seq + 1;                               // 下一个期望序号
    chunks_since_ack_++;                                        // 自上次 ACK 计数 +1

    emit progress(bytes_received_, file_size_);                 // 通知 UI 进度更新

    if (chunks_since_ack_ >= ack_frequency)                     // 收到 64 个块后发送 ACK
    {
        send_ack();
    }

    timeout_timer_->start();                                    // 重置超时定时器 (每收到一个块就重置)

    if (bytes_received_ >= file_size_)                          // 接收完毕
    {
        send_ack();                                             // 发送最终 ACK
        ack_timer_->stop();                                     // 停止 ACK 定时器
        timeout_timer_->stop();                                 // 停止超时定时器

        if (file_)
        {
            QString path = file_->fileName();
            file_->close();                                     // 关闭文件
            emit finished(path);                                // 通知 UI 接收完成
        }
        socket_->disconnectFromHost();                          // 关闭 TCP 连接
        deleteLater();                                          // 自我销毁
    }
}

void file_receiver::send_ack()
{
    ack_timer_->stop();                                         // 停止定时器
    chunks_since_ack_ = 0;                                      // 重置计数

    if (!missing_seqs_.isEmpty())                               // 存在缺失的块
    {
        qint64 seq = missing_seqs_.takeFirst();                 // 取出第一个缺失序号
        socket_->write(protocol::build_nack(seq));              // 发送 NACK 请求重传
        retry_count_++;
        if (retry_count_ > max_retries)                         // 超过最大重传次数
        {
            emit error("Max retries exceeded");                 // 报告错误
            abort();                                            // 中止接收
        }
    }
    else
    {
        retry_count_ = 0;                                       // 重置重传计数
        socket_->write(protocol::build_ack(expected_seq_ - 1)); // 发送 ACK 确认
    }
}

void file_receiver::request_nack(qint64 seq)
{
    if (!missing_seqs_.contains(seq))                           // 避免重复记录
    {
        missing_seqs_.append(seq);
    }

    if (chunks_since_ack_ > 0)                                  // 有未确认的块则立即发送 NACK
    {
        send_ack();
    }
}

void file_receiver::check_timeout()
{
    emit error("Connection timeout");                           // 通知 UI 连接超时
    abort();                                                    // 中止接收并清理
}

} // namespace core
} // namespace landrop
