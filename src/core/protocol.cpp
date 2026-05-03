/**
 * @file protocol.cpp
 * @brief 协议包序列化实现 — 所有 JSON 和二进制数据包的构造与解析逻辑
 * @author Mupengyi
 * @date 2026-05-02
 */
#include "protocol.h"

#include "protocol_constants.h"

#include <QJsonDocument>
#include <QDataStream>
#include <QIODevice>

namespace landrop
{
namespace core
{

// 将 JSON 对象序列化为紧凑单行字节数组, 末尾追加 \n 作为 TCP 行分隔符
QByteArray protocol::build_json_packet(const QJsonObject &obj)
{
    QJsonDocument doc(obj);                     // 从 QJsonObject 创建 JSON 文档
    return doc.toJson(QJsonDocument::Compact) + "\n"; // 紧凑格式输出, 加换行符
}

// 构建设备在线广播包 (UDP 组播)
QByteArray protocol::build_announce(const QString &device_name,
                                     const QString &uuid,
                                     quint16 tcp_port,
                                     const QString &platform)
{
    QJsonObject obj;
    obj["type"] = "announce";                   // 报文类型: 在线声明
    obj["version"] = protocol_version;          // 协议版本号
    obj["device_name"] = device_name;           // 设备显示名
    obj["uuid"] = uuid;                         // 设备唯一标识
    obj["tcp_port"] = static_cast<int>(tcp_port); // TCP 文件接收端口
    obj["platform"] = platform;                 // 操作系统平台
    return build_json_packet(obj);              // 序列化为字节数组
}

// 构建设备离线通知包 (应用关闭时广播)
QByteArray protocol::build_goodbye(const QString &uuid)
{
    QJsonObject obj;
    obj["type"] = "goodbye";                    // 报文类型: 离线通知
    obj["uuid"] = uuid;                         // 设备唯一标识, 用于接收方移除设备
    return build_json_packet(obj);
}

// 构建 TCP 握手包 (发送方连接成功后立即发送)
QByteArray protocol::build_handshake(const QString &device_name, const QString &uuid)
{
    QJsonObject obj;
    obj["type"] = "handshake";                  // 报文类型: 连接握手
    obj["device_name"] = device_name;           // 发送方设备名
    obj["uuid"] = uuid;                         // 发送方 UUID
    return build_json_packet(obj);
}

// 构建握手确认包 (接收方验证握手后返回)
QByteArray protocol::build_handshake_ack()
{
    QJsonObject obj;
    obj["type"] = "handshake_ack";              // 报文类型: 握手确认
    return build_json_packet(obj);
}

// 构建心跳 Ping 包 (每 15 秒发送一次)
QByteArray protocol::build_ping()
{
    QJsonObject obj;
    obj["type"] = "ping";                       // 报文类型: 心跳请求
    return build_json_packet(obj);
}

// 构建心跳 Pong 包 (对 Ping 的响应)
QByteArray protocol::build_pong()
{
    QJsonObject obj;
    obj["type"] = "pong";                       // 报文类型: 心跳响应
    return build_json_packet(obj);
}

// 构建文件元数据包 (传输数据块前发送)
QByteArray protocol::build_file_meta(const QString &file_name, qint64 file_size)
{
    QJsonObject obj;
    obj["type"] = "file_meta";                  // 报文类型: 文件元数据
    obj["file_name"] = file_name;               // 文件名
    obj["file_size"] = file_size;               // 文件大小 (字节)
    obj["chunk_size"] = file_chunk_size;        // 块大小 (8192 字节)
    return build_json_packet(obj);
}

// 构建 ACK 确认包 (接收方确认已连续收到指定序号及之前的所有块)
QByteArray protocol::build_ack(qint64 last_seq)
{
    QJsonObject obj;
    obj["type"] = "ack";                        // 报文类型: 接收确认
    obj["last_seq"] = last_seq;                 // 最后连续接收的块序号
    return build_json_packet(obj);
}

// 构建 NACK 重传请求包 (请求发送方重传指定序号的块)
QByteArray protocol::build_nack(qint64 missing_seq)
{
    QJsonObject obj;
    obj["type"] = "nack";                       // 报文类型: 重传请求
    obj["seq"] = missing_seq;                   // 缺失的块序号
    return build_json_packet(obj);
}

// 构建拒绝包 (接收方拒绝传输)
QByteArray protocol::build_reject(const QString &reason)
{
    QJsonObject obj;
    obj["type"] = "reject";                     // 报文类型: 拒绝
    obj["reason"] = reason;                     // 拒绝原因描述
    return build_json_packet(obj);
}

// 构建接受包 (接收方同意接收, 发送方收到后开始传输数据块)
QByteArray protocol::build_accept()
{
    QJsonObject obj;
    obj["type"] = "accept";                     // 报文类型: 接受
    return build_json_packet(obj);
}

// 将原始字节数组解析为 QJsonObject
QJsonObject protocol::parse_json(const QByteArray &data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data); // 从字节数组解析 JSON
    return doc.object();                               // 返回 QJsonObject
}

// 构建二进制数据块: 4B seq + 4B len + payload + 4B crc32 (全部大端字节序)
QByteArray protocol::build_chunk(qint64 seq, const QByteArray &payload)
{
    QByteArray chunk;                                   // 输出缓冲区
    QDataStream stream(&chunk, QIODevice::WriteOnly);   // 写入模式的数据流
    stream.setByteOrder(QDataStream::BigEndian);        // 设置大端字节序

    qint32 seq32 = static_cast<qint32>(seq);            // 序号转为 32 位
    qint32 len32 = static_cast<qint32>(payload.size()); // 长度转为 32 位

    stream << seq32 << len32;                           // 写入 4B 序号 + 4B 长度
    stream.writeRawData(payload.constData(), payload.size()); // 写入原始载荷数据

    quint32 crc = calculate_crc32(payload);             // 计算载荷的 CRC32
    stream << crc;                                      // 写入 4B CRC32 校验和

    return chunk;                                       // 返回完整的二进制块
}

// 从原始数据中解析一个二进制块
protocol::chunk_info protocol::parse_chunk(const QByteArray &data)
{
    chunk_info info;

