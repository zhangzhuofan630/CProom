# ComfyUI 生成环境搭建记录（S0.3 / S0.4）

> 目标：在本机 RTX 3060 Laptop（6GB 显存）上跑通「照片 → Q版角色」的生成管线。
> 本文记录实际执行的命令、模型清单、以及踩过的坑，供换机重装时复现。

## 环境事实

| 项 | 值 |
|---|---|
| GPU | NVIDIA GeForce RTX 3060 Laptop, 6144 MiB |
| 驱动 / CUDA | 580.178.04 / CUDA 13.0（向下兼容 cu12x wheel） |
| 系统 | Ubuntu 22.04.5 |
| Python | 3.10.12（系统自带，`/usr/bin/python3.10`） |
| ComfyUI | 0.43.0，位于 `/home/zzf/AI/ComfyUI` |
| venv | `/home/zzf/AI/ComfyUI/venv` |

**为什么用 Python 3.10 而不是 3.13**：`insightface` 需要现场编译，3.10 上成功率最高；
`onnxruntime-gpu` 对 3.12+ 的预编译 wheel 覆盖也更晚。

## 安装步骤

```bash
# 1. 取 ComfyUI
mkdir -p ~/AI && cd ~/AI
git clone --depth 1 https://github.com/comfyanonymous/ComfyUI.git

# 2. venv + 镜像（只写 venv 级配置，不要写全局，否则污染本机其他项目）
cd ~/AI/ComfyUI
/usr/bin/python3.10 -m venv venv
./venv/bin/pip install -U pip setuptools wheel
./venv/bin/pip config set --site global.index-url https://mirrors.aliyun.com/pypi/simple/
./venv/bin/pip config set --site global.trusted-host mirrors.aliyun.com

# 3. PyTorch —— TMPDIR 必须指到大磁盘，/tmp 只有 10MB
mkdir -p ~/.cache/pip-tmp
TMPDIR=~/.cache/pip-tmp ./venv/bin/pip install torch torchvision torchaudio

# 4. ComfyUI 依赖
TMPDIR=~/.cache/pip-tmp ./venv/bin/pip install -r requirements.txt

# 5. 自定义节点
cd custom_nodes
git clone --depth 1 https://github.com/cubiq/ComfyUI_IPAdapter_plus.git
git clone --depth 1 https://github.com/cubiq/ComfyUI_InstantID.git

# 6. 人脸相关依赖（insightface 需编译，有 gcc 即可）
TMPDIR=~/.cache/pip-tmp ../venv/bin/pip install insightface onnxruntime-gpu
```

## 模型清单

全部放在 `~/AI/ComfyUI/models/` 下，**不进 Git**（体积 15GB+，且与项目代码无关）。

| 文件 | 放置目录 | 大小 | 来源 |
|---|---|---|---|
| `animagine-xl-4.0.safetensors` | `checkpoints/` | 6.5G | `cagliostrolab/animagine-xl-4.0` |
| `ip-adapter.bin` | `instantid/` | 1.6G | `InstantX/InstantID` |
| `instantid_controlnet.safetensors` | `controlnet/` | 2.4G | `InstantX/InstantID` 的 `ControlNetModel/diffusion_pytorch_model.safetensors` 改名 |
| `ip-adapter-faceid-plusv2_sdxl.bin` | `ipadapter/` | 1.5G | `h94/IP-Adapter-FaceID` |
| `CLIP-ViT-H-14-laion2B-s32B-b79K.safetensors` | `clip_vision/` | 2.5G | `h94/IP-Adapter` 的 `models/image_encoder/model.safetensors` |
| `CLIP-ViT-bigG-14-laion2B-39B-b160k.safetensors` | `clip_vision/` | 3.5G | `h94/IP-Adapter` 的 `sdxl_models/image_encoder/model.safetensors` |
| `antelopev2/`（5 个 onnx） | `insightface/models/antelopev2/` | 350M | `MonsterMMORPG/tools` 的 `antelopev2.zip` |

