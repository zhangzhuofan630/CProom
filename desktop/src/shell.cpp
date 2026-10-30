#include "shell.h"

#include "petpack.h"
#include "petview.h"

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QLockFile>
#include <QMenu>
#include <QPainter>
#include <QScreen>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTextStream>

#include <cstdio>

namespace pet {
namespace shell {

namespace {

constexpr auto kPosKey = "pos/valid";
constexpr auto kPosXKey = "pos/x";
constexpr auto kPosYKey = "pos/y";

QString autostartFilePath()
{
#ifdef Q_OS_WIN
    return QString();  // Windows 走注册表
#else
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                        + QStringLiteral("/autostart");
    return dir + QStringLiteral("/desktoppet.desktop");
#endif
}

/// 桌面项 Exec 字段的引号规则和 shell 不同：用双引号包起来即可，
/// 但字段里的 `"` 和 `\` 要转义。路径里出现引号基本不可能，简单处理。
QString execQuote(const QString &s)
{
    QString t = s;
    t.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    t.replace(QLatin1Char('"'), QLatin1String("\\\""));
    return QLatin1Char('"') + t + QLatin1Char('"');
}

/// 拼出「开机时该跑什么」。
///
/// 用**白名单**而不是「剔掉几个一次性开关」：后者的失效方式很糟——哪天新加一个
/// 命令行开关忘了加进黑名单，它就会原样写进自启项。实测第一版就把
/// `--set-autostart on` 写进去了，那条命令开机后设完自启就退出，桌宠永远起不来。
/// 白名单的失效方式是「少带一个参数」（退回默认值），不会产生错误行为。
QStringList autostartArgs()
{
    // 只有这些会影响「常驻运行时长什么样」
    static const QStringList kKeep = {
        QStringLiteral("-p"),   QStringLiteral("--pack"), QStringLiteral("-w"),
        QStringLiteral("--width"), QStringLiteral("-f"),  QStringLiteral("--fps"),
        QStringLiteral("--idle-fps"),
    };

    const QStringList argv = QCoreApplication::arguments();
    QStringList out;
    for (int i = 1; i < argv.size(); ++i) {
        const QString &a = argv.at(i);
        if (!kKeep.contains(a)) {
            // 丢弃这个开关时，顺手把它可能带着的值也丢掉（值通常不以 - 开头）
            if (i + 1 < argv.size() && !argv.at(i + 1).startsWith(QLatin1Char('-')))
                ++i;
            continue;
        }
        out << a;
        if (i + 1 < argv.size())
            out << argv.at(++i);
    }
    return out;
}

} // namespace

std::unique_ptr<QLockFile> acquireSingleInstance()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty())
        dir = QDir::tempPath();

    auto lock = std::make_unique<QLockFile>(dir + QStringLiteral("/desktoppet.lock"));
    lock->setStaleLockTime(0);  // 只要 pid 还活着就认为锁有效，不按时间判死
    if (!lock->tryLock(0)) {
        return nullptr;
    }
    return lock;
}

QPoint savedPos(bool *ok)
{
    if (ok)
        *ok = false;

    QSettings st;
    if (!st.value(QLatin1String(kPosKey), false).toBool())
        return {};

    const QPoint p(st.value(QLatin1String(kPosXKey), 0).toInt(),
                   st.value(QLatin1String(kPosYKey), 0).toInt());

    // 存下来的位置可能已经不在任何屏幕上（拔了外接屏、改了分辨率）。
    // 这种情况宁可回到右下角，也不要让窗口飘在看不见的地方——那样用户
    // 除了 pkill 没有任何办法把它弄回来。
    if (!QGuiApplication::screenAt(p))
        return {};

    if (ok)
        *ok = true;
    return p;
}

void savePos(const QPoint &pos)
{
    QSettings st;
    st.setValue(QLatin1String(kPosKey), true);
    st.setValue(QLatin1String(kPosXKey), pos.x());
    st.setValue(QLatin1String(kPosYKey), pos.y());
}

bool autostartEnabled()
{
#ifdef Q_OS_WIN
    QSettings run(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                  QSettings::NativeFormat);
    return !run.value(QStringLiteral("Desktoppet")).toString().isEmpty();
#else
    return QFile::exists(autostartFilePath());
#endif
}

