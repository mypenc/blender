/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <android/looper.h>
#include <android/keycodes.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <stdexcept>

#include "GHOST_ContextEGL.hh"
#include "GHOST_EventButton.hh"
#include "GHOST_EventCursor.hh"
#include "GHOST_EventKey.hh"
#include "GHOST_EventTrackpad.hh"
#include "GHOST_EventWheel.hh"
#include "GHOST_SystemAndroid.hh"
#include "GHOST_WindowManager.hh"

struct android_app *g_ghostAndroidApp = nullptr;
static GHOST_SystemAndroid *g_sys = nullptr;

static const float kLongPressMs = 450.0f;
static const float kTouchSlop = 24.0f;

static int32_t onInput(struct android_app *, AInputEvent *ev)
{
  return g_sys ? g_sys->handleInput(ev) : 0;
}

static void onCmd(struct android_app *, int32_t cmd)
{
  if (g_sys) {
    g_sys->handleCmd(cmd);
  }
}

GHOST_SystemAndroid::GHOST_SystemAndroid()
    : GHOST_System(),
      m_curX(0),
      m_curY(0),
      m_btnState(0),
      m_meta(0),
      m_touchDown(false),
      m_touchMulti(false),
      m_longDone(false),
      m_touchStartX(0),
      m_touchStartY(0),
      m_touchStartMs(0),
      m_lastCx(0),
      m_lastCy(0),
      m_lastDist(0),
      m_winReady(false)
{
  if (!g_ghostAndroidApp) {
    throw std::runtime_error("Android app is not initialized");
  }
  g_sys = this;
  g_ghostAndroidApp->onInputEvent = onInput;
  g_ghostAndroidApp->onAppCmd = onCmd;
  m_winReady = g_ghostAndroidApp->window != nullptr;
}

GHOST_SystemAndroid::~GHOST_SystemAndroid()
{
  g_sys = nullptr;
}

GHOST_TSuccess GHOST_SystemAndroid::init()
{
  GHOST_TSuccess ok = GHOST_System::init();
  if (ok) {
    m_displayManager = new GHOST_DisplayManagerNULL();
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

GHOST_WindowAndroid *GHOST_SystemAndroid::mainWindow() const
{
  const std::vector<GHOST_IWindow *> &w = m_windowManager->getWindows();
  return w.empty() ? nullptr : static_cast<GHOST_WindowAndroid *>(w[0]);
}

GHOST_IWindow *GHOST_SystemAndroid::createWindow(const char *title,
                                                 int32_t,
                                                 int32_t,
                                                 uint32_t width,
                                                 uint32_t height,
                                                 GHOST_TWindowState state,
                                                 GHOST_GLSettings glSettings,
                                                 const bool exclusive,
                                                 const bool,
                                                 const GHOST_IWindow *)
{
  if (!m_windowManager->getWindows().empty()) {
    return nullptr;
  }
  uint32_t w = width;
  uint32_t h = height;
  getMainDisplayDimensions(w, h);
  GHOST_WindowAndroid *win = new GHOST_WindowAndroid(this,
                                                     title,
                                                     w,
                                                     h,
                                                     state,
                                                     glSettings.context_type,
                                                     ((glSettings.flags & GHOST_glStereoVisual) != 0),
                                                     exclusive);
  if (win->getValid()) {
    m_windowManager->addWindow(win);
    pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, win));
    return win;
  }
  delete win;
  return nullptr;
}

