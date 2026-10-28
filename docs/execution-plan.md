# 情侣桌宠 · 分步执行指南

> 配套文档：`docs/plan-review.md`（方案评估与修订理由）
> 本文档 = **可执行的步骤清单**。每个 Step 都有：目标 → 前置 → 操作 → 产出物 → 验收标准。
> 执行方式：一次做一步，验收通过后在 `docs/checklist.md` 打勾，再进下一步。

## 使用约定

- **每步都有验收标准**，不满足就不要进下一步（尤其带 ⚠️ 标记的决策门）
- **标注 🔀 的步骤是决策点**，做完会分叉出不同路径，文档里会写明分叉后怎么走
- 命令默认在项目根目录 `/home/zzf/Project/情侣桌宠` 执行
- Shell 统一用 `bash`，Windows 部分会单独标注 PowerShell

## 时间估算口径

按**业余时间每周 10~15 小时**估算。若全职投入，约为此数的 1/2.5。

| 阶段 | 估算 | 累计 |
|---|---|---|
| Sprint 0 验证尖刺 | 1.5~2 周 | 2 周 |
| Phase 1 桌宠引擎 | 5~7 周 | 9 周 |
| Phase 2 角色与生成 | 5~7 周 | 16 周 |
| Phase 3 发布打磨 | 3~4 周 | 20 周 |
| Phase 4 异地情侣 | 4~6 周 | 26 周 |

---

# Sprint 0 · 验证尖刺（1.5~2 周）

> **本阶段唯一目的：在写引擎之前，拿到「照片→Q版角色」可行性的结论，并据此冻结 petpack 格式。**
> 本阶段不写 App 代码（除 S0.9 的最小渲染 demo）。
> **这是整个项目性价比最高的两周——它买掉的是第 4~8 步全部返工的风险。**

## S0.1 · 仓库与目录骨架

**目标**：把项目立起来，后续每一步都有地方放东西。

> **状态：文件骨架已落盘并已本地提交**（2026-10-28，commit `e2db67b` / `34a27fa`）。
> 已生成：`desktop/`（CMake + 最小程序 + ctest 用例）、`.github/workflows/ci.yml`、
> `.gitignore`、`README.md`、`LICENSE`(MIT)、`CONTRIBUTING.md`、`SECURITY.md`、
> Issue/PR 模板、`.editorconfig`、`.gitattributes`。
> `origin` 已指向 `git@github.com:zhangzhuofan630/CProom.git`，SSH key 已生成且链路可通。
> **剩两步**：① 把 `~/.ssh/id_ed25519.pub` 贴到 GitHub → Settings → SSH keys；
> ② `git push -u --force origin main`（远端网页版 `Initial commit` 与本地历史无关，需 force），
> 然后在 Actions 里确认 CI 变绿。

**操作**：

```bash
cd /home/zzf/Project/情侣桌宠
git init -b main
git remote add origin git@github.com:<你的用户名>/<repo名>.git
mkdir -p desktop/ shared/petpack-schema/ assets/examples assets/tests docs .github/workflows tools
```

目录职责：

| 目录 | 内容 |
|---|---|
| `desktop/` | Qt 客户端（C++/QML/CMake），Phase 1 开始填充 |
| `shared/petpack-schema/` | petpack 格式定义 + JSON Schema + 校验脚本（S0.11） |
| `assets/examples/` | 示例角色包（Sprint 0 只放测试图） |
| `assets/tests/` | 验证过程中的生成图归档，好坏都留 |
| `docs/` | 需求、格式规范、素材规范、安装文档 |
| `tools/` | 一次性工具（切图、格式转换） |
| `.github/workflows/` | CI（S0.2 写第一个） |

同时创建：

```bash
gh repo create <repo名> --public --source=. --remote=origin --push
gh repo edit --enable-issues --enable-wiki
```

**产出物**：可访问的 GitHub 仓库 + 目录骨架 + 首个 commit。

**验收**：
- [ ] `git clone` 到别的目录能成功
- [ ] 仓库首页有 README（可以先写一句话占位："Win11 + Ubuntu 桌面宠物，照片生成卡通形象"）
- [ ] `.gitignore` 存在（见 S0.2 末尾的清单）

---

## S0.2 · 开发环境与 CI 骨架

**目标**：本地能编译 Qt 程序，CI 能自动编译。

### 本地 Qt 6 安装

Ubuntu 22.04 apt 只有 **Qt 6.2.4**（偏旧，且 QML 组件不全）。为了和 CI 一致，**推荐用 aqtinstall 装 6.8.3**：

```bash
# 用独立 venv，不污染系统 Python
/home/zzf/.workbuddy/binaries/python/versions/3.13.12/bin/python3 -m venv ~/.venvs/aqt
~/.venvs/aqt/bin/pip install -U pip aqtinstall

# 装 Qt 6.8.3（约 2GB）
~/.venvs/aqt/bin/aqt install-qt linux desktop 6.8.3 -O ~/Qt
~/.venvs/aqt/bin/aqt install-qt linux desktop 6.8.3 -m qtmultimedia qtimageformats -O ~/Qt
```

验证：

```bash
~/Qt/6.8.3/gcc_64/bin/qmake6 -v
```

> **实测提醒（2026-10-28，S0.2 已跑通）**：
> - **aqtinstall 3.x 的 `-b/--base` 已改成「镜像 URL」**，不再是本地目录（3.0 的破坏性变更）。要指定归档存放目录用 `-d`，否则会报 `The '--base' option requires a url where the path online/qtsdkrepository exists`。
> - 报 `No space left on device` 是**临时目录**空间不足，不是 Qt 太大：`TMPDIR=<大盘目录> aqt install-qt ...`。
> - **编译不需要任何 `-dev` 包**（OpenGL/XKB 系统自带，cmake 能 FindOpenGL 成功），但**运行需要 `libxcb-cursor0`**，Ubuntu 22.04 默认没装，缺了 xcb 插件直接加载失败。这是「编译过了但双击没反应」的典型原因。

> 只想快速试水、不在乎版本的话，`sudo apt install qt6-base-dev qt6-declarative-dev cmake ninja-build` 也可以，但**项目正式开发建议统一到 6.8.3**，否则 CI 和本地行为会有差异。

其他依赖：

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git \
  libgl1-mesa-dev libxkbcommon-dev libxkbcommon-x11-0 libxcb-cursor0 \
  libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 \
  libxcb-shape0 libxcb-xinerama0 libxcb-xkb1 libxkbcommon-x11-0 \
  libxcb-shape0 libxcb-util1 libxcb-image0 \
  qt6-image-formats-plugins xvfb
```

### 最小 Qt 工程 + CMake

创建 `desktop/CMakeLists.txt` 与 `desktop/src/main.cpp`（最小空窗口），验证能编译：

```bash
cmake -S desktop -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64
cmake --build build
./build/bin/desktop
```

**关键 CMake 要求**（写进 `desktop/CMakeLists.txt`）：

```cmake
cmake_minimum_required(VERSION 3.21)
project(desktoppet LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Qml Quick)
qt_standard_project_setup(REQUIRES 6.5)