bool setAutostart(bool on)
{
#ifdef Q_OS_WIN
    QSettings run(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                  QSettings::NativeFormat);
    if (!on) {
        run.remove(QStringLiteral("Desktoppet"));
        return true;
    }
    QStringList parts;
    parts << execQuote(QCoreApplication::applicationFilePath());
    for (const QString &a : autostartArgs())
        parts << execQuote(a);
    run.setValue(QStringLiteral("Desktoppet"), parts.join(QLatin1Char(' ')));
    return run.status() == QSettings::NoError;
#else
    const QString path = autostartFilePath();
    if (!on) {
        if (!QFile::exists(path))
            return true;
        return QFile::remove(path);
    }

    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;

    QStringList cmd;
    // Wayland 下必须走 XWayland：原生后端不允许客户端自控置顶与点击穿透，
    // 而登录自启时没有 run.sh 帮我们补这个环境变量。
    if (qEnvironmentVariable("XDG_SESSION_TYPE") == QLatin1String("wayland"))
        cmd << QStringLiteral("env") << QStringLiteral("QT_QPA_PLATFORM=xcb");
    cmd << execQuote(QCoreApplication::applicationFilePath());
    for (const QString &a : autostartArgs())
        cmd << execQuote(a);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;

    QTextStream ts(&f);
    ts << "[Desktop Entry]\n"
       << "Type=Application\n"
       << "Name=情侣桌宠\n"
       << "Comment=Desktop pet\n"
       << "Exec=" << cmd.join(QLatin1Char(' ')) << "\n"
       << "Terminal=false\n"
       << "X-GNOME-Autostart-enabled=true\n";
    return true;
#endif
}

QPixmap trayPixmap(const PetPack &pack, int size)
{
    const Layer *head = pack.layer(QStringLiteral("head"));
    if (!head || head->isNull()) {
        QPixmap fallback(size, size);
        fallback.fill(Qt::transparent);
        return fallback;
    }

    QPixmap scaled = head->pixmap.scaled(size, size, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);

    QPixmap canvas(size, size);
    canvas.fill(Qt::transparent);
    QPainter p(&canvas);
    p.drawPixmap((size - scaled.width()) / 2, (size - scaled.height()) / 2, scaled);
    p.end();
    return canvas;
}

} // namespace shell

// ---------------------------------------------------------------------------
// Tray
// ---------------------------------------------------------------------------

Tray::Tray(PetView &view, const PetPack &pack, QObject *parent)
    : QObject(parent)
    , m_view(view)
{
    // DESKTOPPET_NO_TRAY 是给测试用的开关：托盘有没有是环境决定的，
    // 没有它就没法验证「没有托盘」那条降级路径。
    if (qEnvironmentVariableIsSet("DESKTOPPET_NO_TRAY")
        || !QSystemTrayIcon::isSystemTrayAvailable()) {
        std::printf("[tray] 没有可用的系统托盘，关闭窗口即退出\n");
        std::fflush(stdout);
        return;
    }

    m_icon = new QSystemTrayIcon(this);

    // 一次塞多个尺寸，交给系统挑。22px 是 Plasma 面板的常见值，
    // 只给一个 64px 让系统自己缩会糊。
    QIcon icon;
    for (const int s : {16, 22, 24, 32, 48, 64})
        icon.addPixmap(shell::trayPixmap(pack, s));
    m_icon->setIcon(icon);
    m_icon->setToolTip(QStringLiteral("情侣桌宠"));

    buildMenu();

    // 双击图标＝显示/隐藏。KDE 的 SNI 左键默认弹菜单，所以只挂双击，
    // 挂 Trigger 会和菜单打架（一次点击触发两件事）。
    connect(m_icon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason r) {
                if (r == QSystemTrayIcon::DoubleClick)
                    toggleVisible();
            });

    m_icon->show();
    std::printf("[tray] 已挂上托盘图标\n");
    std::fflush(stdout);
}

bool Tray::isAvailable() const
{
    return m_icon != nullptr;
}

Tray::~Tray()
{
    // QMenu 的父对象只能是 QWidget，而 Tray 是 QObject，挂不上——
    // 所以自己管生命周期，不能指望 Qt 帮我们收尸。
    delete m_menu;
}

