#include "popups.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace popups {
namespace {

constexpr float kViewportFraction = 0.9f;

constexpr float kMessageFontSize = 13.0f;
constexpr float kMessageWidth = 520.0f;
constexpr float kMessageHeight = 220.0f;

constexpr float kUpdateFontSize = 13.0f;
constexpr float kUpdateWidth = 640.0f;
constexpr float kUpdateHeight = 460.0f;
constexpr char kNoChangelogText[] =
    "No changelog provided. See the release page for details.";
constexpr char kUpdateSettingsTip[] =
    "Tip: Disable update checks in F4 settings under SDOJ.";
const ImVec4 kUpdateSettingsTipColor{1.0f, 0.85f, 0.25f, 1.0f};

constexpr char kProgressTitle[] = "Preparing game data";
constexpr float kProgressFontSize = 16.0f;
constexpr float kProgressWidth = 440.0f;
constexpr float kProgressHeight = 156.0f;

}

void Center(float width, float height, float reference_font_size) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float scale = ImGui::GetFontSize() / reference_font_size;
  const ImVec2 size(std::min(width * scale, viewport->WorkSize.x * kViewportFraction),
                    std::min(height * scale, viewport->WorkSize.y * kViewportFraction));
  const ImVec2 position(viewport->WorkPos.x + (viewport->WorkSize.x - size.x) * 0.5f,
                        viewport->WorkPos.y + (viewport->WorkSize.y - size.y) * 0.5f);
  ImGui::SetNextWindowPos(position, ImGuiCond_Always);
  ImGui::SetNextWindowSize(size, ImGuiCond_Always);
}

Message::Message(rex::ui::ImGuiDrawer* drawer, std::string title, std::string text,
                 std::string button, std::function<void()> closed, bool copy_button)
    : ImGuiDialog(drawer),
      title_(std::move(title)),
      text_(std::move(text)),
      button_(std::move(button)),
      closed_(std::move(closed)),
      copy_button_(copy_button) {
  title_ += "##message";
}

