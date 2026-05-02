/**
 * @file main_window.cpp
 * @brief 主窗口实现 — UI 布局, 信号槽连接, 系统托盘, 拖拽文件, 设置对话框
 * @author Mupengyi
 * @date 2026-05-02
 */
#include "main_window.h"
#include "device_list_widget.h"
#include "receive_dialog.h"

#include "core/discovery_service.h"
#include "core/transfer_server.h"
#include "core/file_sender.h"
#include "core/device_info.h"
#include "core/protocol_constants.h"

#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QCloseEvent>
#include <QMenu>
#include <QSettings>
#include <QInputDialog>
#include <QLineEdit>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QStandardPaths>
#include <QApplication>
#include <QElapsedTimer>
#include <QStyle>

namespace landrop
{
namespace ui
{

main_window::main_window(QWidget *parent)
    : QMainWindow(parent)
{
    setup_ui();             // 构建界面布局
    setup_tray();           // 初始化系统托盘
    setup_services();       // 初始化核心服务
}

main_window::~main_window()
{
    discovery_->stop();         // 发送 goodbye, 停止组播
    transfer_server_->stop();   // 关闭所有连接和监听
}

void main_window::setup_ui()
{
    setWindowTitle("LanDrop");                              // 窗口标题
    setMinimumSize(700, 500);                               // 最小尺寸
    setAcceptDrops(true);                                   // 启用拖拽文件

    auto *central = new QWidget(this);                      // 中心控件容器
    setCentralWidget(central);

    auto *main_layout = new QHBoxLayout(central);           // 水平主布局
    main_layout->setContentsMargins(12, 12, 12, 12);        // 外边距

    // ======== 左侧面板 ========
    auto *left_panel = new QWidget(central);
    left_panel->setFixedWidth(240);                         // 固定宽度
    auto *left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(0, 0, 8, 0);

    // 本机信息卡片 (设备名 + IP 地址)
    self_info_label_ = new QLabel(left_panel);
    self_info_label_->setAlignment(Qt::AlignCenter);        // 居中对齐
    self_info_label_->setMinimumHeight(60);
    self_info_label_->setStyleSheet(
        "QLabel { background-color: #e3f2fd; border-radius: 8px; padding: 12px; }");
    left_layout->addWidget(self_info_label_);

    // 在线设备列表
    device_list_ = new device_list_widget(left_panel);
    left_layout->addWidget(device_list_, 1);                // stretch=1 使其填充剩余空间

    // 设置按钮
    settings_button_ = new QPushButton(QStringLiteral("设置"), left_panel);
    connect(settings_button_, &QPushButton::clicked,
            this, &main_window::on_settings_clicked);
    left_layout->addWidget(settings_button_);

    // ======== 右侧主区域 ========
    send_panel_ = new QWidget(central);
    auto *right_layout = new QVBoxLayout(send_panel_);

    // 已选设备名称提示 (未选择时显示引导文字)
    selected_device_label_ = new QLabel(
        QStringLiteral("选择一个设备开始传输"), send_panel_);
    selected_device_label_->setAlignment(Qt::AlignCenter);
    QFont font = selected_device_label_->font();
    font.setPointSize(14);                                  // 较大字号
    selected_device_label_->setFont(font);
    right_layout->addWidget(selected_device_label_);

    // 状态文字标签 (连接中/传输中/完成/失败)
    status_label_ = new QLabel(send_panel_);
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setStyleSheet("QLabel { color: #666; }"); // 灰色文字
    right_layout->addWidget(status_label_);

    // 传输进度条 (默认隐藏, 传输时显示)
    progress_bar_ = new QProgressBar(send_panel_);
    progress_bar_->setVisible(false);
    right_layout->addWidget(progress_bar_);

    // 传输速度标签 (默认隐藏)
    speed_label_ = new QLabel(send_panel_);
    speed_label_->setAlignment(Qt::AlignCenter);
    speed_label_->setVisible(false);
    right_layout->addWidget(speed_label_);

    // "发送文件" 按钮 (选中设备后才可用)
    send_button_ = new QPushButton(QStringLiteral("发送文件"), send_panel_);
    send_button_->setEnabled(false);                        // 初始禁用
    send_button_->setMinimumHeight(48);
    send_button_->setStyleSheet(
        "QPushButton { background-color: #1976D2; color: white; font-size: 14px; "
        "border-radius: 6px; padding: 12px 32px; }"
        "QPushButton:disabled { background-color: #ccc; }");
    connect(send_button_, &QPushButton::clicked,
            this, &main_window::on_send_clicked);
    right_layout->addWidget(send_button_);

    right_layout->addStretch();                             // 底部弹簧, 将控件推到顶部

    main_layout->addWidget(left_panel);                     // 左侧面板
    main_layout->addWidget(send_panel_, 1);                  // 右侧区域 (stretch=1)
}

void main_window::setup_tray()
{
    tray_icon_ = new QSystemTrayIcon(this);
    tray_icon_->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon)); // 使用系统默认图标

