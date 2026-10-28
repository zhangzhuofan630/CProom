#include "petview.h"

#include <QCursor>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QScreen>
#include <QShowEvent>
#include <QtMath>

#include <cmath>
#include <cstdio>

#include "metrics.h"

namespace pet {

namespace {
// 窗口留给动作的余量。
//
// 这两个数直接决定每帧要合成多少像素，而合成面积是 CPU 的主要成本来源。
// 所以它们不是「好看就行」的排版参数，是性能参数：
//   kPadX    —— 角色包围盒外本来就有 ~30px 空白，旋转 ±12° 的水平位移
//              约 32px，所以 4px 就够，给太多纯属浪费。
//   kPadTop  —— 跳跃用窗口位移而不是内容位移实现（见 mousePressEvent），
//              顶部不再需要预留 78px。
constexpr int kPadX = 4;
constexpr int kPadTop = 8;
constexpr int kPadBottom = 4;

constexpr qreal kMaxAngle = PetPack::kMaxHeadAngleDeg;  ///< ±22° 开始「断头」，12° 是实测安全值
constexpr qreal kMaxOffX = 8.0;     ///< px
constexpr qreal kMaxOffY = 4.0;
constexpr qreal kBreathAmp = 1.8;  ///< 呼吸位移幅度（px）
constexpr qreal kBreathPeriod = 4.2;  ///< 秒

constexpr qreal kJumpV0 = -430.0;     ///< px/s
constexpr qreal kGravity = 1900.0;    ///< px/s²
} // namespace

PetView::PetView(const PetPack &pack, QWidget *parent)
    : QWidget(parent)
    , m_pack(pack)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    // Tool 窗口不占任务栏；NA_NoDropShadowWindowHint 避免某些 WM 给透明窗口加阴影
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAttribute(Qt::WA_ShowWithoutActivating, true);

    resize(pack.width() + kPadX * 2, pack.height() + kPadTop + kPadBottom);
    setCursor(Qt::PointingHandCursor);

    m_clock.start();
    m_lastMs = 0;

    connect(&m_timer, &QTimer::timeout, this, &PetView::tick);
}

void PetView::setTargetFps(int activeFps, int idleFps)
{
    m_activeFps = qMax(1, activeFps);
    m_idleFps = qBound(1, idleFps, m_activeFps);
    m_appliedFps = 0;  // 下次 tick 会重新应用
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.start(qMax(1, 1000 / m_idleFps));
    m_appliedFps = m_idleFps;
}

void PetView::freeze(qreal angleDeg, const QString &exprName)
{
    m_timer.stop();
    m_followCursor = false;
    m_angleDeg = angleDeg;
    m_prevAngle = angleDeg;
    m_offX = 0.0;
    m_offY = 0.0;
    m_breathOffset = 0.0;
    m_jumpY = 0.0;
    m_jumpVy = 0.0;
    m_expr = exprName;
    m_exprUntil = 1 << 30;
    update();
}

void PetView::applyFrameRate(bool active)
{
    const int fps = active ? m_activeFps : m_idleFps;
    if (fps == m_appliedFps)
        return;
    m_appliedFps = fps;
    m_timer.setInterval(qMax(1, 1000 / fps));
}

void PetView::setBench(int seconds, BenchMode mode)
{
    m_benchMode = mode;
    m_benchSeconds = seconds;
    m_followCursor = (mode != BenchMode::Idle);
    if (mode == BenchMode::Animate)
        m_lastInput = 1 << 29;  // 别让它中途犯困，测的是纯动作开销
    if (seconds <= 0)
        return;

    m_benchStart = m_clock.elapsed();
    m_benchCpuStart = sampleProcess().cpuSeconds;
    m_benchNextReport = 1000;
    m_benchTimer.setInterval(1000);
    m_benchTimer.setTimerType(Qt::PreciseTimer);
    if (!m_benchTimer.isActive()) {
        connect(&m_benchTimer, &QTimer::timeout, this, &PetView::benchTick);
        m_benchTimer.start();
    }
}

