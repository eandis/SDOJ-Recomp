#pragma once

#include <cstdint>
#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

struct PPCContext;

namespace rex::input {
class InputSystem;
}

namespace rex::ui {
class Window;
}

namespace keyboard_input {

enum class Action : uint32_t {
  Up,
  Down,
  Left,
  Right,
  Shot,
  Bomb,
  RapidShot,
  Extra,
  Start,
  Back,
  Count,
};

enum class MenuAction : uint32_t {
  Up,
  Down,
  Left,
  Right,
  Confirm,
  Back,
  Y,
};

constexpr uint32_t binding_count = static_cast<uint32_t>(Action::Count);
constexpr uint32_t configurable_binding_count = static_cast<uint32_t>(Action::Start);
constexpr uint32_t player_count = 2;

int32_t ReadKey(uint32_t virtual_key);
int32_t ReadAction(uint32_t player, Action action);
int32_t ReadMenuAction(MenuAction action);

uint32_t KeyForAction(uint32_t player, Action action);
const char* Label(Action action);
std::string KeyName(uint32_t player, Action action);

void BeginCapture(uint32_t player, Action action);
bool Capturing();
void CancelCapture();
void Reset(uint32_t player);

void Initialize(rex::ui::Window& window, rex::input::InputSystem& input,
                std::function<void()> save);
void Shutdown();

}

namespace rex::memory {
class Memory;
}

namespace native_menu {

constexpr uint32_t row_size = 0x5C;
constexpr uint32_t id_offset = 4;
constexpr uint32_t alias_id_offset = 20;
constexpr uint32_t alias_flag_offset = 28;
constexpr uint32_t value_offset = 32;

struct Table {
  uint32_t rows = 0;
  uint32_t count = 0;
};

uint32_t EffectiveRowId(uint8_t* base, uint32_t row);
void WriteString(uint8_t* base, uint32_t address, uint32_t capacity,
                 std::string_view value);

}

