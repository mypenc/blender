/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <android/configuration.h>

#include "GHOST_ContextEGL.hh"
#include "GHOST_WindowAndroid.hh"

GHOST_WindowAndroid::GHOST_WindowAndroid(GHOST_SystemAndroid *system,
                                         const char *title,
                                         uint32_t width,
                                         uint32_t height,
                                         GHOST_TWindowState state,
                                         GHOST_TDrawingContextType type,
                                         const bool stereoVisual,
                                         const bool exclusive)
    : GHOST_Window(width, height, state, stereoVisual, exclusive),
      m_system(system),
      m_title(title),
      m_invalid(false)
{
  if (!g_ghostAndroidApp || !g_ghostAndroidApp->window) {
    return;
  }
  if (type != GHOST_kDrawingContextTypeNone) {
    if (!setDrawingContextType(type)) {
      m_drawingContextType = GHOST_kDrawingContextTypeNone;
    }
  }
}

GHOST_WindowAndroid::~GHOST_WindowAndroid() {}

GHOST_Context *GHOST_WindowAndroid::newDrawingContext(GHOST_TDrawingContextType type)
{
  if (type != GHOST_kDrawingContextTypeOpenGL) {
    return nullptr;
  }
  GHOST_Context *ctx = new GHOST_ContextEGL(m_system,
                                            m_wantStereoVisual,
                                            EGLNativeWindowType(g_ghostAndroidApp->window),
                                            EGLNativeDisplayType(EGL_DEFAULT_DISPLAY),
                                            0,
                                            3,
                                            2,
                                            GHOST_OPENGL_EGL_CONTEXT_FLAGS,
                                            GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY,
                                            EGL_OPENGL_ES_API);
  if (ctx->initializeDrawingContext()) {
    return ctx;
  }
  delete ctx;
  return nullptr;
}

GHOST_TSuccess GHOST_WindowAndroid::invalidate()
{
  if (!m_invalid) {
    m_system->addDirtyWindow(this);
    m_invalid = true;
  }
  return GHOST_kSuccess;
}

bool GHOST_WindowAndroid::getValid() const
{
  return g_ghostAndroidApp && g_ghostAndroidApp->window && GHOST_Window::getValid();
}

void GHOST_WindowAndroid::getClientBounds(GHOST_Rect &bounds) const
{
  ANativeWindow *w = g_ghostAndroidApp->window;
  bounds.m_l = 0;
  bounds.m_t = 0;
  bounds.m_r = w ? ANativeWindow_getWidth(w) : 0;
  bounds.m_b = w ? ANativeWindow_getHeight(w) : 0;
}

void GHOST_WindowAndroid::getWindowBounds(GHOST_Rect &bounds) const
{
  getClientBounds(bounds);
}

void GHOST_WindowAndroid::setTitle(const char *title)
{
  m_title = title;
}

std::string GHOST_WindowAndroid::getTitle() const
{
  return m_title;
}

uint16_t GHOST_WindowAndroid::getDPIHint()
{
  int32_t d = AConfiguration_getDensity(g_ghostAndroidApp->config);
  if (d <= 0 || d == ACONFIGURATION_DENSITY_ANY || d == ACONFIGURATION_DENSITY_NONE) {
    return 96;
  }
  return uint16_t(d);
}

void GHOST_WindowAndroid::surfaceLost()
{
  GHOST_ContextEGL *c = dynamic_cast<GHOST_ContextEGL *>(m_context);
  if (c) {
    c->releaseSurface();
  }
}

void GHOST_WindowAndroid::surfaceRestored()
{
  GHOST_ContextEGL *c = dynamic_cast<GHOST_ContextEGL *>(m_context);
  if (c && g_ghostAndroidApp->window) {
    c->restoreSurface(EGLNativeWindowType(g_ghostAndroidApp->window));
  }
}