GHOST_IContext *GHOST_SystemAndroid::createOffscreenContext(GHOST_GLSettings)
{
  GHOST_Context *ctx = new GHOST_ContextEGL(this,
                                            false,
                                            EGLNativeWindowType(nullptr),
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

GHOST_TSuccess GHOST_SystemAndroid::disposeContext(GHOST_IContext *context)
{
  delete context;
  return GHOST_kSuccess;
}

void GHOST_SystemAndroid::getAllDisplayDimensions(uint32_t &width, uint32_t &height) const
{
  getMainDisplayDimensions(width, height);
}

void GHOST_SystemAndroid::getMainDisplayDimensions(uint32_t &width, uint32_t &height) const
{
  ANativeWindow *w = g_ghostAndroidApp->window;
  width = w ? ANativeWindow_getWidth(w) : 0;
  height = w ? ANativeWindow_getHeight(w) : 0;
}

uint8_t GHOST_SystemAndroid::getNumDisplays() const
{
  return 1;
}

uint64_t GHOST_SystemAndroid::getMilliSeconds() const
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return uint64_t(ts.tv_sec) * 1000 + uint64_t(ts.tv_nsec / 1000000);
}

GHOST_TCapabilityFlag GHOST_SystemAndroid::getCapabilities() const
{
  return GHOST_TCapabilityFlag(GHOST_CAPABILITY_FLAG_ALL &
                               ~(GHOST_kCapabilityWindowPosition | GHOST_kCapabilityCursorWarp |
                                 GHOST_kCapabilityPrimaryClipboard |
                                 GHOST_kCapabilityGPUReadFrontBuffer |
                                 GHOST_kCapabilityClipboardImages));
}

char *GHOST_SystemAndroid::getClipboard(bool) const
{
  return nullptr;
}

void GHOST_SystemAndroid::putClipboard(const char *, bool) const {}

GHOST_TSuccess GHOST_SystemAndroid::getCursorPosition(int32_t &x, int32_t &y) const
{
  x = m_curX;
  y = m_curY;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemAndroid::setCursorPosition(int32_t x, int32_t y)
{
  const_cast<GHOST_SystemAndroid *>(this)->m_curX = x;
  const_cast<GHOST_SystemAndroid *>(this)->m_curY = y;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemAndroid::getModifierKeys(GHOST_ModifierKeys &keys) const
{
  keys.set(GHOST_kModifierKeyLeftShift, (m_meta & AMETA_SHIFT_LEFT_ON) != 0);
  keys.set(GHOST_kModifierKeyRightShift, (m_meta & AMETA_SHIFT_RIGHT_ON) != 0);
  keys.set(GHOST_kModifierKeyLeftControl, (m_meta & AMETA_CTRL_LEFT_ON) != 0);
  keys.set(GHOST_kModifierKeyRightControl, (m_meta & AMETA_CTRL_RIGHT_ON) != 0);
  keys.set(GHOST_kModifierKeyLeftAlt, (m_meta & AMETA_ALT_LEFT_ON) != 0);
  keys.set(GHOST_kModifierKeyRightAlt, (m_meta & AMETA_ALT_RIGHT_ON) != 0);
  keys.set(GHOST_kModifierKeyLeftOS, (m_meta & AMETA_META_LEFT_ON) != 0);
  keys.set(GHOST_kModifierKeyRightOS, (m_meta & AMETA_META_RIGHT_ON) != 0);
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemAndroid::getButtons(GHOST_Buttons &buttons) const
{
  buttons.clear();
  buttons.set(GHOST_kButtonMaskLeft, (m_btnState & AMOTION_EVENT_BUTTON_PRIMARY) != 0);
  buttons.set(GHOST_kButtonMaskRight, (m_btnState & AMOTION_EVENT_BUTTON_SECONDARY) != 0);
  buttons.set(GHOST_kButtonMaskMiddle, (m_btnState & AMOTION_EVENT_BUTTON_TERTIARY) != 0);
  return GHOST_kSuccess;
}

void GHOST_SystemAndroid::addDirtyWindow(GHOST_WindowAndroid *win)
{
  m_dirty.push_back(win);
}

bool GHOST_SystemAndroid::flushDirty()
{
  bool any = false;
  for (GHOST_WindowAndroid *w : m_dirty) {
    w->validate();
    pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowUpdate, w));
    any = true;
  }
  m_dirty.clear();
  return any;
}

void GHOST_SystemAndroid::handleCmd(int32_t cmd)
{
  GHOST_WindowAndroid *w = mainWindow();
  switch (cmd) {
    case APP_CMD_INIT_WINDOW:
      m_winReady = true;
      break;
    case APP_CMD_TERM_WINDOW:
      m_winReady = false;
      break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
    case APP_CMD_CONTENT_RECT_CHANGED:
      if (w) {
        pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, w));
      }
      break;
    case APP_CMD_GAINED_FOCUS:
      if (w) {
        pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowActivate, w));
      }
      break;
    case APP_CMD_LOST_FOCUS:
      if (w) {
        pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowDeactivate, w));
      }
      break;
    case APP_CMD_DESTROY:
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventQuitRequest, w));
      break;
  }
}

void GHOST_SystemAndroid::pushCursor(int32_t x, int32_t y)
{
  m_curX = x;
  m_curY = y;
  GHOST_WindowAndroid *w = mainWindow();
  if (w) {
    pushEvent(new GHOST_EventCursor(
        getMilliSeconds(), GHOST_kEventCursorMove, w, x, y, GHOST_TABLET_DATA_NONE));
  }
}

