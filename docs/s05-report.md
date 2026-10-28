# S0.5 验证报告：真人照片 → Q版桌宠形象

> 执行日期：2026-10-28
> 硬件：RTX 3060 Laptop 6GB / CUDA 13.0 / ComfyUI 0.43.0
> 素材：用户提供的 5 张真人照片（**仅存于本机 `~/AI/photos/`，不进 Git**）

---

## 一、结论先行

**判定：GO —— 但限定条件。**

「照片 → 可辨认的 Q 版角色」这条路走通了，而且**不需要训练 LoRA**。代价是必须用**提示词强约束**，光靠 IP-Adapter 会飘。

同时暴露出 petpack 架构必须承认的一条硬边界：

> **AI 只负责「正面 / 微侧」的静态形象。真正的转身、走路、跳跃必须靠骨骼变换，不能靠生成。**

这不是缺陷，反而是原方案的验证——`分层 + 骨架 + 关键帧` 的设计前提成立。

---

## 二、测试方法

三轮迭代，共 19 张图。

| 轮次 | 目的 | 变量 | 张数 |
|---|---|---|---|
| V1/V2 | 建立基线，确认提示词对属性的控制力 | 输入图（证件照/生活照）× weight（0.7/0.85）× 提示词（原版/属性锁定版） | 8 |
| V3 | 修构图：身体塌成球、没脖子、背景脏 | 换竖版 768×1024、去掉 `super deformed`、锁瞳色 | 6 |
| V4 | 修发型：真实发型是露额头的侧分 | `asymmetric bangs` / `swept bangs` / `forehead` | 6 |
| 一致性 | 换姿势脸还一样吗 | 固定 face embedding，只改姿势 tag | 5（含在 19 内） |

**成功产物路径**：`~/AI/ComfyUI/output/`
- 最佳定妆照：`s05v4_00004_.png`（输入 p1，weight 0.75，seed 42）
- 一致性五联：`cons_00001~00005_.png`

---

## 三、关键发现

### 1. 证件照不是最佳输入，生活照才是

反直觉但稳定复现：

| 输入 | 结果 |
|---|---|
| p4（白底证件照，最清晰） | 五官准确，但气质偏「呆」，脸偏瘦，缺少本人神态 |
| p1/p2（生活自拍，光线杂） | **更像本人**，神态自然，缺点是肤色偏红润 |

推测：InsightFace 提取 embedding 时，证件照的正面强光 + 无影照明反而丢失了立体信息；生活照的侧光带了更多面部结构线索。

→ **产品侧应默认引导用户上传生活照，而不是证件照。**

### 2. `super deformed` 是身体塌成球的主因

V2 用 `chibi, super deformed`，身体变成了「企鹅/雪人」——头直接坐在肩膀上，没有脖子。

去掉 `super deformed`、保留 `chibi`、加 `visible neck, collar` 后（V3），比例立刻正常（约 1:1.6 头身比，接近黏土人手办）。

→ **提示词模板里禁用 `super deformed`。**

### 3. 瞳色和发色会被模型「自由发挥」，必须双向锁

| 症状 | 原因 | 解法 |
|---|---|---|
| 黑发被画成蓝黑渐变 | 动漫模型偏好彩色发 | 正向 `black hair` + 负向 `gradient hair` |
| 深棕瞳被画成蓝绿瞳 | 同上，anime 模型爱画蓝眼 | 正向 `black eyes, dark eyes` + 负向 `blue eyes, colored eyes` |

**这两个负面词必须常驻**。中途有一次漏写 `blue eyes`，立刻又飘成蓝瞳（`cons_00001_.png`）。

### 4. 一致性成立——这是最值钱的结果

固定 `p1.jpg` + weight 0.75，只换姿势 tag：

| 姿势 | 脸部一致性 | 可用性 |
|---|---|---|
| 站立垂手 | 基准 | ✅ 直接可用 |
| 挥手张嘴笑 | 脸型/发型/眼镜一致 | ✅ 直接可用 |
| 双手举起欢呼 | 一致，表情生动 | ✅ 直接可用 |
| 盘腿坐 | 脸一致 | ❌ 背景冒出日文字符和冰块状装饰物 |
| 侧身 | **假侧脸**（头转了但五官仍朝正面） | ❌ SDXL chibi 做不出真 profile |

