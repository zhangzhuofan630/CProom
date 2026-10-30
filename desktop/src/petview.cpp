#include "petview.h"

#include <QCursor>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QScreen>
#include <QShowEvent>
#include <QWindow>
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
//   kPadTop  —— 跳跃/拖拽都靠移动窗口实现（内容不做位移），顶部不需要
//              预留「跳起来 50px」那块永远空着的透明区域。
constexpr int kPadX = 4;
constexpr int kPadTop = 8;
constexpr int kPadBottom = 4;

constexpr qreal kMaxAngle = PetPack::kMaxHeadAngleDeg;  ///< ±22° 开始「断头」，12° 是实测安全值
constexpr qreal kMaxOffX = 8.0;     ///< px
constexpr qreal kMaxOffY = 4.0;
constexpr qreal kBreathAmp = 1.8;  ///< 呼吸位移幅度（px）
constexpr qreal kBreathPeriod = 4.2;  ///< 秒

constexpr qreal kJumpV0 = -430.0;     ///< px/s，点一下的起跳速度
constexpr qreal kGravity = 1900.0;    ///< px/s²
constexpr qreal kAirDragX = 1.8;      ///< 空中水平阻尼（1/s）
constexpr qreal kGroundFriction = 14.0;  ///< 落地后的地面摩擦（1/s），比空气阻尼大一个量级
constexpr qreal kStopVx = 20.0;       ///< 落地后横向速度低于此值直接归零（px/s）
constexpr qreal kBounceMin = 620.0;   ///< 触地速度低于此值不再弹，直接站住
constexpr qreal kRestitution = 0.30;  ///< 触地回弹系数
constexpr qreal kMaxThrow = 2400.0;   ///< 甩出速度上限，防止一挥手把它扔到屏幕外

constexpr qreal kDragThreshold = 4.0;  ///< 按下后位移超过此值（px）才算拖动，否则算点击
constexpr qint64 kClickMaxMs = 400;     ///< 按住超过这么久就不算点击

constexpr qint64 kWaveDurationMs = 2600;  ///< 一次挥手持续多久
constexpr qint64 kWaveFadeMs = 220;       ///< 首尾淡入淡出的时长
constexpr qreal kWaveHz = 1.6;            ///< 手臂摆动频率
constexpr qreal kWaveMid = -7.0;          ///< 摆动中心角（度），正 = 顺时针 = 往下放
constexpr qreal kWaveAmp = 11.0;          ///< 摆幅
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
    m_velX = 0.0;
    m_velY = 0.0;
    m_airborne = false;
    m_dragging = false;
    m_squash = 0.0;
    m_angleJitter = 0.0;
    m_waveUntil = 0;
    m_armAngle = 0.0;
    m_expr = exprName;
    m_exprUntil = 1 << 30;
    update();
}

void PetView::throwPet(qreal vx, qreal vy)
{
    updateGround();
    m_velX = vx;
    m_velY = vy;
    m_airborne = true;
    m_dragging = false;
    m_lastInput = m_clock.elapsed();
    setExpr(QStringLiteral("surprised"), 900);
}