    auto *tray_menu = new QMenu(this);
    QAction *show_action = tray_menu->addAction(QStringLiteral("打开主窗口"));
    connect(show_action, &QAction::triggered, this, [this]()
    {
        show();             // 显示窗口
        raise();            // 提升到最前
        activateWindow();   // 激活窗口获得焦点
    });

    QAction *quit_action = tray_menu->addAction(QStringLiteral("退出"));
    connect(quit_action, &QAction::triggered, qApp, &QApplication::quit);

    tray_icon_->setContextMenu(tray_menu);                  // 绑定右键菜单
    tray_icon_->setToolTip("LanDrop");

    connect(tray_icon_, &QSystemTrayIcon::activated,
            this, &main_window::on_tray_activated);

    tray_icon_->show();                                     // 显示托盘图标
}

void main_window::setup_services()
{
    // ====== 设备发现服务 ======
    discovery_ = new core::discovery_service(this);

    connect(discovery_, &core::discovery_service::device_discovered,
            this, &main_window::on_device_discovered);
    connect(discovery_, &core::discovery_service::device_offline,
            this, &main_window::on_device_offline);

    // ====== TCP 传输服务 ======
    transfer_server_ = new core::transfer_server(this);

    // 监听端口就绪后更新 discovery_service 的端口信息
    connect(transfer_server_, &core::transfer_server::listen_port_changed,
            this, [this](quint16 port)
    {
        discovery_->update_tcp_port(port);
    });

    connect(transfer_server_, &core::transfer_server::file_proposal,
            this, &main_window::on_file_proposal);
    connect(transfer_server_, &core::transfer_server::receive_progress,
            this, &main_window::on_receive_progress);
    connect(transfer_server_, &core::transfer_server::receive_finished,
            this, &main_window::on_receive_finished);
    connect(transfer_server_, &core::transfer_server::receive_error,
            this, &main_window::on_receive_error);

    // ====== 接收弹窗 (非模态, 无父对象以便独立定位) ======
    receive_dialog_ = new receive_dialog(nullptr);

    connect(receive_dialog_, &receive_dialog::accepted,
            this, &main_window::on_receive_accepted);
    connect(receive_dialog_, &receive_dialog::rejected,
            this, &main_window::on_receive_rejected);

    // 连接设备列表选择信号
    connect(device_list_, &device_list_widget::device_selected,
            this, &main_window::on_device_selected);

    // ====== 启动服务 ======
    if (!transfer_server_->start())                         // 先启动 TCP 服务 (获取端口)
    {
        status_label_->setText(QStringLiteral("无法启动传输服务"));
        return;
    }

    discovery_->start();                                    // 启动设备发现

    // 获取本机 IPv4 地址用于显示
    QString local_ip;
    QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress &addr : addresses)
    {
        if (addr != QHostAddress::LocalHost && addr.protocol() == QAbstractSocket::IPv4Protocol)
        {
            local_ip = addr.toString();
            break;
        }
    }

    self_info_label_->setText(QStringLiteral("我的设备\n%1\n%2")
                                .arg(discovery_->device_name(), local_ip));
}

