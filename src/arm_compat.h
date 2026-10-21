#pragma once

// Fix for QTBUG-145239: on Apple Silicon macOS, Qt 6.10's qyieldcpu.h calls __yield()
// without including <arm_acle.h>, triggering -Werror,-Wimplicit-function-declaration.
#if defined(__APPLE__) && defined(__aarch64__)
#  if __has_include(<arm_acle.h>)
#    include <arm_acle.h>
#  else
static inline void __yield(void) {
    __builtin_arm_yield();
}
#  endif
#endif
