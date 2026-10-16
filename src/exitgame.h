#pragma once

#include <rex/rex_app.h>

namespace pc_exit {

inline rex::ReXApp* app = nullptr;

inline void set_app(rex::ReXApp& recomp_app) {
  app = &recomp_app;
}

inline void quit() {
  if (app) {
    app->RequestExit();
  }
}

}
