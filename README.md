# 情侣桌宠 / Desktoppet

[![CI](https://github.com/zhangzhuofan630/CProom/actions/workflows/ci.yml/badge.svg)](https://github.com/zhangzhuofan630/CProom/actions/workflows/ci.yml)

> 给异地情侣的桌面宠物。Windows 11 + Ubuntu 双平台，可开机自启。
> 长期目标：上传照片 → 自动生成卡通形象 → 自动变成能跑能跳的桌宠。

**当前状态：Sprint 0 验证完成（可行性 GO），有一版能在桌面上跑的分层渲染原型。**

原型长这样：一个 202×267 的透明置顶窗口，四层图像（腿/身/头/衣领遮罩）用 QPainter 合成，
鼠标移动时头会跟着转，会自己眨眼，点一下会跳，静置 20 秒会犯困。
实测空闲 CPU **0.33%**、内存 **55MB**。详见 [`docs/s09-render-report.md`](docs/s09-render-report.md)。

> 注意：仓库里**没有角色素材**（含人脸，永不入库）。想跑起来需要自己生成一套 petpack，
> 或者等后续提供示例角色包。

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
git clone git@github.com:zhangzhuofan630/CProom.git
cd CProom

# 构建（产物在 build/bin/desktop）
cmake -S desktop -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64
cmake --build build

# 运行。素材默认读 ~/AI/petpack，没有素材会给出提示
./run.sh

# 只测性能，10 秒后自己退出
./run.sh --bench 10

# 测试
ctest --test-dir build --output-on-failure
```

## 文档

| 文件 | 内容 |
|---|---|
| [`docs/plan-review.md`](docs/plan-review.md) | 方案评估：4 个结构性问题与修订后的路线图 |
| [`docs/execution-plan.md`](docs/execution-plan.md) | 分步执行指南，每个 Step = 目标 → 命令 → 验收 |
| [`docs/checklist.md`](docs/checklist.md) | 进度追踪表与决策门结论记录 |
| [`docs/s05-report.md`](docs/s05-report.md) | 验证报告：照片 → Q 版形象（19 张实测，含参数与踩坑） |
| [`docs/petpack-spec-draft.md`](docs/petpack-spec-draft.md) | 角色包格式草案 v0.1（由真实切层数据倒推） |
| [`docs/s09-render-report.md`](docs/s09-render-report.md) | 渲染器实测：CPU/内存、动态帧率策略、渲染选型结论 |

## 许可

代码采用 MIT。桌面端仅使用 **LGPLv3** 版 Qt 模块，不引入任何 GPL-only 模块（详见 `desktop/CMakeLists.txt` 顶部说明）。

## 贡献

见 [`CONTRIBUTING.md`](CONTRIBUTING.md)。项目仍在早期，接口和格式都会变，欢迎提 Issue 讨论而不是直接提大改动 PR。
