#include "tests/phase4_m2/gl_direct_caller.h"

#include <QtGui/qopengl.h>

#if defined(__linux__)
#include <EGL/egl.h>
#include <GL/glx.h>
#endif

namespace phase4_m2 {

bool verifyDirectGlAndGlxSymbolsLinked() {
  // Take addresses of standard OpenGL 1.x functions that were previously missing
  // from the 14-symbol stub table (e.g. glClear, glEnable, glDisable,
  // glDepthFunc, glBlendFunc, glReadPixels, glGetError).
  volatile auto p_glClear = &glClear;
  volatile auto p_glEnable = &glEnable;
  volatile auto p_glDisable = &glDisable;
  volatile auto p_glDepthFunc = &glDepthFunc;
  volatile auto p_glBlendFunc = &glBlendFunc;
  volatile auto p_glReadPixels = &glReadPixels;
  volatile auto p_glGetError = &glGetError;

  bool ok = (p_glClear != nullptr) && (p_glEnable != nullptr) &&
            (p_glDisable != nullptr) && (p_glDepthFunc != nullptr) &&
            (p_glBlendFunc != nullptr) && (p_glReadPixels != nullptr) &&
            (p_glGetError != nullptr);

#if defined(__linux__)
  // Also verify GLX and EGL entry points directly (mirrors rviz_rendering).
  volatile auto p_glXGetCurrentContext = &glXGetCurrentContext;
  volatile auto p_glXGetCurrentDisplay = &glXGetCurrentDisplay;
  volatile auto p_glXGetProcAddress = &glXGetProcAddress;
  volatile auto p_eglGetCurrentContext = &eglGetCurrentContext;
  volatile auto p_eglGetDisplay = &eglGetDisplay;

  ok = ok && (p_glXGetCurrentContext != nullptr) &&
       (p_glXGetCurrentDisplay != nullptr) &&
       (p_glXGetProcAddress != nullptr) &&
       (p_eglGetCurrentContext != nullptr) && (p_eglGetDisplay != nullptr);

  // Calling glXGetCurrentContext() / eglGetCurrentContext() with no context current
  // must safely return nullptr through the real libGLX/libGL/libEGL at runtime.
  if (glXGetCurrentContext() != nullptr) {
    return false;
  }
  if (eglGetCurrentContext() != EGL_NO_CONTEXT) {
    return false;
  }
#endif

  return ok;
}

}  // namespace phase4_m2
