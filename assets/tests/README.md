# 验证过程归档

好坏都留，别只留成功的图 —— 失败的样本才是后面调参的依据。

## 目录约定

| 路径 | 内容 | 是否入库 |
|---|---|---|
| `assets/tests/private/` | 真实照片、含隐私的原图 | **否**（.gitignore 排除） |
| `assets/tests/incoming/` | ComfyUI 原始输出，体积大 | 否 |
| `assets/tests/*.md` | 评分表、结论记录 | 是 |
| `assets/tests/thumbs/` | 从输出里挑出来的代表性缩略图 | 是（请先确认无隐私） |

## 待产出

- `eval-sheet.md` —— S0.5 人脸辨识度评分表
- `consistency-eval.md` —— S0.6 多姿势一致性结论
