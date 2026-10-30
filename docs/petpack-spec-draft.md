# petpack 格式草案 v0.1

> 状态：**草案**，由 S0.5~S0.7 的真实切层数据倒推而来，不是空想设计。
> 数据来源：`~/AI/petpack/`（定稿形象 `4_生活照_s42`）
> 定稿前需要 Phase 1 渲染器实现来反证。

---

## 一、为什么是「分层 + 骨架」而不是帧序列

S0.6 实测：固定人脸 embedding、只换姿势 tag，生成的静态帧脸部一致（站立 / 挥手 / 欢呼三张脸可互换）。
但 **侧脸做不出来**（SDXL 生成的是「头转了、五官仍朝正面」的假侧脸），复杂姿势还会引入背景杂物。

如果走帧序列路线，每个动作都要重新生成并重跑一致性，成本高且不可控。走分层路线则：

- AI **只生成一次**正面外观层
- 动作由**骨骼变换**驱动（旋转、位移、缩放）
- 一致性由架构保证，不依赖模型运气

这是把「不确定性」赶出运行时。

---

## 二、目录结构

```
character.petpack/          # 其实是个目录，不是压缩包（便于用户手改）
├── manifest.json           # 元信息：id / 名称 / 版本 / 画布尺寸
├── rig.json                # 骨架：层定义、锚点、z-order、表情补丁
├── layers/
│   ├── legs.png            # 腿
│   ├── body.png            # 躯干
│   ├── head.png            # 头（含头发、眼镜）
│   ├── collar.png          # 衣领遮罩（关键，见第四节）
│   ├── arm_r.png           # 右臂（可选，球关节，见第六节）
│   ├── body_wave.png       # 无右臂的躯干（可选，与 arm_r 配套）
│   ├── arm_meta.json       # 手臂轴心与角度限位（可选）
│   ├── expr_blink.png      # 表情补丁（见第五节）
│   ├── expr_happy.png
│   ├── expr_sleepy.png
│   ├── expr_wink.png
│   └── expr_surprised.png
└── animations/
    ├── idle.json           # 待机（呼吸 + 微摆）
    ├── jump.json
    └── wave.json
```

**约定**：所有 `layers/*.png` 使用**同一画布尺寸**，不做裁剪、不记录 offset。
理由：裁剪能省 60% 体积（实测 4 层全画布 85KB，裁剪后约 34KB），但需要每层记录 `offset` 并在渲染时补偿，出错成本远高于这点体积。等真的成为问题时再优化。

---

## 三、manifest.json

```json
{
  "format": "petpack",
  "formatVersion": 1,
  "id": "zhangzhuofan-default",
  "displayName": "默认形象",
  "author": "zhangzhuofan630",
  "canvas": { "width": 768, "height": 1024 },
  "baseScale": 0.26,
  "createdAt": "2026-10-28",
  "generator": {
    "tool": "ComfyUI 0.43.0",
    "checkpoint": "animagine-xl-4.0",
    "ipAdapter": "ip-adapter-faceid-plusv2_sdxl",
    "weight": 0.75,
    "promptVersion": "V4"
  }
}
```

`baseScale` = 桌宠在屏幕上的默认缩放（768 × 0.26 ≈ 200px 宽），让不同来源的 petpack 视觉大小一致。

---

## 四、rig.json —— 核心

```json
{
  "canvas": { "width": 768, "height": 1024 },
  "layers": [
    {
      "id": "legs",
      "file": "layers/legs.png",
      "z": 10,
      "attach": "root",
      "bbox": [222, 753, 344, 242]
    },
    {
      "id": "body",
      "file": "layers/body.png",
      "z": 20,
      "attach": "root",
      "bbox": [217, 634, 346, 131]
    },
    {
      "id": "head",
      "file": "layers/head.png",
      "z": 30,
      "attach": "neck",
      "pivot": [384, 601],
      "bbox": [98, 85, 537, 522]
    },
    {
      "id": "collar",
      "file": "layers/collar.png",
      "z": 40,
      "attach": "root",
      "bbox": [277, 597, 203, 45]
    }
  ],
  "joints": {
    "root": [384, 1024],
    "hip":  [384, 757],
    "neck": [384, 601]
  },
  "constraints": {
    "head.rotation": [-12, 12],
    "head.offsetX": [-10, 10],
    "head.offsetY": [-6, 6]
  }
}
```

### 字段说明

