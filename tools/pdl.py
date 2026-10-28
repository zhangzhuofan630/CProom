#!/usr/bin/env python3
"""多线程分片下载器：支持从已有字节续传，用标准库实现，无需第三方依赖。

用法: pdl.py <url> <dest> [线程数]
"""
import os
import sys
import threading
import urllib.request
import urllib.error

CHUNK = 1 << 20  # 1MB
UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"


def head_size(url):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=60) as r:
        n = r.headers.get("Content-Length")
        return int(n) if n else None


def fetch_range(url, start, end, path, state, key, retries=4):
    for attempt in range(retries):
        try:
            req = urllib.request.Request(
                url, headers={"User-Agent": UA, "Range": f"bytes={start}-{end}"}
            )
            with urllib.request.urlopen(req, timeout=90) as r, open(path, "wb") as f:
                got = 0
                while True:
                    b = r.read(CHUNK)
                    if not b:
                        break
                    f.write(b)
                    got += len(b)
                    state[key] = got
            if got == end - start + 1:
                return True
            # 短读，重试剩余部分
            start = start + got
        except Exception as e:  # noqa: BLE001
            state[key] = f"retry{attempt}:{e}"
    return False


def main():
    url, dest = sys.argv[1], sys.argv[2]
    nthreads = int(sys.argv[3]) if len(sys.argv) > 3 else 8

    os.makedirs(os.path.dirname(dest) or ".", exist_ok=True)
    total = head_size(url)
    if total is None:
        print(f"FATAL 无法获取大小: {url}")
        return 1

    cur = os.path.getsize(dest) if os.path.exists(dest) else 0
    if cur >= total:
        print(f"OK 已完整 {dest} ({total} bytes)")
        return 0

    print(f"TOTAL {total}  已有 {cur}  剩余 {total - cur} 线程 {nthreads}", flush=True)

    remain = total - cur
    # 分片：每片至少 4MB，避免过多碎片
    nthreads = max(1, min(nthreads, remain // (4 << 20) or 1))
    seg = remain // nthreads

    segs = []
    for i in range(nthreads):
        s = cur + i * seg
        e = (cur + remain - 1) if i == nthreads - 1 else (s + seg - 1)
        segs.append((s, e, f"{dest}.part{i}"))

    state = {}
    threads = []
    for idx, (s, e, p) in enumerate(segs):
        t = threading.Thread(target=fetch_range, args=(url, s, e, p, state, idx))
        t.start()
        threads.append(t)

    mon = threading.Event()

    def monitor():
        while not mon.wait(5):
            done = sum(v for v in state.values() if isinstance(v, int))
            pct = (cur + done) / total * 100
            print(f"PROGRESS {dest} {pct:5.1f}%  {(cur + done) / 1e6:.0f}/{total / 1e6:.0f} MB", flush=True)

    m = threading.Thread(target=monitor, daemon=True)
    m.start()

    for t in threads:
        t.join()
    mon.set()

    bad = [k for k, v in state.items() if not isinstance(v, int)]
    if bad:
        print(f"FAIL 分片异常 {bad}")
        return 2

    # 合并
    with open(dest, "ab") as out:
        for _, _, p in segs:
            with open(p, "rb") as f:
                while True:
                    b = f.read(CHUNK)
                    if not b:
                        break
                    out.write(b)
            # 分片删除可能被环境的批量删除保护拦截（抛的不是 OSError，
            # 而是 SAFE_DELETE_BULK_CONFIRM_REQUIRED 自定义信号），
            # 必须捕获 BaseException，否则合并结束时会中断，留下半成品文件。
            try:
                os.remove(p)
            except BaseException:  # noqa: BLE001
                pass

    final = os.path.getsize(dest)
    if final != total:
        print(f"FAIL 大小不符 {final} != {total}")
        return 3
    print(f"DONE {dest} {final} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
