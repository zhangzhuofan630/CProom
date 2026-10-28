#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QMouseEvent>
#include <QScreen>
#include <QTextStream>
#include <QTimer>
#include <QtMath>

#include <cstdio>

#include "metrics.h"
#include "petpack.h"
#include "petview.h"
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
    parser.addOptions({packOpt, widthOpt, fpsOpt, idleFpsOpt, benchOpt, followOpt, animateOpt,
                       snapOpt, snapAngleOpt, snapExprOpt, throwOpt, traceOpt, dragTestOpt});
    parser.process(app);

    const QString packDir = parser.value(packOpt);
    const int targetWidth = qBound(80, parser.value(widthOpt).toInt(), 1200);
    const int fps = qBound(1, parser.value(fpsOpt).toInt(), 120);
    const int idleFps = qBound(1, parser.value(idleFpsOpt).toInt(), fps);

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
    std::printf("[pack] pixmap_mem=%.1fMB\n", pack.memoryBytes() / 1048576.0);

    const pet::ProcSample base = pet::sampleProcess();
    std::printf("[proc] baseline_rss=%.1fMB\n", base.rssBytes / 1048576.0);

    pet::PetView view(pack);
    view.setTargetFps(fps, idleFps);
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
        std::printf("[hint] 桌宠已启动。点一下会跳；按住能拖动，甩出去它会落到任务栏上。\n");
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

    if (parser.isSet(snapOpt)) {
        const QString outPath = parser.value(snapOpt);
        view.freeze(parser.value(snapAngleOpt).toDouble(), parser.value(snapExprOpt));
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
