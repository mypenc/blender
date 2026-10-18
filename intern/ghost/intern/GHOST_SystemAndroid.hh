/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <android/input.h>
#include <android_native_app_glue.h>

#include "../GHOST_Types.h"
#include "GHOST_DisplayManagerNULL.hh"
#include "GHOST_Event.hh"
#include "GHOST_System.hh"
#include "GHOST_TimerManager.hh"
#include "GHOST_WindowAndroid.hh"

extern struct android_app *g_ghostAndroidApp;

class GHOST_WindowAndroid;

class GHOST_SystemAndroid : public GHOST_System {
 public:
  GHOST_SystemAndroid();
  ~GHOST_SystemAndroid();

  bool processEvents(bool waitForEvent) override;

  bool setConsoleWindowState(GHOST_TConsoleWindowState) override
  {
    return false;
  }

  GHOST_TSuccess getModifierKeys(GHOST_ModifierKeys &keys) const override;
  GHOST_TSuccess getButtons(GHOST_Buttons &buttons) const override;
  GHOST_TCapabilityFlag getCapabilities() const override;
  char *getClipboard(bool selection) const override;
  void putClipboard(const char *buffer, bool selection) const override;
  uint64_t getMilliSeconds() const override;
  uint8_t getNumDisplays() const override;
  GHOST_TSuccess getCursorPosition(int32_t &x, int32_t &y) const override;
  GHOST_TSuccess setCursorPosition(int32_t x, int32_t y) override;
  void getAllDisplayDimensions(uint32_t &width, uint32_t &height) const override;
  void getMainDisplayDimensions(uint32_t &width, uint32_t &height) const override;
  GHOST_IContext *createOffscreenContext(GHOST_GLSettings glSettings) override;
  GHOST_TSuccess disposeContext(GHOST_IContext *context) override;

  void addDirtyWindow(GHOST_WindowAndroid *win);
  int32_t handleInput(AInputEvent *ev);
  void handleCmd(int32_t cmd);

 private:
  GHOST_TSuccess init() override;

  GHOST_IWindow *createWindow(const char *title,
                              int32_t left,
                              int32_t top,
                              uint32_t width,
                              uint32_t height,
                              GHOST_TWindowState state,
                              GHOST_GLSettings glSettings,
                              const bool exclusive = false,
                              const bool is_dialog = false,
                              const GHOST_IWindow *parentWindow = nullptr) override;

  GHOST_WindowAndroid *mainWindow() const;
  bool flushDirty();
  void pushCursor(int32_t x, int32_t y);
  void pushButton(GHOST_TButton btn, bool down);
  void pushKey(GHOST_TKey key, bool down, bool rep, const char *utf8);
  void handleTouch(AInputEvent *ev);
  bool handleMouse(AInputEvent *ev);
  bool handleKey(AInputEvent *ev);
  void checkLongPress();
  void touchEnd();

  std::vector<GHOST_WindowAndroid *> m_dirty;
  int32_t m_curX;
  int32_t m_curY;
  int32_t m_btnState;
  int32_t m_meta;
  bool m_touchDown;
  bool m_touchMulti;
  bool m_longDone;
  int32_t m_touchStartX;
  int32_t m_touchStartY;
  uint64_t m_touchStartMs;
  float m_lastCx;
  float m_lastCy;
  float m_lastDist;
  bool m_winReady;
};
