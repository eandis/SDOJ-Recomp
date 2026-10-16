#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace rex::ui {
class ImGuiDrawer;
class WindowedAppContext;
}

namespace game_setup {

struct Progress {
  std::string message;
  float fraction = -1.0f;
};

using ProgressCallback = std::function<void(const Progress&)>;

bool IsPrepared(const std::filesystem::path& game_data_path) noexcept;
bool Prepare(const std::filesystem::path& release_root,
             const std::filesystem::path& game_data_path,
             std::string& error,
             const ProgressCallback& progress = {},
             const std::atomic<bool>* active = nullptr) noexcept;
void ReportError(const std::string& error);

class Setup {
 public:
  Setup();
  ~Setup();

  Setup(const Setup&) = delete;
  Setup& operator=(const Setup&) = delete;

  void Start(rex::ui::WindowedAppContext& context, rex::ui::ImGuiDrawer* drawer,
             const std::filesystem::path& release_root,
             const std::filesystem::path& game_data_path,
             std::function<void()> success,
             std::function<void(const std::string&)> failure = {});
  void Stop();

 private:
  struct Data;
  std::unique_ptr<Data> data_;
};

}