void GHOST_SystemAndroid::pushButton(GHOST_TButton btn, bool down)
{
  GHOST_WindowAndroid *w = mainWindow();
  if (w) {
    pushEvent(new GHOST_EventButton(getMilliSeconds(),
                                    down ? GHOST_kEventButtonDown : GHOST_kEventButtonUp,
                                    w,
                                    btn,
                                    GHOST_TABLET_DATA_NONE));
  }
}

void GHOST_SystemAndroid::pushKey(GHOST_TKey key, bool down, bool rep, const char *utf8)
{
  GHOST_WindowAndroid *w = mainWindow();
  if (!w) {
    return;
  }
  char buf[sizeof(GHOST_TEventKeyData::utf8_buf)] = {'\0'};
  if (down && utf8) {
    buf[0] = utf8[0];
  }
  pushEvent(new GHOST_EventKey(
      getMilliSeconds(), down ? GHOST_kEventKeyDown : GHOST_kEventKeyUp, w, key, rep, buf));
}

static GHOST_TKey convertKey(int32_t kc)
{
  if (kc >= AKEYCODE_A && kc <= AKEYCODE_Z) {
    return GHOST_TKey(int(GHOST_kKeyA) + kc - AKEYCODE_A);
  }
  if (kc >= AKEYCODE_0 && kc <= AKEYCODE_9) {
    return GHOST_TKey(int(GHOST_kKey0) + kc - AKEYCODE_0);
  }
  if (kc >= AKEYCODE_F1 && kc <= AKEYCODE_F12) {
    return GHOST_TKey(int(GHOST_kKeyF1) + kc - AKEYCODE_F1);
  }
  if (kc >= AKEYCODE_NUMPAD_0 && kc <= AKEYCODE_NUMPAD_9) {
    return GHOST_TKey(int(GHOST_kKeyNumpad0) + kc - AKEYCODE_NUMPAD_0);
  }
  switch (kc) {
    case AKEYCODE_DEL: return GHOST_kKeyBackSpace;
    case AKEYCODE_TAB: return GHOST_kKeyTab;
    case AKEYCODE_ENTER: return GHOST_kKeyEnter;
    case AKEYCODE_ESCAPE: return GHOST_kKeyEsc;
    case AKEYCODE_BACK: return GHOST_kKeyEsc;
    case AKEYCODE_SPACE: return GHOST_kKeySpace;
    case AKEYCODE_SEMICOLON: return GHOST_kKeySemicolon;
    case AKEYCODE_PERIOD: return GHOST_kKeyPeriod;
    case AKEYCODE_COMMA: return GHOST_kKeyComma;
    case AKEYCODE_APOSTROPHE: return GHOST_kKeyQuote;
    case AKEYCODE_GRAVE: return GHOST_kKeyAccentGrave;
    case AKEYCODE_MINUS: return GHOST_kKeyMinus;
    case AKEYCODE_EQUALS: return GHOST_kKeyEqual;
    case AKEYCODE_SLASH: return GHOST_kKeySlash;
    case AKEYCODE_BACKSLASH: return GHOST_kKeyBackslash;
    case AKEYCODE_LEFT_BRACKET: return GHOST_kKeyLeftBracket;
    case AKEYCODE_RIGHT_BRACKET: return GHOST_kKeyRightBracket;
    case AKEYCODE_SHIFT_LEFT: return GHOST_kKeyLeftShift;
    case AKEYCODE_SHIFT_RIGHT: return GHOST_kKeyRightShift;
    case AKEYCODE_CTRL_LEFT: return GHOST_kKeyLeftControl;
    case AKEYCODE_CTRL_RIGHT: return GHOST_kKeyRightControl;
    case AKEYCODE_ALT_LEFT: return GHOST_kKeyLeftAlt;
    case AKEYCODE_ALT_RIGHT: return GHOST_kKeyRightAlt;
    case AKEYCODE_META_LEFT: return GHOST_kKeyLeftOS;
    case AKEYCODE_META_RIGHT: return GHOST_kKeyRightOS;
    case AKEYCODE_INSERT: return GHOST_kKeyInsert;
    case AKEYCODE_FORWARD_DEL: return GHOST_kKeyDelete;
    case AKEYCODE_MOVE_HOME: return GHOST_kKeyHome;
    case AKEYCODE_MOVE_END: return GHOST_kKeyEnd;
    case AKEYCODE_PAGE_UP: return GHOST_kKeyUpPage;
    case AKEYCODE_PAGE_DOWN: return GHOST_kKeyDownPage;
    case AKEYCODE_DPAD_LEFT: return GHOST_kKeyLeftArrow;
    case AKEYCODE_DPAD_RIGHT: return GHOST_kKeyRightArrow;
    case AKEYCODE_DPAD_UP: return GHOST_kKeyUpArrow;
    case AKEYCODE_DPAD_DOWN: return GHOST_kKeyDownArrow;
    case AKEYCODE_CAPS_LOCK: return GHOST_kKeyCapsLock;
    case AKEYCODE_SCROLL_LOCK: return GHOST_kKeyScrollLock;
    case AKEYCODE_NUM_LOCK: return GHOST_kKeyNumLock;
    case AKEYCODE_SYSRQ: return GHOST_kKeyPrintScreen;
    case AKEYCODE_BREAK: return GHOST_kKeyPause;
    case AKEYCODE_NUMPAD_DOT: return GHOST_kKeyNumpadPeriod;
    case AKEYCODE_NUMPAD_ENTER: return GHOST_kKeyNumpadEnter;
    case AKEYCODE_NUMPAD_ADD: return GHOST_kKeyNumpadPlus;
    case AKEYCODE_NUMPAD_SUBTRACT: return GHOST_kKeyNumpadMinus;
    case AKEYCODE_NUMPAD_MULTIPLY: return GHOST_kKeyNumpadAsterisk;
    case AKEYCODE_NUMPAD_DIVIDE: return GHOST_kKeyNumpadSlash;
    case AKEYCODE_MENU: return GHOST_kKeyApp;
  }
  return GHOST_kKeyUnknown;
}

