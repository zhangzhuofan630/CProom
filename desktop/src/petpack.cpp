#include "petpack.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRect>
#include <QTransform>

namespace pet {

namespace {

/// 已缩放到目标宽度、但还没裁切的层。
struct ScaledLayer
{
    QImage img;
    QRect alphaBox;  ///< 不透明像素的包围盒（全透明时为空）
};

ScaledLayer scaleLayer(const QString &path, int targetWidth, int canvasH, qreal scale)
{
    ScaledLayer out;
    out.img = QImage(path);
    if (out.img.isNull())
        return out;

    out.img = out.img.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    const int tw = targetWidth;
    const int th = qMax(1, qRound(canvasH * scale));
    if (out.img.width() != tw || out.img.height() != th)
        out.img = out.img.scaled(tw, th, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    int minX = out.img.width(), minY = out.img.height(), maxX = -1, maxY = -1;
    for (int y = 0; y < out.img.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(out.img.constScanLine(y));
        for (int x = 0; x < out.img.width(); ++x) {
            if ((line[x] >> 24) != 0) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }
    if (maxX >= 0)
        out.alphaBox = QRect(QPoint(minX, minY), QPoint(maxX, maxY));
    return out;
}

Layer makeLayer(const ScaledLayer &src, const QPoint &origin)
{
    Layer out;
    if (src.alphaBox.isEmpty()) {
        out.pixmap = QPixmap(1, 1);
        out.pixmap.fill(Qt::transparent);
        return out;
    }
    out.pixmap = QPixmap::fromImage(src.img.copy(src.alphaBox));
    out.offset = src.alphaBox.topLeft() - origin;
    return out;
}

} // namespace

bool PetPack::load(const QString &dir, int targetWidth, QString *error)
{
    const QDir d(dir);
    if (!d.exists()) {
        if (error) *error = QStringLiteral("目录不存在: %1").arg(dir);
        return false;
    }

    // --- 读锚点 ---
    QFile f(d.filePath(QStringLiteral("anchors.json")));
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("读不到 anchors.json: %1").arg(f.errorString());
        return false;
    }
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    const QJsonArray canvas = root.value(QStringLiteral("canvas")).toArray();
    const int canvasW = canvas.at(0).toInt();
    const int canvasH = canvas.at(1).toInt();
    if (canvasW <= 0 || canvasH <= 0) {
        if (error) *error = QStringLiteral("anchors.json 里 canvas 无效");
        return false;
    }

    const qreal scale = qreal(targetWidth) / qreal(canvasW);

    // --- 四层，顺序决定 z-order ---
    m_layerNames = {QStringLiteral("legs"), QStringLiteral("body"),
                    QStringLiteral("head"), QStringLiteral("collar")};

    QHash<QString, ScaledLayer> scaled;
    QRect unionBox;
    for (const QString &nm : std::as_const(m_layerNames)) {
        const QString p = d.filePath(QStringLiteral("layers/%1.png").arg(nm));
        if (!QFileInfo::exists(p)) {
            if (error) *error = QStringLiteral("缺层文件: %1").arg(p);
            return false;
        }
        const ScaledLayer sl = scaleLayer(p, targetWidth, canvasH, scale);
        if (sl.img.isNull()) {
            if (error) *error = QStringLiteral("解码失败: %1").arg(p);
            return false;
        }
        unionBox = unionBox.united(sl.alphaBox);
        scaled.insert(nm, sl);
    }

    // 统一裁剪原点取四层的并集 + 头部旋转后扫到的区域。
    //
    // 这一条直接决定窗口大小，也就决定了每帧要合成多少像素：单独裁每层
    // 时窗口仍是整块画布，角色在原画布里偏左，右边一大块空气，那 40% 的
    // 透明面积每帧都在被合成和传输。
    //
    // 但不能只按静止包围盒裁——头部会绕颈部转 ±12°，头顶角点会甩出去。
    // 所以要把旋转后的 head 包围盒并进来，否则一转就削掉头发。
    const qreal neckYCanvas = root.value(QStringLiteral("neck_y")).toDouble() * scale;
    {
        const ScaledLayer head = scaled.value(QStringLiteral("head"));
        if (!head.alphaBox.isEmpty()) {
            const QPointF pivot(targetWidth / 2.0, neckYCanvas);
            for (const qreal deg : {-kMaxHeadAngleDeg, kMaxHeadAngleDeg}) {
                QTransform t;
                t.translate(pivot.x(), pivot.y());
                t.rotate(deg);
                t.translate(-pivot.x(), -pivot.y());
                unionBox = unionBox.united(t.mapRect(head.alphaBox));
            }
        }
    }

    // 留 2px 余量抗锯齿边缘
    unionBox = unionBox.adjusted(-2, -2, 2, 2)
                   .intersected(QRect(0, 0, targetWidth, qMax(1, qRound(canvasH * scale))));
    if (unionBox.isEmpty()) {
        if (error) *error = QStringLiteral("所有层都是空的，检查素材");
        return false;
    }

    m_width = unionBox.width();
    m_height = unionBox.height();
    m_neckY = neckYCanvas - unionBox.top();

    for (const QString &nm : std::as_const(m_layerNames))
        m_layers.insert(nm, makeLayer(scaled.value(nm), unionBox.topLeft()));

    // --- 表情补丁 ---
    const QDir layerDir(d.filePath(QStringLiteral("layers")));
    const QStringList exprs =
        layerDir.entryList({QStringLiteral("expr_*.png")}, QDir::Files, QDir::Name);
    for (const QString &fn : exprs) {
        QString key = fn;
        key.remove(QStringLiteral("expr_")).remove(QStringLiteral(".png"));
        const ScaledLayer sl = scaleLayer(layerDir.filePath(fn), targetWidth, canvasH, scale);
        if (!sl.img.isNull())
            m_exprs.insert(key, makeLayer(sl, unionBox.topLeft()));
    }

    return true;
}

qint64 PetPack::memoryBytes() const
{
    qint64 n = 0;
    auto add = [&n](const Layer &l) {
        if (!l.isNull())
            n += qint64(l.pixmap.width()) * l.pixmap.height() * 4;
    };
    for (const auto &l : m_layers)
        add(l);
    for (const auto &l : m_exprs)
        add(l);
    return n;
}

} // namespace pet