namespace training_start {

constexpr uint32_t training_mode = 13;
constexpr uint32_t stage2_index = 1;
constexpr uint32_t stage3_index = 2;
constexpr uint32_t stage4_index = 3;
constexpr uint32_t stage5_index = 4;
constexpr uint32_t arcade_hd_menu_mode = 2;
constexpr uint32_t arrange_menu_mode = 3;
constexpr uint32_t novice_menu_mode = 4;
constexpr uint32_t version_15_menu_mode = 5;
constexpr uint32_t default_menu_mode = arcade_hd_menu_mode;
constexpr uint32_t boss_checkpoint = 2;
constexpr uint32_t menu_mode_offset = 24;
constexpr uint32_t draw_menu_stack_offset = 124;
constexpr uint32_t draw_stage_stack_offset = 84;
constexpr uint32_t area_maximum_offset = 16;
constexpr uint32_t mode_address = 0x826335F4;
constexpr uint32_t frontend_controller_address = 0x8262CC68;
constexpr uint32_t training_index = 0x826B9608;
constexpr uint32_t training_records = 0x826B960C;
constexpr uint32_t training_bank_bytes = 604;
constexpr uint32_t training_record_count = 5;
constexpr uint32_t record_bytes = 120;
constexpr uint32_t record_stage_offset = 12;
constexpr uint32_t record_areas_offset = 14;
constexpr uint32_t stage_controller_address = 0x886ACBF8;
constexpr uint32_t controller_stage_offset = 100;
constexpr uint32_t controller_checkpoint_offset = 104;
constexpr uint32_t player1_object = stage_controller_address + 5948;
constexpr uint32_t phase_break_delay_frames = 60;
constexpr uint32_t s4_second_break_delay_frames = 240;
constexpr uint32_t boss_phase_offset = 5616;
constexpr uint32_t stage2_phase_offset = 5800;
constexpr uint32_t stage3_phase_offset = 8412;
constexpr uint32_t stage4_phase_offset = 3944;
constexpr uint32_t stage5_phase_offset = 6544;
constexpr uint32_t training_intro_wait_frames = 60;
constexpr uint32_t warning_wait_frames = 660;

// stage 5 menu order outside arrange: stage, mid-boss, boss, boss p2,
// boss p3, hibachi, inbachi, h.final, i.final. only 0..4 are authored
constexpr uint32_t hibachi_value = 5;
constexpr uint32_t inbachi_value = 6;
constexpr uint32_t hibachi_final_value = 7;
constexpr uint32_t inbachi_final_value = 8;
constexpr uint32_t hibachi_checkpoint = 3;
constexpr uint32_t inbachi_checkpoint = 4;
constexpr uint32_t boss_phase_none = 0;
constexpr uint32_t boss_phase2 = 1;
constexpr uint32_t boss_phase3 = 2;
constexpr uint32_t boss_phase2_value = 3;
constexpr uint32_t boss_phase3_value = 4;

struct ExtraStart {
  uint32_t stage;
  int32_t scroll;
  int32_t section;
};

constexpr uint32_t mid_stage_checkpoint = 1;
// this deletes midbosses otherwise fucks it up
constexpr uint32_t skip_mid_boss_flag = 4;

constexpr ExtraStart extra_starts[] = {
    {stage3_index, 450, 1},
    {stage4_index, 176, 1},
    {stage5_index, 216, 3},
    {stage5_index, 295, 3},
};
constexpr uint32_t extra_label_count = 2;
constexpr uint32_t extra_first_value = 2;
constexpr uint32_t extra_value = 0x100;
constexpr int32_t extra_boss_section = 4;
constexpr uint32_t extra_hidden_frames = 600;

constexpr bool IsArrange(uint32_t mode) {
  return mode == arrange_menu_mode;
}

constexpr bool AllowsFinalStarts(uint32_t mode) {
  return mode == arcade_hd_menu_mode || mode == novice_menu_mode ||
         mode == version_15_menu_mode;
}

constexpr uint32_t Stage5LastValue(uint32_t mode) {
  return mode == novice_menu_mode ? hibachi_value : inbachi_final_value;
}

constexpr uint32_t ExtraCount(uint32_t stage, uint32_t mode) {
  if (!AllowsFinalStarts(mode)) return 0;
  uint32_t count = 0;
  for (const ExtraStart& start : extra_starts) {
    if (start.stage == stage) ++count;
  }
  return count;
}

constexpr const ExtraStart* FindExtra(uint32_t stage, uint32_t index) {
  for (const ExtraStart& start : extra_starts) {
    if (start.stage != stage) continue;
    if (index == 0) return &start;
    --index;
  }
  return nullptr;
}




constexpr bool IsExtra(uint32_t value) {
  return value >= extra_value;
}

constexpr uint32_t Original(uint32_t stage, uint32_t mode, uint32_t value) {
  const uint32_t count = ExtraCount(stage, mode);
  if (count == 0 || value < extra_first_value) return value;
  if (value < extra_first_value + count) return extra_value + (value - extra_first_value);
  return value - count;
}

constexpr uint32_t Phase(uint32_t stage, uint32_t value) {
  if (stage >= stage2_index && stage <= stage5_index &&
      value == boss_phase2_value) {
    return boss_phase2;
  }
  if ((stage == stage3_index || stage == stage4_index ||
       stage == stage5_index) && value == boss_phase3_value) {
    return boss_phase3;
  }
  return boss_phase_none;
}

constexpr int32_t mid_boss_label_offset = -18;
constexpr int32_t boss_label_offset = 8;
constexpr int32_t long_label_offset = -10;
constexpr int32_t final_label_offset = -4;

constexpr int32_t LabelOffset(uint32_t value, uint32_t mode, uint32_t stage, int32_t vanilla) {
  if (!AllowsFinalStarts(mode)) return vanilla;
  const uint32_t original = Original(stage, mode, value);
  if (IsExtra(original)) return long_label_offset;
  if (original == 1) return mid_boss_label_offset;
  if (original == 2) return boss_label_offset;
  if (stage == stage5_index &&
      (original == hibachi_final_value || original == inbachi_final_value)) {
    return final_label_offset;
  }
  if (original >= 3) return long_label_offset;
  return vanilla;
}

constexpr uint32_t OriginalMaximum(uint32_t stage, uint32_t mode, uint32_t stock) {
  if (!AllowsFinalStarts(mode)) return stock;
  if (stage == stage2_index) {
    return stock > boss_phase2_value ? stock : boss_phase2_value;
  }
  if (stage == stage3_index || stage == stage4_index) {
    return stock > boss_phase3_value ? stock : boss_phase3_value;
  }
  if (stage == stage5_index) {
    const uint32_t last = Stage5LastValue(mode);
    return stock > last ? stock : last;
  }
  return stock;
}

constexpr uint32_t Maximum(uint32_t stage, uint32_t mode, uint32_t stock) {
  const uint32_t maximum = OriginalMaximum(stage, mode, stock);
  return maximum >= extra_first_value - 1 ? maximum + ExtraCount(stage, mode) : maximum;
}

constexpr uint32_t OriginalCheckpoint(uint32_t value, uint32_t mode, uint32_t stage) {
  if (stage == stage5_index && IsArrange(mode)) {
    return value > inbachi_checkpoint ? boss_checkpoint : value;
  }
  if (stage == stage5_index && value > Stage5LastValue(mode)) {
    return boss_checkpoint;
  }
  if (stage == stage5_index &&
      (value == hibachi_value || value == hibachi_final_value)) {
    return hibachi_checkpoint;
  }
  if (stage == stage5_index &&
      (value == inbachi_value || value == inbachi_final_value)) {
    return inbachi_checkpoint;
  }
  if (Phase(stage, value) != 0) return boss_checkpoint;
  return value;
}

constexpr uint32_t Checkpoint(
    uint32_t value, uint32_t mode = default_menu_mode,
    uint32_t stage = stage5_index) {
  const uint32_t original = Original(stage, mode, value);
  if (IsExtra(original)) return mid_stage_checkpoint;
  return OriginalCheckpoint(original, mode, stage);
}

enum class FinalBossRequest : uint32_t {
  None = 0,
  Hibachi,
  Inbachi,
};

constexpr uint32_t string_slot = 16;
constexpr uint32_t string_bytes = string_slot * (4 + extra_label_count);

extern uint32_t string_storage;

extern FinalBossRequest ca_final_request;
FinalBossRequest CurrentFinalRequestFromTitleSettings(uint8_t* base);
bool IsTrainingHiddenBossStart(uint8_t* base);

void Initialize(rex::memory::Memory& memory);
uint32_t LabelAddress(uint32_t value, uint32_t mode = default_menu_mode,
                      uint32_t stage = stage5_index);
void SetBossPhaseRequest(uint8_t* base, uint32_t object, uint32_t stage);
bool ExtraStartLoading(uint8_t* base);
uint32_t ExtraStartFrame(uint8_t* base, uint32_t flag);
int ExtraStartStop(uint8_t* base);
bool AdvanceBossPhase(uint8_t* base, uint32_t object, uint32_t stage,
                      uint32_t phase_offset);
void BreakBossPhase(PPCContext& ctx, uint8_t* base, void (*transition)(PPCContext&, uint8_t*),
                    uint32_t boss, uint32_t damage_record);

}

