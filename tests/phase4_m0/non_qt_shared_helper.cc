#include <zlib.h>
#include "rules_cc/cc/runfiles/runfiles.h"

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
int non_qt_zlib_runfiles_helper() {
    uLong crc = crc32(0L, Z_NULL, 0);
    (void)crc;
    return 42;
}
