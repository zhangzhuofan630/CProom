#pragma once

#include <QElapsedTimer>
#include <QPoint>
#include <QString>
#include <QTimer>
#include <QWidget>

#include "petpack.h"

namespace pet {

/// 桌宠渲染窗口。
///
/// 这一版的唯一目的：把 web demo 里验证过的分层合成逻辑搬到 Qt 里，
/// 然后测出真实 CPU 与内存。web demo 的 60fps 不能作为依据——Canvas 2D
/// 与 QPainter 后端是两套东西。
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

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void tick();
    void drawLayer(QPainter &p, const QString &name);
    void setExpr(const QString &name, int durationMs);
    void updateFollowTarget(qreal dt);
    void benchTick();
    void applyFrameRate(bool active);

    const PetPack &m_pack;

    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_lastMs = 0;

    // --- 姿态状态 ---
    qreal m_breathOffset = 0.0;  ///< 呼吸的垂直位移（px）
    qreal m_angleDeg = 0.0;   ///< 头部旋转
    qreal m_offX = 0.0;       ///< 头部水平偏移（跟随光标）
    qreal m_offY = 0.0;       ///< 头部垂直偏移
    qreal m_jumpY = 0.0;      ///< 跳跃位移（负数 = 向上），通过移动窗口实现
    qreal m_jumpVy = 0.0;
    QPoint m_basePos;         ///< 站立时的窗口位置
    int m_appliedJump = 0;    ///< 已应用的跳跃像素，避免重复 move

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