**两个 CLIP vision 都要有，不能只留一个**：

| 用途 | 需要哪个 | 维度 |
|---|---|---|
| SDXL 通用 IP-Adapter | ViT-bigG（3.5G） | 1664 |
| IP-Adapter **FaceID Plus v2** | ViT-H（2.5G） | 1280 |

`h94/IP-Adapter` 仓库里两个 `image_encoder` 是**不同的模型**：
`sdxl_models/image_encoder` 是 bigG，`models/image_encoder` 是 ViT-H。
只下前者时 FaceID 会报：

```text
size mismatch for perceiver_resampler.proj_in.weight:
copying a param with shape torch.Size([2048, 1280]) from checkpoint,
the shape in current model is torch.Size([2048, 1664])
```

看到 1280 vs 1664 这个报错，就是喂错了 CLIP vision，不是权重文件坏了。


**底模选择理由**：`animagine-xl-4.0` 是动漫向 SDXL finetune，Q 版化最直接。
如果人脸保真度不够，备选写实底模 `SG161222/RealVisXL_V4.0`（单文件 `RealVisXL_V4.0.safetensors`）
或 `RunDiffusion/Juggernaut-XL-v9`，配合强 Chibi LoRA 走「写实底模 + 风格化」路线。

## 启动

```bash
bash ~/AI/start-comfyui.sh
# 浏览器打开 http://127.0.0.1:8188
```

脚本里的参数针对 6GB 显存：

| 参数 | 作用 |
|---|---|
| `--lowvram` | 模型分块换入换出，6GB 跑 SDXL 的必要条件 |
| `--preview-method none` | 关预览，省显存也省时间 |
| `--use-pytorch-cross-attention` | 用 PyTorch 原生 SDPA，省去装 xformers |

**两个不要加的参数**（都是 6GB 上的反优化）：

- `--disable-smart-memory`：关掉闲置模型自动卸载，反而更容易 OOM
- `--gpu-only`：不往 CPU 卸载任何东西，6GB 直接爆

## 踩坑记录

### 1. `/tmp` 只有 10MB

任何需要临时空间的操作（pip 解包 554MB 的 torch wheel、aqtinstall 解压 Qt）都会
`No space left on device`。**必须前置 `TMPDIR=~/.cache/pip-tmp`**。

### 2. 阿里镜像对大文件限速，且 pip 的 `--resume-retries` 不续传

现象：`pip install torch` 卡在 `Downloading torch-2.14.1...whl (554.6 MB)` 十分钟不动，
临时目录里的 wheel 文件大小不再增长。加 `--resume-retries 30` 后 pip 新建了临时目录
**从头下载**，实测仍只有 0.24 MB/s。

对比实测（同一文件）：

| 源 | 速度 |
|---|---|
| 阿里镜像 单连接 | 1.26 MB/s |
| 阿里镜像 8 线程 | 0.42 MB/s（并发反而被限） |
| HuggingFace 单连接 | 5.4 MB/s |
| HuggingFace 4 并发 | 合计 8.7 MB/s |

结论：**阿里镜像只适合小包**；MB 级以上的大文件走 HF 或直连更快。
而 HF 更适合多线程分片。

### 3. 批量删除保护会打断合并流程

下载器合并分片后要删临时文件，环境的批量删除保护在累计 50 次后拦截
（`SAFE_DELETE_BULK_CONFIRM_REQUIRED`），导致合并中途抛异常、文件不完整
（表现为主文件 2.2G、旁边残留 8 个 225MB 的分片）。

修法：删除包在 `try/except` 里，失败不影响正确性，事后手工清理。
**判断文件是否完整的唯一可靠标准是字节数是否等于 `Content-Length`**。

### 4. HF 大文件会 302 到 `*.cdn.hf.co`