void PetView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setRenderHint(QPainter::Antialiasing, true);

    const qreal pw = m_pack.width();
    const qreal ph = m_pack.height();
    const qreal baseY = height() - kPadBottom - ph;

    // 内容坐标原点。跳跃不在这里做——跳跃是移动窗口，这样窗口不用为
    // 「跳起来 50px」预留一块永远空着的透明区域。
    //
    // 呼吸也不用 scale 做：缩放会让四层每一帧都走一次双线性重采样，
    // 那是纯 CPU 白烧。改成整体上下平移 1~2px，视觉上更像呼吸（重心起伏），
    // 而且平移是 1:1 blit，零重采样。
    p.translate(kPadX, baseY - m_breathOffset);

    drawLayer(p, QStringLiteral("legs"));
    drawLayer(p, QStringLiteral("body"));

    // 头部：以颈部为轴心旋转。这是 petpack 里 neck_y 的唯一用途。
    p.save();
    p.translate(pw / 2.0 + m_offX, m_pack.neckY() + m_offY);
    p.rotate(m_angleDeg);
    p.translate(-pw / 2.0, -m_pack.neckY());
    drawLayer(p, QStringLiteral("head"));
    if (!m_expr.isEmpty())
        drawLayer(p, QStringLiteral("expr_") + m_expr);
    p.restore();

    // 衣领遮罩：固定在身体上，不跟头动。头偏移时靠它盖住脖子缺口。
    drawLayer(p, QStringLiteral("collar"));

    ++m_frameCount;
    ++m_benchFrames;
}

void PetView::drawLayer(QPainter &p, const QString &name)
{
    const Layer *l = name.startsWith(QLatin1String("expr_"))
                         ? m_pack.expr(name.mid(5))
                         : m_pack.layer(name);
    if (!l || l->isNull())
        return;
    // 加载时已缩放到目标尺寸并裁过透明边，这里是 1:1 blit，无重采样
    p.drawPixmap(l->offset, l->pixmap);
}

void PetView::setExpr(const QString &name, int durationMs)
{
    m_expr = name;
    m_exprUntil = m_clock.elapsed() + durationMs;
    // 短表情（眨眼）必须立刻提到满帧，否则 idle 的 8fps 会把 140ms 的眨眼吞掉
    if (durationMs > 0 && durationMs < 2000) {
        m_activeUntil = m_clock.elapsed() + durationMs + 150;
        applyFrameRate(true);
    }
}

void PetView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_jumpY >= 0.0 && m_jumpVy == 0.0)
            m_jumpVy = kJumpV0;
        setExpr(QStringLiteral("happy"), 1100);
        m_lastInput = m_clock.elapsed();
    }
    QWidget::mousePressEvent(event);
}

void PetView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    static bool placed = false;
    if (placed)
        return;
    placed = true;

    QScreen *scr = screen() ? screen() : QGuiApplication::primaryScreen();
    if (!scr)
        return;
    const QRect avail = scr->availableGeometry();
    // 让「内容底边」正好落在可用区域底边：窗口底要往外多探 kPadBottom
    m_basePos = QPoint(avail.right() - width() - 60, avail.bottom() - height() + kPadBottom);
    move(m_basePos);
}

void PetView::updateFollowTarget(qreal dt)
{
    // 缓动系数按 dt 归一，否则改帧率会让动作速度跟着变
    const qreal k = 1.0 - std::exp(-dt * 4.8);

    if (!m_followCursor) {
        m_angleDeg += (0.0 - m_angleDeg) * k;
        m_offX += (0.0 - m_offX) * k;
        m_offY += (0.0 - m_offY) * k;
        return;
    }

    qreal nx = 0.0;
    qreal ny = 0.0;

    if (m_benchMode == BenchMode::Animate) {
        // 合成一段持续摆动，用来测「一直在跟随」时的 CPU 上界。
        // 沙箱里光标不会自己动，靠 QCursor::pos() 测不出这个场景。
        const qreal t = m_clock.elapsed() / 1000.0;
        nx = qSin(t * 1.15);
        ny = qSin(t * 0.73);
    } else {
        const QPoint cursor = QCursor::pos();
        if (cursor != m_cursorPos) {
            m_cursorPos = cursor;
            m_lastInput = m_clock.elapsed();
            if (m_expr == QLatin1String("sleepy"))
                setExpr(QString(), 0);  // 一动就醒
        }

        const QPoint center = mapToGlobal(QPoint(width() / 2, height() / 2));
        QScreen *scr = screen() ? screen() : QGuiApplication::primaryScreen();
        const QSize scrSize = scr ? scr->geometry().size() : QSize(1920, 1080);

        // 光标相对桌宠的水平距离，映射到 ±1，再乘最大角度。
        // 分母用半屏而非窗口宽度：桌宠很小，按窗口宽度算会一动就到极限。
        nx = qreal(cursor.x() - center.x()) / (scrSize.width() * 0.42);
        ny = qreal(cursor.y() - (center.y() - m_pack.height() * 0.25))
           / (scrSize.height() * 0.42);
        nx = qBound(-1.0, nx, 1.0);
        ny = qBound(-1.0, ny, 1.0);
    }

    m_angleDeg += (nx * kMaxAngle - m_angleDeg) * k;
    m_offX += (nx * kMaxOffX - m_offX) * k;
    m_offY += (ny * kMaxOffY - m_offY) * k;
}