    // 数据长度不足 12 字节 (4+4+4), 无法解析
    if (data.size() < chunk_header_size)
    {
        return info;                                    // valid 默认为 false
    }

    QDataStream stream(data);                           // 只读模式的数据流
    stream.setByteOrder(QDataStream::BigEndian);        // 设置大端字节序

    qint32 seq32 = 0;
    qint32 len32 = 0;

    stream >> seq32 >> len32;                           // 读取 4B 序号 + 4B 长度

    info.seq = static_cast<qint64>(seq32);              // 序号转为 64 位
    info.len = static_cast<qint64>(len32);              // 长度转为 64 位

    // 数据长度不足以包含完整的载荷
    if (data.size() < chunk_header_size + info.len)
    {
        return info;
    }

    info.payload = data.mid(8, static_cast<int>(info.len)); // 提取载荷数据 (从第 9 字节开始)
    stream.device()->seek(8 + info.len);                    // 跳过 seq+len+payload, 定位到 CRC 字段

    stream >> info.received_crc;                            // 读取收到的 4B CRC32

    quint32 computed_crc = calculate_crc32(info.payload);   // 重新计算载荷的 CRC32
    info.valid = (computed_crc == info.received_crc);       // 比对校验值, 判定数据完整性

    return info;
}

// 计算 CRC32 校验值 (标准 IEEE 802.3 多项式)
quint32 protocol::calculate_crc32(const QByteArray &data)
{
    constexpr quint32 polynomial = 0xEDB88320;              // 反射多项式
    quint32 crc = 0xFFFFFFFF;                               // 初始值全 1

    for (int i = 0; i < data.size(); ++i)                   // 逐字节处理
    {
        crc ^= static_cast<quint8>(data[i]);                // 当前字节异或进入 CRC
        for (int j = 0; j < 8; ++j)                         // 逐位处理
        {
            if (crc & 1)                                    // 最低位为 1
            {
                crc = (crc >> 1) ^ polynomial;              // 右移并异或多项式
            }
            else
            {
                crc >>= 1;                                  // 直接右移
            }
        }
    }

    return crc ^ 0xFFFFFFFF;                                // 最终异或全 1, 返回结果
}

// 验证 CRC32 校验值是否匹配
bool protocol::verify_crc32(const QByteArray &data, quint32 expected_crc)
{
    return calculate_crc32(data) == expected_crc;           // 重新计算并比对
}

} // namespace core
} // namespace landrop