void main_window::on_device_selected(const landrop::core::device_info &info)
{
    selected_device_ = info;                                // 保存选中设备信息
    has_selected_device_ = true;

    selected_device_label_->setText(
        QStringLiteral("已选择: %1").arg(info.device_name));
    send_button_->setEnabled(true);                         // 启用发送按钮
    status_label_->setText(QStringLiteral("IP: %1  |  端口: %2")
                                .arg(info.ip_address.toString())
                                .arg(info.tcp_port));
}

void main_window::on_device_discovered(const landrop::core::device_info &info)
{
    device_list_->add_device(info);                         // 添加到设备列表控件
}

void main_window::on_device_offline(const QString &uuid)
{
    device_list_->remove_device(uuid);                      // 从设备列表移除

    // 如果离线的是当前选中的设备, 清除选择状态
    if (has_selected_device_ && selected_device_.uuid == uuid)
    {
        has_selected_device_ = false;
        selected_device_label_->setText(QStringLiteral("选择一个设备开始传输"));
        send_button_->setEnabled(false);
        status_label_->setText(QStringLiteral("设备已离线"));
    }
}

void main_window::on_file_proposal(const QString &sender_name,
                                    const QString &file_name,
                                    qint64 file_size)
{
    receive_dialog_->show_proposal(sender_name, file_name, file_size); // 显示接收弹窗

    // 系统托盘气泡通知 (即使窗口最小化也能看到)
    if (tray_icon_)
    {
        tray_icon_->showMessage(
            "LanDrop",
            QStringLiteral("%1 想发送 %2 (%3)")
                .arg(sender_name, file_name, format_size(file_size)),
            QSystemTrayIcon::Information,
            5000);
    }
}

void main_window::on_receive_accepted(const QString &file_name)
{
    // 获取默认下载路径
    QString default_dir = QSettings().value("download_path",
                                            QStandardPaths::writableLocation(
                                                QStandardPaths::DownloadLocation)).toString();

    // 弹出 "另存为" 对话框
    QString save_path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存文件"),
        default_dir + "/" + file_name);

    if (save_path.isEmpty())                                // 用户取消了保存
    {
        transfer_server_->reject_reception();
        return;
    }

    status_label_->setText(QStringLiteral("正在接收: %1").arg(file_name));
    progress_bar_->setVisible(true);                        // 显示进度条
    progress_bar_->setValue(0);                             // 归零
    speed_label_->setVisible(false);

    transfer_server_->accept_reception(save_path);          // 开始接收
}

void main_window::on_receive_rejected()
{
    transfer_server_->reject_reception();                   // 通知服务端拒绝
}

void main_window::on_receive_progress(qint64 received, qint64 total)
{
    if (total > 0)
    {
        progress_bar_->setRange(0, static_cast<int>(total)); // 设置进度条范围
        progress_bar_->setValue(static_cast<int>(received));  // 更新当前进度
    }
    status_label_->setText(QStringLiteral("接收中: %1 / %2")
                                .arg(format_size(received), format_size(total)));
}

void main_window::on_receive_finished(const QString &file_path)
{
    status_label_->setText(QStringLiteral("接收完成: %1").arg(file_path));
    progress_bar_->setVisible(false);                       // 隐藏进度条
    speed_label_->setVisible(false);

    if (tray_icon_)                                         // 托盘通知
    {
        tray_icon_->showMessage("LanDrop", QStringLiteral("文件接收完成"),
                                QSystemTrayIcon::Information, 3000);
    }
}

void main_window::on_receive_error(const QString &reason)
{
    status_label_->setText(QStringLiteral("接收失败: %1").arg(reason));
    progress_bar_->setVisible(false);
    speed_label_->setVisible(false);
}

