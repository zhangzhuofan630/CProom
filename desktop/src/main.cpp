#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QLockFile>
#include <QMouseEvent>
#include <QScreen>
#include <QTextStream>
#include <QTimer>
#include <QtMath>

#include <cstdio>
#include <memory>

#include "metrics.h"
#include "petpack.h"
#include "petview.h"
#include "shell.h"
#include "version.h"

// 情侣桌宠 · 最小渲染器 + 交互原型
//
// 干两件事：
//   1. 把 petpack 的四层 + 表情补丁用 QPainter 合成出来，自己报出 CPU 与内存
//      （S0.9 的验收指标：空闲 CPU < 1%、内存 < 150MB，必须在 Qt 里真跑）
//   2. 拖拽 / 抛掷 / 落地物理（S1.0），跳跃和落体共用一套积分
//
// 用法：
//   desktop                         普通模式，桌宠出现在屏幕右下角
//   desktop --bench 6               跑 6 秒后打印性能数据并退出
//   desktop --bench 6 --follow      bench 时也跟随光标（busy 场景）
//   desktop --bench 6 --animate     让头部持续摆动，测 CPU 上界
//   desktop --throw 600,-800        启动后立刻抛出去（无头环境验证落体）
//   desktop --test-drag             合成一次「按下→拖动→松手」，验证拖拽链路
//   desktop --snapshot out.png      渲染一帧存成 PNG（XWayland 抓不到根窗口）

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Desktoppet"));
    QApplication::setApplicationVersion(QString::fromLatin1(pet::versionString()));
    QApplication::setOrganizationName(QStringLiteral("desktoppet"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("情侣桌宠 · 分层渲染器（S0.9 性能验证）"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption packOpt({QStringLiteral("p"), QStringLiteral("pack")},
                               QStringLiteral("petpack 目录"),
                               QStringLiteral("dir"),
                               QDir::homePath() + QStringLiteral("/AI/petpack"));
    QCommandLineOption widthOpt({QStringLiteral("w"), QStringLiteral("width")},
                                QStringLiteral("桌宠显示宽度（px）"),
                                QStringLiteral("px"), QStringLiteral("200"));
    QCommandLineOption fpsOpt({QStringLiteral("f"), QStringLiteral("fps")},
                              QStringLiteral("有动作时的帧率（20 比 30 省 30% CPU，肉眼几乎无差）"),
                              QStringLiteral("fps"), QStringLiteral("20"));
    QCommandLineOption idleFpsOpt(QStringLiteral("idle-fps"),
                                  QStringLiteral("纯呼吸时的帧率（决定空闲 CPU）"),
                                  QStringLiteral("fps"), QStringLiteral("8"));
    QCommandLineOption benchOpt(QStringLiteral("bench"),
                                QStringLiteral("跑 N 秒后打印性能数据并退出"),
                                QStringLiteral("seconds"));
    QCommandLineOption followOpt(QStringLiteral("follow"),
                                 QStringLiteral("bench 时也跟随光标"));
    QCommandLineOption animateOpt(QStringLiteral("animate"),
                                  QStringLiteral("bench 时让头部持续摆动（测 CPU 上界）"));
    QCommandLineOption snapOpt(QStringLiteral("snapshot"),
                               QStringLiteral("渲染一帧到 PNG 后退出"),
                               QStringLiteral("file"));
    QCommandLineOption snapAngleOpt(QStringLiteral("snap-angle"),
                                    QStringLiteral("截图时的头部角度（度）"),
                                    QStringLiteral("deg"), QStringLiteral("0"));
    QCommandLineOption snapExprOpt(QStringLiteral("snap-expr"),
                                   QStringLiteral("截图时的表情名"),
                                   QStringLiteral("name"), QString());
    QCommandLineOption throwOpt(QStringLiteral("throw"),
                                QStringLiteral("启动后立刻给一个初速度 vx,vy（px/s，向上为负），"
                                               "无头环境验证落体用"),
                                QStringLiteral("vx,vy"));
    QCommandLineOption traceOpt(QStringLiteral("trace"),
                                QStringLiteral("每 100ms 打印一次位置与速度"));
    QCommandLineOption dragTestOpt(QStringLiteral("test-drag"),
                                   QStringLiteral("合成一次「按下→拖动→松手」以验证拖拽链路"));
    QCommandLineOption dblTestOpt(QStringLiteral("test-dbl"),
                                  QStringLiteral("合成一次双击，验证「跳一下 + 挥手」链路"));
    QCommandLineOption snapArmOpt(QStringLiteral("snap-arm"),
                                  QStringLiteral("截图时的手臂角度（度），同时切到挥手姿态"),
                                  QStringLiteral("deg"));
    QCommandLineOption waveOpt(QStringLiteral("wave"),
                               QStringLiteral("启动后 0.6 秒自动挥一次手"));
    QCommandLineOption trayTestOpt(QStringLiteral("test-tray"),
                                   QStringLiteral("依次触发托盘菜单项，验证托盘链路后退出"));
    QCommandLineOption autostartOpt(QStringLiteral("set-autostart"),
                                    QStringLiteral("设置开机自启后退出：on / off / status"),
                                    QStringLiteral("on|off|status"));
    parser.addOptions({packOpt, widthOpt, fpsOpt, idleFpsOpt, benchOpt, followOpt, animateOpt,
                       snapOpt, snapAngleOpt, snapExprOpt, throwOpt, traceOpt, dragTestOpt,
                       snapArmOpt, waveOpt, dblTestOpt, trayTestOpt, autostartOpt});
    parser.process(app);

    // 自启开关不需要窗口，先处理掉再决定要不要建 GUI。
    // 但也得等 QApplication 起来——QSettings / QStandardPaths 都依赖它。
    if (parser.isSet(autostartOpt)) {
        const QString v = parser.value(autostartOpt).toLower().trimmed();
        if (v == QLatin1String("status")) {
            std::printf("[autostart] %s\n", pet::shell::autostartEnabled() ? "on" : "off");
            return 0;
        }
        if (v != QLatin1String("on") && v != QLatin1String("off")) {
            std::fprintf(stderr, "[autostart] 只接受 on / off / status\n");
            return 2;
        }
        const bool wantOn = (v == QLatin1String("on"));
        if (!pet::shell::setAutostart(wantOn)) {
            std::fprintf(stderr, "[autostart] 写入失败\n");
            return 1;
        }
        std::printf("[autostart] 已%s（当前状态：%s）\n", wantOn ? "开启" : "关闭",
                    pet::shell::autostartEnabled() ? "on" : "off");
        return 0;
    }

    const QString packDir = parser.value(packOpt);
    const int targetWidth = qBound(80, parser.value(widthOpt).toInt(), 1200);
    const int fps = qBound(1, parser.value(fpsOpt).toInt(), 120);
    const int idleFps = qBound(1, parser.value(idleFpsOpt).toInt(), fps);

    // 「一次性模式」：测性能、截图——跑完就退，不挂托盘。
    // 不加这个区分的话，每跑一次 bench 都会弹一个托盘图标出来。
    const bool oneShot = parser.isSet(benchOpt) || parser.isSet(snapOpt);

    // 单实例锁只拦「真的要常驻」的启动。测试模式各跑各的，
    // 否则回归脚本第二个实例会被锁挡住。
    std::unique_ptr<QLockFile> instanceLock;
    if (!oneShot && !parser.isSet(trayTestOpt)) {
        instanceLock = pet::shell::acquireSingleInstance();
        if (!instanceLock) {
            std::fprintf(stderr,
                         "[single] 已经有一个桌宠在跑了。要再开一个请加 --bench/--snapshot。\n");
            return 0;
        }
    }

    pet::PetPack pack;
    QString err;
    if (!pack.load(packDir, targetWidth, &err)) {
        std::fprintf(stderr, "[pack] 加载失败: %s\n", qUtf8Printable(err));
        std::fprintf(stderr, "[pack] 提示: 素材在 ~/AI/petpack，需先跑 build_layers.py\n");
        return 2;
    }

    std::printf("[pack] dir=%s\n", qUtf8Printable(packDir));
    std::printf("[pack] canvas=%dx%d neck_y=%.1f\n", pack.width(), pack.height(),
                pack.neckY());
    std::printf("[pack] layers=%s\n", qUtf8Printable(pack.layerNames().join(QLatin1Char(','))));
    std::printf("[pack] exprs=%s\n", qUtf8Printable(pack.exprNames().join(QLatin1Char(','))));
    std::printf("[pack] arm=%d wave=%d pivot=(%.1f,%.1f) angle=[%.0f,%.0f]\n",
                pack.hasArm(), pack.bodyWave() != nullptr, pack.armPivot().x(),
                pack.armPivot().y(), pack.armAngleMin(), pack.armAngleMax());
    std::printf("[pack] pixmap_mem=%.1fMB\n", pack.memoryBytes() / 1048576.0);

    const pet::ProcSample base = pet::sampleProcess();
    std::printf("[proc] baseline_rss=%.1fMB\n", base.rssBytes / 1048576.0);

    pet::PetView view(pack);
    view.setTargetFps(fps, idleFps);
    // 测性能/截图时不恢复上次位置：起点必须每次都一样，否则测出来的数不可比
    // （上次停在半空的话，这次启动要先掉下来，帧数和 CPU 都不对得上）。
    if (oneShot)
        view.setRestoreSavedPos(false);

    // --- 桌面外壳：托盘 / 位置记忆 ---
    //
    // 要在 show() 之前挂上：show() 里会去读上次的位置。
    // 托盘本身也得比窗口先存在，否则关窗那一刻没人接住它。
    if (!oneShot) {
        auto *tray = new pet::Tray(view, pack, &app);
        if (tray->isAvailable()) {
            view.setCloseHidesToTray(true);
            // 有托盘时「最后一个窗口关闭」不等于要退出——桌宠就该常驻
            app.setQuitOnLastWindowClosed(false);
        } else {
            // 没有托盘就别拦着退出，否则用户关掉窗口后进程还在，且没有任何
            // 界面入口能把它叫回来或杀掉
            delete tray;
            tray = nullptr;
        }

        if (tray && parser.isSet(trayTestOpt)) {
            QTimer::singleShot(700, tray, [tray] {
                tray->selfTest();
                QCoreApplication::quit();
            });
        }

        // 停稳时记一次位置（拖动松手、落地静止）。不在拖动过程中存，
        // 那样会变成每帧写一次配置文件。
        QObject::connect(&view, &pet::PetView::settled, &view,
                         [&view] { pet::shell::savePos(view.pos()); });
        // 兜底：任何退出路径（菜单、Ctrl+C、注销）都要把位置留住
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &view,
                         [&view] { pet::shell::savePos(view.pos()); });
    }

    view.show();

    int benchSecs = 0;
    if (parser.isSet(benchOpt)) {
        benchSecs = qMax(1, parser.value(benchOpt).toInt());
        pet::PetView::BenchMode mode = pet::PetView::BenchMode::Idle;
        if (parser.isSet(animateOpt))
            mode = pet::PetView::BenchMode::Animate;
        else if (parser.isSet(followOpt))
            mode = pet::PetView::BenchMode::Follow;
        view.setBench(benchSecs, mode);
    } else {
        std::printf("[hint] 桌宠已启动。点一下会跳；按住能拖动，甩出去它会落到任务栏上；"
                    "双击它会挥手。\n");
    }

    Q_UNUSED(benchSecs);

    if (parser.isSet(traceOpt))
        view.setTrace(true);

    if (parser.isSet(throwOpt)) {
        const QStringList v = parser.value(throwOpt).split(QLatin1Char(','));
        if (v.size() != 2) {
            std::fprintf(stderr, "[throw] 参数格式应为 vx,vy，例如 900,-700\n");
            return 2;
        }
        const qreal vx = v.at(0).trimmed().toDouble();
        const qreal vy = v.at(1).trimmed().toDouble();
        // 等窗口真正 show 完再抛，否则位置还没落定
        QTimer::singleShot(80, &view, [&view, vx, vy] { view.throwPet(vx, vy); });
    }

    // 合成拖拽：把 press/move/release 直接塞进事件队列，走的是和真鼠标
    // 完全相同的代码路径。没有 xdotool 的环境下这是唯一能自动验证拖拽的
    // 办法——外部模拟鼠标还需要 X 会话里的真实光标抓取。
    if (parser.isSet(dragTestOpt)) {
        constexpr int kSteps = 24;
        constexpr int kStepMs = 16;
        const QPoint grabGlobal = view.mapToGlobal(QPoint(view.width() / 2, view.height() / 2));
        for (int i = 0; i <= kSteps; ++i) {
            const qreal t = qreal(i) / kSteps;
            // 划一段抛物线：先斜上，再斜下，松手时带一个向左下的速度。
            // 往左上甩而不是右上，是为了让它全程留在屏幕内——拖拽本身会
            // 钳边界，贴着边甩出来的速度会被钳成 0，测不到抛掷。
            const QPointF g(grabGlobal.x() - 260.0 * t, grabGlobal.y() - 200.0 * qSin(t * M_PI));
            QTimer::singleShot(400 + i * kStepMs, &view, [&view, i, g, kSteps] {
                const QEvent::Type type = i == 0     ? QEvent::MouseButtonPress
                                        : i == kSteps ? QEvent::MouseButtonRelease
                                                      : QEvent::MouseMove;
                QMouseEvent ev(type, view.mapFromGlobal(g.toPoint()), g, Qt::LeftButton,
                               i == kSteps ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(&view, &ev);
            });
        }
    }

    if (parser.isSet(waveOpt))
        QTimer::singleShot(600, &view, [&view] { view.startWave(); });

    // 双击挥手是唯一能让用户触发挥手的人口，而沙箱里没有 xdotool。
    // 这里按 Qt 真实的派发顺序合成一整套事件：press → release → dblclick → release。
    // 注意双击中间还夹着第一次单击，而那一次会触发跳跃——这是真实行为，
    // 所以要按原样复现，否则测不出「跳完再挥手」这条路径。
    if (parser.isSet(dblTestOpt)) {
        const QPoint g = view.mapToGlobal(QPoint(view.width() / 2, view.height() / 2));
        const QPoint lp = view.mapFromGlobal(g);
        const auto send = [&view, lp, g](QEvent::Type t, Qt::MouseButton btn) {
            QMouseEvent ev(t, lp, g, btn, btn, Qt::NoModifier);
            QApplication::sendEvent(&view, &ev);
        };
        QTimer::singleShot(500, &view, [send] { send(QEvent::MouseButtonPress, Qt::LeftButton); });
        QTimer::singleShot(560, &view, [send] { send(QEvent::MouseButtonRelease, Qt::LeftButton); });
        QTimer::singleShot(600, &view, [send] { send(QEvent::MouseButtonDblClick, Qt::LeftButton); });
        QTimer::singleShot(660, &view, [send] { send(QEvent::MouseButtonRelease, Qt::LeftButton); });
    }

    if (parser.isSet(snapOpt)) {
        const QString outPath = parser.value(snapOpt);
        view.freeze(parser.value(snapAngleOpt).toDouble(), parser.value(snapExprOpt));
        if (parser.isSet(snapArmOpt))
            view.snapWave(parser.value(snapArmOpt).toDouble());
        // 等一帧画完再 render，否则拿到的可能是空 backing store
        QTimer::singleShot(350, &view, [&view, outPath] {
            QPixmap pm(view.size());
            pm.fill(Qt::transparent);
            view.render(&pm);
            if (!pm.save(outPath))
                std::fprintf(stderr, "[snap] 保存失败: %s\n", qUtf8Printable(outPath));
            else
                std::printf("[snap] %s  %dx%d\n", qUtf8Printable(outPath), pm.width(),
                            pm.height());
            QCoreApplication::quit();
        });
        return QApplication::exec();
    }

    return QApplication::exec();
}
