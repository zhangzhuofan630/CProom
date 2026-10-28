# 情侣桌宠 · 执行进度追踪

> 详细步骤见 `docs/execution-plan.md`
> 用法：做完一步就勾选并写日期；卡住了在「备注」里记一句原因。

**当前阶段**：Sprint 0
**开始时间**：2026-10-28

---

## Sprint 0 · 验证尖刺（1.5~2 周）

> 状态图例：✅ 完成 ／ ◐ 部分完成（卡在等外部条件）／ ☐ 未开始

| # | 步骤 | 状态 | 完成日 | 备注 |
|---|---|---|---|---|
| S0.1 | 仓库与目录骨架 | ◐ | 10-28 | 文件已全部落盘并本地提交，**只差填仓库名 + push** |
| S0.2 | 开发环境与 CI 骨架 | ◐ | 10-28 | 工程/CI 已写，**差本机装 Qt 6.8.3 + CI 跑绿** |
| S0.3 | 准备 GPU 环境（本机 RTX 3060 6GB） | ☐ | | |
| S0.4 | 部署 ComfyUI 与模型 | ☐ | | |
| S0.5 | 验证项 A：照片→Q版角色 🔀 | ☐ | | |
| S0.6 | 验证项 B：多姿势一致性 🔀 | ☐ | | |
| S0.7 | 自动分层与锚点绑定验证 | ☐ | | |
| S0.8 | 渲染选型与 petpack 格式冻结 | ☐ | | |
| S0.9 | 最小渲染 demo + 性能实测 🔀 | ☐ | | |
| S0.10 | Sprint 0 决策门总汇 | ☐ | | |
| S0.11 | requirements 与素材规范 | ☐ | | |

### S0.1 / S0.2 剩余动作（按顺序做）

已落盘的文件清单：`desktop/`（CMake + 最小程序 + 一个 ctest 用例）、`.github/workflows/ci.yml`（Linux+Windows 双平台）、`.gitignore`、`README.md`、`LICENSE`(MIT)、`CONTRIBUTING.md`、`SECURITY.md`、Issue/PR 模板、`.editorconfig`、`.gitattributes`、目录占位。本地已 `git init` 并提交（commit `e2db67b`）。

**① 授权 SSH key 并推送**

仓库地址已确认：`git@github.com:zhangzhuofan630/CProom.git`（**Public**）。
`origin` 已配好，SSH key 已生成在 `~/.ssh/id_ed25519`，SSH 链路实测可通（返回 `Permission denied (publickey)`，即已到认证环节）。

**剩下唯一动作**：把下面这行公钥贴到 https://github.com/settings/keys → `New SSH key`（Title 随便写，如 `zzf-ubuntu`，Key type 选 `Authentication Key`）：

```
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIJ19a1Q2zyt4pouubH9kqrFs1bf/pSEC7cL0/u8IAMy/ zhangzhuofan630@users.noreply.github.com
```

贴完后验证并推送：

```bash
cd /home/zzf/Project/情侣桌宠
ssh -T git@github.com          # 应返回 "Hi zhangzhuofan630!"
git push -u --force origin main
```

> 首次推送用 `--force`：远端那个网页建的 `Initial commit`（README 只有一个 `-`）与本地历史无关，不 force 会因 unrelated histories 被拒。
> 后续日常推送就不用 force 了。


**② 装 Qt 6.8.3 并本地跑通**

```bash
python3 -m venv ~/.venvs/aqt && ~/.venvs/aqt/bin/pip install -U pip aqtinstall
~/.venvs/aqt/bin/aqt install-qt linux desktop 6.8.3 -O ~/Qt      # 约 2GB
sudo apt install -y ninja-build libgl1-mesa-dev libxkbcommon-dev libxkbcommon-x11-0 libxcb-cursor0

cmake -S desktop -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64
cmake --build build
ctest --test-dir build --output-on-failure
./build/desktop/desktop
```

看到窗口标题「Desktoppet」、内容「情侣桌宠 · 工程骨架」即通过。

**③ 确认 CI 变绿**

推上去后在仓库 Actions 页看 `CI` 这次运行：`Build (Linux)` 和 `Build (Windows)` 都要绿。**这一步比本地编译更重要** —— 后面所有平台问题都是在这里第一次暴露的。

**④ 打开 Issues**

```bash
gh repo edit --enable-issues --enable-discussions
```

> 仓库已确认是 **Public**（`zhangzhuofan630/CProom`），Phase 3 发 Release 无需再改可见性。公开仓库的 Actions 额度不限（私有仓库才限 2000 分钟/月）。


**硬件环境（已确认）**：RTX 3060 Laptop **6GB 显存** / 驱动 580 / CUDA 13.0 / Ubuntu 22.04.5

**Sprint 0 决策结论记录**（完成后填）：

- 验证项 A（单图人脸辨识度）：评分 ___/5，决策：_______________
- 验证项 B（多帧一致性）：路线A 可行？☐ 是 ☐ 否（若 OOM 未测完 → ☐ 待补测）→ petpack 结构定稿为：_______________
- 渲染方案：☐ QPainter ☐ QML
- 底模 / IP-Adapter 方案 / LoRA：_______________
- 实际可用分辨率：_______________（若 6GB 只能 640，记下来，Phase 2 的产品设计要据此调整）
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

- [x] ~~创建 GitHub 远端仓库~~ → 已建：**`zhangzhuofan630/CProom`（Public）**
- [ ] 把 SSH 公钥贴到 GitHub（key 已生成，见上文 ①）
- [x] ~~租云 GPU~~ → **本机 RTX 3060 6GB 即可，无需租**
- [ ] 准备 5 张测试照片（正脸为主，**不要提交到 Git**）
- [ ] 找 1~2 个 Chibi LoRA（Civitai 搜索 chibi / Q版，**优先找 SDXL 版，显存开销小**）
- [ ] 准备一台 Windows 11 测试机（Phase 1 验收用）

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
