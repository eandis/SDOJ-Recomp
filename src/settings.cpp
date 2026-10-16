#include "settings.h"
#include "saidaioujou_recomp_tu1_init.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>

#include <rex/cvar.h>
#include <rex/input/input_system.h>
#include <rex/platform.h>
#include <rex/runtime.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/virtual_key.h>
#include <rex/ui/window.h>
#include <rex/ui/window_listener.h>

#if REX_PLATFORM_WIN32
#include <windows.h>
#include <imm.h>

#include <rex/ui/window_win.h>
#endif

namespace {

using keyboard_input::Action;
using rex::ui::VirtualKey;

constexpr size_t kKeyCount = 256;
constexpr uint32_t kInvalidKey = 0;

struct BindingDescription {
  const char* label;
  std::array<const char*, keyboard_input::player_count> cvar_names;
  std::array<const char*, keyboard_input::player_count> defaults;
  uint32_t default_key;
};

constexpr std::array<BindingDescription, keyboard_input::binding_count> kBindings = {{
    {"Up", {"keyboard_p1_up", "keyboard_p2_up"}, {"Up", ""},
     static_cast<uint32_t>(VirtualKey::kUp)},
    {"Down", {"keyboard_p1_down", "keyboard_p2_down"}, {"Down", ""},
     static_cast<uint32_t>(VirtualKey::kDown)},
    {"Left", {"keyboard_p1_left", "keyboard_p2_left"}, {"Left", ""},
     static_cast<uint32_t>(VirtualKey::kLeft)},
    {"Right", {"keyboard_p1_right", "keyboard_p2_right"}, {"Right", ""},
     static_cast<uint32_t>(VirtualKey::kRight)},
    {"Shot", {"keyboard_p1_shot", "keyboard_p2_shot"}, {"Z", ""},
     static_cast<uint32_t>(VirtualKey::kZ)},
    {"Bomb", {"keyboard_p1_bomb", "keyboard_p2_bomb"}, {"X", ""},
     static_cast<uint32_t>(VirtualKey::kX)},
    {"Rapid Shot", {"keyboard_p1_rapid_shot", "keyboard_p2_rapid_shot"}, {"C", ""},
     static_cast<uint32_t>(VirtualKey::kC)},
    {"Extra", {"keyboard_p1_extra", "keyboard_p2_extra"}, {"V", ""},
     static_cast<uint32_t>(VirtualKey::kV)},
    {"Start", {nullptr, nullptr}, {nullptr, nullptr}, kInvalidKey},
    {"Back", {"keyboard_p1_back", "keyboard_p2_back"}, {"Escape", ""},
     static_cast<uint32_t>(VirtualKey::kEscape)},
}};

REXCVAR_DEFINE_STRING(keyboard_p1_up, kBindings[static_cast<size_t>(Action::Up)].defaults[0],
                      "Input/Keyboard", "Player 1 up");
REXCVAR_DEFINE_STRING(keyboard_p1_down, kBindings[static_cast<size_t>(Action::Down)].defaults[0],
                      "Input/Keyboard", "Player 1 down");
REXCVAR_DEFINE_STRING(keyboard_p1_left, kBindings[static_cast<size_t>(Action::Left)].defaults[0],
                      "Input/Keyboard", "Player 1 left");
REXCVAR_DEFINE_STRING(keyboard_p1_right, kBindings[static_cast<size_t>(Action::Right)].defaults[0],
                      "Input/Keyboard", "Player 1 right");
REXCVAR_DEFINE_STRING(keyboard_p1_shot, kBindings[static_cast<size_t>(Action::Shot)].defaults[0],
                      "Input/Keyboard", "Player 1 shot");
REXCVAR_DEFINE_STRING(keyboard_p1_bomb, kBindings[static_cast<size_t>(Action::Bomb)].defaults[0],
                      "Input/Keyboard", "Player 1 bomb");
REXCVAR_DEFINE_STRING(keyboard_p1_rapid_shot,
                      kBindings[static_cast<size_t>(Action::RapidShot)].defaults[0],
                      "Input/Keyboard", "Player 1 rapid shot");
REXCVAR_DEFINE_STRING(keyboard_p1_extra, kBindings[static_cast<size_t>(Action::Extra)].defaults[0],
                      "Input/Keyboard", "Player 1 extra");
REXCVAR_DEFINE_STRING(keyboard_p1_back, kBindings[static_cast<size_t>(Action::Back)].defaults[0],
                      "Input/Keyboard", "Player 1 back");

REXCVAR_DEFINE_STRING(keyboard_p2_up, kBindings[static_cast<size_t>(Action::Up)].defaults[1],
                      "Input/Keyboard", "Player 2 up");
REXCVAR_DEFINE_STRING(keyboard_p2_down, kBindings[static_cast<size_t>(Action::Down)].defaults[1],
                      "Input/Keyboard", "Player 2 down");
REXCVAR_DEFINE_STRING(keyboard_p2_left, kBindings[static_cast<size_t>(Action::Left)].defaults[1],
                      "Input/Keyboard", "Player 2 left");
REXCVAR_DEFINE_STRING(keyboard_p2_right, kBindings[static_cast<size_t>(Action::Right)].defaults[1],
                      "Input/Keyboard", "Player 2 right");
REXCVAR_DEFINE_STRING(keyboard_p2_shot, kBindings[static_cast<size_t>(Action::Shot)].defaults[1],
                      "Input/Keyboard", "Player 2 shot");
REXCVAR_DEFINE_STRING(keyboard_p2_bomb, kBindings[static_cast<size_t>(Action::Bomb)].defaults[1],
                      "Input/Keyboard", "Player 2 bomb");
REXCVAR_DEFINE_STRING(keyboard_p2_rapid_shot,
                      kBindings[static_cast<size_t>(Action::RapidShot)].defaults[1],
                      "Input/Keyboard", "Player 2 rapid shot");
REXCVAR_DEFINE_STRING(keyboard_p2_extra, kBindings[static_cast<size_t>(Action::Extra)].defaults[1],
                      "Input/Keyboard", "Player 2 extra");
REXCVAR_DEFINE_STRING(keyboard_p2_back, kBindings[static_cast<size_t>(Action::Back)].defaults[1],
                      "Input/Keyboard", "Player 2 back");

constexpr bool IsValidAction(Action action) {
  return static_cast<uint32_t>(action) < keyboard_input::binding_count;
}

constexpr bool IsValidKey(uint32_t key) { return key < kKeyCount; }

constexpr bool IsDirectionalAction(Action action) {
  return action == Action::Up || action == Action::Down ||
         action == Action::Left || action == Action::Right;
}

constexpr Action OppositeDirection(Action action) {
  switch (action) {
    case Action::Up:
      return Action::Down;
    case Action::Down:
      return Action::Up;
    case Action::Left:
      return Action::Right;
    case Action::Right:
      return Action::Left;
    default:
      return Action::Count;
  }
}

struct State {
  std::array<std::atomic<uint32_t>, keyboard_input::player_count * keyboard_input::binding_count>
      keys{};
  std::array<std::atomic<bool>, kKeyCount> held{};
  std::array<std::atomic<bool>, kKeyCount> suppressed{};
  std::array<std::atomic<uint64_t>, kKeyCount> press_order{};
  std::atomic<uint64_t> next_press_order{0};

