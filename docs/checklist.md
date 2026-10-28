# 情侣桌宠 · 执行进度追踪

> 详细步骤见 `docs/execution-plan.md`
> 用法：做完一步就勾选并写日期；卡住了在「备注」里记一句原因。

**当前阶段**：Sprint 0
**开始时间**：2026-10-28
**远端仓库**：`git@github.com:zhangzhuofan630/CProom.git`（Public）

---

## Sprint 0 · 验证尖刺（1.5~2 周）

> 状态图例：✅ 完成 ／ ◐ 部分完成（卡在等外部条件）／ ☐ 未开始

| # | 步骤 | 状态 | 完成日 | 备注 |
|---|---|---|---|---|
| S0.1 | 仓库与目录骨架 | ✅ | 10-28 | 已推送，CI 双平台绿 |
| S0.2 | 开发环境与 CI 骨架 | ✅ | 10-28 | Qt 6.8.3 本地编译通过，ctest 通过 |
| S0.3 | 准备 GPU 环境（本机 RTX 3060 6GB） | ✅ | 10-28 | ComfyUI 0.43.0 + torch 2.14.1+cu130，GPU 可用 |
| S0.4 | 部署 ComfyUI 与模型 | ✅ | 10-28 | 15GB 模型就位，IPAdapter/InstantID 节点全注册 |
| S0.5 | 验证项 A：照片→Q版角色 🔀 | ✅ | 10-28 | **GO**。19 张实测，可辨认 Q 版形象成立，无需 LoRA。详见 `docs/s05-report.md` |
| S0.6 | 验证项 B：多姿势一致性 🔀 | ✅ | 10-28 | **GO**。固定 embedding 换姿势，脸一致；但侧脸做不出、复杂姿势有杂物 |
| S0.7 | 自动分层与锚点绑定验证 | ✅ | 10-28 | 几何切层通过（三层+12px overlap）；发现**衣领需独立成层且 z 序在头之上**，否则头转 >5° 露缝 |
| S0.8 | 渲染选型与 petpack 格式冻结 | ◐ | 10-28 | **草案已写**：`docs/petpack-spec-draft.md`（四层 + z-order + constraints）。待 Qt 实测反证后冻结 |
| S0.9 | 最小渲染 demo + 性能实测 🔀 | ◐ | 10-28 | Web Canvas 版跑通（4 层 60fps），但**不能替代 Qt 实测**，CPU/内存指标仍待测 |
| S0.10 | Sprint 0 决策门总汇 | ☐ | | |
| S0.11 | requirements 与素材规范 | ☐ | | |

### S0.1 / S0.2 完成记录

**实际做法与实测结果**（以此为准，`execution-plan.md` 里的命令是原始设计）：

```bash
# Qt 6.8.3 —— 注意 aqtinstall 3.x 的参数变了
python3 -m venv ~/.venvs/aqt
~/.venvs/aqt/bin/pip install -U pip aqtinstall
~/.venvs/aqt/bin/aqt install-qt linux desktop 6.8.3 -O ~/Qt -d ~/.cache/aqt-archives

# 构建（不需要 ninja，系统 make 即可；也没装任何 -dev 包，OpenGL 系统自带）
cmake -S desktop -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
./build/bin/desktop
```

| 验收项 | 结果 |
|---|---|
| 仓库可 clone、远端有内容 | ✅ `main` 分支已推 |
| CI Linux + Windows 双平台 | ✅ 两个 job 都 success |
| 本地能编译 | ✅ gcc 11 / Qt 6.8.3 |
| ctest 通过 | ✅ 1/1 passed |
| 程序能启动 | ✅ offscreen 下事件循环正常 |
| `.gitignore` 生效 | ✅ `build/`、照片、模型均被排除 |

### 环境坑（都踩过了，别再重复）

