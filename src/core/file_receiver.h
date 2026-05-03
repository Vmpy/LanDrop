/**
 * @file file_receiver.h
 * @brief 文件接收器 — 处理单个传入文件的接收, 组包, CRC 校验, ACK/NACK
 * @author Mupengyi
 * @date 2026-05-02
 */
#pragma once

#include <QObject>
#include <QList>

class QTcpSocket; // TCP 套接字, 与发送方通信
class QFile;      // 文件对象, 用于写入接收到的数据
class QTimer;     // 定时器, 控制 ACK 间隔和超时检测

namespace landrop
{
namespace core
{

// 文件接收器: 接管一个已握手的 TCP 连接, 接收二进制数据块并写入文件
// 生命周期: 由 transfer_server 创建, 传输完成或出错后自动 deleteLater
class file_receiver : public QObject
{
    Q_OBJECT

public:
    explicit file_receiver(QTcpSocket *socket, qint64 file_size, QObject *parent = nullptr);
    ~file_receiver() override;

    // 用户接受传输后调用: 创建文件并开始写入数据
    void accept(const QString &save_path);

    // 用户拒绝传输: 发送拒绝包并断开连接
    void reject();

    // 中止传输: 删除已写入的文件, 断开连接
    void abort();

signals:
    // 接收进度 (已接收字节数, 总字节数)
    void progress(qint64 received, qint64 total);

    // 接收完成 (文件保存路径)
    void finished(const QString &file_path);

    // 接收出错 (错误原因描述)
    void error(const QString &reason);

private slots:
    // 读取套接字中的新数据并处理
    void read_data();

    // 发送 ACK/NACK 确认
    void send_ack();

    // 检查是否超时
    void check_timeout();

private:
    // 处理一个完整的二进制块
    void process_chunk(const QByteArray &chunk_data);

    // 请求重传指定序号的块 (追加到缺失列表)
    void request_nack(qint64 seq);

    QTcpSocket *socket_ = nullptr;          // TCP 连接套接字 (所有权归本对象)
    QFile *file_ = nullptr;                 // 目标文件 (用户接受后才创建)
    QTimer *ack_timer_ = nullptr;           // ACK 定时器 (每秒发送一次)
    QTimer *timeout_timer_ = nullptr;       // 超时定时器 (45 秒无数据则断连)

    QString file_name_;                     // 文件名 (仅记录)
    qint64 file_size_ = 0;                  // 文件总大小 (来自 file_meta)
    qint64 bytes_received_ = 0;             // 已接收字节数
    qint64 last_acked_seq_ = -1;            // 上次确认的最大连续序号
    qint64 expected_seq_ = 0;               // 期望的下一个块序号
    qint64 chunks_since_ack_ = 0;           // 自上次 ACK 以来收到的块数

    int retry_count_ = 0;                   // 当前重传计数

    QList<qint64> missing_seqs_;            // 缺失的块序号列表 (用于 NACK 请求)
    QByteArray buffer_;                     // 接收缓冲区 (拼接不完整的数据块)
    QByteArray pending_payloads_;           // accept 前缓存的已校验载荷, accept 后写入文件并清空
};

} // namespace core
} // namespace landrop