void Tray::buildMenu()
{
    auto *menu = new QMenu();
    m_menu = menu;

    // 挥手。双击虽然能触发，但没人猜得到；给一个菜单项才算真的可发现。
    QAction *wave = menu->addAction(QStringLiteral("挥挥手"));
    connect(wave, &QAction::triggered, this, [this] {
        if (!m_view.isVisible())
            toggleVisible();
        m_view.startWave();
    });

    m_showAction = menu->addAction(QStringLiteral("隐藏"));
    connect(m_showAction, &QAction::triggered, this, [this] { toggleVisible(); });

    QAction *home = menu->addAction(QStringLiteral("回到右下角"));
    connect(home, &QAction::triggered, this, [this] { resetHome(); });

    menu->addSeparator();

    m_autoAction = menu->addAction(QStringLiteral("开机自启"));
    m_autoAction->setCheckable(true);
    m_autoAction->setChecked(shell::autostartEnabled());
    connect(m_autoAction, &QAction::toggled, this, [this](bool on) {
        if (!shell::setAutostart(on)) {
            std::fprintf(stderr, "[tray] 设置开机自启失败\n");
            // 写失败要把勾去掉，否则界面在骗人
            QSignalBlocker block(m_autoAction);
            m_autoAction->setChecked(!on);
        }
    });

    menu->addSeparator();

    // 唯一正经营生：退出。没有它，用户只能开终端 pkill。
    // 位置不在这里存——统一在 main 的 aboutToQuit 里存，避免「隐藏后退出」
    // 「菜单退出」「Ctrl+C」三条路径各写一遍、漏掉其中一条。
    QAction *quit = menu->addAction(QStringLiteral("退出"));
    connect(quit, &QAction::triggered, this, [] { QCoreApplication::quit(); });

    // 不设 parent：QMenu 的父对象只能是 QWidget，而 Tray 是 QObject。
    // 生命周期在析构函数里自己管。
    m_icon->setContextMenu(menu);
}

void Tray::toggleVisible()
{
    if (m_view.isVisible()) {
        shell::savePos(m_view.pos());
        m_view.hide();
        m_showAction->setText(QStringLiteral("显示"));
    } else {
        m_view.show();
        m_view.raise();
        m_showAction->setText(QStringLiteral("隐藏"));
    }
}

void Tray::resetHome()
{
    if (!m_view.isVisible())
        toggleVisible();
    m_view.resetHome();
}

void Tray::selfTest()
{
    auto dump = [this](const char *tag) {
        std::printf("[tray-test] %-10s visible=%d geo=(%d,%d %dx%d)\n", tag,
                    int(m_view.isVisible()), m_view.x(), m_view.y(), m_view.width(),
                    m_view.height());
        std::fflush(stdout);
    };

    dump("初始");
    for (QAction *a : m_menu->actions()) {
        if (!a->isSeparator())
            std::printf("[tray-test]   菜单项: %s%s\n", qUtf8Printable(a->text()),
                        a->isCheckable() ? (a->isChecked() ? " [x]" : " [ ]") : "");
    }

    m_view.resetHome();
    dump("回右下角");

    // 走菜单动作本身，而不是直接调 m_view 的方法——要验证的是「点了菜单项会发生什么」
    for (QAction *a : m_menu->actions()) {
        if (a->text().contains(QStringLiteral("挥手"))) {
            a->trigger();
            break;
        }
    }
    dump("挥手后");

    auto *hideAct = m_showAction;
    hideAct->trigger();  // 隐藏
    dump("隐藏后");
    hideAct->trigger();  // 显示
    dump("再显示");

    // 自启开关走一遍再还原，确认写文件/删文件都成功
    const bool before = shell::autostartEnabled();
    m_autoAction->setChecked(!before);
    const bool flipped = shell::autostartEnabled();
    m_autoAction->setChecked(before);
    const bool restored = shell::autostartEnabled();
    std::printf("[tray-test] 自启: before=%d flipped=%d restored=%d => %s\n", int(before),
                int(flipped), int(restored),
                (flipped != before && restored == before) ? "OK" : "FAIL");

    // 位置记忆的存取。用一个确定落在屏幕内的点，否则 savedPos() 会因为
    // 「不在任何屏幕上」而拒绝返回，测出来的是假失败。
    bool hadOld = false;
    const QPoint old = shell::savedPos(&hadOld);
    QScreen *scr = m_view.screen();
    const QPoint probe = scr ? scr->availableGeometry().center() : QPoint(200, 200);
    shell::savePos(probe);
    bool ok = false;
    const QPoint readBack = shell::savedPos(&ok);
    std::printf("[tray-test] 位置存取: 写入=(%d,%d) 读回=(%d,%d) => %s\n", probe.x(), probe.y(),
                readBack.x(), readBack.y(), (ok && readBack == probe) ? "OK" : "FAIL");
    shell::savePos(hadOld ? old : m_view.pos());
    dump("测试结束");
}

} // namespace pet