| # | 现象 | 原因 | 解法 |
|---|---|---|---|
| 1 | `aqt: The '--base' option requires a url...` | **aqtinstall 3.0 起 `-b/--base` 含义变成镜像 URL**，不再是本地目录 | 归档目录用 `-d`；不传 `-b` |
| 2 | `No space left on device` | 下载解压的临时目录空间不足 | 设 `TMPDIR=<大盘目录>` 再跑 |
| 3 | GUI 起不来 / xcb 插件加载失败 | **Ubuntu 22.04 缺 `libxcb-cursor0`**，Qt 6.5+ 的 xcb 插件硬依赖它 | `sudo apt install libxcb-cursor0` |
| 4 | 可执行文件找不到 | 原文档写 `build/desktop/desktop`，实际 CMake 输出是 `build/desktop`（文件） | 已在 CMake 里加 `RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin`，现在路径是 `build/bin/desktop` |
| 5 | 文件属主是 root，`git status` 报 dubious ownership | 用 AI 助手执行时以 root 身份写入 | 见下方「一次性修复」 |
| 6 | `pip install torch` 卡十分钟不动 | 阿里镜像对超大 wheel 限速（实测 0.24 MB/s，HF 是 5.4 MB/s）；`--resume-retries` 会重建临时目录**从头下**，等于没续传 | 大文件改走 `files.pythonhosted.org`（7.5 MB/s）或多线程下载器 |
| 7 | 下载完的文件只有一半大，旁边一堆 `.partN` | 环境的批量删除保护累计 50 次后拦截分片清理，合并中途抛异常 | 分片删除包 `try/except BaseException`；校验只认字节数 |
| 8 | `curl` 报 `try 'curl --help'` 且 exit 2 | **Ubuntu 22.04 的 curl 7.81 没有 `--timeout`**，只有 `--max-time` | 大文件别设 `--max-time`（会截断），只设 `--connect-timeout` |
| 9 | FaceID 报 `proj_in` 形状 1280 vs 1664 不匹配 | `h94/IP-Adapter` 里有**两个不同的** `image_encoder`：`sdxl_models/` 是 ViT-bigG(1664)，`models/` 是 ViT-H(1280)；FaceID Plus v2 要 ViT-H | 两个都下，FaceID 用 ViT-H |
| 10 | InstantID 出图头大身小、构图崩 | InstantID 用 ControlNet 钉住面部关键点位置，与 chibi 的大头比例冲突 | Q 版路线改用 IP-Adapter FaceID，InstantID 留给写实风格 |

### 一次性修复（需你自己在终端执行，需要 sudo 密码）

```bash
sudo chown -R $USER:$USER ~/Project/情侣桌宠 ~/Qt ~/.venvs ~/.cache
sudo apt install -y libxcb-cursor0
```

> 第一条修属主（否则 `git status`、重新编译都可能因权限失败），第二条补上 Qt 运行必需的 xcb 库。

**硬件环境（已确认）**：RTX 3060 Laptop **6GB 显存** / 驱动 580 / CUDA 13.0 / Ubuntu 22.04.5

**Sprint 0 决策结论记录**（完成后填）：

- 验证项 A（单图人脸辨识度）：评分 ___/5，决策：_______________（**管线已跑通，待真人照片**）
- 验证项 B（多帧一致性）：路线A 可行？☐ 是 ☐ 否（若 OOM 未测完 → ☐ 待补测）→ petpack 结构定稿为：_______________
- 渲染方案：☐ QPainter ☐ QML
- 底模 / IP-Adapter 方案 / LoRA：**animagine-xl-4.0 + IP-Adapter FaceID Plus v2** ✅ 已实测；
  InstantID 在 Q 版上构图崩坏，仅保留给写实风格；LoRA 待下载
- 实际可用分辨率：**832×832 已稳定（显存 3.8GB / 6GB，耗时 29s）**，可上探 1024×1024
- petpack 格式版本：_______________

**Sprint 0 期间遇到的显存问题与解法**（供 Phase 2 参考）：

| 现象 | 原因 | 解法 |
|---|---|---|
|  |  |  |

---

## Phase 1 · 桌宠引擎（5~7 周）

