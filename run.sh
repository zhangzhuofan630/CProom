#!/usr/bin/env bash
# 启动情侣桌宠
#
# 用法：
#   ./run.sh                 正常启动
#   ./run.sh --fps 30        换帧率
#   ./run.sh --bench 10      只测性能，10 秒后自己退出
#
# 操作：点一下会跳；按住拖动可以拎起来，甩出去会落到任务栏上弹一下。
#
# 素材默认读 $HOME/AI/petpack，可用 PETPACK=/path ./run.sh 覆盖。

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$HERE/build/bin/desktop"
PACK="${PETPACK:-$HOME/AI/petpack}"
QT_DIR="${QT_DIR:-$HOME/Qt/6.8.3/gcc_64}"

if [ ! -x "$BIN" ]; then
    echo "还没有编译。先跑："
    echo
    echo "  cmake -S \"$HERE/desktop\" -B \"$HERE/build\" -DCMAKE_BUILD_TYPE=Release \\"
    echo "        -DCMAKE_PREFIX_PATH=\"$QT_DIR\""
    echo "  cmake --build \"$HERE/build\" -j\$(nproc)"
    echo
    exit 1
fi

if [ ! -d "$PACK" ]; then
    echo "找不到素材目录：$PACK"
    echo
    echo "素材不在仓库里（含人脸，永不入库）。生成方式见 docs/s05-report.md，"
    echo "产出应该在 $HOME/AI/petpack/layers/ 下有 legs/body/head/collar 四层。"
    echo
    exit 1
fi

# Wayland 会话下 Qt 默认走 wayland 平台，此时窗口置顶（_NET_WM_STATE_ABOVE）
# 和点击穿透都无效——这是 Wayland 协议本身的限制，不是 Qt 的问题。
# 强制走 XWayland 才能拿到置顶能力。
if [ "${XDG_SESSION_TYPE:-}" = "wayland" ] || [ -n "${WAYLAND_DISPLAY:-}" ]; then
    export QT_QPA_PLATFORM=xcb
    echo "[run] 检测到 Wayland 会话，已强制 QT_QPA_PLATFORM=xcb（否则窗口无法置顶）"
fi

exec "$BIN" --pack "$PACK" "$@"
