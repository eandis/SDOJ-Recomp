// saidaioujou_recomp_tu1 - ReXGlue Recompiled Project

#pragma once

#include <rex/graphics/flags.h>
#include <rex/rex_app.h>

class SaidaioujouRecompTu1App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<SaidaioujouRecompTu1App>(new SaidaioujouRecompTu1App(ctx, "saidaioujou_recomp_tu1",
        PPCImageConfig));
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    REXCVAR_SET(gpu_allow_invalid_fetch_constants, true);
  }
};