static char keyChar(int32_t kc, int32_t meta)
{
  bool sh = (meta & AMETA_SHIFT_ON) != 0;
  if (kc >= AKEYCODE_A && kc <= AKEYCODE_Z) {
    return char((sh ? 'A' : 'a') + kc - AKEYCODE_A);
  }
  if (kc >= AKEYCODE_0 && kc <= AKEYCODE_9) {
    static const char shd[] = ")!@#$%^&*(";
    return sh ? shd[kc - AKEYCODE_0] : char('0' + kc - AKEYCODE_0);
  }
  switch (kc) {
    case AKEYCODE_SPACE: return ' ';
    case AKEYCODE_COMMA: return sh ? '<' : ',';
    case AKEYCODE_PERIOD: return sh ? '>' : '.';
    case AKEYCODE_SLASH: return sh ? '?' : '/';
    case AKEYCODE_SEMICOLON: return sh ? ':' : ';';
    case AKEYCODE_APOSTROPHE: return sh ? '"' : '\'';
    case AKEYCODE_MINUS: return sh ? '_' : '-';
    case AKEYCODE_EQUALS: return sh ? '+' : '=';
    case AKEYCODE_LEFT_BRACKET: return sh ? '{' : '[';
    case AKEYCODE_RIGHT_BRACKET: return sh ? '}' : ']';
    case AKEYCODE_BACKSLASH: return sh ? '|' : '\\';
    case AKEYCODE_GRAVE: return sh ? '~' : '`';
    case AKEYCODE_NUMPAD_DOT: return '.';
    case AKEYCODE_NUMPAD_ADD: return '+';
    case AKEYCODE_NUMPAD_SUBTRACT: return '-';
    case AKEYCODE_NUMPAD_MULTIPLY: return '*';
    case AKEYCODE_NUMPAD_DIVIDE: return '/';
  }
  if (kc >= AKEYCODE_NUMPAD_0 && kc <= AKEYCODE_NUMPAD_9) {
    return char('0' + kc - AKEYCODE_NUMPAD_0);
  }
  return 0;
}

bool GHOST_SystemAndroid::handleKey(AInputEvent *ev)
{
  int32_t kc = AKeyEvent_getKeyCode(ev);
  int32_t act = AKeyEvent_getAction(ev);
  m_meta = AKeyEvent_getMetaState(ev);
  GHOST_TKey gk = convertKey(kc);
  if (gk == GHOST_kKeyUnknown) {
    return false;
  }
  if (act != AKEY_EVENT_ACTION_DOWN && act != AKEY_EVENT_ACTION_UP) {
    return false;
  }
  bool down = act == AKEY_EVENT_ACTION_DOWN;
  char u[2] = {keyChar(kc, m_meta), 0};
  if ((m_meta & (AMETA_CTRL_ON | AMETA_ALT_ON | AMETA_META_ON)) != 0) {
    u[0] = 0;
  }
  pushKey(gk, down, down && AKeyEvent_getRepeatCount(ev) > 0, u);
  return true;
}

