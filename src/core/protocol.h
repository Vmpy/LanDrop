/**
 * @file protocol.h
 * @brief 协议包序列化与反序列化 — 提供所有 JSON 和二进制数据包的构造/解析方法
 * @author Mupengyi
 * @date 2026-05-02
 */
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace landrop
{
namespace core
{

// 文件元数据: 描述待传输文件的基本信息
struct file_meta_info
{
    QString file_name;  // 文件名 (不含路径)
    qint64 file_size = 0; // 文件大小 (字节)
};

// 协议工具类: 所有方法均为静态方法, 无状态, 不依赖 GUI
class protocol
{
public:
    // ====== JSON 控制包构造方法 ======

    // 构建设备在线广播包: type=announce, 携带设备名/UUID/端口/平台
    static QByteArray build_announce(const QString &device_name,
                                     const QString &uuid,
                                     quint16 tcp_port,
                                     const QString &platform);

    // 构建设备离线通知包: type=goodbye, 携带 UUID 用于识别
    static QByteArray build_goodbye(const QString &uuid);

    // 构建 TCP 握手包: 发送方连接成功后首先发送, 宣告自身身份
    static QByteArray build_handshake(const QString &device_name, const QString &uuid);

    // 构建握手确认包: 接收方验证通过后返回
    static QByteArray build_handshake_ack();

    // 构建心跳 Ping 包: 用于 TCP 长连接保活
    static QByteArray build_ping();

    // 构建心跳 Pong 包: 对 Ping 的响应
    static QByteArray build_pong();

    // 构建文件元数据包: 在传输数据块之前发送, 告知文件名/大小/块大小
    static QByteArray build_file_meta(const QString &file_name, qint64 file_size);

    // 构建 ACK 确认包: 携带最后连续接收的块序号
    static QByteArray build_ack(qint64 last_seq);

    // 构建 NACK 重传请求包: 携带缺失的块序号, 请求发送方重传
    static QByteArray build_nack(qint64 missing_seq);

    // 构建拒绝包: 携带拒绝原因 (用户拒绝/接收方忙碌/非法数据包)
    static QByteArray build_reject(const QString &reason);

    // 构建接受包: 接收方同意接收后发送, 发送方收到后才开始传输数据块
    static QByteArray build_accept();

    // ====== JSON 解析方法 ======

    // 将原始字节数组解析为 JSON 对象
    static QJsonObject parse_json(const QByteArray &data);

    // ====== 二进制数据块构造与解析 ======

    // 构建二进制数据块: 4B 序号(大端) + 4B 长度(大端) + 载荷 + 4B CRC32
    static QByteArray build_chunk(qint64 seq, const QByteArray &payload);

    // 二进制块解析结果
    struct chunk_info
    {
        qint64 seq = -1;          // 块序号, 从 0 开始
        qint64 len = 0;           // 有效载荷长度 (字节)
        QByteArray payload;       // 实际文件数据
        quint32 received_crc = 0; // 收到的 CRC32 校验值
        bool valid = false;       // CRC32 校验是否通过
    };

    // 从原始数据中解析一个二进制块, 返回 chunk_info
    static chunk_info parse_chunk(const QByteArray &data);

    // ====== CRC32 校验方法 ======

    // 计算数据的 CRC32 校验值 (IEEE 802.3 多项式)
    static quint32 calculate_crc32(const QByteArray &data);

    // 验证数据的 CRC32 校验值是否匹配
    static bool verify_crc32(const QByteArray &data, quint32 expected_crc);

private:
    // 将 JSON 对象序列化为紧凑格式的字节数组, 末尾附加换行符作为分隔
    static QByteArray build_json_packet(const QJsonObject &obj);

    // 二进制块头部固定大小: 4(seq) + 4(len) + 4(crc) = 12 字节
    static constexpr int chunk_header_size = 12;
};

} // namespace core
} // namespace landrop