void PetView::setTrace(bool on)
{
    m_trace = on;
    m_traceNext = 0;
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

QRect PetView::currentAvail() const
{
    QScreen *scr = windowHandle() ? windowHandle()->screen() : nullptr;
    if (!scr)
        scr = screen();
    if (!scr)
        scr = QGuiApplication::primaryScreen();
    return scr ? scr->availableGeometry() : QRect(0, 0, 1920, 1080);
}

void PetView::updateGround()
{
    const QRect avail = currentAvail();
    // 让「内容底边」正好落在可用区域底边（任务栏上沿）：
    // 窗口高度含 kPadBottom 的内边距，所以要多减一层。
    m_groundY = avail.bottom() + 1 - height() + kPadBottom;
}

void PetView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setRenderHint(QPainter::Antialiasing, true);

    const qreal pw = m_pack.width();
    const qreal ph = m_pack.height();
    const qreal baseY = height() - kPadBottom - ph;

    p.save();

    // 落地压扁：以内容底边中点为锚点整体缩放。
    // 只在 150ms 内发生（m_squash 指数衰减到 0.01 以下就跳过），
    // 短暂重采样换手感是值得的；常驻缩放才是性能陷阱。
    if (m_squash > 0.01) {
        const qreal sx = 1.0 + 0.10 * m_squash;
        const qreal sy = 1.0 - 0.14 * m_squash;
        const qreal ax = kPadX + pw * 0.5;
        const qreal ay = baseY + ph;
        p.translate(ax, ay);
        p.scale(sx, sy);
        p.translate(-ax, -ay);
    }

    // 内容坐标原点。跳跃、拖拽、落体全都不在这里做——那些是移动窗口，
    // 这样窗口不用为「跳起来 50px」预留一块永远空着的透明区域。
    //
    // 呼吸也不用 scale 做：缩放会让四层每一帧都走一次双线性重采样，
    // 那是纯 CPU 白烧。改成整体上下平移 1~2px，视觉上更像呼吸（重心起伏），
    // 而且平移是 1:1 blit，零重采样。
    p.translate(kPadX, baseY - m_breathOffset);

    drawLayer(p, QStringLiteral("legs"));

    // 挥手时身体换成「右臂被挖掉」的那版，否则会同时看到插兜的袖子和举起的手臂。
    // mix 在 0..1 之间时是新旧两版交叠（淡化过渡期），此时先铺挥手版，
    // 再把常态版按 (1-mix) 盖上去 —— 两版只在右臂那一片不同，别处都是同一批像素，
    // 所以重叠区域以外不会有任何变化。
    const qreal mix = waveMix();
    if (mix > 0.001 && m_pack.bodyWave()) {
        const Layer *bw = m_pack.bodyWave();
        p.drawPixmap(bw->offset, bw->pixmap);
        if (mix < 0.999) {
            p.setOpacity(1.0 - mix);
            drawLayer(p, QStringLiteral("body"));
            p.setOpacity(1.0);
        }
    } else {
        drawLayer(p, QStringLiteral("body"));
    }

    // 手臂排在 body 之后、head 之前：
    //   · 放到 body 之前会被身体压住
    //   · 放到 head 之后，臂根那块被裁成直边的白肉会露在外面
    // 手掌在头右侧、不与脸重叠，被 head 盖住没有副作用。
    if (mix > 0.001) {
        p.setOpacity(mix);
        drawArm(p, m_armAngle);
        p.setOpacity(1.0);
    }

    // 头部：以颈部为轴心旋转。这是 petpack 里 neck_y 的唯一用途。
    p.save();
    p.translate(pw / 2.0 + m_offX, m_pack.neckY() + m_offY);
    p.rotate(m_angleDeg + m_angleJitter);
    p.translate(-pw / 2.0, -m_pack.neckY());
    drawLayer(p, QStringLiteral("head"));
    if (!m_expr.isEmpty())
        drawLayer(p, QStringLiteral("expr_") + m_expr);
    p.restore();

    // 衣领遮罩：固定在身体上，不跟头动。头偏移时靠它盖住脖子缺口。
    drawLayer(p, QStringLiteral("collar"));

    p.restore();

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

void PetView::drawArm(QPainter &p, qreal angleDeg)
{
    const Layer *a = m_pack.arm();
    if (!a || a->isNull())
        return;

    // 绕肩关节旋转。臂层自带一块以肩为圆心的圆形补丁，转动时它跟着转，
    // 臂根那道缝始终被盖住——这就是球关节。
    const QPointF pivot = m_pack.armPivot();
    p.save();
    p.translate(pivot);
    p.rotate(angleDeg);
    p.translate(-pivot);
    p.drawPixmap(a->offset, a->pixmap);
    p.restore();
}

void PetView::snapWave(qreal armAngleDeg)
{
    // 用「永远不到期」把挥手姿态钉住；配合 freeze() 时定时器已经停了，
    // tick 不会再改写 m_armAngle。起始时刻推到很久以前，让 waveMix() 恒为 1
    // （否则截图会抓到淡化途中的半透明状态）。
    m_waveStart = m_clock.elapsed() - kWaveDurationMs * 4;
    m_waveUntil = 1LL << 40;
    m_armAngle = armAngleDeg;
    update();
}

qreal PetView::waveMix() const
{
    if (m_waveUntil <= 0)
        return 0.0;
    const qint64 now = m_clock.elapsed();
    const qint64 remain = m_waveUntil - now;
    if (remain <= 0)
        return 0.0;
    const qreal fadeIn = qBound(0.0, double(now - m_waveStart) / kWaveFadeMs, 1.0);
    const qreal fadeOut = qBound(0.0, double(remain) / kWaveFadeMs, 1.0);
    return qMin(fadeIn, fadeOut);
}

void PetView::startWave()
{
    if (!m_pack.hasArm() || !m_pack.bodyWave()) {
        std::printf("[wave] 这个 petpack 没有手臂层，跳过\n");
        return;
    }
    const qint64 now = m_clock.elapsed();
    m_waveStart = now;
    m_waveUntil = now + kWaveDurationMs;
    m_lastInput = now;
    m_activeUntil = now + kWaveDurationMs;
    m_followCursor = true;
    setExpr(QStringLiteral("happy"), int(kWaveDurationMs));
    applyFrameRate(true);
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
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const qint64 now = m_clock.elapsed();
    m_dragging = true;
    m_dragMoved = false;
    // 一按下就脱离地面：手指按住的瞬间它不该继续往下掉
    m_airborne = true;
    m_velX = 0.0;
    m_velY = 0.0;
    m_pressMs = now;
    m_dragLastMs = now;
    m_lastInput = now;

    const QPoint gp = event->globalPosition().toPoint();
    m_grabOffset = gp - pos();
    m_pressGlobal = gp;

    setCursor(Qt::ClosedHandCursor);
    setExpr(QStringLiteral("surprised"), 1 << 30);
    m_activeUntil = now + 300;
    event->accept();
}

void PetView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    // 不用 event->position()：窗口被 move 之后，局部坐标会跟着变，
    // 而全局坐标不受影响，用全局坐标减按下时的抓取偏移才是稳的。
    const QPoint gp = event->globalPosition().toPoint();

    // 拖动阈值要跟「按下时的鼠标位置」比，不能跟窗口当前位置比：
    // 慢慢挪的时候每帧只差 1px，跟窗口比永远超不过阈值，会被误判成点击。
    if (!m_dragMoved) {
        if (qAbs(gp.x() - m_pressGlobal.x()) <= kDragThreshold
            && qAbs(gp.y() - m_pressGlobal.y()) <= kDragThreshold) {
            event->accept();  // 还在死区内：窗口不动，避免手抖把桌宠挪走
            return;
        }
        m_dragMoved = true;
        m_dragLastMs = m_clock.elapsed();
    }

    const qreal rawX = gp.x() - m_grabOffset.x();
    const qreal rawY = gp.y() - m_grabOffset.y();

    // 拖动时必须钳在屏幕内。不钳的话可以把它拖到屏幕外面，松手后窗口
    // 就再也点不到了——用户会以为桌宠没了。
    updateGround();
    const QRect avail = currentAvail();
    const qreal maxX = qMax(qreal(avail.left()),
                            qreal(avail.left() + avail.width() - width()));
    const qreal nx = qBound(qreal(avail.left()), rawX, maxX);
    const qreal ny = qBound(qreal(avail.top()), rawY, m_groundY);

    const qint64 now = m_clock.elapsed();
    const qreal dt = (now - m_dragLastMs) / 1000.0;
    if (dt > 0.0005) {
        // 速度要用「钳制后」的位移算：贴着边拖时速度为 0，松手不会弹射出去
        const qreal ivx = (nx - m_posX) / dt;
        const qreal ivy = (ny - m_posY) / dt;
        // 低通滤波：鼠标事件本身有抖动，直接拿瞬时速度会在松手时把它甩飞
        m_velX = m_velX * 0.55 + ivx * 0.45;
        m_velY = m_velY * 0.55 + ivy * 0.45;
        m_dragLastMs = now;
    }

    m_posX = nx;
    m_posY = ny;

    const int ix = qRound(m_posX);
    const int iy = qRound(m_posY);
    if (ix != m_appliedX || iy != m_appliedY) {
        m_appliedX = ix;
        m_appliedY = iy;
        move(ix, iy);
    }
    event->accept();
}

