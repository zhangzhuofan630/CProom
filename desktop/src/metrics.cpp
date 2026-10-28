#include "metrics.h"

#if defined(Q_OS_LINUX)
#include <sys/resource.h>
#include <QFile>
#include <QByteArray>
#elif defined(Q_OS_WIN)
#include <windows.h>
#include <psapi.h>
#endif

namespace pet {

ProcSample sampleProcess()
{
    ProcSample s;

#if defined(Q_OS_LINUX)
    struct rusage ru;
    if (::getrusage(RUSAGE_SELF, &ru) == 0) {
        s.cpuSeconds = double(ru.ru_utime.tv_sec) + double(ru.ru_utime.tv_usec) / 1e6
                     + double(ru.ru_stime.tv_sec) + double(ru.ru_stime.tv_usec) / 1e6;
    }
    QFile f(QStringLiteral("/proc/self/status"));
    if (f.open(QIODevice::ReadOnly)) {
        for (const QByteArray &line : f.readAll().split('\n')) {
            if (line.startsWith("VmRSS:")) {
                // 形如 "VmRSS:\t  123456 kB"
                const QByteArray num = line.mid(6).trimmed().split(' ').value(0);
                s.rssBytes = num.toLongLong() * 1024;
                break;
            }
        }
    }
#elif defined(Q_OS_WIN)
    FILETIME c, e, k, u;
    if (::GetProcessTimes(::GetCurrentProcess(), &c, &e, &k, &u)) {
        auto toSec = [](const FILETIME &ft) {
            ULARGE_INTEGER v;
            v.LowPart = ft.dwLowDateTime;
            v.HighPart = ft.dwHighDateTime;
            return double(v.QuadPart) / 1e7;  // 100ns 单位
        };
        s.cpuSeconds = toSec(k) + toSec(u);
    }
    PROCESS_MEMORY_COUNTERS pmc{};
    if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc)))
        s.rssBytes = qint64(pmc.WorkingSetSize);
#endif

    return s;
}

} // namespace pet