# ★ 许可红线：只允许 LGPLv3 模块
# 禁止引入：Charts / DataVisualization / Purchasing / VirtualKeyboard
# （这些只有 GPLv3 或商业许可，会污染整个项目许可）
```

### CI（第一个 workflow）

创建 `.github/workflows/ci.yml`：

```yaml
name: CI
on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  build:
    strategy:
      fail-fast: false
      matrix:
        include:
          - os: ubuntu-22.04
            arch: linux_gcc_64
            name: Linux
          - os: windows-latest
            arch: win64_msvc2022_64
            name: Windows
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4

      - name: Install Linux deps
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y ninja-build libgl1-mesa-dev libxkbcommon-dev \
            libxkbcommon-x11-0 libxcb-cursor0 libvulkan-dev

      - name: Install MSVC
        if: runner.os == 'Windows'
        uses: ilammy/msvc-dev-cmd@v1

      - name: Install Qt
        uses: jurplel/install-qt-action@v4
        with:
          version: '6.8.3'
          target: desktop
          arch: ${{ matrix.arch }}
          modules: 'qtshadertools qtimageformats'
          dir: ${{ github.workspace }}/qt
          cache: true

      - name: Configure
        run: >
          cmake -S desktop -B build -G Ninja
          -DCMAKE_BUILD_TYPE=Release
          -DCMAKE_PREFIX_PATH=${{ env.Qt6_DIR }}

      - name: Build
        run: cmake --build build --config Release

      - name: Test
        run: ctest --test-dir build -C Release --output-on-failure
```

> `install-qt-action@v4` 会自动识别 runner 系统，Linux 用 `linux_gcc_64`（Qt 6.7+ 架构名变更，6.6 及更早是 `linux64`），Windows 用 `win64_msvc2022_64`（Qt 6.8+）。

> **实际落盘版本与上面示例的差异**（都是为了避免 CI 假红，`desktop/CMakeLists.txt` 与 `.github/workflows/ci.yml` 以文件为准）：
> 1. `find_package` 目前只要 `Core Gui Widgets`，**没要 Qml/Quick**。Qml 属于 `qtdeclarative`，不是 qtbase，如果 `install-qt-action` 没装这个 archive，configure 阶段会直接挂。等 S0.9 确定渲染方案后再加，并同时在 CI 的 `modules:` 里补上。
> 2. ctest 加了一个真实用例（`desktop/tests/test_version.cpp`，纯 C++ 不依赖 Qt）。**没有测试时 `ctest` 会返回非零码，Test 步骤会假红**，所以这个用例是必要的。
> 3. `.gitignore` 在示例基础上补了模型文件（`*.safetensors/*.ckpt/*.onnx`）、打包产物、以及 `.workbuddy/`（本地开发笔记，不进公开仓库）。


### .gitignore

```gitignore
build/
*.user
.qmake.stash
CMakeLists.txt.user
CMakeUserPresets.json
__pycache__/
*.pyc
.venv/
venv/
.DS_Store
# 关键：绝不入库真实照片与生成结果
assets/tests/private/
*.jpg
*.jpeg
!assets/examples/**/*.jpg
```

**产出物**：`desktop/` 最小工程、CI workflow、本地 Qt 6.8.3 环境。

**验收**：
- [ ] 本地能编译并弹出空窗口
- [ ] 推一个 commit，GitHub Actions 两个平台都绿
- [ ] `.gitignore` 生效，`git status` 干净

---

## S0.3 · 准备 GPU 环境

**目标**：把本机 GPU 配成能跑 ComfyUI 的环境。

**硬件已确认**（本机实测）：

| 项 | 值 |
|---|---|
| GPU | NVIDIA GeForce RTX 3060 Laptop |
| 显存 | **6 GB** |
| 驱动 | 580.178.04 |
| CUDA | 13.0（驱动支持的最高版本） |
| 系统 | Ubuntu 22.04.5 LTS（ASUS TUF Gaming A15 FA507RM） |

> **本机 6GB 显存是本项目最重要的硬件约束**，直接影响模型选型。见下方显存预算。

### 显存预算（6GB 怎么用）

实测参考（ComfyUI 官方 troubleshooting + 社区实测）：

| 组合 | 最低 | 舒适 | 6GB 能否跑 |
|---|---|---|---|
| SD 1.5 | 4 GB | 6 GB | ✅ 轻松 |
| SDXL | 6 GB | 8 GB | ⚠️ 紧，需 `--lowvram` |
| SDXL + IP-Adapter FaceID | ~7 GB | 10 GB | ⚠️ 需 `--lowvram` + 768px |
| SDXL + IP-Adapter + ControlNet | ~8 GB | 12 GB | ❌ 6GB 很吃力 |
| SDXL + InstantID（含自带 ControlNet） | ~8 GB | 12 GB | ❌ 同上 |
| FLUX.1 | 8 GB（量化） | 12 GB | ⚠️ 勉强，需 fp8 |

**结论：Sprint 0 主线用 SDXL + IP-Adapter FaceID，全程 `--lowvram`，分辨率先 768×768。**
InstantID 与 OpenPose ControlNet 属于「加分项」，6GB 上大概率要分步跑（关掉其他节点再单独试）。

### 安装步骤

```bash
# 1. 确认 PyTorch 能看到 GPU
python3 -c "import torch; print(torch.cuda.is_available(), torch.cuda.get_device_name(0))"

# 2. 装 ComfyUI
sudo apt install -y git python3-venv python3-dev
git clone https://github.com/comfyanonymous/ComfyUI.git
cd ComfyUI
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

# 3. 装 xformers（NVIDIA 卡，显著省显存 + 提速）
pip install xformers