void PetView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_dragging) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    m_dragging = false;
    setCursor(Qt::PointingHandCursor);

    const qint64 now = m_clock.elapsed();
    m_lastInput = now;
    updateGround();

    if (!m_dragMoved && now - m_pressMs < kClickMaxMs) {
        // 点一下：原地小跳。和落体走同一套积分，没有第二套动画系统
        m_velX = 0.0;
        m_velY = kJumpV0;
        m_airborne = true;
        setExpr(QStringLiteral("happy"), 1100);
    } else {
        // 甩出去：限速，别让一次挥手把它扔到屏幕外
        m_velX = qBound(-kMaxThrow, m_velX, kMaxThrow);
        m_velY = qBound(-kMaxThrow, m_velY, kMaxThrow);
        m_airborne = true;
        // 拖到任务栏下方松手时，别让它在屏幕外往下坠
        if (m_posY > m_groundY)
            m_posY = m_groundY;
        setExpr(QStringLiteral("happy"), 700);
    }

    m_activeUntil = now + 400;
    applyFrameRate(true);
    event->accept();
}

void PetView::mouseDoubleClickEvent(QMouseEvent *event)
{
    // 双击 = 挥挥手。单击是跳，所以不能都放在 press 里。
    if (event->button() == Qt::LeftButton) {
        startWave();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void PetView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    static bool placed = false;
    if (placed)
        return;
    placed = true;

    const QRect avail = currentAvail();
    updateGround();

    // 右下角，离右边 60px。m_posX/m_posY 是唯一的位置真相，
    // 窗口坐标只是它的整数投影，校验过一次就不再依赖 frameGeometry()。
    m_posX = avail.left() + avail.width() - width() - 60;
    m_posY = m_groundY;
    m_appliedX = qRound(m_posX);
    m_appliedY = qRound(m_posY);
    move(m_appliedX, m_appliedY);
}

void PetView::updateFollowTarget(qreal dt)
{
    // 拎在半空时不该转头看光标——手是把它吊起来的，头跟着光标转会显得断线
    if (m_dragging)
        return;

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
        const QRect avail = currentAvail();
        const QSize scrSize = avail.size();

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

void PetView::stepPhysics(qreal dt)
{
    if (m_dragging)
        return;

    updateGround();
    const QRect avail = currentAvail();

    // --- 水平：空中靠空气阻尼，落地后靠地面摩擦 ---
    if (qAbs(m_velX) > 0.5) {
        m_posX += m_velX * dt;
        // 落地后必须换成大得多的摩擦系数：一开始两处都用空气阻尼，
        // 结果甩一下它能在屏幕上滑三秒多，看着像踩在冰上。
        m_velX *= std::exp(-dt * (m_airborne ? kAirDragX : kGroundFriction));

        const qreal minX = avail.left();
        const qreal maxX = avail.left() + avail.width() - width();
        if (m_posX < minX) {
            m_posX = minX;
            m_velX = -m_velX * 0.35;
        } else if (m_posX > maxX) {
            m_posX = maxX;
            m_velX = -m_velX * 0.35;
        }
    } else if (m_velX != 0.0) {
        m_velX = 0.0;
    }
    // 摩擦是指数衰减，永远逼近但不到零；落地后必须有个截断，
    // 否则会为了最后 1px 的位移继续满帧重绘好几秒。
    if (!m_airborne && qAbs(m_velX) < kStopVx)
        m_velX = 0.0;

    // --- 垂直：重力积分 ---
    if (m_airborne || m_velY != 0.0) {
        m_velY += kGravity * dt;
        m_posY += m_velY * dt;

        if (m_posY >= m_groundY) {
            m_posY = m_groundY;
            m_velX *= 0.5;  // 触地一次吃掉一半横向速度：落地是擦地，不是弹球
            if (m_velY > kBounceMin) {
                // 砸得够重才弹：小跳落地直接站住，从高处摔下来才有回弹
                m_velY = -m_velY * kRestitution;
                m_squash = 1.0;
                m_angleJitter = qBound(-2.5, m_velY / 260.0, 2.5);
            } else {
                const qreal impact = m_velY;
                m_velY = 0.0;
                if (m_airborne) {
                    m_airborne = false;
                    m_squash = qMax(m_squash, qMin(1.0, impact / 700.0));
                    m_angleJitter = 0.0;
                    m_lastInput = m_clock.elapsed();
                    if (m_expr.isEmpty() || m_expr == QLatin1String("surprised"))
                        setExpr(QStringLiteral("happy"), 500);
                }
            }
        }
    }

    // --- 落地形变衰减 ---
    if (m_squash > 0.001)
        m_squash *= std::exp(-dt * 9.0);
    else
        m_squash = 0.0;

    if (qAbs(m_angleJitter) > 0.01)
        m_angleJitter *= std::exp(-dt * 7.0);
    else
        m_angleJitter = 0.0;

    // 只在跨过整像素时才 move：一次跳跃约 13 次窗口移动，不是每帧都动
    const int ix = qRound(m_posX);
    const int iy = qRound(m_posY);
    if (ix != m_appliedX || iy != m_appliedY) {
        m_appliedX = ix;
        m_appliedY = iy;
        move(ix, iy);
    }
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

    stepPhysics(dt);
    updateFollowTarget(dt);

    // --- 挥手 ---
    if (m_waveUntil > 0) {
        if (now >= m_waveUntil) {
            m_waveUntil = 0;
            m_armAngle = 0.0;
        } else {
            // 用一个正弦摆：比关键帧省事，且频率/幅度随时可调。
            // 相位从 m_waveStart 算，淡化不影响摆动进度。
            const qreal t = (now - m_waveStart) / 1000.0;
            m_armAngle = kWaveMid + qSin(t * 2.0 * M_PI * kWaveHz) * kWaveAmp;
        }
    }

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
    // 只有呼吸在动，8fps 足够；一旦头部在追、在跳、在落地形变，就上满帧。
    const bool headMoving = qAbs(m_angleDeg - m_prevAngle) > 0.05
                         || qAbs(m_offX - m_prevOffX) > 0.05;
    const bool physicsMoving = m_airborne || m_dragging || m_squash > 0.01
                            || qAbs(m_velX) > 1.0;
    const bool exprAnim = now < m_exprUntil && (m_exprUntil - now) < 400;
    applyFrameRate(headMoving || physicsMoving || exprAnim || m_waveUntil > 0
                   || now < m_activeUntil);
    m_prevAngle = m_angleDeg;
    m_prevOffX = m_offX;

    if (m_trace && now >= m_traceNext) {
        m_traceNext = now + 100;
        std::printf("[trace] t=%6.2fs pos=(%7.1f,%7.1f) vel=(%7.0f,%7.0f) air=%d drag=%d "
                    "squash=%4.2f ground=%7.1f arm=(%5.1fdeg,mix=%.2f)\n",
                    now / 1000.0, m_posX, m_posY, m_velX, m_velY, int(m_airborne),
                    int(m_dragging), m_squash, m_groundY, m_armAngle, waveMix());
        std::fflush(stdout);
    }

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
