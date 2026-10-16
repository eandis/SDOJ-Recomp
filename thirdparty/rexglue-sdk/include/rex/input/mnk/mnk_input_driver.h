/**
 * @file        rex/input/mnk/mnk_input_driver.h
 * @brief       Keyboard/mouse input driver - maps MnK to Xbox 360 controller.
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#pragma once

#include <rex/input/input_driver.h>
#include <rex/ui/window_listener.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

namespace rex::input::mnk {

class MnkInputDriver final : public InputDriver,
                             public rex::ui::WindowInputListener,
                             public rex::ui::WindowListener {
 public:
  explicit MnkInputDriver(rex::ui::Window* window, size_t window_z_order);
  ~MnkInputDriver() override;

  X_STATUS Setup() override;

  X_RESULT GetCapabilities(uint32_t user_index, uint32_t flags,
                           X_INPUT_CAPABILITIES* out_caps) override;
  X_RESULT GetState(uint32_t user_index, X_INPUT_STATE* out_state) override;
  X_RESULT SetState(uint32_t user_index, X_INPUT_VIBRATION* vibration) override;
  X_RESULT GetKeystroke(uint32_t user_index, uint32_t flags,
                        X_INPUT_KEYSTROKE* out_keystroke) override;

  void OnWindowAvailable(rex::ui::Window* window) override;
  void OnActiveStateChanged() override;

  // WindowInputListener
  void OnKeyDown(rex::ui::KeyEvent& e) override;
  void OnKeyUp(rex::ui::KeyEvent& e) override;
  void OnMouseDown(rex::ui::MouseEvent& e) override;
  void OnMouseUp(rex::ui::MouseEvent& e) override;
  void OnMouseMove(rex::ui::MouseEvent& e) override;

  // WindowListener
  void OnOpened(rex::ui::UISetupEvent& e) override;
  void OnClosing(rex::ui::UIEvent& e) override;
  void OnLostFocus(rex::ui::UISetupEvent& e) override;
  void OnGotFocus(rex::ui::UISetupEvent& e) override;

 private:
  uint32_t UserIndex() const;
  bool IsEnabled() const;
  struct State;
  void DetachWindow();
  void QueueMouseUpdate(bool active);
  static void UpdateMouseMode(State& state);
  void SetKeyState(uint16_t vk, bool down);
  void EnqueueKeystroke(uint16_t vk_pad, bool down);

  struct State {
    std::mutex mutex;
    rex::ui::Window* window = nullptr;
    rex::ui::WindowedAppContext* app_context = nullptr;
    bool mouse_update_queued = false;
    uint64_t window_generation = 0;
    bool active = true;
    bool has_focus = true;
    bool mouse_active = false;
    uintptr_t ime_context = 0;
    bool ime_disabled = false;
    bool key_down[256]{};
    int32_t mouse_dx = 0, mouse_dy = 0;
    int32_t prev_mouse_x = 0, prev_mouse_y = 0;
    std::queue<X_INPUT_KEYSTROKE> keystroke_queue;
    uint32_t packet_number = 0;
  };
  std::shared_ptr<State> state_ = std::make_shared<State>();
};

}  // namespace rex::input::mnk
