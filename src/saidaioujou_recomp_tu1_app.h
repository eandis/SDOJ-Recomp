// saidaioujou_recomp_tu1 - ReXGlue Recompiled Project

#pragma once

#include <rex/graphics/flags.h>
#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/system.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>
#include <rex/input/input_system.h>
#include <toml++/toml.hpp>

#include <functional>
#include <optional>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "exitgame.h"
#include "game_setup.h"
#include "popups.h"
#include "settings.h"
#include "update_checker.h"

// red flash when hit when muteki is on
namespace muteki_flash {
void Start(rex::ui::WindowedAppContext& context, rex::ui::ImGuiDrawer* drawer);
void Stop();
}

class SaidaioujouRecompTu1App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    auto app = std::unique_ptr<SaidaioujouRecompTu1App>(new SaidaioujouRecompTu1App(
        ctx, "saidaioujou_recomp_tu1", PPCImageConfig));
    rex::cvar::UnregisterFlag("debug");
    return app;
  }

  bool SetupEnvironment() override {
    release_root_ = rex::filesystem::GetExecutableFolder();
    if (!rex::ReXApp::SetupEnvironment()) {
      return false;
    }

    std::error_code user_data_error;
    std::filesystem::create_directories(user_data_root(), user_data_error);
    if (user_data_error) {
      pending_error_ = "Could not create user data folder: " + user_data_error.message();
    }

    rex::cvar::CaptureStartupValues();
    return true;
  }

  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& defaults,
      std::function<void(rex::PathConfig)> resume) override {
    update_checker_.Attach(app_context(), imgui_drawer(), defaults.user_data_root);
    if (!pending_error_.empty()) {
      FailSetup(pending_error_);
      return std::nullopt;
    }
    if (game_setup::IsPrepared(defaults.game_data_root)) {
      return defaults;
    }

    update_checker_.StartBeforeSetup(
        app_context(), imgui_drawer(), defaults.user_data_root,
        [this, defaults, resume = std::move(resume)]() mutable {
          setup_.Start(
              app_context(), imgui_drawer(), release_root_, defaults.game_data_root,
              [defaults, resume = std::move(resume)]() mutable {
                resume(defaults);
              },
              [this](const std::string& error) { FailSetup(error); });
        });
    return std::nullopt;
  }

  bool OnStartupError(const std::string& message) override {
    return ShowStartupError(message);
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    rex::cvar::SetFlagByName("mnk_mode", "false");
    rex::cvar::UnregisterFlag("mnk_mode");
    pc_exit::set_app(*this);
    REXCVAR_SET(gpu_allow_invalid_fetch_constants, true);
    if (config.graphics && XFAILED(config.graphics->SetupPresentation(&app_context()))) {
      rex::ShowSimpleMessageBox(
          rex::SimpleMessageBoxType::Error,
          "The game could not start because your GPU or graphics driver isn't supported.\n\n"
          "ReXGlue needs D3D12 or Vulkan with full feature support");
    }
  }

  void OnPostSetup() override {
    if (!window()) {
      return;
    }

    window()->SetTitle("DoDonPachi SaiDaiOuJou");

#ifdef _WIN32
    auto hwnd = static_cast<HWND>(window()->GetNativeWindowHandle());
    auto instance = GetModuleHandleW(nullptr);
    if (hwnd && instance) {
      constexpr int kAppIconResourceId = 101;
      auto big_icon = static_cast<HICON>(LoadImageW(
          instance, MAKEINTRESOURCEW(kAppIconResourceId), IMAGE_ICON,
          GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
      auto small_icon = static_cast<HICON>(LoadImageW(
          instance, MAKEINTRESOURCEW(kAppIconResourceId), IMAGE_ICON,
          GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
      if (big_icon) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big_icon));
      }
      if (small_icon) {
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
      }
    }
#endif
  }

  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (REXCVAR_GET(game_data_root).empty()) {
      if (std::filesystem::exists(paths.config_path)) {
        try {
          const auto config = toml::parse_file(paths.config_path.string());
          if (const auto* saved_path = config.get_as<std::string>("game_data_root");
              saved_path && !saved_path->get().empty()) {
            paths.game_data_root = saved_path->get();
          }
        } catch (const toml::parse_error&) {
        }
      }
      if (paths.game_data_root.empty()) {
        paths.game_data_root = release_root_ / "game_data";
      }
    }
    if (REXCVAR_GET(user_data_root).empty()) {
      paths.user_data_root = release_root_ / "user_data";
      if (REXCVAR_GET(cache_path).empty()) {
        paths.cache_root = paths.user_data_root / "cache";
      }
    }
    settings_path_ = paths.config_path;
  }

  void OnPreLaunchModule() override {
    auto memory = runtime()->memory();
    if (memory) {
      pc_settings::Initialize(*memory);
      training_start::Initialize(*memory);
      training_difficulty::Initialize(*memory);
    }
    pc_settings::save_config = [context = &window()->app_context(), path = settings_path_] {
      context->CallInUIThread([path] { rex::cvar::SaveConfig(path); });
    };

    auto* input = static_cast<rex::input::InputSystem*>(runtime()->input_system());
    if (window() && input) {
      keyboard_input::Initialize(*window(), *input, pc_settings::save_config);
    }
    if (memory) {
      keyboard_menu::Initialize(*memory);
    }
  }

  void OnPostLaunchModule(rex::system::XThread*) override {
    update_checker_.Start(app_context(), imgui_drawer(), user_data_root());
    muteki_flash::Start(app_context(), imgui_drawer());
  }

  void OnShutdown() override {
    muteki_flash::Stop();
    update_checker_.Stop();
    setup_.Stop();
    error_dialog_.reset();
    keyboard_input::Shutdown();
  }

 private:
  bool ShowStartupError(const std::string& message) {
    REXLOG_ERROR("Startup failed: {}", message);
    if (!imgui_drawer()) {
      return false;
    }
    error_dialog_ = std::make_unique<popups::Message>(
        imgui_drawer(), "Setup failed", "The game could not start:\n\n" + message, "Exit",
        [this] {
          app_context().CallInUIThreadDeferred([this] { app_context().QuitFromUIThread(); });
        },
        true);
    return true;
  }

  void FailSetup(const std::string& message) {
    if (!ShowStartupError(message)) {
      game_setup::ReportError(message);
      app_context().QuitFromUIThread();
    }
  }

  std::filesystem::path release_root_;
  std::filesystem::path settings_path_;
  update_checker::Checker update_checker_;
  game_setup::Setup setup_;
  std::unique_ptr<popups::Message> error_dialog_;
  std::string pending_error_;
};
