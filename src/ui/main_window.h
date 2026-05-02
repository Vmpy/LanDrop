/**
 * @file main_window.h
 * @brief 主窗口 — 整合设备列表、发送面板、系统托盘、发现服务和传输服务
 * @author Mupengyi
 * @date 2026-05-02
 */
#pragma once

#include "core/device_info.h"  // 需要 device_info 的完整定义 (成员变量使用)

#include <QMainWindow>
#include <QSystemTrayIcon>

class QLabel;          // 文本标签
class QPushButton;     // 按钮
class QProgressBar;    // 进度条
class QLineEdit;       // 单行文本输入
class QTextEdit;       // 多行文本显示

namespace landrop
{
namespace core
{
class discovery_service;   // 设备发现服务 (UDP 组播)
class transfer_server;     // TCP 传输服务器
class file_sender;         // 文件发送器
} // namespace core

namespace ui
{
class device_list_widget;  // 在线设备列表控件
class receive_dialog;      // 文件接收确认弹窗

// 主窗口: MVC 架构中的 View 层, 只负责展示和交互, 不含业务逻辑
class main_window : public QMainWindow
{
    Q_OBJECT

public:
    explicit main_window(QWidget *parent = nullptr);
    ~main_window() override;

protected:
    // 关闭窗口 → 最小化到系统托盘而非退出
    void closeEvent(QCloseEvent *event) override;

    // 拖拽文件进入窗口 → 接受拖放
    void dragEnterEvent(QDragEnterEvent *event) override;

    // 放下文件 → 如果已选择目标设备则开始发送
    void dropEvent(QDropEvent *event) override;

private slots:
    // ====== 设备列表交互 ======

    // 用户点击了设备列表中的设备
    void on_device_selected(const landrop::core::device_info &info);

    // 发现新设备 (来自 discovery_service)
    void on_device_discovered(const landrop::core::device_info &info);

    // 设备离线 (来自 discovery_service)
    void on_device_offline(const QString &uuid);

    // ====== 文件接收相关 ======

    // 收到文件传输请求 (来自 transfer_server)
    void on_file_proposal(const QString &sender_name,
                          const QString &file_name,
                          qint64 file_size);

    // 用户点击 "接收" 按钮
    void on_receive_accepted(const QString &file_name);

    // 用户点击 "拒绝" 按钮
    void on_receive_rejected();

    // 接收进度更新
    void on_receive_progress(qint64 received, qint64 total);

    // 接收完成
    void on_receive_finished(const QString &file_path);

    // 接收出错
    void on_receive_error(const QString &reason);

    // ====== 文件发送相关 ======

    // 发送进度更新
    void on_send_progress(qint64 sent, qint64 total);

    // 发送完成
    void on_send_finished();

    // 发送出错
    void on_send_error(const QString &reason);

    // 点击 "发送文件" 按钮
    void on_send_clicked();

    // ====== 设置 ======

    // 点击 "设置" 按钮 → 弹出设置对话框
    void on_settings_clicked();

    // ====== 系统托盘 ======

    // 双击托盘图标 → 显示主窗口
    void on_tray_activated(QSystemTrayIcon::ActivationReason reason);

private:
    // 初始化 UI 布局 (左侧: 本机信息 + 设备列表 + 设置, 右侧: 发送面板)
    void setup_ui();

    // 初始化系统托盘 (图标, 右键菜单)
    void setup_tray();

    // 初始化核心服务 (discovery_service, transfer_server) 并连接信号
    void setup_services();

    // 启动向选中设备发送文件的流程
    void start_send_file(const QString &file_path);

    // 格式化文件大小 (B / KB / MB / GB)
    QString format_size(qint64 bytes) const;

    // 格式化传输速度
    QString format_speed(qint64 bytes_per_sec) const;

    // ==== 核心服务 ====
    core::discovery_service *discovery_ = nullptr;       // 设备发现服务
    core::transfer_server *transfer_server_ = nullptr;   // TCP 传输服务
    core::file_sender *current_sender_ = nullptr;        // 当前活动的发送器

    // ==== UI 控件 ====
    QLabel *self_info_label_ = nullptr;                  // 本机信息卡片
    ui::device_list_widget *device_list_ = nullptr;      // 在线设备列表
    ui::receive_dialog *receive_dialog_ = nullptr;       // 接收确认弹窗

    QWidget *send_panel_ = nullptr;                      // 右侧发送面板容器
    QLabel *selected_device_label_ = nullptr;            // 已选设备名称标签
    QPushButton *send_button_ = nullptr;                 // "发送文件" 按钮
    QLabel *status_label_ = nullptr;                     // 状态/进度文字标签
    QProgressBar *progress_bar_ = nullptr;               // 传输进度条
    QLabel *speed_label_ = nullptr;                      // 传输速度标签

    QPushButton *settings_button_ = nullptr;             // "设置" 按钮

    // ==== 系统托盘 ====
    QSystemTrayIcon *tray_icon_ = nullptr;               // 托盘图标

    // ==== 状态 ====
    core::device_info selected_device_;                  // 当前选中的目标设备
    bool has_selected_device_ = false;                   // 是否已选择目标设备
};

} // namespace ui
} // namespace landrop