  std::atomic<bool> initialized{false};
  std::atomic<bool> active{false};
  std::atomic<bool> focused{false};
  std::atomic<bool> closing{true};
  std::atomic<bool> capture_active{false};
  std::atomic<uint32_t> capture_player{0};
  std::atomic<uint32_t> capture_action{0};

  rex::ui::Window* window = nullptr;
  rex::input::InputSystem* input = nullptr;
  std::function<void()> save;
  bool callbacks_registered = false;

  State() {
    for (auto& key : keys) {
      key.store(0, std::memory_order_relaxed);
    }
    for (auto& key : held) {
      key.store(false, std::memory_order_relaxed);
    }
    for (auto& key : suppressed) {
      key.store(false, std::memory_order_relaxed);
    }
    for (auto& order : press_order) {
      order.store(0, std::memory_order_relaxed);
    }
    for (uint32_t action = 0; action < keyboard_input::binding_count; ++action) {
      keys[action].store(kBindings[action].default_key, std::memory_order_relaxed);
    }
  }

#if REX_PLATFORM_WIN32
  std::atomic<HWND> hwnd{nullptr};
  HIMC ime_context = nullptr;
  bool ime_detached = false;
#endif
};

State g_state;

State& GetState() { return g_state; }

size_t BindingIndex(uint32_t player, Action action) {
  return static_cast<size_t>(player) * keyboard_input::binding_count +
         static_cast<size_t>(action);
}

uint32_t ParseKey(std::string_view name) {
  if (name.empty()) {
    return kInvalidKey;
  }
  const VirtualKey key = rex::ui::ParseVirtualKey(name);
  const uint32_t value = static_cast<uint32_t>(key);
  return IsValidKey(value) ? value : kInvalidKey;
}

std::string KeyString(uint32_t key) {
  if (!IsValidKey(key) || key == kInvalidKey) {
    return "Unbound";
  }
  switch (static_cast<VirtualKey>(key)) {
    case VirtualKey::kReturn:
      return "Return";
    case VirtualKey::kBack:
      return "Backspace";
    case VirtualKey::kOem3:
      return "Backtick";
    default:
      break;
  }
  const auto name = rex::ui::VirtualKeyToString(static_cast<VirtualKey>(key));
  return name.empty() ? "Unbound" : name;
}

bool IsReservedKey(uint32_t key, bool alt_pressed) {
  if (key == static_cast<uint32_t>(VirtualKey::kF3) ||
      key == static_cast<uint32_t>(VirtualKey::kF4) ||
      key == static_cast<uint32_t>(VirtualKey::kF11) ||
      key == static_cast<uint32_t>(VirtualKey::kOem3)) {
    return true;
  }
  return alt_pressed && key == static_cast<uint32_t>(VirtualKey::kReturn);
}

bool PhysicalKeyDown(uint32_t key) {
  auto& state = GetState();
  if (!IsValidKey(key)) {
    return false;
  }
#if REX_PLATFORM_WIN32
  const HWND hwnd = state.hwnd.load(std::memory_order_acquire);
  if (hwnd) {
    return (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
  }
#endif
  return state.held[key].load(std::memory_order_acquire);
}

void BlockCurrentlyHeld() {
  auto& state = GetState();
  for (uint32_t key = 1; key < kKeyCount; ++key) {
    if (PhysicalKeyDown(key)) {
      state.suppressed[key].store(true, std::memory_order_release);
    }
  }
}

void ClearHeld() {
  auto& state = GetState();
  for (auto& key : state.held) {
    key.store(false, std::memory_order_release);
  }
}

void ClearSuppressed() {
  auto& state = GetState();
  for (auto& key : state.suppressed) {
    key.store(false, std::memory_order_release);
  }
}

bool AnySuppressed() {
  auto& state = GetState();
  for (const auto& key : state.suppressed) {
    if (key.load(std::memory_order_acquire)) {
      return true;
    }
  }
  return false;
}

void SetBoundKey(uint32_t player, Action action, uint32_t key) {
  if (player >= keyboard_input::player_count || !IsValidAction(action)) {
    return;
  }
  GetState().keys[BindingIndex(player, action)].store(key, std::memory_order_release);
}

void LoadBindings() {
  for (uint32_t player = 0; player < keyboard_input::player_count; ++player) {
    for (uint32_t action = 0; action < keyboard_input::binding_count; ++action) {
      const char* cvar_name = kBindings[action].cvar_names[player];
      if (!cvar_name) {
        continue;
      }
      const auto name = rex::cvar::GetFlagByName(cvar_name);
      SetBoundKey(player, static_cast<Action>(action), ParseKey(name));
    }
  }
}

void OnBindingChanged(std::string_view name, std::string_view value) {
  auto& state = GetState();
  if (!state.initialized.load(std::memory_order_acquire)) {
    return;
  }
  for (uint32_t player = 0; player < keyboard_input::player_count; ++player) {
    for (uint32_t action = 0; action < keyboard_input::binding_count; ++action) {
      const char* cvar_name = kBindings[action].cvar_names[player];
      if (cvar_name && name == cvar_name) {
        SetBoundKey(player, static_cast<Action>(action), ParseKey(value));
        return;
      }
    }
  }
}

void RegisterBindingCallbacks() {
  auto& state = GetState();
  if (state.callbacks_registered) {
    return;
  }
  for (const auto& binding : kBindings) {
    for (const char* name : binding.cvar_names) {
      if (name) {
        rex::cvar::RegisterChangeCallback(name, OnBindingChanged);
      }
    }
  }
  state.callbacks_registered = true;
}

void SaveIfConfigured() {
  auto& state = GetState();
  if (!state.initialized.load(std::memory_order_acquire) ||
      state.closing.load(std::memory_order_acquire)) {
    return;
  }
  auto& save = state.save;
  if (save) {
    save();
  }
}

class Listener final : public rex::ui::WindowInputListener, public rex::ui::WindowListener {
 public:
  void OnOpened(rex::ui::UISetupEvent&) override {
    auto& state = GetState();
    state.closing.store(false, std::memory_order_release);
    state.focused.store(state.window && state.window->HasFocus(), std::memory_order_release);
    if (state.focused.load(std::memory_order_acquire)) {
#if REX_PLATFORM_WIN32
      AttachIme();
#endif
    }
    BlockCurrentlyHeld();
  }

  void OnClosing(rex::ui::UIEvent&) override {
    auto& state = GetState();
    state.closing.store(true, std::memory_order_release);
    state.focused.store(false, std::memory_order_release);
    state.capture_active.store(false, std::memory_order_release);
    ClearHeld();
    ClearSuppressed();
#if REX_PLATFORM_WIN32
    RestoreIme();
#endif
  }

  void OnGotFocus(rex::ui::UISetupEvent&) override {
    auto& state = GetState();
    state.focused.store(true, std::memory_order_release);
    state.closing.store(false, std::memory_order_release);
    state.capture_active.store(false, std::memory_order_release);
#if REX_PLATFORM_WIN32
    AttachIme();
#endif
    BlockCurrentlyHeld();
  }

  void OnLostFocus(rex::ui::UISetupEvent&) override {
    auto& state = GetState();
    state.focused.store(false, std::memory_order_release);
    state.capture_active.store(false, std::memory_order_release);
    ClearHeld();
    ClearSuppressed();
#if REX_PLATFORM_WIN32
    RestoreIme();
#endif
  }

  void OnKeyDown(rex::ui::KeyEvent& event) override {
    auto& state = GetState();
    const uint32_t key = static_cast<uint32_t>(event.virtual_key());
    if (!IsValidKey(key) || key == kInvalidKey) {
      return;
    }

    const bool was_held = state.held[key].load(std::memory_order_acquire);
    const bool repeat = event.prev_state() || was_held;
    if (!repeat) {
      const uint64_t order =
          state.next_press_order.fetch_add(1, std::memory_order_acq_rel) + 1;
      state.press_order[key].store(order, std::memory_order_release);
    }
    state.held[key].store(true, std::memory_order_release);
    const bool capture = state.capture_active.load(std::memory_order_acquire);

    if (capture) {
      const bool reserved = IsReservedKey(key, event.is_alt_pressed());
      event.set_handled(reserved ? false : true);
      if (state.suppressed[key].load(std::memory_order_acquire)) {
        event.set_handled(true);
        return;
      }
      state.suppressed[key].store(true, std::memory_order_release);
      if (reserved) {
        state.capture_active.store(false, std::memory_order_release);
        return;
      }
      if (repeat) {
        return;
      }
      if (key == static_cast<uint32_t>(VirtualKey::kEscape)) {
        state.capture_active.store(false, std::memory_order_release);
        return;
      }

      const uint32_t player = state.capture_player.load(std::memory_order_acquire);
      const auto action = static_cast<Action>(state.capture_action.load(std::memory_order_acquire));
      if (player < keyboard_input::player_count &&
          static_cast<uint32_t>(action) < keyboard_input::configurable_binding_count) {
        const auto cvar_name =
            kBindings[static_cast<uint32_t>(action)].cvar_names[player];
        const auto key_name = KeyString(key);
        if (cvar_name && rex::cvar::SetFlagByName(cvar_name, key_name)) {
          SaveIfConfigured();
          state.capture_active.store(false, std::memory_order_release);
        }
      }
      return;
    }

    if (state.suppressed[key].load(std::memory_order_acquire)) {
      event.set_handled(true);
      return;
    }

  }

  void OnKeyUp(rex::ui::KeyEvent& event) override {
    auto& state = GetState();
    const uint32_t key = static_cast<uint32_t>(event.virtual_key());
    if (!IsValidKey(key) || key == kInvalidKey) {
      return;
    }
    state.held[key].store(false, std::memory_order_release);
    if (state.suppressed[key].exchange(false, std::memory_order_acq_rel)) {
      event.set_handled(true);
    }
  }

  void OnKeyChar(rex::ui::KeyEvent& event) override {
    auto& state = GetState();
    if (state.capture_active.load(std::memory_order_acquire) || AnySuppressed()) {
      event.set_handled(true);
    }
  }

#if REX_PLATFORM_WIN32
 private:
  void AttachIme() {
    auto& state = GetState();
    const HWND hwnd = state.hwnd.load(std::memory_order_acquire);
    if (!hwnd || state.ime_detached) {
      return;
    }
    state.ime_context = ImmAssociateContext(hwnd, nullptr);
    state.ime_detached = true;
  }

  void RestoreIme() {
    auto& state = GetState();
    const HWND hwnd = state.hwnd.load(std::memory_order_acquire);
    if (!hwnd || !state.ime_detached) {
      return;
    }
    const HIMC current = ImmGetContext(hwnd);
    if (!current) {
      ImmAssociateContext(hwnd, state.ime_context);
    } else {
      ImmReleaseContext(hwnd, current);
    }
    state.ime_context = nullptr;
    state.ime_detached = false;
  }
#endif
};

Listener& GetListener() {
  static Listener listener;
  return listener;
}

}

namespace keyboard_input {

int32_t ReadKey(uint32_t virtual_key) {
  auto& state = GetState();
  if (!state.initialized.load(std::memory_order_acquire) ||
      !state.active.load(std::memory_order_acquire) ||
      !state.focused.load(std::memory_order_acquire) ||
      state.closing.load(std::memory_order_acquire) ||
      state.capture_active.load(std::memory_order_acquire) || virtual_key == kInvalidKey ||
      !IsValidKey(virtual_key)) {
    return 0;
  }
  if (state.suppressed[virtual_key].load(std::memory_order_acquire)) {
#if REX_PLATFORM_WIN32
    const HWND hwnd = state.hwnd.load(std::memory_order_acquire);
    if (hwnd && (GetAsyncKeyState(static_cast<int>(virtual_key)) & 0x8000) == 0) {
      state.suppressed[virtual_key].store(false, std::memory_order_release);
    }
#endif
    return 0;
  }
#if REX_PLATFORM_WIN32
  const HWND hwnd = state.hwnd.load(std::memory_order_acquire);
  if (hwnd && GetForegroundWindow() != hwnd) {
    return 0;
  }
  if (hwnd) {
    return (GetAsyncKeyState(static_cast<int>(virtual_key)) & 0x8000) != 0;
  }
#endif
  return state.held[virtual_key].load(std::memory_order_acquire) ? 1 : 0;
}

int32_t ReadAction(uint32_t player, Action action) {
  if (player >= player_count || !IsValidAction(action)) {
    return 0;
  }
  if (action == Action::Start) {
    if (player == 0) {
      return ReadKey(static_cast<uint32_t>(VirtualKey::kReturn)) != 0 ||
             ReadKey(static_cast<uint32_t>(VirtualKey::k1)) != 0;
    }
    return ReadKey(static_cast<uint32_t>(VirtualKey::k2)) != 0;
  }

  const uint32_t key = KeyForAction(player, action);
  if (ReadKey(key) == 0) {
    return 0;
  }
  if (!IsDirectionalAction(action)) {
    return 1;
  }

  const Action opposite_action = OppositeDirection(action);
  const uint32_t opposite_key = KeyForAction(player, opposite_action);
  if (opposite_key == key) {
    return 0;
  }
  if (ReadKey(opposite_key) == 0) {
    return 1;
  }

  auto& state = GetState();
  const uint64_t order = state.press_order[key].load(std::memory_order_acquire);
  const uint64_t opposite_order =
      state.press_order[opposite_key].load(std::memory_order_acquire);
  return order > opposite_order ? 1 : 0;
}

int32_t ReadMenuAction(MenuAction action) {
  const auto key_down = [](VirtualKey key) {
    return ReadKey(static_cast<uint32_t>(key)) != 0;
  };
  switch (action) {
    case MenuAction::Up:
      return key_down(VirtualKey::kUp);
    case MenuAction::Down:
      return key_down(VirtualKey::kDown);
    case MenuAction::Left:
      return key_down(VirtualKey::kLeft);
    case MenuAction::Right:
      return key_down(VirtualKey::kRight);
    case MenuAction::Confirm:
      return key_down(VirtualKey::kZ) || key_down(VirtualKey::kReturn);
    case MenuAction::Back:
      return key_down(VirtualKey::kX) || key_down(VirtualKey::kBack) ||
             key_down(VirtualKey::kEscape);
  }
  return 0;
}

uint32_t KeyForAction(uint32_t player, Action action) {
  if (player >= player_count || !IsValidAction(action)) {
    return kInvalidKey;
  }
  return GetState().keys[BindingIndex(player, action)].load(std::memory_order_acquire);
}

const char* Label(Action action) {
  const auto index = static_cast<uint32_t>(action);
  return index < binding_count ? kBindings[index].label : "";
}

std::string KeyName(uint32_t player, Action action) {
  return KeyString(KeyForAction(player, action));
}

void BeginCapture(uint32_t player, Action action) {
  auto& state = GetState();
  if (!state.initialized.load(std::memory_order_acquire) || player >= player_count ||
      !IsValidAction(action) ||
      static_cast<uint32_t>(action) >= configurable_binding_count) {
    return;
  }
  for (uint32_t key = 1; key < kKeyCount; ++key) {
    state.suppressed[key].store(PhysicalKeyDown(key), std::memory_order_release);
  }
  state.capture_player.store(player, std::memory_order_release);
  state.capture_action.store(static_cast<uint32_t>(action), std::memory_order_release);
  state.capture_active.store(true, std::memory_order_release);
}

bool Capturing() { return GetState().capture_active.load(std::memory_order_acquire); }

void CancelCapture() {
  auto& state = GetState();
  state.capture_active.store(false, std::memory_order_release);
}

void Reset(uint32_t player) {
  auto& state = GetState();
  if (player >= player_count || !state.initialized.load(std::memory_order_acquire) ||
      state.closing.load(std::memory_order_acquire)) {
    return;
  }
  for (uint32_t action = 0; action < binding_count; ++action) {
    const auto name = kBindings[action].cvar_names[player];
    const auto value = kBindings[action].defaults[player];
    if (name && value) {
      rex::cvar::SetFlagByName(name, value);
    }
  }
  SaveIfConfigured();
}

void Initialize(rex::ui::Window& window, rex::input::InputSystem& input,
                std::function<void()> save) {
  auto& state = GetState();
  if (state.initialized.exchange(false, std::memory_order_acq_rel)) {
    Shutdown();
  }

  state.window = &window;
  state.input = &input;
  state.save = std::move(save);
  state.closing.store(false, std::memory_order_release);
  state.focused.store(window.HasFocus(), std::memory_order_release);
  state.active.store(true, std::memory_order_release);
  state.capture_active.store(false, std::memory_order_release);

#if REX_PLATFORM_WIN32
  state.hwnd.store(nullptr, std::memory_order_release);
  if (auto* win32_window = dynamic_cast<rex::ui::Win32Window*>(&window)) {
    state.hwnd.store(win32_window->hwnd(), std::memory_order_release);
  }
#endif

  RegisterBindingCallbacks();
  LoadBindings();
  window.AddListener(&GetListener());
  window.AddInputListener(&GetListener(), SIZE_MAX);
  input.SetActiveStateObserver([](bool active) {
    auto& current = GetState();
    if (!active) {
      current.active.store(false, std::memory_order_release);
      current.capture_active.store(false, std::memory_order_release);
      BlockCurrentlyHeld();
      ClearHeld();
      return;
    }
    BlockCurrentlyHeld();
    current.active.store(true, std::memory_order_release);
  });
  state.initialized.store(true, std::memory_order_release);
  rex::ui::UISetupEvent opened_event(&window, true);
  GetListener().OnOpened(opened_event);
}

void Shutdown() {
  auto& state = GetState();
  state.initialized.store(false, std::memory_order_release);
  state.active.store(false, std::memory_order_release);
  state.focused.store(false, std::memory_order_release);
  state.closing.store(true, std::memory_order_release);
  state.capture_active.store(false, std::memory_order_release);
  if (state.window) {
    state.window->RemoveInputListener(&GetListener());
    state.window->RemoveListener(&GetListener());
  }
  rex::ui::UIEvent closing_event(state.window);
  GetListener().OnClosing(closing_event);
#if REX_PLATFORM_WIN32
  state.hwnd.store(nullptr, std::memory_order_release);
#endif
  if (state.input) {
    state.input->SetActiveStateObserver(nullptr);
  }
  state.window = nullptr;
  state.input = nullptr;
}

}

namespace native_menu {

uint32_t EffectiveRowId(uint8_t* base, uint32_t row) {
  return REX_LOAD_U32(row + alias_flag_offset) != 0 ? REX_LOAD_U32(row + alias_id_offset)
                                                    : REX_LOAD_U32(row + id_offset);
}

void WriteString(uint8_t* base, uint32_t address, uint32_t capacity,
                 std::string_view value) {
  if (!base || !address || capacity == 0) return;
  const size_t length = std::min(value.size(), size_t(capacity - 1));
  std::memset(base + address, 0, capacity);
  std::memcpy(base + address, value.data(), length);
}

}

namespace training_start {

uint32_t string_storage = 0;

void Initialize(rex::memory::Memory& memory) {
  if (!string_storage) {
    string_storage = memory.SystemHeapAlloc(string_bytes);
  }
  if (!string_storage) return;

  auto* base = memory.virtual_membase();
  native_menu::WriteString(base, string_storage, string_slot, "H.FINAL");
  native_menu::WriteString(base, string_storage + string_slot, string_slot, "I.FINAL");
  native_menu::WriteString(base, string_storage + string_slot * 2,
                           string_slot, "BOSS P2");
  native_menu::WriteString(base, string_storage + string_slot * 3,
                           string_slot, "BOSS P3");
}

uint32_t LabelAddress(uint32_t value, uint32_t mode, uint32_t stage) {
  if (!string_storage || !AllowsFinalStarts(mode)) return 0;
  if (stage == stage5_index && value > Stage5LastValue(mode)) return 0;
  if (stage == stage5_index && value == hibachi_final_value) {
    return string_storage;
  }
  if (stage == stage5_index && value == inbachi_final_value) {
    return string_storage + string_slot;
  }
  const uint32_t phase = Phase(stage, value);
  if (phase == boss_phase2) return string_storage + string_slot * 2;
  if (phase == boss_phase3) return string_storage + string_slot * 3;
  return 0;
}

}

namespace keyboard_menu {

bool active = false;
uint32_t system_storage = 0;
uint32_t popup_storage = 0;
uint32_t string_storage = 0;
uint32_t popup_player = 0;
uint32_t capture_action = UINT32_MAX;
bool ready_for_input = false;

namespace {

constexpr uint32_t kStringSlot = 64;
constexpr uint32_t kSystemLabelSlot = 0;
constexpr uint32_t kPlayerLabelSlot = 1;
constexpr uint32_t kPlayerValueSlot = 2;
constexpr uint32_t kBindingLabelSlot = 3;
constexpr uint32_t kBindingValueSlot = kBindingLabelSlot + binding_row_count;
constexpr uint32_t kDefaultSlot = kBindingValueSlot + binding_row_count;
constexpr uint32_t kExitSlot = kDefaultSlot + 1;
constexpr uint32_t kStringSlotCount = kExitSlot + 1;
constexpr uint32_t kStringBytes = kStringSlotCount * kStringSlot;

uint32_t StringAddress(uint32_t slot) {
  return string_storage ? string_storage + slot * kStringSlot : 0;
}

void InitializeText(uint8_t* base) {
  if (!base || !string_storage) return;
  native_menu::WriteString(base, StringAddress(kSystemLabelSlot), kStringSlot, "KEYBOARD");
  native_menu::WriteString(base, StringAddress(kPlayerLabelSlot), kStringSlot, "PLAYER");
  for (uint32_t action = 0; action < binding_row_count; ++action) {
    const auto value_action = static_cast<keyboard_input::Action>(action);
    native_menu::WriteString(base, StringAddress(kBindingLabelSlot + action), kStringSlot,
                             keyboard_input::Label(value_action));
  }
  native_menu::WriteString(base, StringAddress(kDefaultSlot), kStringSlot, "DEFAULT");
  native_menu::WriteString(base, StringAddress(kExitSlot), kStringSlot, "EXIT");
}

}

void Initialize(rex::memory::Memory& memory) {
  if (!system_storage) {
    system_storage = memory.SystemHeapAlloc(system_row_capacity * row_size);
  }
  if (!popup_storage) {
    popup_storage = memory.SystemHeapAlloc(popup_row_count * row_size);
  }
  if (!string_storage) {
    string_storage = memory.SystemHeapAlloc(kStringBytes);
  }
  popup_player = 0;
  ready_for_input = false;
  active = false;
  if (string_storage) {
    auto* base = memory.virtual_membase();
    InitializeText(base);
    UpdateText(base);
  }
}

void UpdateText(uint8_t* base) {
  if (!base || !string_storage) return;
  native_menu::WriteString(base, StringAddress(kPlayerValueSlot), kStringSlot,
                           popup_player == 0 ? "P1" : "P2");
  for (uint32_t action = 0; action < binding_row_count; ++action) {
    const auto value_action = static_cast<keyboard_input::Action>(action);
    const std::string value = keyboard_input::Capturing() && action == capture_action
                                  ? "PRESS KEY"
                                  : keyboard_input::KeyName(popup_player, value_action);
    native_menu::WriteString(base, StringAddress(kBindingValueSlot + action), kStringSlot, value);
  }
}

native_menu::Table BuildSystemTable(uint8_t* base, uint32_t source, uint32_t count) {
  if (!base || !system_storage || count >= system_row_capacity) {
    return {source, count};
  }

  uint32_t insert = count;
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t row = source + i * row_size;
    if (native_menu::EffectiveRowId(base, row) == controller_row_id) {
      insert = i;
      break;
    }
  }
  if (insert == count) {
    return {source, count};
  }

