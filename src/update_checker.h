#pragma once

#include <filesystem>
#include <functional>
#include <memory>

namespace rex::ui {
class ImGuiDrawer;
class WindowedAppContext;
}

namespace update_checker {

class Checker {
 public:
  Checker();
  ~Checker();

  Checker(const Checker&) = delete;
  Checker& operator=(const Checker&) = delete;

  void Start(rex::ui::WindowedAppContext& context, rex::ui::ImGuiDrawer* drawer,
             const std::filesystem::path& user_data_path);
  void Attach(rex::ui::WindowedAppContext& context, rex::ui::ImGuiDrawer* drawer,
              const std::filesystem::path& user_data_path);
  void CheckNow();
  void StartBeforeSetup(rex::ui::WindowedAppContext& context,
                        rex::ui::ImGuiDrawer* drawer,
                        const std::filesystem::path& user_data_path,
                        std::function<void()> completion);
  void Stop();

 private:
  struct Data;
  std::unique_ptr<Data> data_;
};

void CheckNowFromSettings();

}