→ **静态帧一致性达标**，说明「AI 生成一次外观层 + 骨骼驱动动作」的架构可行。
→ 但 **`sitting` 等复杂姿势会引入背景杂物**，负面词需补 `text, letter, logo`。

### 5. 缩到桌宠尺寸后反而更好看

768×1024 缩到 200×300 后，细节噪点（斑块、描边不匀）被抹平，轮廓反而更干净；缩到 100×150 五官仍可辨认。

→ **不必为了桌宠尺寸追求超高分辨率**，768×1024 足够，还能省显存和时间。

---

## 四、显存与耗时（6GB 实测）

| 配置 | 耗时 | 显存峰值 |
|---|---|---|
| 768×1024 + FaceID Plus v2 (w0.75) | 18~26s | 3.8 GB |
| 832×832 + FaceID Plus v2 (w0.85) | 18~24s | 3.8 GB |
| 832×832 + InstantID（上一轮） | 37s | 4.0 GB（构图崩坏，已弃） |

**6GB 完全不紧张**，还有余量挂 LoRA。批量出 19 张总耗时约 7 分钟。

---

## 五、最终提示词模板（V4 定稿）

```
masterpiece, best quality, very aesthetic, absurdres,
1boy, solo, male focus, chibi, cute, full body, standing, simple pose,
black hair, short hair,
asymmetric bangs, swept bangs, forehead, hair over one eye,
black eyes, dark eyes,
round eyewear, black-framed glasses,
white sweater, long sleeves, visible neck, collar,
white background, simple background, flat background
```

负面：
```
lowres, bad anatomy, bad hands, text, error, extra digits,
worst quality, low quality, normal quality, jpeg artifacts,
signature, watermark, username, blurry, multiple views, extra limbs,
female, girl, realistic, photorealistic, 3d, render,
colorful, gradient background, glowing, sparkles, lens flare,
gradient hair, blue eyes, colored eyes, scenery, shadow,
heavy bangs, hair covering eyes, bangs over forehead,
holding object, plush toy, animal, wings, floating hair
```

**参数**：`weight 0.75`、`weight_faceidv2 1.0`、`embeds_scaling V only`、28 steps、cfg 6.0、euler_ancestral / normal、768×1024。

**weight 调参结论**：0.70 偏「通用动漫男」，0.85 开始过度卡通化（眼睛过大、脸更圆）。**0.75 是甜点**。

---

## 六、仍未解决的问题

| # | 问题 | 影响 | 建议 |
|---|---|---|---|
| 1 | **眼镜框被画粗**：真人是细金属丝框，生成为黑粗框 | 识别度下降 | 试 `thin frame glasses` / `rimless`，或后期手绘修 |
| 2 | 脸比真人圆胖 | 「不像」的主要来源 | Q 版必然取舍；可试 `mature face` 或降低 chibi 强度 |
| 3 | 侧脸做不出来 | 转身动作无法生成 | **由骨骼变换承担**，这正是 petpack 的设计意图 |
| 4 | 复杂姿势引入背景杂物 | 影响抠图 | 负面补 `text, letter, logo, standing object` |
| 5 | 肤色偏红润（生活照输入） | 轻微 | 后期调色，或提示词加 `pale skin` |
| 6 | 未试 Chibi LoRA | 可能有更好的一致性 | 值得花 30 分钟试一个 SDXL chibi LoRA |

---

## 七、顺带验证的两个下游门槛

### 1. 抠图：**通过，而且不需要 AI 抠图**

原以为要用 `rembg` / `isnet-anime`，实测发现**纯 OpenCV 连通域 flood fill 就够了**：

1. 从四角取背景色中位数（BGR 255,255,255）
2. 按色差阈值生成候选背景掩码
3. **只删除与画布边缘连通的区域** —— 关键点：白毛衣、头发高光这些「角色内部的白色」被黑色描边隔断，不会误删
4. 边缘 0.8px 高斯羽化 + 5×5 模糊去白边

| 指标 | 结果 |
|---|---|
| 透明区域 | 54~62% |
| 不透明区域 | 37~45% |
| 半透明边缘 | 0.98~1.19% |
| 误删（白毛衣/高光被吃掉） | **0** |
| 白底残留 | 无 |

