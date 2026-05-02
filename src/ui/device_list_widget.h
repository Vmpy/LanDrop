/**
 * @file device_list_widget.h
 * @brief 在线设备列表控件 — 显示局域网内发现的 LanDrop 设备
 * @author Mupengyi
 * @date 2026-05-02
 */
#pragma once

#include <QWidget>
#include <QHash>

class QListWidget;   // Qt 列表控件, 用于显示设备列表
class QLabel;        // 标题标签

namespace landrop
{
namespace core
{
struct device_info;  // 设备信息结构体
}

namespace ui
{

// 设备列表控件: 左侧面板中的在线设备列表, 支持点击选择
class device_list_widget : public QWidget
{
    Q_OBJECT

public:
    explicit device_list_widget(QWidget *parent = nullptr);

public slots:
    // 添加或更新设备 (发现新设备或设备信息变化时调用)
    void add_device(const landrop::core::device_info &info);

    // 移除设备 (设备离线时调用)
    void remove_device(const QString &uuid);

signals:
    // 用户点击选择了某个设备
    void device_selected(const landrop::core::device_info &info);

private slots:
    // 列表项被点击
    void on_item_clicked();

private:
    QListWidget *list_widget_ = nullptr;                    // 列表控件
    QLabel *hint_label_ = nullptr;                          // 标题 "在线设备"

    QHash<QString, landrop::core::device_info> device_infos_; // UUID → 设备信息的映射
};

} // namespace ui
} // namespace landrop
