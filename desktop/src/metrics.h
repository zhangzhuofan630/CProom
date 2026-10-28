#pragma once

#include <QtGlobal>

namespace pet {

/// 一次进程资源采样。
///
/// 桌宠的验收指标是「空闲 CPU < 1%、内存 < 150MB」，所以这两项必须能从
/// 程序自己嘴里报出来，而不是靠外部 top 观察（top 的 %CPU 是区间均值，
/// 窗口一换就对不上）。这里直接读系统计数器。
struct ProcSample
{
    double cpuSeconds = 0.0;  ///< 用户态 + 内核态累计 CPU 时间（秒）
    qint64 rssBytes = 0;      ///< 常驻内存集（字节）
};

ProcSample sampleProcess();

} // namespace pet