| 字段 | 含义 |
|---|---|
| `z` | 绘制顺序，**大者在上**。实测必须的顺序：腿 10 → 身 20 → 头 30 → 衣领 40 |
| `attach` | 该层跟随哪个关节。`head` 跟 `neck` 旋转，`collar` 跟 `root`（固定不动） |
| `pivot` | 旋转轴心（画布坐标）。头部绕颈部转，不是绕画布中心 |
| `bbox` | `[x, y, w, h]`，仅供命中测试与调试，不参与渲染 |
| **`constraints`** | **实测得出的硬限制，见下** |

### 关于衣领遮罩（collar）

这是本次验证的**净收益**，之前设计时没预料到。

Q 版角色「头直接坐在肩膀上」，颈部只有 83px 宽（占身高 9%）。头一旦位移或旋转，下巴与毛衣领口之间会露出背景：

| 动作 | 无遮罩 | 有遮罩 |
|---|---|---|
| 头下移 14px（1.5% 身高） | 无可见缝隙 | 无差异 |
| 头转 ±8° | 几乎无差异 | 无差异 |
| **头右移 40px** | **左侧露出脖子缺口** | 被领口补上 |
| **头上抬 25px** | **颈部露出背景** | 被领口补上 |
| 头转 ±22° | 头缘翘起，与肩之间有空隙 | 略改善，仍不自然 |

结论：**collar 必须独立成层，且 z 序在 head 之上、不跟随头部运动。**

### 关于 constraints（不是建议，是实测边界）

| 维度 | 安全范围 | 超出后的现象 |
|---|---|---|
| 头部旋转 | **±12°** | ±22° 时头缘与肩之间出现空隙，像「断头」 |
| 水平位移 | ±10px | 40px 时虽被衣领救回，但整体像头在滑 |
| 垂直位移 | ±6px | 上抬 25px 出现颈部悬空 |

这些值会随头身比变化，所以**每套 petpack 自带 constraints，而不是全局常量**。

---

## 五、表情层（表情补丁）

表情**不是**重画整张脸，而是在 head 之上叠一张「补丁」——只覆盖眼睛和嘴，其余透明。

```
layers/
├── head.png
├── expr_blink.png       # 平静闭眼
├── expr_happy.png       # 笑眼 + 张嘴
├── expr_sleepy.png      # 眼皮半垂
├── expr_wink.png        # 单眼闭（只盖一只眼）
└── expr_surprised.png   # 只换嘴成 O
```

### 为什么不用 AI 重绘

试过两个方案，都失败：

| 方案 | 结果 | 原因 |
|---|---|---|
| **SDXL 局部 inpaint** | 眼睛变成灰褐色实心圆盘 | SDXL 的 latent 是 1/8 分辨率。眼半径 58px 在 latent 里只有 7px，**模型没有空间画细节** |
| cv2.inpaint（TELEA）填底色 | 放射状拉丝伪影 | TELEA 对大区域会从边缘向内「推」颜色 |

**最终方案：程序化绘制。** 因为这个风格是扁平色块 + 单色描边，Q 版闭眼本来就是一条弧线，程序化能完美匹配，还零依赖、可参数化。

### 关键实现细节（踩过的坑）

1. **底色决定成败**。不能填纯肤色——会和脸颊的渐变形成一圈色差。
   正确做法：**逐列垂直渐变插值**，每列取眼睛上方和下方最近的皮肤色做线性过渡，保留光影。
   插值后加 **7×1 横向模糊**，否则逐列独立计算会产生竖直条纹。

2. **遮罩必须按表情分**。`wink` 只盖一只眼，`surprised` 只盖嘴。
   一开始所有表情共用「双眼+嘴」遮罩，结果 wink 的另一只眼被擦成了空白。

3. **PIL `arc()` 的角度方向**（实测标定，别再猜）：
   - `203°–337°` → **∩**（弧顶朝上）= 闭眼 / 笑眼
   - `20°–160°` → **∪**（弧顶朝下）= 微笑的嘴
   - 搞反了 wink 就会变成哭脸

4. **实测坐标**（画布 768×1024）：
   - 左眼 `(331, 481)` r=54　右眼 `(520, 451)` r=59
   - **两眼圆心不在同一水平线**（差 30px），因为原图头部微倾——所以参数必须按实际测量给，不能镜像复用
   - 嘴 `(430, 567)`，原图里就是一个点
   - 肤色 `RGB(222,243,255)`　描边色 `RGB(23,25,32)`　线宽 8px

### rig.json 里的表达

