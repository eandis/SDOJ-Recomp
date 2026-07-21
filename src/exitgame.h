#pragma once

#include <rex/ui/window.h>

namespace sdoj_pc_exit {

inline rex::ui::Window* window = nullptr;

inline void set_window(rex::ui::Window& app_window) {
  window = &app_window;
}

inline void quit() {
  auto* app_window = window;
  app_window->app_context().CallInUIThread(
      [app_window] { app_window->RequestClose(); });
}

}
