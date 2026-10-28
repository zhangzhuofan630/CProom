# 情侣桌宠 / Desktoppet

> 给异地情侣的桌面宠物。Windows 11 + Ubuntu 双平台，可开机自启。
> 长期目标：上传照片 → 自动生成卡通形象 → 自动变成能跑能跳的桌宠。

**当前状态：Sprint 0（可行性验证阶段），尚无可用版本。**

---

## 这个项目在做什么

分两层，边界很硬：

| 层 | 职责 | 依赖 |
|---|---|---|
| **桌宠引擎** | 加载 `petpack` 角色包，播放动作，处理窗口/托盘/交互 | 只依赖本地 petpack 格式，**不依赖任何 AI 模型** |
| **形象生成** | 照片 → Q 版卡通角色 → 切成 petpack 可用的分层素材 | 可插拔的生成适配器（本地 ComfyUI / 云端 API） |

这样分层的原因：模型换代很快，桌宠不该跟着重写；而角色包格式一旦定了，就是所有素材的契约。

## 平台支持

| 平台 | 状态 | 说明 |
|---|---|---|
| Windows 11 x64 | 目标平台 | 置顶、穿透、自启动均可用 |
| Ubuntu 24.04 · X11 | 目标平台 | 完整支持 |
| Ubuntu · Wayland | 兼容模式 | **系统层面限制**：普通客户端无法自控置顶与点击穿透。需要 `QT_QPA_PLATFORM=xcb` 走 XWayland |

> Wayland 的限制不是本项目的 bug，是合成器的安全设计。GNOME/Mutter 对普通原生客户端直接禁用置顶。KDE Plasma 6 支持 layer-shell，体验明显更好。

## 隐私

- 默认**照片不出本机**：生成走本地 ComfyUI，而不是上传到某个服务器。
- 仓库里不会有任何真实照片（`.gitignore` 已排除）。
- 云端生成是可选路径，需要用户自备 API key。

## 开发

环境要求：CMake ≥ 3.21、Qt 6.5+（CI 用 6.8.3）、MSVC 2022 或 GCC。

```bash
# 构建
cmake -S desktop -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64
cmake --build build

# 运行
./build/desktop/desktop

# 测试
ctest --test-dir build --output-on-failure
```

## 文档

| 文件 | 内容 |
|---|---|
| [`docs/plan-review.md`](docs/plan-review.md) | 方案评估：4 个结构性问题与修订后的路线图 |
| [`docs/execution-plan.md`](docs/execution-plan.md) | 分步执行指南，每个 Step = 目标 → 命令 → 验收 |
| [`docs/checklist.md`](docs/checklist.md) | 进度追踪表与决策门结论记录 |

## 许可

代码采用 MIT。桌面端仅使用 **LGPLv3** 版 Qt 模块，不引入任何 GPL-only 模块（详见 `desktop/CMakeLists.txt` 顶部说明）。

## 贡献

见 [`CONTRIBUTING.md`](CONTRIBUTING.md)。项目仍在早期，接口和格式都会变，欢迎提 Issue 讨论而不是直接提大改动 PR。
