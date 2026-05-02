/**
 * @file file_sender.h
 * @brief 文件发送器 — 连接目标设备, 握手, 分块发送, 跟踪未确认块, 处理 NACK 重传
 * @author Mupengyi
 * @date 2026-05-02
 */
#pragma once

#include <QObject>
#include <QHash>
#include <QHostAddress>

class QTcpSocket; // TCP 套接字, 与接收方通信
class QFile;      // 文件对象, 用于读取要发送的文件
class QTimer;     // 定时器, 控制发送速率和超时检测

namespace landrop
{
namespace core
{

// 文件发送器: 主动连接目标设备, 执行握手, 分块发送文件, 处理 ACK/NACK
// 每个 file_sender 实例处理单个文件的一次发送
class file_sender : public QObject
{
    Q_OBJECT

public:
    explicit file_sender(QObject *parent = nullptr);
    ~file_sender() override;

    // 连接到目标设备 (IP + 端口)
    void connect_to_host(const QHostAddress &address, quint16 port,
                         const QString &device_name, const QString &uuid);

    // 开始发送文件 (握手完成后调用, 或连接前调用则在握手完成后自动发送)
    void send_file(const QString &file_path);

    // 取消发送: 关闭文件, 断开连接, 停止定时器
    void cancel();

signals:
    // TCP 连接建立 + 握手完成
    void connected();

    // 发送进度 (已发送字节数, 总字节数)
    void progress(qint64 sent, qint64 total);

    // 发送完成
    void finished();

    // 发送出错 (错误原因描述)
    void error(const QString &reason);

    // TCP 连接断开 (非主动取消)
    void disconnected();

private slots:
    // TCP 连接建立后立即发送握手包
    void on_connected();

    // 读取接收方发来的 ACK/NACK/Reject
    void on_ready_read();

    // 发送下一个数据块
    void send_next_chunk();

    // 检查发送超时
    void check_timeout();

private:
    // 发送单个二进制数据块
    void send_chunk(qint64 seq, const QByteArray &data);

    // 处理 ACK: 更新已确认的最大序号, 清理未确认块列表
    void handle_ack(qint64 last_seq);

    // 处理 NACK: 重传指定序号的块 (需检查重传次数限制)
    void handle_nack(qint64 missing_seq);

    QTcpSocket *socket_ = nullptr;              // TCP 连接套接字
    QFile *file_ = nullptr;                     // 源文件
    QTimer *send_timer_ = nullptr;              // 发送定时器 (尽快发送模式, 间隔 0ms)
    QTimer *timeout_timer_ = nullptr;           // 超时定时器 (45 秒)

    QString device_name_;                       // 本机设备名 (用于握手)
    QString uuid_;                              // 本机 UUID (用于握手)

    QString current_file_path_;                 // 待发送文件路径
    qint64 file_size_ = 0;                      // 文件总大小
    qint64 bytes_sent_ = 0;                     // 已发送字节数 (用于进度报告)
    qint64 current_seq_ = 0;                    // 当前发送序号
    qint64 last_acked_seq_ = -1;                // 接收方已确认的最大序号
    qint64 total_chunks_ = 0;                   // 总块数

    int retry_count_ = 0;                       // 当前重传计数

    QHash<qint64, QByteArray> unacked_chunks_;  // 未确认的块: key=序号, value=原始数据 (用于重传)
    bool handshake_done_ = false;               // 握手是否完成
    bool sending_ = false;                      // 是否正在发送
};

} // namespace core
} // namespace landrop