```json
"patches": [
  { "id": "expr_blink",     "file": "layers/expr_blink.png",     "attach": "head", "z": 31 },
  { "id": "expr_happy",     "file": "layers/expr_happy.png",     "attach": "head", "z": 31 },
  { "id": "expr_sleepy",    "file": "layers/expr_sleepy.png",    "attach": "head", "z": 31 },
  { "id": "expr_wink",      "file": "layers/expr_wink.png",      "attach": "head", "z": 31 },
  { "id": "expr_surprised", "file": "layers/expr_surprised.png", "attach": "head", "z": 31 }
],
"patchPolicy": { "holdMs": 150, "blinkRangeMs": [2600, 6600] }
```

`attach: "head"` 表示补丁**跟随头部一起旋转**（否则转头时表情会留在原地）。

### 表情状态机（demo 已验证）

| 触发 | 表情 | 时长 |
|---|---|---|
| 定时（2.6~6.6 秒随机） | `blink`，25% 概率换成 `wink` | 150ms |
| 点击 | `happy` | 1.1s |
| 静置 20 秒 | `sleepy` | 持续，直到鼠标移动 |
| — | `surprised` | 预留（暂无触发源） |

---

## 六、手臂层（可选，球关节）

> 状态：已在渲染器实现并通过实测（`docs/s11-arm-layer.md`）。
> **三个文件都是可选的**：老 petpack 缺这几个文件照样能跑，只是做不了挥手。
> 加层不能让旧角色包失效。

### 三个文件

| 文件 | 内容 |
|---|---|
| `layers/arm_r.png` | 手臂（袖子 + 手 + **以肩为圆心的圆形补丁**） |
| `layers/body_wave.png` | 右臂已被挖掉、原位补成衣摆的躯干 |
| `layers/arm_meta.json` | 轴心、关节半径、角度限位 |

`body_wave` 不能由 `body` 挖洞得到——挖出来是透明洞，会露出桌面。必须来自
**在原图上 inpaint 掉手臂**的那一版，让 AI 把袖子位置补成完整衣摆。

### 球关节：为什么臂层要带一块圆

手臂绕肩旋转时，臂根与躯干之间必然裂开一条缝。解是让臂层多带一块
**以旋转轴为圆心的圆形像素**——转的时候圆跟着转，缝始终被盖住。

```
arm_mask = polygon(沿描边读出的轮廓) ∪ circle(pivot, r=joint_radius)
         → dilate → 减去躯干保护区（防止把衣摆吃进来）
```

两个必须做的修正：

1. **多边形要 dilate**。直线连的多边形切不掉手掌/袖口的圆弧，会削掉一圈。
   背景 alpha=0，多包进来画不出来，无副作用。
2. **dilate 后必须减掉躯干保护区**。否则臂根会带进一块衣摆，边缘被切成直边，
   旋转时那块白肉会露在外面。

### arm_meta.json

```json
{
  "arm_r": {
    "pivot": [486, 656],        // 肩关节，原图像素坐标
    "joint_radius": 46,         // 圆形补丁半径，须 ≥ 轴心到臂根最远点
    "angle_min": -24,           // 负 = 逆时针 = 抬手（屏幕坐标）
    "angle_max": 20,            // 正 = 顺时针 = 放手
    "note": "正角度 = 顺时针（屏幕坐标），正值让手臂往下放"
  },
  "z_order": ["legs", "body", "arm_r", "head", "collar"]
}
```

`angle_max` 只有 20°：再往下转手臂会开始和衣摆互相穿插，+48° 袖根直接扎进躯干。
**所以这个 rig 没有「手垂下」的姿态**，idle 必须用另一套 `body`。这一点直接决定了
挥手首尾只能用交叉淡化过渡（见 `docs/s11-arm-layer.md` 第五节）。

### 渲染位置：不是最上层

**z-order：`legs → body → arm → head → collar`**

手臂放最上层会露馅——臂根那块被裁成直边的地方会浮在头的上面。放在 `head`
之前就没有副作用：手掌在头的右侧，不与脸重叠。

### 包围盒

手臂的包围盒必须把**绕轴转到两个角度限位**的区域都并进来，否则一举手窗口就把
手裁掉。和 head 旋转的处理同理，只是多一个旋转轴：

```
for deg in (angle_min, angle_max):
    unionBox ∪= rotate(arm_alphaBox, pivot, deg)
```

---

## 七、animations/*.json

关键帧 + 缓动，不存逐帧位图。

