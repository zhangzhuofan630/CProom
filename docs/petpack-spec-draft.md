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
├── rig.json                # 骨架：层定义、锚点、z-order
├── layers/
│   ├── legs.png            # 腿
│   ├── body.png            # 躯干
│   ├── head.png            # 头（含头发、眼镜）
│   └── collar.png          # 衣领遮罩（关键，见第四节）
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

## 五、animations/*.json

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

## 六、渲染顺序（伪代码）

```
for layer in layers sorted by z:
    if layer.attach == "neck":
        ctx.translate(neck)      // 或 pivot
        ctx.rotate(headRotation)
        ctx.translate(-pivot)
    ctx.drawImage(layer.file, 0, 0)
```

实测这个顺序下，4 层 300×400 画布在 Chromium 上稳定 60fps（demo 自带 FPS 计数）。
**CPU 占用待 Qt 实现后复测**——Web Canvas 与 QML Scene Graph 不是一回事，不能互证。

---

## 七、还没解决的问题

| # | 问题 | 影响 |
|---|---|---|
| 1 | **没有表情系统** | 只有一张中性脸，笑/闭眼做不出来。需要额外生成表情层，或在分层时单独切出眼睛/嘴 |
| 2 | **没有手臂层** | 挥手只能整体摆，手不能单独动。当前形象手插在兜里，勉强够用 |
| 3 | 层是矩形切分，不是语义切分 | 头发和脸在同一层，做不了「头发飘动」 |
| 4 | 只有正面一个视角 | 转身需要新素材（AI 做不出来，得手绘或后期） |
| 5 | `constraints` 的数值只对这一个头身比成立 | 换形象要重新测 |

第 1、2 项是**下一版必须先解决的**——桌宠没有表情就没有生命力。

---

## 八、下一步

1. 用生成流程补做**表情层**（眼睛/嘴单独切，或生成闭眼、笑版本再对齐）
2. 在 Qt 里实现最小渲染器，实测 CPU/内存（S0.9 的硬指标：空闲 CPU < 1%，内存 < 150MB）
3. 拿 Qt 实测结果回来修这份草案
