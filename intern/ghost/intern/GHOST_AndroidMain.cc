/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <android/asset_manager.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "GHOST_SystemAndroid.hh"

extern "C" int main(int argc, const char **argv);

static void mkdirs(const std::string &p)
{
  for (size_t i = 1; i < p.size(); i++) {
    if (p[i] == '/') {
      mkdir(p.substr(0, i).c_str(), 0755);
    }
  }
  mkdir(p.c_str(), 0755);
}

static bool copyAsset(AAssetManager *am, const std::string &rel, const std::string &dst)
{
  AAsset *a = AAssetManager_open(am, rel.c_str(), AASSET_MODE_STREAMING);
  if (!a) {
    return false;
  }
  size_t sl = dst.find_last_of('/');
  if (sl != std::string::npos) {
    mkdirs(dst.substr(0, sl));
  }
  FILE *f = fopen(dst.c_str(), "wb");
  if (!f) {
    AAsset_close(a);
    return false;
  }
  char buf[65536];
  int n;
  while ((n = AAsset_read(a, buf, sizeof(buf))) > 0) {
    fwrite(buf, 1, size_t(n), f);
  }
  fclose(f);
  AAsset_close(a);
  return true;
}

static void installData(struct android_app *app, const std::string &root)
{
  std::string stamp = root + "/.installed";
  if (access(stamp.c_str(), F_OK) == 0) {
    return;
  }
  AAsset *m = AAssetManager_open(app->activity->assetManager, "blender_files.txt", AASSET_MODE_BUFFER);
  if (!m) {
    return;
  }
  std::string list(static_cast<const char *>(AAsset_getBuffer(m)), size_t(AAsset_getLength(m)));
  AAsset_close(m);
  size_t pos = 0;
  while (pos < list.size()) {
    size_t e = list.find('\n', pos);
    if (e == std::string::npos) {
      e = list.size();
    }
    std::string rel = list.substr(pos, e - pos);
    pos = e + 1;
    if (!rel.empty()) {
      copyAsset(app->activity->assetManager, "blender/" + rel, root + "/" + rel);
    }
  }
  FILE *f = fopen(stamp.c_str(), "wb");
  if (f) {
    fclose(f);
  }
}

static void waitCmd(int32_t cmd)
{
  switch (cmd) {
    case APP_CMD_INIT_WINDOW:
      break;
  }
}

void android_main(struct android_app *app)
{
  g_ghostAndroidApp = app;
  app->onAppCmd = waitCmd;
  while (!app->window && !app->destroyRequested) {
    int ev;
    struct android_poll_source *src;
    if (ALooper_pollAll(-1, nullptr, &ev, reinterpret_cast<void **>(&src)) >= 0 && src) {
      src->process(app, src);
    }
  }
  if (app->destroyRequested) {
    return;
  }
  std::string base = app->activity->internalDataPath;
  std::string root = base + "/blender";
  mkdirs(root);
  installData(app, root);
  mkdirs(base + "/home");
  mkdirs(base + "/tmp");
  setenv("HOME", (base + "/home").c_str(), 1);
  setenv("TMPDIR", (base + "/tmp").c_str(), 1);
  setenv("BLENDER_SYSTEM_RESOURCES", root.c_str(), 1);
  setenv("BLENDER_USER_RESOURCES", (base + "/home").c_str(), 1);
  setenv("GALLIUM_DRIVER", "zink", 1);
  setenv("MESA_LOADER_DRIVER_OVERRIDE", "zink", 1);
  setenv("MESA_GL_VERSION_OVERRIDE", "4.3", 1);
  setenv("MESA_GLSL_VERSION_OVERRIDE", "430", 1);
  setenv("MESA_NO_ERROR", "0", 1);
  setenv("ZINK_DESCRIPTORS", "lazy", 1);
  const char *argv[] = {"blender", nullptr};
  main(1, argv);
  ANativeActivity_finish(app->activity);
  while (!app->destroyRequested) {
    int ev;
    struct android_poll_source *src;
    if (ALooper_pollAll(-1, nullptr, &ev, reinterpret_cast<void **>(&src)) >= 0 && src) {
      src->process(app, src);
    }
  }
}
