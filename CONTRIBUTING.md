# 贡献指南

## 当前阶段

项目处于 **Sprint 0（可行性验证）**，`petpack` 格式尚未冻结。这个阶段改格式是常态。

因此：

- **欢迎**：Issue 讨论设计、报告平台兼容问题（尤其 Win11 / Wayland）、提供测试照片生成结果
- **暂时不适合**：大规模重构 PR、新增依赖、改 `petpack` 结构 —— 等格式冻结后再说

## 开发环境

| 依赖 | 版本 |
|---|---|
| CMake | ≥ 3.21 |
| Qt | ≥ 6.5（与 CI 对齐请用 **6.8.3**） |
| 编译器 | MSVC 2022 / GCC 11+ |

Ubuntu 上用 aqtinstall 装 Qt（apt 只有 6.2.4，太旧）：

```bash
python3 -m venv ~/.venvs/aqt && ~/.venvs/aqt/bin/pip install -U aqtinstall
~/.venvs/aqt/bin/aqt install-qt linux desktop 6.8.3 -O ~/Qt
```

## 提交前

```bash
cmake -S desktop -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64
cmake --build build
ctest --test-dir build --output-on-failure
```

CI 会在 Linux + Windows 双平台编译。**本地能过不代表 Windows 能过**，涉及平台代码时请注意条件编译。

## 两条硬规矩

1. **不要提交任何真实照片**。`.gitignore` 已排除 `*.jpg/*.jpeg/*.webp`，但仍需自己确认。测试图放 `assets/tests/private/`（已被忽略）。
2. **不要引入 GPL-only 的 Qt 模块**：Qt Charts、Data Visualization、Purchasing、Virtual Keyboard。需要图表自己画。理由见 `desktop/CMakeLists.txt` 顶部。

## 代码风格

- C++17，4 空格缩进，指针/引用贴类型（`int *p`）
- 文件编码 UTF-8
- 平台相关代码集中在平台层，不要散落在业务逻辑里

## PR

请说明：改了什么、为什么这样改、怎么验证的（截图/命令输出）。涉及 UI 的附截图。
