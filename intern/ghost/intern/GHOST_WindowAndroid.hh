/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <android/native_window.h>

#include "GHOST_SystemAndroid.hh"
#include "GHOST_Window.hh"

class GHOST_SystemAndroid;

class GHOST_WindowAndroid : public GHOST_Window {
 public:
  GHOST_WindowAndroid(GHOST_SystemAndroid *system,
                      const char *title,
                      uint32_t width,
                      uint32_t height,
                      GHOST_TWindowState state,
                      GHOST_TDrawingContextType type,
                      const bool stereoVisual,
                      const bool exclusive);
  ~GHOST_WindowAndroid();

  GHOST_TSuccess invalidate() override;
  void validate()
  {
    m_invalid = false;
  }
  bool getValid() const override;
  void getWindowBounds(GHOST_Rect &bounds) const override;
  void getClientBounds(GHOST_Rect &bounds) const override;
  GHOST_TSuccess hasCursorShape(GHOST_TStandardCursor) override
  {
    return GHOST_kSuccess;
  }
  uint16_t getDPIHint() override;
  void surfaceLost();
  void surfaceRestored();

 protected:
  GHOST_Context *newDrawingContext(GHOST_TDrawingContextType type) override;
  GHOST_TSuccess setWindowCursorGrab(GHOST_TGrabCursorMode) override
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor) override
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess setWindowCustomCursorShape(
      uint8_t *, uint8_t *, int, int, int, int, bool) override
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess setWindowCursorVisibility(bool) override
  {
    return GHOST_kSuccess;
  }
  void setTitle(const char *title) override;
  std::string getTitle() const override;
  GHOST_TSuccess setClientWidth(uint32_t) override
  {
    return GHOST_kFailure;
  }
  GHOST_TSuccess setClientHeight(uint32_t) override
  {
    return GHOST_kFailure;
  }
  GHOST_TSuccess setClientSize(uint32_t, uint32_t) override
  {
    return GHOST_kFailure;
  }
  void screenToClient(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override
  {
    outX = inX;
    outY = inY;
  }
  void clientToScreen(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override
  {
    outX = inX;
    outY = inY;
  }
  GHOST_TSuccess setState(GHOST_TWindowState) override
  {
    return GHOST_kSuccess;
  }
  GHOST_TWindowState getState() const override
  {
    return GHOST_kWindowStateFullScreen;
  }
  GHOST_TSuccess setOrder(GHOST_TWindowOrder) override
  {
    return GHOST_kSuccess;
  }
  GHOST_TSuccess beginFullScreen() const override
  {
    return GHOST_kFailure;
  }
  GHOST_TSuccess endFullScreen() const override
  {
    return GHOST_kFailure;
  }

 private:
  GHOST_SystemAndroid *m_system;
  std::string m_title;
  bool m_invalid;
};