void PetView::tick()
{
    const qint64 now = m_clock.elapsed();
    qreal dt = (now - m_lastMs) / 1000.0;
    m_lastMs = now;
    if (dt <= 0.0)
        dt = 1.0 / 60.0;
    if (dt > 0.25)  // 掉帧或挂起后不要让物理跳变
        dt = 0.25;

    // 呼吸：整体上下浮动。这里是位移不是缩放，理由见上。
    m_breathOffset = qSin(2.0 * M_PI * (now / 1000.0) / kBreathPeriod) * kBreathAmp;

    // --- 跳跃物理 ---
    if (m_jumpY < 0.0 || m_jumpVy < 0.0) {
        m_jumpVy += kGravity * dt;
        m_jumpY += m_jumpVy * dt;
        if (m_jumpY >= 0.0) {
            m_jumpY = 0.0;
            m_jumpVy = 0.0;
        }
    }
    // 跳跃用窗口位移：只在跨过整像素时才 move，一次跳跃约 13 次
    if (!m_basePos.isNull()) {
        const int jy = qRound(m_jumpY);
        if (jy != m_appliedJump) {
            m_appliedJump = jy;
            move(m_basePos + QPoint(0, jy));
        }
    }

    updateFollowTarget(dt);

    // --- 表情状态机 ---
    if (now > m_exprUntil) {
        if (now - m_lastInput > 20000) {
            if (m_expr != QLatin1String("sleepy"))
                setExpr(QStringLiteral("sleepy"), 1 << 30);
        } else {
            if (!m_expr.isEmpty())
                setExpr(QString(), 0);
            if (now > m_nextBlink) {
                const bool wink = QRandomGenerator::global()->bounded(4) == 0;
                setExpr(wink ? QStringLiteral("wink") : QStringLiteral("blink"), 140);
                m_nextBlink = now + 2600 + QRandomGenerator::global()->bounded(4000);
            }
        }
    }

    // --- 动态帧率 ---
    // 判定标准是「这一帧画面真的变了」，而不是「有定时器在跑」。静止时
    // 只有呼吸在动，8fps 足够；一旦头部在追、在跳、在眨眼，就上满帧。
    const bool headMoving = qAbs(m_angleDeg - m_prevAngle) > 0.05
                         || qAbs(m_offX - m_prevOffX) > 0.05;
    const bool jumping = m_jumpY < -0.5 || m_jumpVy != 0.0;
    const bool exprAnim = now < m_exprUntil && (m_exprUntil - now) < 400;
    applyFrameRate(headMoving || jumping || exprAnim || now < m_activeUntil);
    m_prevAngle = m_angleDeg;
    m_prevOffX = m_offX;

    update();
}

void PetView::benchTick()
{
    const qint64 nowMs = m_clock.elapsed();
    const qint64 dur = nowMs - m_benchStart;
    const double elapsed = qMax(0.001, dur / 1000.0);

    const ProcSample s = sampleProcess();
    m_benchRssPeak = qMax(m_benchRssPeak, s.rssBytes);
    const double cpuPct = (s.cpuSeconds - m_benchCpuStart) / elapsed * 100.0;
    const qint64 dFrames = m_benchFrames - m_benchLastFrames;
    m_benchLastFrames = m_benchFrames;

    std::printf("[bench] t=%4.1fs cpu=%6.2f%% rss=%5.1fMB fps=%5.1f\n", elapsed, cpuPct,
                s.rssBytes / 1048576.0, double(dFrames));
    std::fflush(stdout);

    if (dur >= qint64(m_benchSeconds) * 1000) {
        m_benchTimer.stop();
        std::printf("[bench] RESULT cpu=%.2f%% rss_peak=%.1fMB fps=%.1f frames=%lld secs=%.1f\n",
                    cpuPct, m_benchRssPeak / 1048576.0, double(m_benchFrames) / elapsed,
                    static_cast<long long>(m_benchFrames), elapsed);
        std::fflush(stdout);
        QCoreApplication::quit();
    }
}

} // namespace pet
