/**
 * @file receive_dialog.cpp
 * @brief 文件接收确认弹窗实现 — 布局/定位/样式/按钮逻辑/超时处理
 * @author Mupengyi
 * @date 2026-05-02
 */
#include "receive_dialog.h"
#include "core/protocol_constants.h"

#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QScreen>

using landrop::core::receive_dialog_timeout_ms;             // 30 秒超时常量

namespace landrop
{
namespace ui
{

receive_dialog::receive_dialog(QWidget *parent)
    : QWidget(parent, Qt::WindowStaysOnTopHint               // 始终置顶
                    | Qt::FramelessWindowHint                // 无边框
                    | Qt::Tool)                              // 不在任务栏显示
{
    setFixedSize(360, 180);                                 // 固定弹窗尺寸

    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(16, 16, 16, 16);
    main_layout->setSpacing(12);

    // 标题
    title_label_ = new QLabel(QStringLiteral("文件传输请求"), this);
    QFont title_font = title_label_->font();
    title_font.setBold(true);                               // 加粗
    title_font.setPointSize(12);                            // 较大字号
    title_label_->setFont(title_font);
    main_layout->addWidget(title_label_);

    // 信息内容 (发送方/文件名/大小)
    info_label_ = new QLabel(this);
    info_label_->setWordWrap(true);                         // 自动换行
    main_layout->addWidget(info_label_);

    // 按钮行
    auto *button_layout = new QHBoxLayout();
    button_layout->addStretch();                            // 左弹簧

    accept_button_ = new QPushButton(QStringLiteral("接收"), this);
    accept_button_->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; " // 绿色按钮
        "padding: 8px 24px; border-radius: 4px; }");
    button_layout->addWidget(accept_button_);

    reject_button_ = new QPushButton(QStringLiteral("拒绝"), this);
    reject_button_->setStyleSheet(
        "QPushButton { background-color: #ccc; color: #333; "   // 灰色按钮
        "padding: 8px 24px; border-radius: 4px; }");
    button_layout->addWidget(reject_button_);

    main_layout->addLayout(button_layout);

    // 按钮点击连接
    connect(accept_button_, &QPushButton::clicked,
            this, &receive_dialog::on_accept_clicked);
    connect(reject_button_, &QPushButton::clicked,
            this, &receive_dialog::on_reject_clicked);

    // 30 秒自动关闭定时器
    auto_close_timer_ = new QTimer(this);
    auto_close_timer_->setInterval(receive_dialog_timeout_ms);
    auto_close_timer_->setSingleShot(true);                 // 单次触发
    connect(auto_close_timer_, &QTimer::timeout, this, [this]()
    {
        emit rejected();                                    // 超时视为拒绝
        hide();
    });

    // 整体弹窗样式
    setStyleSheet("QWidget { background-color: #ffffff; border: 1px solid #ddd; "
                  "border-radius: 8px; }");
}

void receive_dialog::show_proposal(const QString &sender_name,
                                    const QString &file_name,
                                    qint64 file_size)
{
    pending_file_name_ = file_name;                         // 保存文件名
    info_label_->setText(QStringLiteral("%1 想发送\n%2 (%3) 给你。")
                            .arg(sender_name, file_name, format_size(file_size)));

    // 定位到屏幕右下角 (距离边缘 20px)
    QScreen *screen = QApplication::primaryScreen();
    if (screen)
    {
        QRect screen_geom = screen->availableGeometry();    // 可用区域 (不含任务栏)
        move(screen_geom.right() - width() - 20,            // 右对齐
             screen_geom.bottom() - height() - 20);         // 底对齐
    }

    show();                                                 // 显示弹窗
    raise();                                                // 提升到最前
    activateWindow();                                       // 激活获得焦点

    auto_close_timer_->start();                             // 启动 30 秒倒计时
}

QString receive_dialog::format_size(qint64 bytes) const
{
    if (bytes < 1024)
    {
        return QStringLiteral("%1 B").arg(bytes);           // < 1KB 显示字节
    }
    else if (bytes < 1024 * 1024)
    {
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1); // KB
    }
    else if (bytes < 1024LL * 1024 * 1024)
    {
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1); // MB
    }
    else
    {
        return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2); // GB
    }
}

void receive_dialog::on_accept_clicked()
{
    auto_close_timer_->stop();                              // 停止超时定时器
    hide();                                                 // 隐藏弹窗
    emit accepted(pending_file_name_);                      // 通知主窗口: 用户接受
}

void receive_dialog::on_reject_clicked()
{
    auto_close_timer_->stop();                              // 停止超时定时器
    hide();                                                 // 隐藏弹窗
    emit rejected();                                        // 通知主窗口: 用户拒绝
}

} // namespace ui
} // namespace landrop
