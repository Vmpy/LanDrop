/**
 * @file main.cpp
 * @brief 应用程序入口 — 创建 QApplication, 显示主窗口, 进入事件循环
 * @author Mupengyi
 * @date 2026-05-02
 */
#include "ui/main_window.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);                           // 创建 Qt 应用实例
    app.setWindowIcon(QIcon("icon/icon.ico"));              // 设置应用图标 (窗口标题栏)
    app.setQuitOnLastWindowClosed(false);                   // 关闭窗口不退出 (因为有系统托盘)

    landrop::ui::main_window window;                        // 创建主窗口
    window.show();                                          // 显示主窗口

    return QCoreApplication::exec();                        // 进入事件循环, 直到 quit() 被调用
}
