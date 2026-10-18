/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <GL/osmesa.h>
#include <android/native_window.h>
#include <cstring>

#include "GHOST_ContextOSMesa.hh"
#include "GHOST_SystemAndroid.hh"

void *GHOST_ContextOSMesa::s_shared = nullptr;

OSMesaContext GHOST_ContextOSMesa::ctxh() const
{
  return static_cast<OSMesaContext>(m_ctx);
}
int GHOST_ContextOSMesa::s_sharedCount = 0;

GHOST_ContextOSMesa::GHOST_ContextOSMesa(bool stereoVisual, bool presenting)
    : GHOST_Context(stereoVisual),
      m_ctx(nullptr),
      m_presenting(presenting),
      m_w(4),
      m_h(4),
      m_swap(1)
{
}

GHOST_ContextOSMesa::~GHOST_ContextOSMesa()
{
  if (m_ctx) {
    if (OSMesaGetCurrentContext() == ctxh()) {
      OSMesaMakeCurrent(nullptr, nullptr, 0, 0, 0);
    }
    if (m_ctx == s_shared && s_sharedCount > 1) {
      s_sharedCount--;
      return;
    }
    OSMesaDestroyContext(ctxh());
    if (m_ctx == s_shared) {
      s_shared = nullptr;
      s_sharedCount = 0;
    }
  }
}

bool GHOST_ContextOSMesa::resizeBuffer()
{
  int w = 4;
  int h = 4;
  if (m_presenting) {
    ANativeWindow *nw = g_ghostAndroidApp ? g_ghostAndroidApp->window : nullptr;
    if (nw) {
      w = ANativeWindow_getWidth(nw);
      h = ANativeWindow_getHeight(nw);
    }
    else {
      w = m_w;
      h = m_h;
    }
  }
  if (w < 1 || h < 1) {
    return false;
  }
  if (w == m_w && h == m_h && !m_buf.empty()) {
    return true;
  }
  m_w = w;
  m_h = h;
  m_buf.assign(size_t(w) * size_t(h), 0);
  return true;
}

GHOST_TSuccess GHOST_ContextOSMesa::initializeDrawingContext()
{
  const int attribs[] = {OSMESA_FORMAT,
                         OSMESA_RGBA,
                         OSMESA_DEPTH_BITS,
                         24,
                         OSMESA_STENCIL_BITS,
                         8,
                         OSMESA_ACCUM_BITS,
                         0,
                         OSMESA_PROFILE,
                         OSMESA_CORE_PROFILE,
                         OSMESA_CONTEXT_MAJOR_VERSION,
                         4,
                         OSMESA_CONTEXT_MINOR_VERSION,
                         3,
                         0};
  OSMesaContext share = static_cast<OSMesaContext>(s_shared);
  OSMesaContext c = OSMesaCreateContextAttribs(attribs, share);
  if (!c) {
    return GHOST_kFailure;
  }
  m_ctx = c;
  if (!s_shared) {
    s_shared = m_ctx;
  }
  s_sharedCount++;
  resizeBuffer();
  if (!OSMesaMakeCurrent(c, m_buf.data(), GL_UNSIGNED_BYTE, m_w, m_h)) {
    return GHOST_kFailure;
  }
  OSMesaPixelStore(OSMESA_Y_UP, 0);
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextOSMesa::activateDrawingContext()
{
  if (!m_ctx) {
    return GHOST_kFailure;
  }
  resizeBuffer();
  if (!OSMesaMakeCurrent(ctxh(), m_buf.data(), GL_UNSIGNED_BYTE, m_w, m_h)) {
    return GHOST_kFailure;
  }
  OSMesaPixelStore(OSMESA_Y_UP, 0);
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextOSMesa::releaseDrawingContext()
{
  return OSMesaMakeCurrent(nullptr, nullptr, 0, 0, 0) ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextOSMesa::updateDrawingContext()
{
  return activateDrawingContext();
}

GHOST_TSuccess GHOST_ContextOSMesa::releaseNativeHandles()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextOSMesa::setSwapInterval(int interval)
{
  m_swap = interval;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextOSMesa::getSwapInterval(int &intervalOut)
{
  intervalOut = m_swap;
  return GHOST_kSuccess;
}

void GHOST_ContextOSMesa::present()
{
  ANativeWindow *nw = g_ghostAndroidApp ? g_ghostAndroidApp->window : nullptr;
  if (!nw) {
    return;
  }
  if (ANativeWindow_setBuffersGeometry(nw, m_w, m_h, WINDOW_FORMAT_RGBX_8888) != 0) {
    return;
  }
  ANativeWindow_Buffer out;
  if (ANativeWindow_lock(nw, &out, nullptr) != 0) {
    return;
  }
  int rows = out.height < m_h ? out.height : m_h;
  size_t bytes = size_t(out.width < m_w ? out.width : m_w) * 4;
  uint8_t *dst = static_cast<uint8_t *>(out.bits);
  const uint8_t *src = reinterpret_cast<const uint8_t *>(m_buf.data());
  for (int y = 0; y < rows; y++) {
    memcpy(dst + size_t(y) * size_t(out.stride) * 4, src + size_t(y) * size_t(m_w) * 4, bytes);
  }
  ANativeWindow_unlockAndPost(nw);
}

GHOST_TSuccess GHOST_ContextOSMesa::swapBuffers()
{
  if (!m_ctx) {
    return GHOST_kFailure;
  }
  glFinish();
  if (m_presenting) {
    present();
    resizeBuffer();
    if (OSMesaGetCurrentContext() == ctxh()) {
      OSMesaMakeCurrent(ctxh(), m_buf.data(), GL_UNSIGNED_BYTE, m_w, m_h);
      OSMesaPixelStore(OSMESA_Y_UP, 0);
    }
  }
  return GHOST_kSuccess;
}