| # | 步骤 | 状态 | 完成日 | 备注 |
|---|---|---|---|---|
| P1.1 | 正式工程骨架 | ☐ | | |
| P1.2 | 平台能力层：窗口 | ☐ | | |
| P1.3 | 托盘与单实例 | ☐ | | |
| P1.4 | petpack 加载与校验 | ☐ | | |
| P1.5 | 渲染层实现 | ☐ | | |
| P1.6 | 动作状态机 | ☐ | | |
| P1.7 | 交互行为 | ☐ | | |
| P1.8 | 设置与配置持久化 | ☐ | | |
| P1.9 | 开机自启 | ☐ | | |
| P1.10 | 多显示器与高 DPI | ☐ | | |
| P1.11 | Phase 1 验收与 v0.1 发布 | ☐ | | |

---

## Phase 2 · 角色与生成（5~7 周）

| # | 步骤 | 状态 | 完成日 | 备注 |
|---|---|---|---|---|
| P2.1 | 手工角色包（2 个） | ☐ | | |
| P2.2 | 角色包分发机制 | ☐ | | |
| P2.3 | 生成适配器接口 | ☐ | | |
| P2.4 | 生成页面 UI | ☐ | | |
| P2.5 | 槽位化逐部件生成 | ☐ | | |
| P2.6 | 自动切层与锚点绑定 | ☐ | | |
| P2.7 | 角色资料库 | ☐ | | |
| P2.8 | 一键转桌宠 | ☐ | | |

---

## Phase 3 · 发布与打磨（3~4 周）

| # | 步骤 | 状态 | 完成日 | 备注 |
|---|---|---|---|---|
| P3.1 | Windows 打包 | ☐ | | |
| P3.2 | Linux 打包（.deb / AppImage） | ☐ | | |
| P3.3 | GitHub Actions 发布 | ☐ | | |
| P3.4 | 文档 | ☐ | | |
| P3.5 | 测试矩阵 | ☐ | | |
| P3.6 | v1.0 发布 | ☐ | | |

---

## Phase 4 · 异地情侣（4~6 周）

| # | 步骤 | 状态 | 完成日 | 备注 |
|---|---|---|---|---|
| P4.1 | 配对 | ☐ | | |
| P4.2 | 同步通道（E2E 加密中继） | ☐ | | |
| P4.3 | 情侣动作 | ☐ | | |
| P4.4 | 撤销配对与安全 | ☐ | | |
| P4.5 | 双机联动测试 | ☐ | | |

---

## 待办备忘

- [x] ~~创建 GitHub 远端仓库~~ → **`zhangzhuofan630/CProom`（Public）**
- [x] ~~SSH key 与推送~~ → key 已生成并授权，3 个 commit 已推
- [ ] `sudo chown -R` 修属主 + 装 `libxcb-cursor0`（见上方「一次性修复」）
- [x] ~~租云 GPU~~ → **本机 RTX 3060 6GB 即可，无需租**
- [x] ~~准备 5 张测试照片~~ → 已提供并跑完矩阵，**文件在 `~/AI/photos/`，不要提交到 Git**
- [ ] 找 1~2 个 Chibi LoRA（Civitai 搜索 chibi / Q版，**优先找 SDXL 版，显存开销小**）
- [ ] 准备一台 Windows 11 测试机（Phase 1 验收用）
- [ ] **S0.6 收尾**：定稿角色形象（用户确认哪张最像）+ 抠图与切层验证

## 6GB 显存速查

| 想做的事 | 建议分辨率 | 备注 |
|---|---|---|
| SDXL 纯文生图 | 768×768 | 起点 |
| SDXL + IP-Adapter FaceID | 768×768 | Sprint 0 主线配置 |
| SDXL + FaceID + LoRA | 768×768 | LoRA 开销小 |
| SDXL + FaceID + ControlNet | 640×640 或分段跑 | 接近上限 |
| SDXL + InstantID | 640×640 或最后再试 | 自带 ControlNet，最重 |
| FLUX.1 | 不建议 | 6GB 需 fp8 量化 |

**启动参数**：`python main.py --lowvram --preview-method none --use-pytorch-cross-attention`
**降级顺序**：分辨率 → 断 ControlNet → 断 LoRA → `--use-cpu-offload` → 换 SD1.5
