#pragma once

#include <QHash>
#include <QPixmap>
#include <QString>
#include <QStringList>

namespace pet {

/// 一层图像的运行时表示。
///
/// 关键设计：加载时就缩放到目标尺寸并裁掉透明边，绘制时是 1:1 纯 blit，
/// 不做任何缩放。桌宠每帧都在画，把缩放成本从「每帧」挪到「加载时」。
///
/// 裁剪带来一个必须配套的东西：offset。层不再是全画布尺寸，绘制前要知道
/// 它原本贴在画布的哪个位置。这就是 docs/petpack-spec-draft.md 里
/// 「裁剪省体积但要记 offset」那条取舍的具体落地。
struct Layer {
    QPixmap pixmap;
    QPoint offset;  ///< 层左上角在画布中的坐标（已按缩放后尺寸）

    bool isNull() const { return pixmap.isNull(); }
};

/// 一个 petpack 目录的运行时数据。
///
/// 只读取，不解释动画/骨架——S0.9 的目的是测渲染开销，不是实现完整规范。
class PetPack
{
public:
    /// 头部最大旋转角度（度）。
    ///
    /// 这个值有两个用途，必须同源：渲染时用它限幅，加载时用它把「旋转后
    /// 会扫到的区域」算进窗口尺寸。写死在这里是因为它是这套 petpack 的
    /// 属性（±22° 开始「断头」，12° 是实测安全值），换形象要重测。
    static constexpr qreal kMaxHeadAngleDeg = 12.0;

    /// 加载 dir 下的四层 + 全部 expr_*.png。
    /// targetWidth 是桌宠在屏幕上的显示宽度（px），层会缩放到这个宽度。
    bool load(const QString &dir, int targetWidth, QString *error = nullptr);

    /// 返回 nullptr 表示没有这一层。
    /// 注意不能返回引用——QHash::value() 返回的是值，取引用会悬空。
    const Layer *layer(const QString &name) const
    {
        const auto it = m_layers.constFind(name);
        return it == m_layers.constEnd() ? nullptr : &it.value();
    }

    const Layer *expr(const QString &name) const
    {
        const auto it = m_exprs.constFind(name);
        return it == m_exprs.constEnd() ? nullptr : &it.value();
    }

    QStringList layerNames() const { return m_layerNames; }
    QStringList exprNames() const { return m_exprs.keys(); }

    int width() const { return m_width; }
    int height() const { return m_height; }
    qreal neckY() const { return m_neckY; }

    /// 所有层图像占用的像素内存估算（字节）
    qint64 memoryBytes() const;

private:
    QHash<QString, Layer> m_layers;
    QHash<QString, Layer> m_exprs;
    QStringList m_layerNames;
    int m_width = 0;
    int m_height = 0;
    qreal m_neckY = 0;
};

} // namespace pet