void main_window::on_send_progress(qint64 sent, qint64 total)
{
    if (total > 0)
    {
        progress_bar_->setRange(0, static_cast<int>(total));
        progress_bar_->setValue(static_cast<int>(sent));
    }
    status_label_->setText(QStringLiteral("发送中: %1 / %2")
                                .arg(format_size(sent), format_size(total)));
}

void main_window::on_send_finished()
{
    status_label_->setText(QStringLiteral("发送完成"));
    progress_bar_->setVisible(false);
    speed_label_->setVisible(false);
    send_button_->setEnabled(has_selected_device_);           // 恢复按钮状态

    disconnect(current_sender_, nullptr, this, nullptr);      // 断开所有信号连接
    current_sender_->deleteLater();                           // 延迟删除发送器
    current_sender_ = nullptr;

    if (tray_icon_)
    {
        tray_icon_->showMessage("LanDrop", QStringLiteral("文件发送完成"),
                                QSystemTrayIcon::Information, 3000);
    }
}

void main_window::on_send_error(const QString &reason)
{
    status_label_->setText(QStringLiteral("发送失败: %1").arg(reason));
    progress_bar_->setVisible(false);
    speed_label_->setVisible(false);
    send_button_->setEnabled(has_selected_device_);

    disconnect(current_sender_, nullptr, this, nullptr);
    current_sender_->deleteLater();
    current_sender_ = nullptr;
}

void main_window::on_send_clicked()
{
    if (!has_selected_device_)                              // 未选择设备
    {
        return;
    }

    QString file_path = QFileDialog::getOpenFileName(this, QStringLiteral("选择文件"));
    if (file_path.isEmpty())                                // 用户取消
    {
        return;
    }

    start_send_file(file_path);                             // 开始发送
}

void main_window::start_send_file(const QString &file_path)
{
    // 取消之前的发送 (如果存在)
    if (current_sender_)
    {
        current_sender_->cancel();
        current_sender_->deleteLater();
        current_sender_ = nullptr;
    }

    QFileInfo info(file_path);

    current_sender_ = new core::file_sender(this);          // 创建新的发送器

    // 握手完成后自动开始发送文件
    connect(current_sender_, &core::file_sender::connected, this, [this, file_path]()
    {
        current_sender_->send_file(file_path);
    });

    connect(current_sender_, &core::file_sender::progress,
            this, &main_window::on_send_progress);
    connect(current_sender_, &core::file_sender::finished,
            this, &main_window::on_send_finished);
    connect(current_sender_, &core::file_sender::error,
            this, &main_window::on_send_error);

    status_label_->setText(QStringLiteral("正在连接 %1...").arg(selected_device_.device_name));
    progress_bar_->setVisible(true);
    progress_bar_->setValue(0);
    speed_label_->setVisible(false);
    send_button_->setEnabled(false);                        // 发送中禁用按钮

    current_sender_->connect_to_host(selected_device_.ip_address,
                                      selected_device_.tcp_port,
                                      discovery_->device_name(),
                                      discovery_->uuid());
}