  const uint32_t table = system_storage;
  std::memcpy(base + table, base + source, count * row_size);
  std::memmove(base + table + (insert + 1) * row_size, base + table + insert * row_size,
               (count - insert) * row_size);

  const uint32_t row = table + insert * row_size;
  REX_STORE_U32(row + native_menu::id_offset, system_row_id);
  if (REX_LOAD_U32(row + native_menu::alias_flag_offset) != 0) {
    REX_STORE_U32(row + native_menu::alias_id_offset, system_row_id);
  }
  return {table, count + 1};
}

native_menu::Table BuildPopupTable(uint8_t* base, uint32_t source_p1, uint32_t source_p2) {
  if (!base || !popup_storage) {
    return {};
  }

  uint32_t value_template = source_p1;
  uint32_t default_template = source_p1;
  uint32_t exit_template = source_p1;
  for (uint32_t bank = 0; bank < 2; ++bank) {
    const uint32_t source = bank == 0 ? source_p1 : source_p2;
    for (uint32_t i = 0; i < controller_row_count; ++i) {
      const uint32_t row = source + i * row_size;
      const uint32_t id = native_menu::EffectiveRowId(base, row);
      if (id == controller_default_id) default_template = row;
      if (id == controller_exit_id) exit_template = row;
    }
  }

  const uint32_t table = popup_storage;
  for (uint32_t i = 0; i < popup_row_count; ++i) {
    const uint32_t source = i <= binding_row_count
                                ? value_template
                                : (i == binding_row_count + 1 ? default_template : exit_template);
    const uint32_t id = i == 0
                            ? player_row_id
                            : (i <= binding_row_count
                                   ? binding_first_row_id + (i - 1)
                                   : (i == binding_row_count + 1 ? default_row_id : exit_row_id));
    const uint32_t row = table + i * row_size;
    std::memcpy(base + row, base + source, row_size);
    REX_STORE_U32(row + native_menu::id_offset, id);
    if (REX_LOAD_U32(row + native_menu::alias_flag_offset) != 0) {
      REX_STORE_U32(row + native_menu::alias_id_offset, id);
    }
  }

  UpdateText(base);
  return {table, popup_row_count};
}

bool IsBindingRow(uint32_t id) {
  return id >= binding_first_row_id && id < binding_first_row_id + binding_row_count;
}

bool IsKeyboardRow(uint32_t id) {
  return id == system_row_id || id == player_row_id || IsBindingRow(id) ||
         id == default_row_id || id == exit_row_id;
}

uint32_t ActionIndex(uint32_t id) {
  return IsBindingRow(id) ? id - binding_first_row_id : UINT32_MAX;
}

uint32_t LabelAddress(uint32_t id) {
  if (id == system_row_id) return StringAddress(kSystemLabelSlot);
  if (id == player_row_id) return StringAddress(kPlayerLabelSlot);
  if (IsBindingRow(id)) return StringAddress(kBindingLabelSlot + ActionIndex(id));
  if (id == default_row_id) return StringAddress(kDefaultSlot);
  if (id == exit_row_id) return StringAddress(kExitSlot);
  return 0;
}

uint32_t ValueAddress(uint32_t id) {
  if (id == player_row_id) return StringAddress(kPlayerValueSlot);
  return IsBindingRow(id) ? StringAddress(kBindingValueSlot + ActionIndex(id)) : 0;
}

}

