# 纯 C++ 测试，不依赖 Qt 平台插件，CI 里无需 xvfb 也能跑。
# 后面写引擎逻辑时，把新测试文件加进 SOURCES 即可。

set(SOURCES
    test_version.cpp
)

foreach(src IN LISTS SOURCES)
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE petcore)
    add_test(NAME ${name} COMMAND ${name})
endforeach()