void main_window::on_settings_clicked()
{
    // 从 QSettings 读取当前值
    QString current_name = QSettings().value("device_name",
                                             QHostInfo::localHostName()).toString();
    QString current_path = QSettings().value("download_path",
                                             QStandardPaths::writableLocation(
                                                 QStandardPaths::DownloadLocation)).toString();

    // 创建设置对话框
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("设置"));
    dialog.setFixedSize(460, 180);                          // 固定对话框大小

    auto *layout = new QVBoxLayout(&dialog);

    // ---- 设备名称行 ----
    auto *name_layout = new QHBoxLayout();
    auto *name_label = new QLabel(QStringLiteral("设备显示名:"));
    auto *name_edit = new QLineEdit(current_name);
    name_layout->addWidget(name_label);
    name_layout->addWidget(name_edit, 1);                   // stretch=1
    layout->addLayout(name_layout);

    // ---- 下载路径行 ----
    auto *path_layout = new QHBoxLayout();
    auto *path_label = new QLabel(QStringLiteral("默认下载目录:"));
    auto *path_display = new QLineEdit(current_path);
    path_display->setReadOnly(true);                        // 只读: 不能手动输入
    auto *path_button = new QPushButton(QStringLiteral("设置默认下载目录"));
    path_layout->addWidget(path_label);
    path_layout->addWidget(path_display, 1);                // stretch=1
    path_layout->addWidget(path_button);
    layout->addLayout(path_layout);

    QString selected_path = current_path;

    // 点击 "设置默认下载目录" 按钮 → 弹出目录选择对话框
    connect(path_button, &QPushButton::clicked, &dialog, [&]()
    {
        QString dir = QFileDialog::getExistingDirectory(&dialog,
            QStringLiteral("选择默认下载目录"), selected_path);
        if (!dir.isEmpty())
        {
            selected_path = dir;
            path_display->setText(dir);                     // 更新显示
        }
    });

    // ---- 确定/取消按钮 ----
    auto *button_layout = new QHBoxLayout();
    button_layout->addStretch();
    auto *ok_button = new QPushButton(QStringLiteral("确定"));
    auto *cancel_button = new QPushButton(QStringLiteral("取消"));
    button_layout->addWidget(ok_button);
    button_layout->addWidget(cancel_button);
    layout->addLayout(button_layout);

    connect(ok_button, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancel_button, &QPushButton::clicked, &dialog, &QDialog::reject);

    // 显示对话框, 等待用户操作
    if (dialog.exec() == QDialog::Accepted)
    {
        QString new_name = name_edit->text().trimmed();
        if (!new_name.isEmpty() && new_name != current_name)  // 设备名有变化
        {
            QSettings().setValue("device_name", new_name);
            discovery_->update_device_name(new_name);         // 更新并重新广播
            self_info_label_->setText(QStringLiteral("我的设备\n%1").arg(new_name));
        }

        if (selected_path != current_path)                    // 下载路径有变化
        {
            QSettings().setValue("download_path", selected_path);
        }
    }
}

void main_window::on_tray_activated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)               // 双击托盘图标
    {
        show();
        raise();
        activateWindow();                                     // 显示并激活主窗口
    }
}

void main_window::closeEvent(QCloseEvent *event)
{
    // 点击关闭按钮 → 最小化到系统托盘 (不退出)
    if (tray_icon_ && tray_icon_->isVisible())
    {
        hide();                                               // 隐藏窗口
        tray_icon_->showMessage(
            "LanDrop",
            QStringLiteral("LanDrop 已最小化到系统托盘"),
            QSystemTrayIcon::Information,
            2000);
        event->ignore();                                      // 阻止关闭
    }
}

void main_window::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())                         // 拖入的是文件/URL
    {
        event->acceptProposedAction();                        // 接受拖放
    }
}

void main_window::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();             // 获取拖入的 URL 列表
    if (urls.isEmpty())
    {
        return;
    }

    QString file_path = urls.first().toLocalFile();           // 取第一个文件路径
    if (file_path.isEmpty())
    {
        return;
    }

    if (!has_selected_device_)                                // 未选择目标设备
    {
        status_label_->setText(QStringLiteral("请先选择目标设备"));
        return;
    }

    start_send_file(file_path);                               // 直接开始发送
}

QString main_window::format_size(qint64 bytes) const
{
    if (bytes < 1024)
    {
        return QStringLiteral("%1 B").arg(bytes);
    }
    else if (bytes < 1024 * 1024)
    {
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1); // 保留 1 位小数
    }
    else if (bytes < 1024LL * 1024 * 1024)
    {
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    }
    else
    {
        return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2); // 保留 2 位小数
    }
}

QString main_window::format_speed(qint64 bytes_per_sec) const
{
    return QStringLiteral("%1/s").arg(format_size(bytes_per_sec));
}

} // namespace ui
} // namespace landrop