namespace pc_settings {

uint32_t storage = 0;
std::function<void()> save_config;

const std::array<Setting, pc_rows> settings{{
    {"vsync", "VSYNC", "VSYNC TOGGLE."},
    {"fullscreen", "FULLSCREEN", "FULLSCREEN TOGGLE. RESTART THE GAME TO APPLY."},
    {"input_patch", "INPUT PATCH",
     "INPUT PATCH, REDUCES 1 FRAME OF LAG. RESTART THE GAME TO APPLY."},
    {"render_patch", "RENDER PATCH",
     "RENDER PATCH, REDUCES 1 FRAME OF LAG. RESTART THE GAME TO APPLY."},
}};

void Initialize(rex::memory::Memory& memory) {
  if (!storage) {
    storage = memory.SystemHeapAlloc(storage_bytes);
  }
  if (storage) {
    InitializeStrings(memory.virtual_membase());
  }
}

native_menu::Table BuildTable(uint8_t* base, uint32_t source, uint32_t count) {
  if (!base || !storage || !source || count != original_rows) {
    return {source, count};
  }

  const uint32_t table = storage;
  std::memcpy(base + table, base + source, back_index * row_size);
  std::memcpy(base + table + (back_index + pc_rows) * row_size,
              base + source + back_index * row_size, row_size);
  for (uint32_t i = 0; i < pc_rows; ++i) {
    const uint32_t row = table + (back_index + i) * row_size;
    std::memcpy(base + row, base + source + (back_index - 1) * row_size, row_size);
    REX_STORE_U32(row + native_menu::id_offset, first_pc_id + i);
  }
  return {table, menu_row_count};
}

int SettingIndex(uint32_t id) {
  return id >= first_pc_id && id < first_pc_id + pc_rows ? int(id - first_pc_id) : -1;
}

bool Enabled(size_t i) {
  return rex::cvar::GetFlagByName(settings[i].flag) == "true";
}

uint32_t LabelAddress(size_t i) {
  return storage + table_bytes + uint32_t(i) * string_slot;
}

uint32_t DescriptionAddress(size_t i) {
  return storage + description_offset + uint32_t(i) * description_slot;
}

void InitializeStrings(uint8_t* base) {
  for (size_t i = 0; i < settings.size(); ++i) {
    native_menu::WriteString(base, LabelAddress(i), string_slot, settings[i].label);
    native_menu::WriteString(base, DescriptionAddress(i), description_slot,
                             settings[i].description);
  }
}

void SyncValues(uint8_t* base) {
  if (storage) {
    for (uint32_t i = 0; i < pc_rows; ++i) {
      REX_STORE_U32(storage + (back_index + i) * row_size + native_menu::value_offset,
                    std::bit_cast<uint32_t>(Enabled(i) ? 1.0f : 0.0f));
    }
  }
}

}
