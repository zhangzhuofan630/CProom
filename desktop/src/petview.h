#pragma once

#include <QElapsedTimer>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <climits>

#include "petpack.h"

namespace pet {

/// 桌宠渲染窗口。
///
/// S0.9 的职责是测渲染开销，S1.0 加上交互：把它拎起来、甩出去、看着它
/// 落到任务栏上弹一下。交互用的是同一套物理——跳跃只是「给一个向上的
/// 初速度」，和拖拽落体共用一条积分路径，没有第二套动画系统。
///
/// 刻意保持极简：没有设置界面、没有托盘、没有配置文件。全部参数走命令行，
/// 这样 bench 模式可以无人值守地跑。
class PetView : public QWidget
{
    Q_OBJECT

public:
    enum class BenchMode { Off, Idle, Follow, Animate };

    explicit PetView(const PetPack &pack, QWidget *parent = nullptr);

    /// active = 有动作（跟随/眨眼/跳跃）时的帧率；idle = 纯呼吸时的帧率。
    ///
    /// 分开设是有实测依据的：xcb 下满帧 15fps 恒定重绘要 1.22% CPU，超过
    /// 「空闲 < 1%」的指标。而静止时呼吸周期 4.2s，8fps 已经看不出跳跃，
    /// 没必要为它付满帧的代价。
    void setTargetFps(int activeFps, int idleFps);
    void setBench(int seconds, BenchMode mode);

    /// 冻结到指定姿态并停止动画，用于截图核对渲染结果。
    /// （XWayland 下抓不到根窗口，只能让程序自己 render 出图。）
    void freeze(qreal angleDeg, const QString &exprName);

    /// 给桌宠一个初速度（px/s，y 向上为负）。无头环境下验证落体用。
    void throwPet(qreal vx, qreal vy);

    /// 挥一次手（也可以用它做无头验证：--wave）。
    void startWave();

    /// 冻结在某个手臂角度并保持挥手姿态，用于截图核对（配合 freeze 使用）。
    void snapWave(qreal armAngleDeg);

    /// 每 100ms 打印一次位置与速度，配合 --throw 观察轨迹。
    void setTrace(bool on);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void tick();
    void drawLayer(QPainter &p, const QString &name);
    void drawArm(QPainter &p, qreal angleDeg);
    void setExpr(const QString &name, int durationMs);
    void updateFollowTarget(qreal dt);
    void stepPhysics(qreal dt);
    void updateGround();
    void benchTick();
    void applyFrameRate(bool active);
    QRect currentAvail() const;
    bool isWaving() const { return m_waveUntil > 0; }

    /// 挥手整体的「浓度」：1 = 完全挥手，0 = 完全是常态。首尾各用 220ms 过渡。
    ///
    /// 臂层只有「举起」这一个姿态，没有「垂下」，所以抬手和收手都没法用旋转
    /// 演出来——转多了手臂会扎进躯干。既然演不出来就淡化：两个 body 版本只有
    /// 右臂那一片像素不同，其余完全一致，叠上去不会在别处产生重影。
    qreal waveMix() const;

    const PetPack &m_pack;

    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_lastMs = 0;

    // --- 姿态状态 ---
    qreal m_breathOffset = 0.0;  ///< 呼吸的垂直位移（px）
    qreal m_angleDeg = 0.0;   ///< 头部旋转
    qreal m_offX = 0.0;       ///< 头部水平偏移（跟随光标）
    qreal m_offY = 0.0;       ///< 头部垂直偏移

    // --- 窗口位置与物理（跳跃和落体共用一套积分） ---
    //
    // 位置存 qreal 而不是 QPoint：拖拽时每帧位移常常不足 1px，用整数
    // 位置会被反复取整，慢速拖动看起来像卡帧。窗口本身仍然是整数坐标，
    // 只在 move() 那一刻取整。
    qreal m_posX = 0.0;
    qreal m_posY = 0.0;
    qreal m_velX = 0.0;
    qreal m_velY = 0.0;
    qreal m_groundY = 0.0;    ///< 站立时窗口左上角的 y（随所在屏幕重算）
    bool m_airborne = false;  ///< 不在地面：受重力支配
    int m_appliedX = INT_MIN;  ///< 已 move 到的整数坐标，避免重复 move
    int m_appliedY = INT_MIN;

    // --- 拖拽 ---
    bool m_dragging = false;
    bool m_dragMoved = false;  ///< 位移超过阈值，判定为拖动而非点击
    QPoint m_grabOffset;       ///< 按下时鼠标相对窗口左上角的偏移
    QPoint m_pressGlobal;      ///< 按下时的鼠标全局位置，用于算拖动阈值
    qint64 m_dragLastMs = 0;
    qint64 m_pressMs = 0;

    // --- 落地压扁 ---
    qreal m_squash = 0.0;  ///< 0..1，落地瞬间置 1，指数衰减
    qreal m_angleJitter = 0.0;  ///< 落地时头部被震一下

    // --- 挥手（手臂骨骼）---
    //
    // 挥手要同时做两件事：把 body 换成"没有右臂"的挥手版，再让手臂层绕肩轴
    // 摆动。只换其一都不成立——不换 body 会看见两条手臂，不摆手就是张静态图。
    qint64 m_waveUntil = 0;   ///< 0 = 没在挥手
    qint64 m_waveStart = 0;   ///< 挥手开始的时刻，淡化与摆动相位都从它算
    qreal m_armAngle = 0.0;

    // --- 表情状态机 ---
    QString m_expr;
    qint64 m_exprUntil = 0;
    qint64 m_nextBlink = 0;
    qint64 m_lastInput = 0;   ///< 0 = 启动时刻，别设负数，否则一启动就被判成「闲置 20 秒」

    // --- 鼠标跟随 ---
    QPoint m_cursorPos;
    bool m_followCursor = true;

    // --- 动态帧率 ---
    int m_activeFps = 30;
    int m_idleFps = 8;
    int m_appliedFps = 0;
    qint64 m_activeUntil = 0;   ///< 事件触发的强制高帧率窗口
    qreal m_prevAngle = 0.0;
    qreal m_prevOffX = 0.0;

    // --- trace ---
    bool m_trace = false;
    qint64 m_traceNext = 0;

    // --- bench ---
    BenchMode m_benchMode = BenchMode::Off;
    int m_benchSeconds = 0;
    qint64 m_benchStart = 0;
    qint64 m_benchNextReport = 0;
    int m_frameCount = 0;
    qint64 m_benchFrames = 0;
    qint64 m_benchLastFrames = 0;
    double m_benchCpuStart = 0.0;
    qint64 m_benchRssPeak = 0;
    QTimer m_benchTimer;
};

} // namespace pet