```json
{
  "name": "idle",
  "loop": true,
  "duration": 3200,
  "tracks": {
    "body.scaleY": [
      { "t": 0,    "v": 1.000, "ease": "sine" },
      { "t": 1600, "v": 1.009, "ease": "sine" },
      { "t": 3200, "v": 1.000, "ease": "sine" }
    ],
    "head.rotation": [
      { "t": 0,    "v": 0.0, "ease": "sine" },
      { "t": 1600, "v": 1.5, "ease": "sine" },
      { "t": 3200, "v": 0.0, "ease": "sine" }
    ]
  }
}
```

```json
{
  "name": "jump",
  "loop": false,
  "duration": 700,
  "tracks": {
    "root.offsetY": [
      { "t": 0,   "v": 0,    "ease": "quadIn" },
      { "t": 120, "v": -8,   "ease": "quadOut" },
      { "t": 350, "v": -46,  "ease": "quadIn" },
      { "t": 700, "v": 0,    "ease": "quadOut" }
    ],
    "body.scaleX": [
      { "t": 0,   "v": 1.00 },
      { "t": 90,  "v": 1.05 },
      { "t": 350, "v": 0.96 },
      { "t": 700, "v": 1.00 }
    ]
  }
}
```

**轨道命名约定**：`<layerId>.scaleX` / `.scaleY` / `.rotation` / `.offsetX` / `.offsetY`，
以及 `root.offsetX/Y` 表示整体位移。

渲染器需保证：同一 layer 的多个轨道叠加后仍满足 `constraints`（后写的钳制）。

---

## 八、渲染顺序（伪代码）

```
draw(legs)
if waving:                              // 见第六节
    draw(body_wave)                     // 无右臂版
    if mix < 1: draw(body, alpha=1-mix) // 首尾淡化过渡
else:
    draw(body)
if waving: draw(arm_r, rotate(armAngle, pivot=armPivot), alpha=mix)

save()
translate(neck + headOffset); rotate(headAngle); translate(-neck)
draw(head)
if expr: draw(expr_patch)
restore()

draw(collar)                            // 固定在身体上，不跟头动
```

实测这个顺序下，4 层 300×400 画布在 Chromium 上稳定 60fps（demo 自带 FPS 计数）。
Qt 实现已复测：**空闲 0.29% CPU / 57MB 内存**（`docs/s09-render-report.md`、
`docs/s11-arm-layer.md`），QWidget + QPainter 足够，不需要 QML。

---

## 九、还没解决的问题

| # | 问题 | 影响 |
|---|---|---|
| 1 | ~~没有手臂层~~ | **已解决**（第六节）。但只有右臂，且没有「垂下」姿态，抬手/收手只能靠淡化 |
| 2 | 层是矩形切分，不是语义切分 | 头发和脸在同一层，做不了「头发飘动」 |
| 3 | 只有正面一个视角 | 转身需要新素材（AI 做不出来，得手绘或后期） |
| 4 | `constraints` 的数值只对这一个头身比成立 | 换形象要重新测 |
| 5 | 表情坐标是硬编码的 | 换形象要重新测眼睛/嘴坐标。长期方案是自动检测（霍夫圆 + 颜色聚类已跑通，可固化） |
| 6 | 嘴部表情少 | 只有「张嘴笑」和「O 型嘴」，没有「撇嘴」「说话」 |

下一版重点：左臂（素材里另一只手没露出来，得先补素材）、以及把动作从代码搬到
`animations/*.json` —— 现在跳跃/呼吸/挥手/落地形变全是硬编码。

---

## 十、下一步

1. **补手臂层**：需要重新生成一张手臂分开的素材，或从现有图手工分离
2. 在 Qt 里实现最小渲染器，实测 CPU/内存（S0.9 的硬指标：空闲 CPU < 1%，内存 < 150MB）
3. 拿 Qt 实测结果回来修这份草案

---

## 十一、实测数据附录

画布 768×1024，定稿形象 `4_生活照_s42`：

| 项 | 值 |
|---|---|
| 角色包围盒 | x[98,634] y[85,994]，身高 909px |
| 颈线（逐行宽度最小处） | y=601，占身高 56.8%，颈宽 83px（9%） |
| 肩线 | y=626 |
| 腰线 | y=757 |
| 四层体积 | 85KB（300×400 缩放后） |
| 五表情体积 | 85KB |
| demo 单文件 | 175KB（base64 内嵌全部素材） |