void Message::OnDraw(ImGuiIO& io) {
  (void)io;
  if (!opened_) {
    ImGui::OpenPopup(title_.c_str());
    opened_ = true;
  }
  Center(kMessageWidth, kMessageHeight, kMessageFontSize);
  if (!ImGui::BeginPopupModal(title_.c_str(), nullptr,
                              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
    if (!closed_sent_) {
      ImGui::OpenPopup(title_.c_str());
    }
    return;
  }

  const float footer = button_.empty()
      ? 0.0f
      : ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y * 2.0f;
  const float text_height = std::max(1.0f, ImGui::GetContentRegionAvail().y - footer);
  if (ImGui::BeginChild("MessageText", ImVec2(0.0f, text_height))) {
    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(text_.data(), text_.data() + text_.size());
    ImGui::PopTextWrapPos();
  }
  ImGui::EndChild();

  if (!button_.empty()) {
    ImGui::Spacing();
    if (ImGui::Button(button_.c_str()) && !closed_sent_) {
      closed_sent_ = true;
      ImGui::CloseCurrentPopup();
      if (closed_) {
        closed_();
      }
    }
    if (copy_button_) {
      ImGui::SameLine();
      if (ImGui::Button("Copy text")) {
        ImGui::SetClipboardText(text_.c_str());
      }
    }
  }
  ImGui::EndPopup();
}

Update::Update(rex::ui::ImGuiDrawer* drawer, std::string version, std::string changelog,
               ActionCallback callback)
    : ImGuiDialog(drawer),
      version_(std::move(version)),
      changelog_(std::move(changelog)),
      callback_(std::move(callback)) {}

void Update::OnDraw(ImGuiIO& io) {
  (void)io;
  constexpr char kTitle[] = "Update available";
  if (!opened_) {
    ImGui::OpenPopup(kTitle);
    opened_ = true;
  }
  Center(kUpdateWidth, kUpdateHeight, kUpdateFontSize);
  if (!ImGui::BeginPopupModal(kTitle, nullptr,
                              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
    if (!action_sent_) {
      action_sent_ = true;
      callback_(UpdateAction::Later);
    }
    return;
  }

  ImGui::TextWrapped("SaiDaiOuJou Recomp %s is available.", version_.c_str());
  ImGui::Spacing();

  const float available_width = ImGui::GetContentRegionAvail().x;
  const ImGuiStyle& style = ImGui::GetStyle();
  const float tip_height = ImGui::CalcTextSize(
      kUpdateSettingsTip, nullptr, false, available_width).y;
  const float buttons_width =
      ImGui::CalcTextSize("Open release page").x +
      ImGui::CalcTextSize("Later").x +
      ImGui::CalcTextSize("Skip this version").x +
      style.FramePadding.x * 6.0f + style.ItemSpacing.x * 2.0f;
  const bool stack_buttons = available_width < buttons_width;
  const float button_rows_height = ImGui::GetFrameHeight() *
      (stack_buttons ? 3.0f : 1.0f) +
      style.ItemSpacing.y * (stack_buttons ? 2.0f : 0.0f);
  const float footer_height = style.ItemSpacing.y * 2.0f + tip_height +
                              style.ItemSpacing.y + button_rows_height;
  const float changelog_height = std::max(
      1.0f, ImGui::GetContentRegionAvail().y - footer_height);
  if (ImGui::BeginChild("ReleaseChangelog", ImVec2(0.0f, changelog_height), true)) {
    if (changelog_.empty()) {
      ImGui::TextWrapped("%s", kNoChangelogText);
    } else {
      ImGui::PushTextWrapPos();
      ImGui::TextUnformatted(changelog_.data(), changelog_.data() + changelog_.size());
      ImGui::PopTextWrapPos();
    }
  }
  ImGui::EndChild();
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, kUpdateSettingsTipColor);
  ImGui::TextWrapped("%s", kUpdateSettingsTip);
  ImGui::PopStyleColor();

  if (ImGui::Button("Open release page") && !action_sent_) {
    action_sent_ = true;
    ImGui::CloseCurrentPopup();
    callback_(UpdateAction::Open);
  }
  if (!stack_buttons) ImGui::SameLine();
  if (ImGui::Button("Later") && !action_sent_) {
    action_sent_ = true;
    ImGui::CloseCurrentPopup();
    callback_(UpdateAction::Later);
  }
  if (!stack_buttons) ImGui::SameLine();
  if (ImGui::Button("Skip this version") && !action_sent_) {
    action_sent_ = true;
    ImGui::CloseCurrentPopup();
    callback_(UpdateAction::Skip);
  }
  ImGui::EndPopup();
}

Progress::Progress(rex::ui::ImGuiDrawer* drawer, ReadCallback read)
    : ImGuiDialog(drawer), read_(std::move(read)) {}

void Progress::OnDraw(ImGuiIO& io) {
  (void)io;
  if (!opened_) {
    ImGui::OpenPopup(kProgressTitle);
    opened_ = true;
  }

  Center(kProgressWidth, kProgressHeight, kProgressFontSize);
  if (!ImGui::BeginPopupModal(
          kProgressTitle, nullptr,
          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
    ImGui::OpenPopup(kProgressTitle);
    return;
  }

  game_setup::Progress progress;
  if (read_) {
    progress = read_();
  }
  if (progress.message.empty()) {
    progress.message = "Preparing game data...";
  }
  ImGui::TextWrapped("%s", progress.message.c_str());
  if (std::isfinite(progress.fraction) && progress.fraction >= 0.0f) {
    ImGui::Spacing();
    ImGui::ProgressBar(std::clamp(progress.fraction, 0.0f, 1.0f),
                       ImVec2(-1.0f, 0.0f), "Current file");
  }
  ImGui::EndPopup();
}

}
