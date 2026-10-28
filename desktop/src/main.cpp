#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QScreen>
#include <QTextStream>
#include <QTimer>

#include <cstdio>

#include "metrics.h"
#include "petpack.h"
#include "petview.h"
#include "version.h"

// 情侣桌宠 · S0.9 最小渲染器
//
// 这一版只干一件事：把 petpack 的四层 + 表情补丁用 QPainter 合成出来，
// 然后自己报出 CPU 与内存。web demo 的 60fps 不能当依据——Canvas 2D 和
// QPainter 是两套后端，S0.9 的验收指标（空闲 CPU < 1%、内存 < 150MB）
// 必须在 Qt 里真跑一遍。
//
// 用法：
//   desktop                         普通模式，桌宠出现在屏幕右下角
//   desktop --bench 6               跑 6 秒后打印性能数据并退出
//   desktop --bench 6 --follow      bench 时也跟随光标（busy 场景）
//   desktop --fps 0                 不刷新（基线 CPU）

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
    parser.addOptions({packOpt, widthOpt, fpsOpt, idleFpsOpt, benchOpt, followOpt, animateOpt,
                       snapOpt, snapAngleOpt, snapExprOpt});
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
        std::printf("[hint] 桌宠已启动。点它一下会跳；鼠标移动会跟着转头。\n");
    }

    Q_UNUSED(benchSecs);

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
