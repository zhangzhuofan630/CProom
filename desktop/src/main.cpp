#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QWidget>

#include "version.h"

// S0.2 的最小可运行程序。
// 目的只有一个：证明「本地能编译 + CI 能编译 + 程序能起来」这条链路是通的。
//
// 这里刻意【不】做透明/无边框/置顶 —— 那是 Phase 1 的事，而且 Wayland 下的
// 行为差异需要单独处理（见 docs/execution-plan.md 的 S0.9 与平台能力层）。
// 现在保持最普通的一个窗口，出问题就一定是工具链问题，不会和窗口管理混在一起。

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Desktoppet"));
    QApplication::setApplicationVersion(QString::fromLatin1(pet::versionString()));
    QApplication::setOrganizationName(QStringLiteral("desktoppet"));

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("Desktoppet"));
    window.resize(520, 300);

    auto *central = new QWidget(&window);
    auto *layout = new QVBoxLayout(central);

    auto *title = new QLabel(QStringLiteral("情侣桌宠 · 工程骨架"), central);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.5);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);

    auto *detail = new QLabel(
        QStringLiteral("版本 %1\n\n看到这个窗口说明：Qt 已装好、CMake 配置通过、编译链路正常。\n"
                       "下一步是 S0.3（ComfyUI 环境），不在这里写界面。")
            .arg(QString::fromLatin1(pet::versionString())),
        central);
    detail->setAlignment(Qt::AlignCenter);
    detail->setWordWrap(true);

    layout->addStretch();
    layout->addWidget(title);
    layout->addSpacing(12);
    layout->addWidget(detail);
    layout->addStretch();

    window.setCentralWidget(central);
    window.show();

    return QApplication::exec();
}
