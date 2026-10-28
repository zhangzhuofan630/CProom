cmake_minimum_required(VERSION 3.21)

project(desktoppet
    VERSION 0.1.0
    DESCRIPTION "Desktop pet for couples — Win11 + Ubuntu"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# ★ 许可红线（不可妥协）
#
# 本项目整体采用宽松/自由许可，因此只能链接 LGPLv3 版 Qt 模块。
# 以下模块在 Qt 6 里「只有 GPLv3 或商业许可」，一旦引入，整个项目被迫 GPLv3：
#     Qt Charts / Qt Data Visualization / Qt Purchasing / Qt Virtual Keyboard
#
# 需要图表就自己画（QPainter / QML Canvas），需要输入法就用系统输入法。
# 新增任何 Qt 模块前，先确认它在 LGPLv3 清单里。
# ---------------------------------------------------------------------------

# 阶段说明：S0.2 只验证 qtbase 链路，故只依赖 Core/Gui/Widgets。
# Phase 1 做窗口时会加：Qml Quick（S0.9 渲染选型确认后再加，避免早期依赖未定）。
# 加模块时记得确认 aqtinstall / CI 里真的装了那个模块，否则 CI 会在 configure 阶段挂。
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup(REQUIRES 6.5)

# ---- 核心静态库：与平台无关的逻辑放这里，便于单测 ----
add_library(petcore STATIC
    src/version.cpp
    src/version.h
)
target_include_directories(petcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(petcore PRIVATE PET_VERSION="${PROJECT_VERSION}")
target_link_libraries(petcore PUBLIC Qt6::Core)

# ---- 可执行文件 ----
qt_add_executable(desktop
    src/main.cpp
)
target_link_libraries(desktop PRIVATE petcore Qt6::Widgets)

# Windows 上不弹控制台窗口
set_target_properties(desktop PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE FALSE
)

# ---- 测试 ----
enable_testing()
add_subdirectory(tests)
