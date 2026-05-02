/**
 * @file device_list_widget.cpp
 * @brief 在线设备列表控件实现 — QListWidget 展示设备名/平台/IP
 * @author Mupengyi
 * @date 2026-05-02
 */
#include "device_list_widget.h"
#include "core/device_info.h"

#include <QListWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QDateTime>

namespace landrop
{
namespace ui
{

device_list_widget::device_list_widget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    hint_label_ = new QLabel(QStringLiteral("在线设备"), this); // 标题
    QFont font = hint_label_->font();
    font.setBold(true);                                     // 加粗
    hint_label_->setFont(font);
    layout->addWidget(hint_label_);

    list_widget_ = new QListWidget(this);
    list_widget_->setSelectionMode(QAbstractItemView::SingleSelection); // 单选模式
    list_widget_->setAlternatingRowColors(true);            // 交替行颜色 (便于阅读)
    connect(list_widget_, &QListWidget::itemClicked,
            this, &device_list_widget::on_item_clicked);

    layout->addWidget(list_widget_);
}

void device_list_widget::add_device(const landrop::core::device_info &info)
{
    device_infos_[info.uuid] = info;                        // 更新内部映射

    // 检查是否已有该设备的列表项 (根据 UUID 匹配)
    for (int i = 0; i < list_widget_->count(); ++i)
    {
        QListWidgetItem *item = list_widget_->item(i);
        if (item->data(Qt::UserRole).toString() == info.uuid)
        {
            // 更新现有项的文本
            QString label = QStringLiteral("%1 [%2]")
                                .arg(info.device_name, info.platform.toUpper());
            item->setText(label);
            item->setData(Qt::UserRole + 1, info.ip_address.toString()); // 更新 IP
            return;
        }
    }

    // 创建新的列表项
    QString label = QStringLiteral("%1 [%2]")
                        .arg(info.device_name, info.platform.toUpper()); // e.g. "MyPC [WIN]"
    auto *item = new QListWidgetItem(label, list_widget_);
    item->setData(Qt::UserRole, info.uuid);                 // 隐藏数据: UUID
    item->setData(Qt::UserRole + 1, info.ip_address.toString()); // 隐藏数据: IP
    item->setToolTip(QStringLiteral("IP: %1\n端口: %2")
                        .arg(info.ip_address.toString())
                        .arg(info.tcp_port));               // 鼠标悬停提示
}

void device_list_widget::remove_device(const QString &uuid)
{
    device_infos_.remove(uuid);                             // 从内部映射移除

    for (int i = 0; i < list_widget_->count(); ++i)
    {
        QListWidgetItem *item = list_widget_->item(i);
        if (item->data(Qt::UserRole).toString() == uuid)     // 匹配 UUID
        {
            delete list_widget_->takeItem(i);               // 移除列表项
            return;
        }
    }
}

void device_list_widget::on_item_clicked()
{
    QListWidgetItem *item = list_widget_->currentItem();     // 获取当前选中项
    if (!item)
    {
        return;
    }

    QString uuid = item->data(Qt::UserRole).toString();      // 读取 UUID
    if (device_infos_.contains(uuid))                         // 映射中存在
    {
        emit device_selected(device_infos_[uuid]);            // 发射设备选择信号
    }
}

} // namespace ui
} // namespace landrop
