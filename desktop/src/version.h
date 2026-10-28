#pragma once

namespace pet {

/// 版本号由 CMake 的 project(VERSION ...) 注入，避免两处手写不同步。
const char *versionString() noexcept;

}  // namespace pet
