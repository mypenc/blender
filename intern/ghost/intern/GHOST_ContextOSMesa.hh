/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <vector>

#include "GHOST_Context.hh"

typedef struct osmesa_context *OSMesaContext;

class GHOST_ContextOSMesa : public GHOST_Context {
 public:
  GHOST_ContextOSMesa(bool stereoVisual, bool presenting);
  ~GHOST_ContextOSMesa() override;

  GHOST_TSuccess swapBuffers() override;
  GHOST_TSuccess activateDrawingContext() override;
  GHOST_TSuccess releaseDrawingContext() override;
  GHOST_TSuccess initializeDrawingContext() override;
  GHOST_TSuccess updateDrawingContext() override;
  GHOST_TSuccess releaseNativeHandles() override;
  GHOST_TSuccess setSwapInterval(int interval) override;
  GHOST_TSuccess getSwapInterval(int &intervalOut) override;

 private:
  OSMesaContext ctxh() const;
  bool resizeBuffer();
  void present();

  void *m_ctx;
  bool m_presenting;
  int m_w;
  int m_h;
  int m_swap;
  std::vector<uint32_t> m_buf;

  static void *s_shared;
  static int s_sharedCount;
};
