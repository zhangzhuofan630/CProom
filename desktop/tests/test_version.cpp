#include <cstdio>
#include <cstring>

#include "version.h"

// 存在的意义不是测版本号，而是让 ctest 有一个真实的测试用例。
// 没有测试时 ctest 会返回非零退出码，CI 的 Test 步骤会假红。

int main()
{
    const char *v = pet::versionString();

    if (v == nullptr || std::strlen(v) == 0) {
        std::fprintf(stderr, "FAIL: versionString() 为空\n");
        return 1;
    }

    if (std::strcmp(v, "0.0.0-unknown") == 0) {
        std::fprintf(stderr, "FAIL: 版本号未由 CMake 注入，检查 PET_VERSION 编译定义\n");
        return 1;
    }

    std::printf("ok: version = %s\n", v);
    return 0;
}