bool GHOST_SystemAndroid::handleMouse(AInputEvent *ev)
{
  int32_t act = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
  int32_t x = int32_t(AMotionEvent_getX(ev, 0));
  int32_t y = int32_t(AMotionEvent_getY(ev, 0));
  m_meta = AMotionEvent_getMetaState(ev);
  int32_t bs = AMotionEvent_getButtonState(ev);
  if (act == AMOTION_EVENT_ACTION_SCROLL) {
    float v = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_VSCROLL, 0);
    GHOST_WindowAndroid *w = mainWindow();
    if (w && v != 0.0f) {
      pushEvent(new GHOST_EventWheel(getMilliSeconds(), w, v > 0 ? 1 : -1));
    }
    return true;
  }
  if (x != m_curX || y != m_curY) {
    pushCursor(x, y);
  }
  int32_t chg = bs ^ m_btnState;
  if (chg) {
    static const struct {
      int32_t a;
      GHOST_TButton g;
    } map[] = {{AMOTION_EVENT_BUTTON_PRIMARY, GHOST_kButtonMaskLeft},
               {AMOTION_EVENT_BUTTON_SECONDARY, GHOST_kButtonMaskRight},
               {AMOTION_EVENT_BUTTON_TERTIARY, GHOST_kButtonMaskMiddle},
               {AMOTION_EVENT_BUTTON_BACK, GHOST_kButtonMaskButton4},
               {AMOTION_EVENT_BUTTON_FORWARD, GHOST_kButtonMaskButton5}};
    for (const auto &m : map) {
      if (chg & m.a) {
        pushButton(m.g, (bs & m.a) != 0);
      }
    }
    m_btnState = bs;
  }
  return true;
}

static void centroid(AInputEvent *ev, float &cx, float &cy, float &dist, int skip = -1)
{
  size_t n = AMotionEvent_getPointerCount(ev);
  size_t ids[16];
  size_t k = 0;
  for (size_t i = 0; i < n && k < 16; i++) {
    if (int(i) != skip) {
      ids[k++] = i;
    }
  }
  cx = cy = dist = 0;
  if (k == 0) {
    return;
  }
  for (size_t i = 0; i < k; i++) {
    cx += AMotionEvent_getX(ev, ids[i]);
    cy += AMotionEvent_getY(ev, ids[i]);
  }
  cx /= float(k);
  cy /= float(k);
  if (k >= 2) {
    float dx = AMotionEvent_getX(ev, ids[0]) - AMotionEvent_getX(ev, ids[1]);
    float dy = AMotionEvent_getY(ev, ids[0]) - AMotionEvent_getY(ev, ids[1]);
    dist = std::sqrt(dx * dx + dy * dy);
  }
}

void GHOST_SystemAndroid::touchEnd()
{
  if (m_touchDown && !m_touchMulti && !m_longDone) {
    pushButton(GHOST_kButtonMaskLeft, false);
  }
  m_touchDown = false;
  m_touchMulti = false;
  m_longDone = false;
}

void GHOST_SystemAndroid::checkLongPress()
{
  if (!m_touchDown || m_touchMulti || m_longDone) {
    return;
  }
  if (float(getMilliSeconds() - m_touchStartMs) < kLongPressMs) {
    return;
  }
  if (std::abs(m_curX - m_touchStartX) > kTouchSlop ||
      std::abs(m_curY - m_touchStartY) > kTouchSlop)
  {
    return;
  }
  pushButton(GHOST_kButtonMaskLeft, false);
  pushButton(GHOST_kButtonMaskRight, true);
  pushButton(GHOST_kButtonMaskRight, false);
  m_longDone = true;
}