# 4. 装 insightface（IP-Adapter FaceID / InstantID 依赖）
pip install insightface onnxruntime-gpu
```

**PyTorch 版本选择**：驱动 580 / CUDA 13.0 向下兼容，装 PyTorch 时选 **cu121 或 cu124** 的 wheel 即可（PyTorch 官方 wheel 尚未普遍提供 cu13，但驱动向后兼容没问题）：

```bash
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121
```

### 启动参数（6GB 专用）

```bash
source venv/bin/activate
python main.py --lowvram --preview-method none --use-pytorch-cross-attention
```

| 参数 | 作用 |
|---|---|
| `--lowvram` | 把模型部分留在 CPU，按需搬上 GPU。**6GB 必加** |
| `--preview-method none` | 禁用实时预览，省显存 |
| `--use-pytorch-cross-attention` | 不用 xformers 时的省显存替代方案（装了 xformers 就不需要这条） |
| `--reserve-vram 0.5` | 给系统留 0.5GB，避免桌面合成器抢占导致 OOM |

> ⚠️ **不要用 `--disable-smart-memory`**：它会关掉自动卸载闲置模型，反而更容易 OOM。
> ⚠️ **不要用 `--gpu-only`**：6GB 会直接爆。

### 备选方案（仅在 6GB 实在跑不动时启用）

- **降级到 SD 1.5**：显存降到 4GB 级别，IP-Adapter FaceID 有 SD1.5 版本，Chibi LoRA 生态也更大。代价：面部特征保持效果弱于 SDXL
- **分段跑**：先只挂 IP-Adapter 出图，再单独跑 ControlNet 修姿态，两步合并
- **临时租云 GPU**：仅当本机反复 OOM 且 SD1.5 也救不回来时，租 RunPod（$0.25~0.5/小时）做对照实验

**验收**：
- [ ] `torch.cuda.is_available()` 返回 True
- [ ] ComfyUI 启动日志显示 `Total VRAM: 6144 MB` 和 lowvram 模式
- [ ] 能跑出第一张 768×768 的图
- [ ] `nvidia-smi` 能看到 ComfyUI 进程的显存占用

---

## S0.4 · 部署 ComfyUI 与模型

**目标**：把生成环境配齐。

### 安装自定义节点

用 ComfyUI-Manager（`Manager → Install Custom Nodes`）搜索安装，或手动：

```bash
cd ~/ComfyUI/custom_nodes
git clone https://github.com/cubiq/ComfyUI_IPAdapter_plus.git
git clone https://github.com/cubiq/ComfyUI_InstantID.git
```

> `insightface` / `onnxruntime-gpu` 已在 S0.3 装好，这里不用重复。

### 模型清单（按 6GB 显存排优先级）

**第一批（必装，主线用）**

| 模型 | 放置目录 | 来源 | 说明 |
|---|---|---|---|
| SDXL 底模 | `models/checkpoints/` | Civitai / HuggingFace | **选 fp16 版本**，别用 fp32 |
| **IP-Adapter FaceID Plus v2** | `models/ipadapter/` | h94/IP-Adapter | 主线人脸保持方案 |
| CLIP-Vision（IP-Adapter 依赖） | `models/clip_vision/` | h94/IP-Adapter | FaceID 需对应版本 |
| antelopev2 人脸模型 | `models/insightface/models/antelopev2/` | 自动下载或 HF | FaceID 必需 |
| **Q版/Chibi LoRA** | `models/loras/` | Civitai 搜 chibi / Q版 | LoRA 很轻，显存开销小 |

**第二批（6GB 吃力，备用）**

| 模型 | 放置目录 | 来源 |
|---|---|---|
| ControlNet OpenPose SDXL | `models/controlnet/` | lllyasviel/ControlNet-ViT |
| InstantID ip-adapter.bin | `models/instantid/` | InstantX/InstantID |
| InstantID ControlNet | `models/controlnet/` → `controlnet_instantid.safetensors` | InstantX/InstantID |

> **注意**：第一批就够跑 S0.5 验证项 A 了。第二批在 6GB 上要配合「关掉其他节点单独试」或降分辨率。
> **Q版 LoRA 是关键**。提示词写 "chibi" 远不如挂一个专门的 Chibi LoRA 稳定。这是本步最值得花时间的一步，而且 LoRA 显存开销很小，优先找。

### 参数基线（6GB 版本，先照抄，后面再调）

| 参数 | 值 | 说明 |
|---|---|---|
| 分辨率 | **768×768** | 6GB 起步值。SDXL 训练尺度是 1024，但 6GB 先用 768 跑通；显存有余再升 1024 |
| 采样器 | `dpmpp_2m_karras` | |
| Steps | 30 | 6GB 紧张时可降到 25 |
| CFG | 5~6 | 比常规低，因为 IP-Adapter/InstantID 自带"压力" |
| IP-Adapter FaceID 权重 | 0.7~0.8 | |
| InstantID 权重 | 0.8，ControlNet 权重 0.5 | 第二批才用 |
| LoRA 权重 | 0.6~0.8 | |
| batch_size | 1 | 必须 |

**显存不足时的降级顺序**（从上往下试）：
1. 分辨率 768 → 640
2. 断开 ControlNet（只留 IP-Adapter）
3. 断开 LoRA
4. `--lowvram` 换成加 `--use-cpu-offload`
5. 底模换 SD 1.5 版（Chibi LoRA 要找 SD1.5 版本）

**产出物**：能跑的 ComfyUI + 第一批模型就位。

**验收**：
- [ ] 能在 ComfyUI 加载 checkpoint 并出图（768×768）
- [ ] IP-Adapter FaceID 节点无报错，能检测到人脸
- [ ] 加载 LoRA 后能出图
- [ ] `nvidia-smi` 观察：生成时显存占用不超过 6GB

---

## S0.5 · 验证项 A：照片 → Q版角色

**目标**：确认**单张**照片能否生成可辨识的 Q版角色。

**测试素材准备**（`assets/tests/`）：

- 准备 **5 张同一人**的照片：正脸为主、光线均匀、无遮挡、背景简单
- 不要用：大角度侧脸、浓重遮挡、极端表情、逆光、低分辨率
- 注意：这些照片**不要提交到 Git**（`.gitignore` 已含 `*.jpg`）

**测试矩阵**（每组跑 3 张，固定 seed 便于复现）：

> ⚠️ **6GB 显存下的执行顺序**：A1→A2→A3→A5 是**逐步叠加**的，**不要一次全挂**。每加一个节点就观察 `nvidia-smi`，OOM 就按 S0.4 的降级顺序退。
> A4（InstantID）自带一个 ControlNet，在 6GB 上大概率要断开 IP-Adapter 单独试，或放到最后做。

| 组 | 配置 | 目的 | 6GB 风险 |
|---|---|---|---|
| A1 | 仅提示词（无 IP-Adapter、无 LoRA） | 找基线：不用参考图能做到什么程度 | 🟢 无风险，先跑这个 |
| A2 | + IP-Adapter FaceID (w=0.7) | 验证脸部特征保持 | 🟡 主力配置 |
| A3 | FaceID 权重升到 0.85 | 看权重升高是否更保真还是更僵硬 | 🟡 同 A2 |
| A4 | FaceID + Chibi LoRA (w=0.7) | **最终目标配置** | 🟡 LoRA 开销小 |
| A5 | InstantID (w=0.8, cn=0.5) | 对比：空间结构约束的路线 | 🔴 6GB 吃力，放最后 |

> 表格顺序与原方案的 A4/A5 对调了：**LoRA 比 InstantID 轻得多，6GB 上应该先验证 LoRA 路线**。如果 LoRA + FaceID 就够好，InstantID 甚至可以不用。

**提示词模板**：

```
chibi character, 1girl, [年龄], [发型发色], [眼睛颜色], [服装],
[表情], simple background, full body, standing, masterpiece, best quality
```

**评分维度**（每张图打 1~5 分，记录到 `assets/tests/eval-sheet.md`）：

| 维度 | 判断标准 |
|---|---|
| 脸部辨识度 | 能否认出是照片里的人？（发型/脸型/眼镜/配饰至少保住 1 项） |
| Q版程度 | 是否明显是 Q 版而非写实 |
| 完整性 | 手脚/发型完整，无畸变 |
| 构图 | 全身居中，透明背景可抠 |

**产出物**：`assets/tests/eval-sheet.md` + 15~25 张生成图。

**🔀 决策门 A**：

| 结果 | 决策 |
|---|---|
| A4 组平均脸部辨识度 ≥ 3 分 | ✅ 继续验证项 B |
| 平均 2~3 分 | ⚠️ 继续但加做：换更强底模 / 提高参考图质量 / 加 FaceDetailer 修脸 |
| 平均 < 2 分 | 🔀 **降级方案**：放弃"照片直接生成"，改做「照片 → 生成参考描述 + 风格提示词 → 人工微调角色」的半自动流程，或纯手绘工具路线 |

> **注意**：脸部辨识度低有时不是模型问题，而是**参考照片质量**问题。先确认照片清晰、正脸、无遮挡，再怀疑模型。

**验收**：
- [ ] 评分表填完，有明确结论
- [ ] 至少保存 1 张满意的图作为后续基准
- [ ] 记录下 A4 配置的完整参数（后续 workflow JSON 要用）

---

## S0.6 · 验证项 B：多姿势一致性

**目标**：回答那个决定架构的问题——**能不能靠"生成多张图"得到同一个角色的多个姿势？**

### 路线 A：ControlNet OpenPose + 锁 seed

> ⚠️ **6GB 显存警告**：ControlNet OpenPose + IP-Adapter FaceID 同时挂 ≈ 8GB 需求，6GB 会 OOM。
> **执行策略（分步法）**：
> 1. 先只挂 **ControlNet OpenPose + 底模**（不挂 IP-Adapter），确认姿态控制本身能跑（≈6GB，可行）
> 2. 再加 **IP-Adapter FaceID**，如果 OOM：
>    - 降分辨率到 640×640
>    - 或改用 **IP-Adapter 的 SD1.5 版**（显存降 2GB）
>    - 或**用同一张参考图做 img2img**（denoise 0.4~0.5）代替 IP-Adapter 保持一致性 —— 这个方案在 6GB 上最省显存，值得优先试

具体步骤：
1. 准备 4 个 OpenPose 骨架图：站立 / 走路 / 坐下 / 睡觉
2. 加载 ControlNet OpenPose SDXL，权重 0.3~0.5
3. **固定 seed**，IP-Adapter / InstantID 权重与 S0.5 相同
4. 用同一张参考照片生成 4 个姿势

### 路线 B：单图 + 骨骼变换（架构验证）

**这条路线才是重点。** 拿 S0.5 的成品图，人工切成 4~6 个部件（头/身/头发前/头发后/手臂），写一个**最小 HTML 或 Python 脚本**做 2D 仿射变换，验证：

- 能否做出"呼吸起伏"（整体缩放 + Y 位移）
- 能否做出"眨眼"（切换眼部图层）
- 能否做出"走路"（腿部旋转 + 身体上下位移）

**这一步不需要写正式引擎**，一个 100 行的脚本就够，但它能验证架构假设。
**而且路线 B 完全不占 GPU 显存**（纯 CPU 的 PIL/Canvas 操作），可以随时做。

### 评估

对路线 A 的 4 张图打分：**"能看出是同一个人吗？"**（不是"像不像"，是"是不是同一个人"）

**🔀 决策门 B（关键）**：

| 路线 A（多图）一致性 | 路线 B（骨架）可行性 | 决策 |
|---|---|---|
| 好（4/4 可认同一人） | 可行 | petpack 用**分层+骨架**结构（推荐），AI 仍出外观层 |
| 差（≤2/4 可认同一人） | 可行 | petpack **必须**用分层+骨架。**放弃"AI 直接生成多帧"这条路** |
| 差 | 不可行 | 回到 S0.6 路线 B 深挖：分层是否真能做出可接受的动态 |

> **无论路线 A 结果如何，petpack 都应该用分层+骨架结构。** 路线 A 成功只是锦上添花；失败则证明这个选择是必需的。所以这一验证的真实作用是**确认"必须"而非"可能"**。
>
> **6GB 特别提示**：如果路线 A 因为显存跑不完 4 个姿势（OOM），**这不算"一致性差"**，而是"没测出来"。这种情况下先跑 2 个姿势（站立 + 走路），能出图就算通过初步验证，把"4 姿势"记为待办，**不要因为显存不足就误判架构**。

**产出物**：路线 A 的 4 张对比图（或 2 张 + 待办说明）+ 路线 B 的最小 demo + `assets/tests/consistency-eval.md`。

**验收**：
- [ ] 明确写下结论：AI 直接出多帧是否可行
- [ ] 路线 B demo 能播放至少 2 种动态

---

## S0.7 · 自动分层与锚点自动绑定验证

**目标**：验证"AI 出外观 → 自动切层 → 自动绑锚点"这条自动链路。

### 方案：固定槽位参考图 + inpainting 逐部件生成

这是比"生成整图再切"稳得多的做法，**因为每个部件的位置和尺寸都是被约束的**。

1. **手工制作槽位参考图** `assets/tests/slots.png`：在 1024×1024 画布上画出标注好的 Q版角色轮廓，标出 head / body / hair_back / hair_front / arm_l / arm_r 的区域和锚点十字
2. 选中 head 区域 → inpainting（用 S0.5 的成品图作参考）→ 生成只有头的图层
3. 依次生成其余部件
4. 每个部件导出为**透明背景 PNG**

### 自动切层（对照方案）

若槽位法太慢，可对比测试后置分割：
- `rembg`（U2Net）抠前景
- SAM 按提示分割部件

### 锚点自动绑定

- 若用槽位法 → 锚点**天然已知**（就是槽位图上的坐标），纯代码
- 若用 SAM → 需要写"部件 bbox 中心 → 锚点"的启发式映射，容错性差

**产出物**：`slots.png` + 至少 3 个成功切出的图层 PNG + 锚点表草稿。

**验收**：
- [ ] 切出的图层能重新组合成完整角色（对得上、不错位）
- [ ] 锚点表能让部件正确旋转（手臂绕肩转、头绕颈转）

---

## S0.8 · 写作系统选型与 petpack 格式冻结

**目标**：定下"什么渲染方案"，并据此冻结 `petpack` 格式。

### 渲染方案对比（要写代码实测，不能拍脑袋）

桌宠是**小尺寸动态图形**，这个场景下 QML Scene Graph 的 GPU 管线开销可能不划算。

| 方案 | 优点 | 风险 |
|---|---|---|
| **QWidget + QPainter** | CPU 渲染，内存低（<50MB），对小图足够；桌宠只需 30fps | 无 GPU 加速，复杂特效吃力 |
| QML / Qt Quick | GPU 加速，动画表达力强，易做缩放/滤镜 | 内存高（100MB+），Scene Graph 初始化开销 |

**S0.9 会做一个最小实测**（见下）来定。

### petpack 格式 v0.1

写入 `docs/petpack-spec-draft.md`，核心结构：

```
petpack/
├── character.json      角色元数据 + 骨架定义 + 锚点
├── layers/             分层 PNG（透明背景）
│   ├── body.png
│   ├── head.png
│   ├── hair_back.png
│   ├── hair_front.png
│   ├── arm_l.png / arm_r.png
│   ├── eye_l.png / eye_r.png   （可多张眨眼帧）
│   ├── mouth.png               （可多张嘴型帧）
│   └── accessory_*.png
├── animations/
│   ├── idle.json  walk.json  sleep.json
│   ├── drag.json  click.json  fall.json
│   └── ...
├── sounds/             可选
├── preview.png
└── petpack.json        清单：版本、作者、许可、适配的引擎版本
```

**character.json 示例**：

```json
{
  "format_version": "0.1",
  "name": "示例角色",
  "author": "someone",
  "license": "CC0",
  "canvas": { "width": 200, "height": 300 },
  "bones": [
    { "id": "root",   "parent": null,    "pivot": [100, 280] },
    { "id": "body",   "parent": "root",  "pivot": [100, 200] },
    { "id": "head",   "parent": "body",  "pivot": [100, 120] },
    { "id": "arm_l",  "parent": "body",  "pivot": [ 70, 150] },
    { "id": "arm_r",  "parent": "body",  "pivot": [130, 150] }
  ],
  "layer_anchors": {
    "body":      { "bone": "body",  "offset": [0, 0] },
    "head":      { "bone": "head",  "offset": [0, 0] },
    "hair_back": { "bone": "head",  "offset": [0, 0], "z": -1 },
    "hair_front":{ "bone": "head",  "offset": [0, 0], "z":  2 },
    "arm_l":     { "bone": "arm_l", "offset": [0, 0], "z":  1 }
  },
  "z_order": ["hair_back", "body", "arm_l", "arm_r", "head", "hair_front"]
}
```

**animations/idle.json 示例**：

```json
{
  "name": "idle",
  "loop": true,
  "duration": 2000,
  "tracks": {
    "body": [
      { "t": 0,    "scaleY": 1.00, "dy": 0 },
      { "t": 1000, "scaleY": 1.03, "dy": -2 },
      { "t": 2000, "scaleY": 1.00, "dy": 0 }
    ],
    "head": [
      { "t": 0,    "rot": -1.5 },
      { "t": 1000, "rot":  1.5 },
      { "t": 2000, "rot": -1.5 }
    ]
  }
}
```

**关键设计决策（写进 spec）**：

1. **版本号 + 向后兼容**：`format_version` 必填。引擎遇到高版本 petpack 要报错并给出提示，不能静默出错
2. **预留异地扩展位**（Phase 4 要用，现在就留）：
   ```json
   "character_id": "uuid-v4",
   "remote": { "pairable": true, "trigger_hooks": ["idle", "click", "greet"] }
   ```
3. **单位统一**：像素，坐标系原点左上角，Y 向下
4. **性能约束写进 spec**：单图层 PNG ≤50KB，canvas ≤512×512，空闲 CPU <1%，内存 <150MB

**产出物**：`docs/petpack-spec-draft.md` + `shared/petpack-schema/petpack.schema.json`

**验收**：
- [ ] spec 文档能让一个没参与讨论的人照着实现
- [ ] 格式里已预留 `character_id` 和远程钩子

---

## S0.9 · 最小渲染 demo + 渲染方案实测 🔀

**目标**：用最小代码验证「分层 + 骨架」能出可看的动态，并定下渲染方案。

**做法**：写一个独立的最小程序（不纳入正式工程），实现：

- 加载 `character.json` + `layers/*.png` + `animations/idle.json`
- 按 `z_order` 绘制图层
- 按 `tracks` 做仿射变换（缩放 / 旋转 / 位移）
- 播放 idle + 走路两个动画

**同时实测两套实现**（各写 200 行左右）：
- 变体 A：QWidget + QPainter
- 变体 B：QML + Canvas / Image + Rotation / Scale

**实测指标**：

| 指标 | 目标 |
|---|---|
| 空闲 CPU | < 1% |
| 常驻内存 | < 150MB（QPainter）/ < 250MB（QML） |
| 启动到出画面 | < 500ms |
| 动画帧率 | ≥ 30fps |
| 透明窗口 | 能正常显示且不拖影 |

**🔀 决策门 C**：

| 结果 | 决策 |
|---|---|
| QPainter 达标且更省资源 | **选 QWidget + QPainter**（桌宠场景够用，内存友好） |
| QPainter 不达标 / QML 明显更顺 | 选 QML / Qt Quick |
| 两者都达标 | 选 QPainter（工程更简单）；但**代码层要保证渲染层可替换** |

**无论选哪个，都要保证渲染层是独立模块**（`desktop/src/render/`），与宠物逻辑解耦——这样以后换渲染方案不用重写引擎。

**产出物**：最小 demo + 性能测试报告 `docs/render-benchmark.md`。

**验收**：
- [ ] demo 能播放 idle + walk，视觉上自然（不是"部件在滑动"的机械感）
- [ ] 性能数据写进文档
- [ ] 渲染方案定稿

---

## S0.10 · Sprint 0 决策门总汇

**目标**：把三周的所有结论收敛成一份可执行的技术选型。

**写 `docs/sprint0-conclusion.md`**，必须包含：

1. **技术选型**：底模、IP-Adapter 方案（FaceID / InstantID / PuLID）、LoRA、ControlNet、参数基线
2. **可行性结论**：验证项 A/B/C 各自的结果与最终决策
3. **petpack 格式终稿**：从 v0.1 draft 升级为 `v0.1-final`
4. **可复现工作流**：ComfyUI workflow JSON（存 `assets/workflows/`）
5. **生成管线规格**：从照片到 petpack 的完整步骤（含每步耗时、失败模式、回退策略）
6. **遗留风险与缓解**：明确写出哪些没解决

**产出物**：`docs/sprint0-conclusion.md` + `assets/workflows/*.json` + `shared/petpack-schema/petpack.schema.json`（终稿）

**验收（Gate）**：
- [ ] 三份决策文档（sprint0-conclusion / petpack-spec / render-benchmark）齐备
- [ ] 存在至少 1 个可复现跑通的 ComfyUI workflow
- [ ] **petpack 格式已冻结**——Phase 1 将以此为准，格式变更需走版本号升级

---

## S0.11 · 补齐 requirements 与素材规范

**目标**：把 `plan-review.md` 第五节列的 7 项遗漏补进正式需求文档。

**写 `docs/requirements.md`**，必须包含：

1. **支持平台矩阵**
   - Windows 11 x64：完整支持
   - Ubuntu 24.04 x64：X11 完整支持 / Wayland 降级模式
   - **Wayland 降级具体说明**（依据已核实事实）：
     - Wayland 下客户端无法自行控制 Z 序，`_NET_WM_STATE_ABOVE` / `Qt::X11BypassWindowManagerHint` 均被忽略
     - **GNOME/Mutter 对普通原生客户端直接禁用置顶**，仅 XWayland 客户端可用 `_NET_WM_STATE_ABOVE`
     - Wayland 下 `WindowTransparentForInput`（点击穿透）不可用
     - → 提供 `QT_QPA_PLATFORM=xcb` 启动选项；KDE Plasma 6 支持 layer-shell，置顶体验明显好于 GNOME → README 按 DE 分别说明
2. **Qt 模块许可红线**：仅 LGPLv3；禁止 Qt Charts / Data Visualization / Purchasing / VirtualKeyboard
3. **性能预算**：空闲 CPU <1%、内存 <150MB（QPainter 方案）、启动 <500ms、单图层 PNG ≤50KB、canvas ≤512×512
4. **隐私规则**：照片默认不出机器（本地 ComfyUI）；云端适配器需用户显式配置并同意；照片与生成物不入库、不入安装包；提供「一键清除所有照片与生成物」
5. **petpack 版本策略**：格式版本号 + 向后兼容规则 + 迁移路径
6. **首版范围界定**：不做联网情侣互动、不做账号系统、不做聊天
7. **渲染方案结论**（来自 S0.9）

**写 `docs/asset-spec.md`（素材规范）**：

- 画布尺寸与基准坐标（200×300 为默认，其他尺寸如何缩放）
- 分层清单与命名约定
- 锚点表规范（如何标、如何在图层上标注）
- 透明通道要求（32bit RGBA，预乘 vs 直通要统一）
- 文件大小与帧数上限
- **一个可运行的 `tools/validate_petpack.py`**：检查必填字段、文件存在性、尺寸、锚点是否在画布内、z_order 是否覆盖所有图层
- **一个可运行的 `tools/preview_petpack.py`**：不依赖 Qt，直接用 PIL 渲染 idle 动画的若干帧成 GIF，用于快速检查素材

> 这两个工具是 Phase 2 "让别人也能做角色包"的基础，也是你 GitHub 项目最有说服力的贡献点之一。

**验收**：
- [ ] `requirements.md` 覆盖上述 7 项
- [ ] `tools/validate_petpack.py` 能对 S0.8 的示例 petpack 正确报错
- [ ] `tools/preview_petpack.py` 能渲染出 idle 预览

---

# Phase 1 · 桌宠引擎（5~7 周）

> 目标：**一个能跑、能拖、能自启、能打包下载的桌宠**，用占位或手绘角色，不含 AI 生成。

## P1.1 · 正式工程骨架

把 S0.2 的最小工程扩展为正式结构：

```
desktop/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── app/           应用生命周期、单实例、命令行
│   ├── pet/           宠物核心：状态机、行为、交互
│   ├── pack/          petpack 加载/校验/缓存
│   ├── render/        渲染层（接口 + 选定实现）★ 可替换
│   ├── platform/      平台能力层：窗口、自启、托盘、显示器
│   └── ui/            设置页等 QML
├── resources/
├── qml/
└── tests/
```

**关键原则**：`platform/` 与 `pet/` 严格分离。`platform/` 里不允许出现任何业务逻辑，`pet/` 里不允许出现任何 `#ifdef Q_OS_WIN`。

**验收**：[ ] 新克隆的机器一条命令能构建并运行

---

## P1.2 · 平台能力层：窗口

**目标**：透明、无边框、置顶、可穿透的桌宠窗口。

**通用部分**：

```cpp
QWidget* w = new QWidget();
w->setWindowFlags(Qt::FramelessWindowHint
                | Qt::Tool                      // 不占任务栏
                | Qt::WindowStaysOnTopHint
                | Qt::WindowDoesNotAcceptFocus  // 不抢焦点
                | Qt::WindowTransparentForInput // 角色外区域点击穿透
               );
w->setAttribute(Qt::WA_TranslucentBackground);
w->setAttribute(Qt::WA_ShowWithoutActivating);
```

**Windows 补充**（Win32 层面）：

```cpp
#ifdef Q_OS_WIN
HWND hwnd = reinterpret_cast<HWND>(w->winId());
LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
SetWindowLong(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
#endif
```

**Ubuntu 补充**：

- X11：`Qt::WindowTransparentForInput` 可用；置顶走 `_NET_WM_STATE_ABOVE`
- Wayland：置顶与穿透均不可用 → 降级为普通悬浮窗（可移动、可置顶于同工作区），并在托盘菜单里提示「Wayland 下为兼容模式，建议用 `QT_QPA_PLATFORM=xcb` 启动以获得完整体验」
- 运行时检测：`qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland"`

**验收**：
- [ ] 桌宠窗口无边框、置顶、不在任务栏
- [ ] 点击角色外的透明区域，事件穿透到下层应用
- [ ] 在 Ubuntu Wayland 下能正确降级并给出提示
- [ ] 桌宠不抢焦点（打字时它不打断）

---

## P1.3 · 托盘与单实例

**托盘**：`QSystemTrayIcon`，菜单项：显示/隐藏、设置、开机自启（勾选项）、退出。

> Linux 注意：托盘图标依赖 `libayatana-appindicator3-1`（Ubuntu 22.04+）或 `libappindicator3-1`，**必须在 .deb 的 Depends 里声明**，否则托盘图标不显示。

**单实例**：用 `QLocalServer` / `QLocalSocket`：

```cpp
// 启动时
if (QLocalSocket::connectToServer("desktoppet-single") .waitForConnected(300)) {
    // 已有实例 → 通知它显示自己，然后退出
    QLocalSocket s; s.connectToServer("desktoppet-single"); s.write("show"); s.flush();
    return 0;
}
QLocalServer* server = new QLocalServer();
QLocalServer::removeServer("desktoppet-single");   // 清理崩溃残留
server->listen("desktoppet-single");
```

**验收**：
- [ ] 重复启动不产生第二个窗口，第二个进程唤起第一个
- [ ] 关闭主窗口后仍可从托盘找回
- [ ] 托盘图标在 Ubuntu 上正常显示

---

## P1.4 · petpack 加载与校验

**目标**：引擎能加载 S0.8 冻结的 petpack 格式。

- C++ 侧用 `QJsonDocument` 解析
- 校验规则与 `tools/validate_petpack.py` **保持一致**（同一份规则文档，两个实现）
- 校验失败要有**可读的错误提示**（给用户看，不是给开发者看）
- 图层资源做缓存，避免重复解码

**验收**：
- [ ] 能加载示例 petpack 并显示
- [ ] 故意改坏一个字段能被检出并给出清晰提示
- [ ] Python 与 C++ 校验器对同一份文件的结论一致

---

## P1.5 · 渲染层实现

按 S0.9 的决策实现 `desktop/src/render/`：

- `RenderBackend` 抽象接口（`loadPack` / `update(dt)` / `render` / `hitTest`）
- 选定实现（QPainter 或 QML）
- 图层变换：平移 / 旋转 / 缩放 / 透明度
- z-order 合成
- 帧率控制（默认 30fps，可配置）

**验收**：
- [ ] 能按 `animations/idle.json` 播放呼吸起伏
- [ ] 走路动画看起来自然（重心转移、身体倾斜配合腿部）
- [ ] 性能达标（对照 S0.9 的指标表）

---

## P1.6 · 动作状态机

**状态**：`Idle` / `Walk` / `Sleep` / `Drag` / `Fall` / `Click` / `Pause`

**规则**：

- 动作优先级：`Drag > Click > Fall > Sleep > Walk > Idle`
- 冷却时间：同一动作最小间隔（防抖）
- 随机待机：Idle 状态下随机播放小动作（眨眼、转头、抖身体）
- 时间段：早晚时段切 Sleep 或 Greet

**验收**：
- [ ] 状态切换符合优先级，不会被低优先级动作打断
- [ ] 快速连续点击不会导致动作卡死
- [ ] 长时间挂机不出现异常状态

---

## P1.7 · 交互行为

- 点击角色 → 播放 click 反馈
- 拖拽 → 跟随鼠标，松手后落地（Fall → Idle）
- 屏幕边缘 → 转向或停下
- 右键 → 菜单
- 锁定位置（不响应拖拽）

**验收**：
- [ ] 拖拽跟手，松手有落地动画
- [ ] 拖到屏幕边缘会转向
- [ ] 拖到屏幕外不会丢失角色（约束在虚拟桌面内）

---

## P1.8 · 设置与配置持久化

**设置项**：开机自启、置顶开关、锁定位置、点击穿透、缩放比例、音量/静音、动作频率、显示哪只宠物

**存储**：`QSettings`（Win: 注册表 / Linux: `~/.config/`），存 JSON 格式便于调试。

**验收**：
- [ ] 设置重启后保留
- [ ] 配置文件损坏时能回退到默认值而不是崩溃

---

## P1.9 · 开机自启

**Windows**：写注册表 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`（当前用户范围，无需管理员权限）

```cpp
QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
              QSettings::NativeFormat);
reg.setValue("DesktopPet", appPath);
```

**Ubuntu**：写 `~/.config/autostart/desktoppet.desktop`

```ini
[Desktop Entry]
Type=Application
Name=DesktopPet
Exec=/path/to/desktoppet
Icon=/path/to/icon.png
X-GNOME-Autostart-enabled=true
```

**验收**：
- [ ] 注销重登后自动启动
- [ ] 位置、大小、设置都保持
- [ ] 关闭自启后，对应注册表项 / desktop 文件被正确清除

---

## P1.10 · 多显示器与高 DPI

- 记录宠物所在显示器，跨屏时保持相对位置
- 高 DPI：Qt 6 默认 `AA_EnableHighDpiScaling`，需验证 125%/150% 缩放下素材清晰
- 不同分辨率的显示器之间移动时缩放比例处理

**验收**：[ ] 双屏不同缩放比例下都正常

---

## P1.11 · Phase 1 验收与首个可下载版本

**产出**：v0.1 安装包（Windows .exe + Linux .deb），GitHub Release。

**验收清单**：
- [ ] Win11：安装、运行、托盘、自启、卸载
- [ ] Ubuntu 24.04：X11 下全功能；Wayland 下正确降级
- [ ] 空闲 CPU <1%、内存 <150MB
- [ ] 连续运行 8 小时无内存泄漏

---

# Phase 2 · 角色与生成（5~7 周）

## P2.1 · 手工角色包（2 个）

按 `docs/asset-spec.md` 产出 2 个 Q 版角色（各含 idle/walk/sleep/click/fall 等动作）。

> 参考 VPet 规格：单张 PNG ≤50KB、单组动画 ≤20 帧、推荐 200×300px。

**验收**：[ ] 两个角色能被引擎加载，切换角色正常

## P2.2 · 角色包分发机制

- 角色包 = 一个 zip（`*.petpack`），带元数据（作者、许可、版本）
- 引擎支持导入/导出
- 为 GitHub 项目打基础：用户能互相分享角色包

**验收**：[ ] 导出的角色包能在另一台机器导入

## P2.3 · 生成适配器接口

**定义统一接口**（关键设计，务必在此固化）：

```cpp
class GeneratorAdapter {
public:
    virtual ~GeneratorAdapter() = default;
    virtual QString name() const = 0;
    virtual bool isAvailable() const = 0;          // 本地后端是否在运行
    virtual TaskHandle submit(const GenerationRequest&) = 0;
    virtual void cancel(TaskHandle) = 0;
    virtual GenerationResult poll(TaskHandle) = 0;   // 或用信号回调
};

struct GenerationRequest {
    QStringList photoPaths;
    QString style;          // "chibi" / "pixel" / "anime" / "flat"
    QString description;    // 服装、配色、饰品偏好
    int candidateCount = 4;
    QString outputDir;      // 本地临时目录
};
```

**实现三个适配器**：
1. `ComfyUIAdapter`（默认，本地 `http://127.0.0.1:8188`）
   - **本机已有 RTX 3060 6GB 可用**，这个适配器就是主路径
   - 客户端通过 ComfyUI 的 HTTP API（`/prompt` 提交、`/history` 查结果、`/view` 取图）调用，**不直接起 ComfyUI 进程**
   - 需处理 ComfyUI 未启动的情况：给用户明确提示 + 一键启动命令
   - **分辨率与耗时提示**：6GB 机器出 768×768 约 20~60 秒/张，UI 上要显示进度和预计时间
2. `ManualImportAdapter`（永远可用的兜底：用户直接放一个 petpack 进来）
3. `CloudApiAdapter`（后续，可选，用户自备 key）

**验收**：[ ] 三种适配器可互换，引擎不感知具体是哪个

## P2.4 · 生成页面 UI

独立页面，**不与桌宠窗口混在一起**。

- 上传 1~3 张照片（拖拽）
- 上传前明确提示：**只能使用自己或已获同意者的照片**
- 本地预处理：裁切、压缩、**清除 EXIF**（含 GPS）
- 风格选择：Q版 / 像素 / 日系 / 扁平插画
- 偏好输入：服装、配色、饰品
- 生成队列 + 进度 + 失败重试 + 候选结果网格
- **生成服务不可用时，桌宠功能完全不受影响**（这条是硬要求）

**验收**：
- [ ] 断网 / 后端未启动时，桌宠仍正常工作
- [ ] 照片 EXIF 已清除（用 `exiftool` 验证）

## P2.5 · 槽位化逐部件生成

把 S0.7 的验证变成正式功能：

- 引擎内置 `slots.png` 槽位参考图
- 逐部件 inpainting 生成
- 输出到临时目录

**验收**：[ ] 能稳定产出完整的一套分层 PNG

## P2.6 · 自动切层与锚点绑定

- 切层：槽位法为主，SAM/rembg 为辅
- 锚点绑定：读槽位图坐标（确定性）
- **绑定失败要给「需要微调」提示，而不是产出失真桌宠**（这条在原方案里就有，很关键，保留）

## P2.7 · 角色资料库

- 生成结果先存为「角色资料」（外观层 + 元数据），**不直接变成桌宠**
- 列表页支持：预览、重命名、删除、重新编辑、导出 petpack
- 存储位置：`~/.local/share/desktoppet/characters/`（Linux）/ `%APPDATA%`（Win）

**验收**：[ ] 角色资料可管理，可单独重生成某个部件

## P2.8 · 一键转桌宠

流程：选定角色资料 → 自动切层/绑定 → 输出 petpack → 载入引擎 → 预览各动作 → 可单独重生成/替换某个动作。

**验收**：
- [ ] 从选形象到桌宠出现，一条路径走完
- [ ] **动作切换时外观完全一致**（这是分层+骨架架构的核心收益，必须验证）
- [ ] 无法可靠适配时显示「需要微调」而不是产出坏角色

---

# Phase 3 · 发布与打磨（3~4 周）

## P3.1 · Windows 打包

- 工具：`windeployqt`（Qt 自带）收集依赖 DLL
- 安装器：**Inno Setup**（当前用户范围，无需管理员）或 NSIS
- 产物：`DesktopPet-Setup-x64.exe`

```bash
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw desktop.exe
```

**注意**：未签名的 exe 会被 Windows SmartScreen 拦截。
- 短期：README 里写明"需在 SmartScreen 提示中选择『更多信息 → 仍要运行』"
- 长期：购买代码签名证书（可选，不阻塞发布）

## P3.2 · Linux 打包

- `.deb`（主）：`CPack` 的 DEB generator，或 `fpm`
- `AppImage`（便携）：`linuxdeploy` + `linuxdeploy-qt`
- **依赖声明必须写全**，否则用户机器上托盘图标不显示：

```
Depends: libqt6core6t64, libqt6gui6t64, libqt6widgets6t64, libqt6qml6,
         libqt6quick6, libqt6svg6, libglib2.0-0, libayatana-appindicator3-1
```

> `libayatana-appindicator3-1` 是 Ubuntu 22.04+ 的托盘依赖，漏了它托盘图标直接消失。

**AppImage 自启修复**：AppImage 被移动位置后 `~/.config/autostart` 里的路径会失效 → 启动时检测路径失效，弹提示"AppImage 位置已变更，是否修复自启配置"。

## P3.3 · GitHub Actions 发布

在 CI 基础上加 `release.yml`：打 tag `v*` 时自动构建三平台产物并创建 Release。

```yaml
on:
  push:
    tags: ['v*']
```

**产出**：Windows exe、Linux .deb、Linux AppImage 三个产物

## P3.4 · 文档

- `README.md`（含 GIF 演示、安装、Wayland 说明、隐私声明）
- `docs/install.md`（三平台安装步骤）
- `docs/privacy.md`（隐私政策：照片不收集、不上传、可一键清除）
- `docs/petpack-spec.md`（公开的格式规范 → 别人能做角色包）
- `docs/asset-spec.md`（素材规范）
- `CONTRIBUTING.md`、`SECURITY.md`
- Issue / PR 模板

## P3.5 · 测试矩阵

| 平台 | 场景 |
|---|---|
| Win11 | 安装、卸载、托盘、双屏、高 DPI、自启、升级 |
| Ubuntu 24.04 | GNOME / KDE、Wayland / X11、AppImage 移动后自启修复 |
| 通用 | 断网、生成失败、重复启动、磁盘空间不足、旧配置与旧 petpack 升级迁移、照片/角色删除与临时文件清理 |

## P3.6 · v1.0 发布

**验收**：
- [ ] GitHub Release 有可安装包 + 安装文档 + 隐私政策 + 已知限制
- [ ] 新用户按 README 能独立完成安装

---

# Phase 4 · 异地情侣（4~6 周）

> ⚠️ **petpack 格式已在 S0.8 预留 `character_id` 与 `remote.trigger_hooks`**。本阶段不改动引擎核心，只加同步层。

## P4.1 · 配对

- 双方各生成一个配对码（短字符串 + 校验位）
- 交换后建立绑定关系
- 配对信息本地加密存储

## P4.2 · 同步通道

**架构**（保持"项目方不接触数据"的原则）：

```
客户端A  ──HTTPS/WebSocket──▶  中继服务（只转发密文，不解密）  ──▶  客户端B
```

- 中继服务只见到密文
- 传输层 TLS + 消息层 E2E 加密（可用 NaCl/libsodium 的 box 方案，或 libsodium secretbox + 预共享密钥）
- 密钥交换：配对时用 PAKE（如 SPAKE2）避免中间人

**或者更简单**：既然是两个人，也可以用「WebRTC 直连 + 中继打洞」，但首版用中继更稳。

## P4.3 · 情侣动作

- 送心：A 点击送心 → B 的桌宠出现爱心特效
- 状态同步：对方桌宠在做什么（B 显示"A 在睡觉"）
- 留言小纸条
- 共同动作：同时触发"一起睡觉"

## P4.4 · 撤销配对与安全

- 一键解除配对 → 销毁本地密钥
- 中继服务上的数据按 TTL 删除
- 配对码有尝试次数限制，防暴力枚举

## P4.5 · 双机联动测试

- [ ] 两台机器（Win + Ubuntu）配对成功
- [ ] 送心能实时到达
- [ ] 断网重连后状态正确
- [ ] 解除配对后不再互通

---

# 附录 A · 关键命令速查

## 构建

```bash
# 配置
cmake -S desktop -B build/desktop -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64

# 编译
cmake --build build/desktop

# 跑测试
ctest --test-dir build/desktop --output-on-failure

# Linux 部署（收集依赖）
linuxdeploy-qt build/desktop/desktop -AppDir
```

## 调试桌宠

```bash
# 强制 XWayland（Ubuntu 获得完整置顶+穿透）
QT_QPA_PLATFORM=xcb ./desktop

# 查看 Wayland/X11 模式
echo $XDG_SESSION_TYPE

# 抓取窗口属性（X11）
xprop -id <window_id>
```

## 素材工具

```bash
# 校验 petpack
python3 tools/validate_petpack.py path/to/role.petpack

# 渲染预览 GIF
python3 tools/preview_petpack.py path/to/role.petpack -o preview.gif
```

---

# 附录 B · 每阶段的「完成定义」（DoD）

一个阶段只有全部满足才算完成：

### Sprint 0 完成定义
- [ ] 三份技术文档齐备（conclusion / petpack-spec / render-benchmark）
- [ ] 至少 1 个可复现的 ComfyUI workflow
- [ ] petpack 格式冻结，schema 校验器可用
- [ ] 明确写下"AI 直接生成多帧是否可行"的结论
- [ ] `requirements.md` 覆盖 7 项遗漏
- [ ] CI 双平台绿

### Phase 1 完成定义
- [ ] v0.1 安装包可下载安装
- [ ] 双平台托盘 / 自启 / 穿透 / 多屏 / 高 DPI 全部通过
- [ ] 性能达标（CPU <1%，内存 <150MB）
- [ ] 连续运行 8 小时无泄漏

### Phase 2 完成定义
- [ ] 照片 → 角色资料 → 桌宠 全流程跑通
- [ ] 动作切换时外观完全一致
- [ ] 后端不可用时桌宠不受影响
- [ ] 照片 EXIF 已清除，隐私可一键清除
- [ ] 角色包可导入导出

### Phase 3 完成定义
- [ ] GitHub Release 三平台产物齐全
- [ ] README / 安装文档 / 隐私政策 / 格式规范齐备
- [ ] 测试矩阵全部通过

### Phase 4 完成定义
- [ ] 双机配对、送心、状态同步可用
- [ ] E2E 加密，撤销配对后完全失效
- [ ] 中继服务不接触明文
