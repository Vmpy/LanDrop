/**
 * @file receive_dialog.h
 * @brief 文件接收确认弹窗 — 非模态, 屏幕右下角显示, 支持接受/拒绝/超时自动拒绝
 * @author Mupengyi
 * @date 2026-05-02
 */
#pragma once

#include <QWidget>

class QLabel;          // 文本标签
class QPushButton;     // 按钮
class QTimer;          // 自动关闭定时器

namespace landrop
{
namespace ui
{

// 接收确认弹窗: 无边框, 置顶, 从屏幕右下角弹出
// 显示发送方名称、文件名、文件大小, 提供 "接收" / "拒绝" 按钮
// 30 秒不操作自动拒绝
class receive_dialog : public QWidget
{
    Q_OBJECT

public:
    explicit receive_dialog(QWidget *parent = nullptr);

    // 显示文件传输请求
    void show_proposal(const QString &sender_name,
                       const QString &file_name,
                       qint64 file_size);

signals:
    // 用户点击 "接收" (携带文件名用于保存对话框)
    void accepted(const QString &file_name);

    // 用户点击 "拒绝" 或超时自动关闭
    void rejected();

private slots:
    // 点击 "接收" 按钮
    void on_accept_clicked();

    // 点击 "拒绝" 按钮
    void on_reject_clicked();

private:
    // 格式化文件大小 (B / KB / MB / GB)
    QString format_size(qint64 bytes) const;

    QLabel *title_label_ = nullptr;           // 标题 "文件传输请求"
    QLabel *info_label_ = nullptr;            // 内容: 发件人/文件名/大小
    QPushButton *accept_button_ = nullptr;    // "接收" 按钮
    QPushButton *reject_button_ = nullptr;    // "拒绝" 按钮
    QTimer *auto_close_timer_ = nullptr;      // 30 秒自动关闭定时器

    QString pending_file_name_;               // 待接收的文件名 (用于信号传递)
};

} // namespace ui
} // namespace landrop