namespace keyboard_menu {

constexpr uint32_t row_size = native_menu::row_size;
constexpr uint32_t system_row_id = 0x1100;
constexpr uint32_t player_row_id = 0x1110;
constexpr uint32_t binding_first_row_id = player_row_id + 1;
constexpr uint32_t binding_row_count = keyboard_input::configurable_binding_count;
constexpr uint32_t default_row_id = binding_first_row_id + binding_row_count;
constexpr uint32_t exit_row_id = default_row_id + 1;
constexpr uint32_t popup_row_count = binding_row_count + 3;
constexpr uint32_t system_row_capacity = 9;
constexpr uint32_t controller_row_id = 120;
constexpr uint32_t controller_default_id = 26;
constexpr uint32_t controller_exit_id = 30;
constexpr uint32_t controller_row_count = 6;
constexpr uint32_t closing_state = 4;
constexpr uint32_t menu_pointer_offset = 24;
constexpr uint32_t selection_index_offset = 4;
constexpr uint32_t rows_pointer_offset = 28;
constexpr uint32_t keyboard_popup_x = 200;
constexpr uint32_t keyboard_popup_width = 320;
constexpr uint32_t keyboard_popup_height = 288;
constexpr float value_text_spacing = 140.0f;

enum MenuResult : uint32_t {
  result_idle = 0,
  result_no_change = 1,
  result_move = 2,
  result_player_toggle = 3,
  result_confirm = 4,
  result_back = 5,
  result_confirm_alternate = 6,
  result_cancel = 7,
};

extern bool active;
extern uint32_t system_storage;
extern uint32_t popup_storage;
extern uint32_t string_storage;
extern uint32_t popup_player;
extern uint32_t capture_action;
extern bool ready_for_input;

void Initialize(rex::memory::Memory& memory);
void UpdateText(uint8_t* base);
native_menu::Table BuildSystemTable(uint8_t* base, uint32_t source, uint32_t count);
native_menu::Table BuildPopupTable(uint8_t* base, uint32_t source_p1, uint32_t source_p2);

bool IsBindingRow(uint32_t id);
bool IsKeyboardRow(uint32_t id);
uint32_t ActionIndex(uint32_t id);
uint32_t LabelAddress(uint32_t id);
uint32_t ValueAddress(uint32_t id);

}

namespace pc_settings {

constexpr uint32_t row_size = native_menu::row_size;
constexpr uint32_t original_rows = 10;
constexpr uint32_t back_index = original_rows - 1;
constexpr uint32_t first_pc_id = 0x1000;
constexpr uint32_t pc_rows = 4;
constexpr uint32_t menu_row_count = original_rows + pc_rows;
constexpr uint32_t menu_row_height = 20;
constexpr uint32_t panel_vertical_padding = 40;
constexpr uint32_t panel_height =
    menu_row_count * menu_row_height + panel_vertical_padding;
constexpr uint32_t table_bytes = menu_row_count * row_size;
constexpr uint32_t string_slot = 32;
constexpr uint32_t description_slot = 128;
constexpr uint32_t description_offset = table_bytes + pc_rows * string_slot;
constexpr uint32_t storage_bytes = description_offset + pc_rows * description_slot;
constexpr uint32_t help_text_pointer_offset = 28436;
constexpr uint32_t help_state_offset = 28456;
constexpr uint32_t help_refresh_state = 4;

enum MenuResult : uint32_t {
  result_value_change = 3,
  result_confirm = 4,
};

struct Setting {
  const char* flag;
  const char* label;
  const char* description;
};

extern const std::array<Setting, pc_rows> settings;
extern uint32_t storage;
extern std::function<void()> save_config;

void Initialize(rex::memory::Memory& memory);
native_menu::Table BuildTable(uint8_t* base, uint32_t source, uint32_t count);
int SettingIndex(uint32_t id);
bool Enabled(size_t index);
uint32_t LabelAddress(size_t index);
uint32_t DescriptionAddress(size_t index);
void InitializeStrings(uint8_t* base);
void SyncValues(uint8_t* base);

}
