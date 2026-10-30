#pragma once

#include <QObject>
#include <QPixmap>
#include <QPoint>
#include <QString>

#include <memory>

class QAction;
class QLockFile;
class QMenu;
class QSystemTrayIcon;

namespace pet {

class PetPack;
class PetView;

/// 桌宠和操作系统之间的那层「外壳」：托盘、位置记忆、开机自启、单实例。
///
/// 单独拆出来是因为这些全是平台相关的杂事，和渲染/物理没有交集。
/// 放在 petview.cpp 里会把一个本来只画图的类搞成三不像。
namespace shell {

/// 单实例锁。返回 nullptr 表示已经有另一个实例在跑，调用方应当直接退出。
///
/// 锁文件放在运行时目录（XDG_RUNTIME_DIR，回退到临时目录）。不用固定路径，
/// 因为多用户同时登录时各人应各有一份。
///
/// 返回值的生命周期必须覆盖整个进程——析构时自动解锁并删文件。
std::unique_ptr<QLockFile> acquireSingleInstance();

/// 上次退出时的窗口位置。ok 为 false 表示没有记录（或坐标已不在任何屏幕上）。
QPoint savedPos(bool *ok);

void savePos(const QPoint &pos);

/// 开机自启状态。Linux 读 XDG autostart 目录，Windows 读注册表 Run 键。
bool autostartEnabled();
bool setAutostart(bool on);

/// 用 petpack 的头部生成托盘图标。
///
/// 不单独出图标资源：托盘图标只有 22px，画一个 200px 的角色整体缩放会糊成一团，
/// 头部剪影反而好认。而且这样换角色时图标自动跟着换。
QPixmap trayPixmap(const PetPack &pack, int size);

} // namespace shell

/// 托盘图标与右键菜单。
///
/// 这是用户关掉桌宠的唯一入口——之前只能 pkill。所以「退出」必须是菜单项，
/// 不能藏在别处。
class Tray : public QObject
{
    Q_OBJECT

public:
    Tray(PetView &view, const PetPack &pack, QObject *parent = nullptr);
    ~Tray() override;

    /// 系统没有托盘（部分极简桌面/纯 Wayland 环境）时为 false。
    /// 调用方要据此决定「关窗=退出」还是「关窗=收进托盘」。
    bool isAvailable() const;

    /// 依次触发每个菜单项（不触发「退出」），用于没鼠标的环境验证整条链路。
    ///
    /// 和 --test-drag 是同一个思路：不另写一套「测试专用逻辑」，而是把用户
    /// 真实会走的那条路径按顺序走一遍，看状态对不对。
    void selfTest();

private:
    void buildMenu();
    void toggleVisible();
    void resetHome();

    PetView &m_view;
    QSystemTrayIcon *m_icon = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_showAction = nullptr;
    QAction *m_autoAction = nullptr;
};

} // namespace pet