深色底上仍能看到一圈浅色轮廓，但那是**原图自带的白色外描边**（anime 贴纸风格），不是抠图缺陷。保留反而让角色在任意桌面上都「贴得牢」。

→ 抠图管线**零依赖、确定性、无显存占用**，比 AI 分割更适合产品化（用户机器上不用装 onnx 运行时）。

### 2. 切层 + 头部位移：**小幅度可行，大幅度需要遮罩层**

角色包围盒 `x[98,634] y[85,994]`，身高 909px。按比例切三刀：

| 层 | 范围 | 高度占比 |
|---|---|---|
| 头（含发/眼镜） | y 85~557 | 52% |
| 身 | y 557~757 | 22% |
| 腿 | y 757~994 | 26% |

相邻层保留 **12px 重叠**后做位移测试：

| 动作 | 结果 |
|---|---|
| 三层原样拼回 | ✅ 与原图无差别，无缝隙 |
| 头下移 14px（约身高 1.5%，模拟点头） | ✅ **完全看不出接缝** |
| 头下移 14px + 旋转 8° | ⚠️ 下巴与毛衣领口之间露出背景色 |

**结论（直接决定 petpack 格式）**：

1. 相邻层必须有 `overlap` 字段，12px（约身高 1.3%）够用
2. 头部旋转必须**小幅度**（≤5°），或
3. **衣领/脖子需要单独成层且 z-order 在头部之上** —— 低头时衣领盖住接缝

第 3 点是格式设计里之前没想到的，属于这次验证的净收益。

---

## 八、仍未解决的问题

| # | 问题 | 影响 | 建议 |
|---|---|---|---|
| 1 | **眼镜框被画粗**：真人是细金属丝框，生成为黑粗框 | 识别度下降 | 试 `thin frame glasses` / `rimless`，或后期手绘修 |
| 2 | 脸比真人圆胖 | 「不像」的主要来源 | Q 版必然取舍；可试 `mature face` 或降低 chibi 强度 |
| 3 | 侧脸做不出来 | 转身动作无法生成 | **由骨骼变换承担**，这正是 petpack 的设计意图 |
| 4 | 复杂姿势引入背景杂物 | 影响抠图 | 负面补 `text, letter, logo, standing object` |
| 5 | 肤色偏红润（生活照输入） | 轻微 | 后期调色，或提示词加 `pale skin` |
| 6 | 未试 Chibi LoRA | 可能有更好的一致性 | 值得花 30 分钟试一个 SDXL chibi LoRA |
| 7 | 头部旋转 >5° 露接缝 | 动作幅度受限 | 增加衣领遮罩层（见上） |

---

## 九、下一步（S0.6 收尾）

1. **定稿角色形象**：用户从候选里挑一张，或说明哪里不像，继续微调
2. **写 petpack 草案**：现在有真实数据了 —— 三层 + 12px overlap + z-order + 遮罩层
3. **最小渲染 demo**：把这三层读进来，用鼠标驱动头部轻微跟随，实测帧率和 CPU 占用

---

## 十、文件清单

| 文件 | 说明 |
|---|---|
| `~/AI/gen_matrix.py` | V1/V2 矩阵脚本 |
| `~/AI/gen_v3.py` | V3 构图修正 |
| `~/AI/gen_v4.py` | V4 发型修正（**定稿版本**） |
| `~/AI/gen_consistency.py` | 多姿势一致性验证 |
| `~/AI/cutout_test.py` | 闭运算 flood fill 抠图 |
| `~/AI/layer_test.py` | 几何切层 + 位移接缝测试 |
| `~/AI/photos/p1~p5.jpg` | 原始照片（**不进 Git**） |
| `~/AI/s05*_panel.png` | 各轮对比图 |
| `~/AI/final_check.png` | 原图/生成图/桌宠尺寸 四列对比 |
| `~/AI/cons_labeled.png` | 一致性五联 |
| `~/AI/cutout_verify.png` | 抠图在棋盘格/深色底上的验收 |
| `~/AI/layer_test.png` | 切层与头部位移接缝 |

⚠️ 所有含人脸的文件均留在 `~/AI/`，**不提交到 Public 仓库**。
