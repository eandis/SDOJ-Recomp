#pragma once

#include "game_setup.h"

#include <rex/ui/imgui_dialog.h>

#include <functional>
#include <string>

namespace popups {

void Center(float width, float height, float reference_font_size);

class Message final : public rex::ui::ImGuiDialog {
 public:
  Message(rex::ui::ImGuiDrawer* drawer, std::string title, std::string text,
          std::string button = {}, std::function<void()> closed = {},
          bool copy_button = false);

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  std::string title_;
  std::string text_;
  std::string button_;
  std::function<void()> closed_;
  bool copy_button_ = false;
  bool opened_ = false;
  bool closed_sent_ = false;
};

enum class UpdateAction { Open, Skip, Later };

class Update final : public rex::ui::ImGuiDialog {
 public:
  using ActionCallback = std::function<void(UpdateAction)>;

  Update(rex::ui::ImGuiDrawer* drawer, std::string version, std::string changelog,
         ActionCallback callback);

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  std::string version_;
  std::string changelog_;
  ActionCallback callback_;
  bool opened_ = false;
  bool action_sent_ = false;
};

class Progress final : public rex::ui::ImGuiDialog {
 public:
  using ReadCallback = std::function<game_setup::Progress()>;

  Progress(rex::ui::ImGuiDrawer* drawer, ReadCallback read);

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  ReadCallback read_;
  bool opened_ = false;
};

}