void GHOST_SystemAndroid::handleTouch(AInputEvent *ev)
{
  int32_t raw = AMotionEvent_getAction(ev);
  int32_t act = raw & AMOTION_EVENT_ACTION_MASK;
  size_t n = AMotionEvent_getPointerCount(ev);
  int32_t x = int32_t(AMotionEvent_getX(ev, 0));
  int32_t y = int32_t(AMotionEvent_getY(ev, 0));
  GHOST_WindowAndroid *w = mainWindow();
  if (!w) {
    return;
  }
  switch (act) {
    case AMOTION_EVENT_ACTION_DOWN:
      pushCursor(x, y);
      m_touchDown = true;
      m_touchMulti = false;
      m_longDone = false;
      m_touchStartX = x;
      m_touchStartY = y;
      m_touchStartMs = getMilliSeconds();
      pushButton(GHOST_kButtonMaskLeft, true);
      break;
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
      if (!m_touchMulti) {
        if (!m_longDone) {
          pushButton(GHOST_kButtonMaskLeft, false);
        }
        m_touchMulti = true;
      }
      centroid(ev, m_lastCx, m_lastCy, m_lastDist);
      break;
    case AMOTION_EVENT_ACTION_POINTER_UP:
      centroid(ev,
               m_lastCx,
               m_lastCy,
               m_lastDist,
               int((raw & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                   AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT));
      break;
    case AMOTION_EVENT_ACTION_MOVE:
      if (n >= 2 && m_touchMulti) {
        float cx, cy, d;
        centroid(ev, cx, cy, d);
        int32_t dx = int32_t(cx - m_lastCx);
        int32_t dy = int32_t(cy - m_lastCy);
        if (m_lastDist > 1.0f && d > 1.0f) {
          float r = d / m_lastDist;
          int32_t mg = int32_t((r - 1.0f) * 250.0f);
          if (mg != 0) {
            pushEvent(new GHOST_EventTrackpad(getMilliSeconds(),
                                              w,
                                              GHOST_kTrackpadEventMagnify,
                                              m_curX,
                                              m_curY,
                                              mg,
                                              mg,
                                              false));
            m_lastDist = d;
          }
        }
        if (dx != 0 || dy != 0) {
          pushCursor(int32_t(cx), int32_t(cy));
          pushEvent(new GHOST_EventTrackpad(getMilliSeconds(),
                                            w,
                                            GHOST_kTrackpadEventScroll,
                                            int32_t(cx),
                                            int32_t(cy),
                                            dx,
                                            dy,
                                            false));
          m_lastCx = cx;
          m_lastCy = cy;
        }
      }
      else if (m_touchDown && !m_touchMulti && !m_longDone) {
        if (x != m_curX || y != m_curY) {
          pushCursor(x, y);
        }
      }
      break;
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_CANCEL:
      touchEnd();
      break;
  }
}

int32_t GHOST_SystemAndroid::handleInput(AInputEvent *ev)
{
  int32_t type = AInputEvent_getType(ev);
  if (type == AINPUT_EVENT_TYPE_KEY) {
    return handleKey(ev) ? 1 : 0;
  }
  if (type != AINPUT_EVENT_TYPE_MOTION) {
    return 0;
  }
  int32_t src = AInputEvent_getSource(ev);
  if ((src & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE) {
    return handleMouse(ev) ? 1 : 0;
  }
  if ((src & AINPUT_SOURCE_TOUCHSCREEN) == AINPUT_SOURCE_TOUCHSCREEN) {
    handleTouch(ev);
    return 1;
  }
  return 0;
}

bool GHOST_SystemAndroid::processEvents(bool waitForEvent)
{
  bool any = false;
  do {
    GHOST_TimerManager *tm = getTimerManager();
    int timeout = 0;
    if (waitForEvent && m_dirty.empty() && (m_eventManager->getNumEvents() == 0)) {
      uint64_t next = tm->nextFireTime();
      timeout = next == GHOST_kFireTimeNever ? 50 : int(std::max<int64_t>(0, int64_t(next - getMilliSeconds())));
      if (m_touchDown && !m_touchMulti && !m_longDone) {
        timeout = std::min(timeout, 50);
      }
    }
    int ev;
    struct android_poll_source *src;
    while (ALooper_pollAll(timeout, nullptr, &ev, reinterpret_cast<void **>(&src)) >= 0) {
      if (src) {
        src->process(g_ghostAndroidApp, src);
      }
      any = true;
      timeout = 0;
      if (g_ghostAndroidApp->destroyRequested) {
        pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventQuitRequest, mainWindow()));
        return true;
      }
    }
    checkLongPress();
    if (tm->fireTimers(getMilliSeconds())) {
      any = true;
    }
    if (flushDirty()) {
      any = true;
    }
    if (!(m_eventManager->getNumEvents() == 0)) {
      any = true;
    }
  } while (waitForEvent && !any);
  return any;
}