`curl -L` 能跟随，但 `-C -`（断点续传）在与重定向 + Range 组合时会返回 exit 2。
要么不带 `-C -` 从头下，要么用自己实现 Range 的下载器。

### 5. curl 7.81 不认识 `--timeout`

Ubuntu 22.04 自带 curl 7.81.0，**没有 `--timeout` 这个选项**，只有 `--max-time`。
报错信息是隐晦的 `curl: try 'curl --help'`（exit 2），看起来像 URL 或参数写错，
实际是选项不存在。

另一个连带坑：大文件不能设 `--max-time`，否则传输会被定时截断，
反复重试都从头发起。大文件只用 `--connect-timeout`。

### 6. 判断下载完整的唯一标准是字节数

本机环境的批量删除保护（`SAFE_DELETE_BULK_CONFIRM_REQUIRED`）会在累计 50 次删除后
拦截操作。下载器合并分片后要删 12 个临时文件，正好撞上这个阈值，
结果是合并中途抛异常、留下半成品。

所以校验模型文件不要看 `ls -lh` 显示的大小（四舍五入会骗人），
要 `stat -c%s` 精确比对 `Content-Length`。


## 验证结果（2026-10-28 实测）

环境跑通，三种路线各出过图。**结论：6GB 显存比预期宽松得多，不构成瓶颈。**

### 性能实测

| 路线 | 尺寸 | 步数 | 耗时 | 显存峰值 |
|---|---|---|---|---|
| 纯文生图 | 832×832 | 28 | **21 s** | 3.7 GB |
| InstantID | 832×832 | 28 | 37~39 s | 4.0 GB |
| IP-Adapter FaceID | 832×832 | 28 | 29~30 s | 3.8 GB |

采样速度约 2.2 it/s（纯 SDXL），加身份控制后降到 1.1 it/s。
显存始终在 4 GB 以内，`--lowvram` 留出的余量充足，后续可以试 1024×1024。

### 三条路线的画质结论

**1. 纯文生图（无参考图）** —— 质量最好，Q 版风格稳定，线条干净。
但角色长相完全随机，与真人无关。只能作为「风格基线」。

**2. InstantID（weight 0.8）** —— ❌ 不适合 Q 版。

出图构图崩坏：头占满画面、身体被压扁、颜色完全脱离参考图。

原因是架构冲突：InstantID 靠 ControlNet 把**面部关键点空间结构**钉在原照片位置，
而 chibi 风格恰恰要把头放大 3~4 倍。两个约束直接打架，模型只能牺牲构图。
这条路更适合写实或半写实风格。

**3. IP-Adapter FaceID Plus v2（weight 0.8）** —— ✅ 走这条。

只注入 face embedding，不强加空间结构，因此和 Q 版的大头比例能共存：
全身完整、手脚齐全、比例正常、构图稳定。耗时也最短（29 s）。

### 已知待调项

- **发色不受控**：提示词没写发色时模型自由发挥（实测把棕发生成成蓝色）。
  需要在提示词里显式写 `brown hair, short hair` 等，或从照片自动提取发色。
- **背景不够纯**：`simple background, white background` 压不住装饰元素，
  出图带彩色光斑。抠图前需要额外处理，或改用 `--rembg` 类节点。
- **相似度尚未评估**：需要真人照片才能判断，公开测试图没有评估价值。

## 相关工具

| 文件 | 用途 |
|---|---|
| `~/AI/start-comfyui.sh` | 启动脚本（含 6GB 参数） |
| `~/AI/gen_test.py` | 纯文生图，`gen_test.py [宽] [高] [步数]` |
| `~/AI/gen_faceid.py` | FaceID 路线，`gen_faceid.py <图> [weight] [宽] [高] [seed] [前缀]` |
| `~/AI/gen_instantid.py` | InstantID 路线（备用） |
| `~/AI/pdl.py` | 多线程分片下载器 |
