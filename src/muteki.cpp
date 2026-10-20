#include <rex/cvar.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/windowed_app_context.h>

#include <imgui.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

REXCVAR_DECLARE(bool, muteki_flash);

namespace muteki_flash {
namespace {

using Clock = std::chrono::steady_clock;
using HitsFunction = uint32_t (*)();

constexpr auto kPollInterval = std::chrono::milliseconds(5);
constexpr auto kFlashTime = std::chrono::milliseconds(300);
constexpr float kFlashAlpha = 0.35f;

std::atomic<bool> running{false};
std::atomic<bool> flash_open{false};
std::atomic<int64_t> last_hit{0};
std::thread worker;

int64_t Now() {
  return Clock::now().time_since_epoch().count();
}

class Flash final : public rex::ui::ImGuiDialog {
 public:
  explicit Flash(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {}

 protected:
  void OnDraw(ImGuiIO& io) override {
    const auto elapsed = Clock::duration(Now() - last_hit.load(std::memory_order_relaxed));
    if (elapsed >= kFlashTime) {
      flash_open.store(false, std::memory_order_release);
      Close();
      return;
    }
    const float fade = 1.0f - std::chrono::duration<float>(elapsed) /
                                  std::chrono::duration<float>(kFlashTime);
    const ImU32 color = ImGui::GetColorU32(ImVec4(1.0f, 0.0f, 0.0f, kFlashAlpha * fade));
    ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), io.DisplaySize, color);
  }
};

HitsFunction FindHits() {
#if defined(_WIN32)
  const HMODULE module = GetModuleHandleW(L"saidaioujou_recomp_tu1_CA022100.dll");
  return module ? reinterpret_cast<HitsFunction>(GetProcAddress(module, "muteki_hits"))
                : nullptr;
#else
  void* module = dlopen("libsaidaioujou_recomp_tu1_CA022100.so", RTLD_LAZY | RTLD_NOLOAD);
  if (!module) return nullptr;
  const auto hits = reinterpret_cast<HitsFunction>(dlsym(module, "muteki_hits"));
  dlclose(module);
  return hits;
#endif
}

void Run(rex::ui::WindowedAppContext* context, rex::ui::ImGuiDrawer* drawer) {
  uint32_t seen = 0;
  bool known = false;
  while (running.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(kPollInterval);
    const HitsFunction read_hits = FindHits();
    if (!read_hits) {
      known = false;
      continue;
    }
    const uint32_t hits = read_hits();
    if (!known) {
      known = true;
      seen = hits;
      continue;
    }
    if (hits == seen) continue;
    seen = hits;
    if (!REXCVAR_GET(muteki_flash)) continue;

    last_hit.store(Now(), std::memory_order_relaxed);
    if (!flash_open.exchange(true, std::memory_order_acq_rel)) {
      context->CallInUIThreadDeferred([drawer] { new Flash(drawer); });
    }
  }
}

}  // namespace

void Start(rex::ui::WindowedAppContext& context, rex::ui::ImGuiDrawer* drawer) {
  if (!drawer || running.exchange(true)) return;
  worker = std::thread(Run, &context, drawer);
}

void Stop() {
  if (!running.exchange(false)) return;
  if (worker.joinable()) worker.join();
}

}  // namespace muteki_flash
