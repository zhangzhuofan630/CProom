#include "version.h"

#ifndef PET_VERSION
#  define PET_VERSION "0.0.0-unknown"
#endif

namespace pet {

const char *versionString() noexcept
{
    return PET_VERSION;
}

}  // namespace pet
