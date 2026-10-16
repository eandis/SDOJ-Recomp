#pragma once

#include <cstdint>

namespace render_patch {
constexpr uint32_t kRunRenderFrame = 0x88030D90;
constexpr uint32_t kStartRenderEarly = 0x8217F65E;
constexpr uint32_t kEndRenderEarly = 0x8217F65F;
constexpr uint32_t kWaitForRender = 0x8211F07D;

constexpr uint32_t kGameplayController = 0x8262CC68;
constexpr uint32_t kArrangeModeOffset = 0xB8;
constexpr uint32_t kRequiredArrangeValue = 2;
constexpr uint32_t kModuleRenderReadyOffset = 0x28;
constexpr uint32_t GameplayModule(bool arrange) { return arrange ? 0x8279C238 : 0x8279C230; }
constexpr uint32_t RenderCallback(bool arrange) { return arrange ? 0x886BC73C : 0x886B4214; }
constexpr uint32_t ExpectedRenderCallback(bool arrange) {
  return arrange ? 0x88051308u : 0x88051390u;
}
}

#if defined(LAG_PATCH_INPUT)
#include <array>
#include <cstdint>
#include <cstdlib>
#include <rex/ui/virtual_key.h>

#include "settings.h"

namespace {

namespace PhysicalPadControl {
enum Control : uint32_t {
	Unbound = 0,
	Up = 1, Down = 2, Left = 3, Right = 4,
	Start = 5, Back = 6,
	LeftStickPress = 7, RightStickPress = 8,
	LeftShoulder = 9, RightShoulder = 10,
	LeftTrigger = 11, RightTrigger = 12,
	LeftStickX = 13, LeftStickY = 14,
	RightStickX = 15, RightStickY = 16,
	A = 17, B = 18, X = 19, Y = 20,
};
}

namespace XInputButtonMask {
enum : uint16_t {
	DpadUp = 0x0001, DpadDown = 0x0002, DpadLeft = 0x0004, DpadRight = 0x0008,
	Start = 0x0010, Back = 0x0020,
	LeftStickPress = 0x0040, RightStickPress = 0x0080,
	LeftShoulder = 0x0100, RightShoulder = 0x0200,
	A = 0x1000, B = 0x2000, X = 0x4000, Y = 0x8000,
};
}

namespace GameActionId {
enum : uint32_t {
	Up = 0, Down = 1, Left = 2, Right = 3, Button4Fallback = 5,
	LeftStickX = 12, LeftStickY = 13, AlternateUp = 20, AlternateRight = 21,
};
}

namespace GameplayMask {
enum : uint32_t {
	Up = 0x01, Down = 0x02, Left = 0x04, Right = 0x08,
	PrimaryAction = 0x10, Button2 = 0x20, Button3 = 0x40, Button4 = 0x80,
};
}

namespace ConfiguredBindingOffset {
enum Offset : uint32_t {
	Set1Button1 = 120, Set1Button2 = 124, Set1Button3 = 128, Set1Button4 = 132,
	Set2Button1 = 152, Set2Button2 = 156, Set2Button3 = 160, Set2Button4 = 164,
};
}

namespace SharedActionOffset {
enum : uint32_t {
	Up = 0, Down = 1, Left = 2, Right = 3,
	Button1 = 4, Button2 = 5, Button3 = 6, Button4 = 7,
	Start = 8, Back = 9,
};
}

constexpr uint32_t kXInputGetStateFunction = 0x822169E0;
constexpr uint32_t kXInputStackFrameOffset = 64;
constexpr uint32_t kXInputButtonsOffset = 4;
constexpr uint32_t kXInputLeftTriggerOffset = 6;
constexpr uint32_t kXInputRightTriggerOffset = 7;
constexpr uint32_t kXInputThumbAxesOffset = 8;
constexpr uint32_t kBindingControlNumberMask = 0xFFu;
constexpr uint32_t kBindingTypeShift = 8;
constexpr uint32_t kXInputBindingType = 0x10000;
constexpr uint32_t kHidBindingTypeMask = 0xFFFF0000u;
constexpr uint32_t kHidBindingType = 0x00100000u;
constexpr uint32_t kGetInputHeldFunction = 0x8217CDD0;
constexpr uint32_t kPrivateKeyboardDispatchFlag = 0xFFFFFFFFu;
constexpr uint32_t kPrivateKeyboardMenuDispatchFlag = 0xFFFFFFFEu;
constexpr uint32_t kKeyboardQueryBinding = kHidBindingType | 1u;
constexpr uint32_t kNativeKeyboardMenuUpActionId = 0;
constexpr uint32_t kNativeKeyboardMenuDownActionId = 1;
constexpr uint32_t kNativeKeyboardMenuLeftActionId = 2;
constexpr uint32_t kNativeKeyboardMenuRightActionId = 3;
constexpr uint32_t kNativeKeyboardMenuConfirmActionId = 16;
constexpr uint32_t kNativeKeyboardMenuBackActionId = 17;
constexpr uint32_t kPlayerStartActionId = 4;
constexpr uint32_t kPlayerBackActionId = 5;
constexpr uint32_t kSharedGameplayActionCount =
    static_cast<uint32_t>(keyboard_input::Action::Start);
constexpr uint32_t kSavedInputValueCapacity = 12;

constexpr uint32_t kInputManagerPlayerDataOffset = 14956;
constexpr uint32_t kPlayerDataStride = 1272;
constexpr uint32_t kPlayerBindingsPointerOffset = 100;
constexpr uint32_t kPlayerBindingCountOffset = 104;
constexpr uint32_t kPlayerActionCountOffset = 200;
constexpr uint32_t kPlayerRawActionsPointerOffset = 204;
constexpr uint32_t kPlayerActionsPointerOffset = 212;
constexpr uint32_t kBindingStride = 40;
constexpr uint32_t kBindingActionIdOffset = 0;
constexpr uint32_t kBindingControlOffset = 4;
constexpr uint32_t kActionStride = 36;
constexpr uint32_t kActionValueOffset = 8;

constexpr uint32_t kConfiguredPlayerMapOffset = 380;
constexpr uint32_t kConfiguredPlayerMapStride = 4;
constexpr uint32_t kPlayerConfigStride = 64;
constexpr uint32_t kSharedActionPort = 0;
constexpr uint32_t kSharedActionBaseId = 27;
constexpr uint32_t kSharedActionPlayerStride = 13;
constexpr int32_t kDigitalStickThreshold = 9000;
constexpr uint32_t kGameplayPlayerCount = 2;
constexpr uint32_t kControllerPortCount = 4;
constexpr uint32_t kGameplayInputMaskOffset = 15;
constexpr uint32_t kGameplayInputPlayerStride = 160;
constexpr uint32_t kInputManagerOffset = 588;
constexpr uint32_t InputObject(bool arrange) { return arrange ? 0x88881720 : 0x888791F8; }
constexpr uint32_t PlayerConfigSlot(bool arrange) { return arrange ? 0x88881724 : 0x888791FC; }
constexpr uint32_t GameplayInputBlock(bool arrange) { return arrange ? 0x8887D838 : 0x88875310; }

struct GameplayPad {
	int8_t status = -1;
	uint16_t buttons = 0;
	uint8_t triggers[2]{};
	int16_t sticks[4]{};
};

using VK = rex::ui::VirtualKey;
constexpr uint16_t VirtualKeyCode(VK key) {
	return static_cast<uint16_t>(key);
}

bool IsNativeKeyboardBinding(uint32_t binding) {
	return (binding & kHidBindingTypeMask) == kHidBindingType;
}

uint16_t NativeKeyboardVirtualKey(uint32_t binding) {
	if (!IsNativeKeyboardBinding(binding)) return 0;
	switch (binding & kBindingControlNumberMask) {
	case 1: return VirtualKeyCode(VK::kEscape);
	case 2: return VirtualKeyCode(VK::k1);
	case 3: return VirtualKeyCode(VK::k2);
	case 4: return VirtualKeyCode(VK::k3);
	case 5: return VirtualKeyCode(VK::k4);
	case 6: return VirtualKeyCode(VK::k5);
	case 7: return VirtualKeyCode(VK::k6);
	case 8: return VirtualKeyCode(VK::k7);
	case 9: return VirtualKeyCode(VK::k8);
	case 10: return VirtualKeyCode(VK::k9);
	case 11: return VirtualKeyCode(VK::k0);
	case 12: return VirtualKeyCode(VK::kOemMinus);
	case 14: return VirtualKeyCode(VK::kBack);
	case 15: return VirtualKeyCode(VK::kTab);
	case 16: return VirtualKeyCode(VK::kQ);
	case 17: return VirtualKeyCode(VK::kW);
	case 18: return VirtualKeyCode(VK::kE);
	case 19: return VirtualKeyCode(VK::kR);
	case 20: return VirtualKeyCode(VK::kT);
	case 21: return VirtualKeyCode(VK::kY);
	case 22: return VirtualKeyCode(VK::kU);
	case 23: return VirtualKeyCode(VK::kI);
	case 24: return VirtualKeyCode(VK::kO);
	case 25: return VirtualKeyCode(VK::kP);
	case 28: return VirtualKeyCode(VK::kReturn);
	case 29: return VirtualKeyCode(VK::kLControl);
	case 30: return VirtualKeyCode(VK::kA);
	case 31: return VirtualKeyCode(VK::kS);
	case 32: return VirtualKeyCode(VK::kD);
	case 33: return VirtualKeyCode(VK::kF);
	case 34: return VirtualKeyCode(VK::kG);
	case 35: return VirtualKeyCode(VK::kH);
	case 36: return VirtualKeyCode(VK::kJ);
	case 37: return VirtualKeyCode(VK::kK);
	case 38: return VirtualKeyCode(VK::kL);
	case 42: return VirtualKeyCode(VK::kLShift);
	case 44: return VirtualKeyCode(VK::kZ);
	case 45: return VirtualKeyCode(VK::kX);
	case 46: return VirtualKeyCode(VK::kC);
	case 47: return VirtualKeyCode(VK::kV);
	case 48: return VirtualKeyCode(VK::kB);
	case 49: return VirtualKeyCode(VK::kN);
	case 50: return VirtualKeyCode(VK::kM);
	case 51: return VirtualKeyCode(VK::kOemComma);
	case 52: return VirtualKeyCode(VK::kOemPeriod);
	case 54: return VirtualKeyCode(VK::kRShift);
	case 55: return VirtualKeyCode(VK::kMultiply);
	case 56: return VirtualKeyCode(VK::kLMenu);
	case 57: return VirtualKeyCode(VK::kSpace);
	case 58: return VirtualKeyCode(VK::kCapital);
	case 59: return VirtualKeyCode(VK::kF1);
	case 60: return VirtualKeyCode(VK::kF2);
	case 61: return VirtualKeyCode(VK::kF3);
	case 62: return VirtualKeyCode(VK::kF4);
	case 63: return VirtualKeyCode(VK::kF5);
	case 64: return VirtualKeyCode(VK::kF6);
	case 65: return VirtualKeyCode(VK::kF7);
	case 66: return VirtualKeyCode(VK::kF8);
	case 67: return VirtualKeyCode(VK::kF9);
	case 68: return VirtualKeyCode(VK::kF10);
	case 69: return VirtualKeyCode(VK::kNumLock);
	case 70: return VirtualKeyCode(VK::kScroll);
	case 71: return VirtualKeyCode(VK::kNumpad7);
	case 72: return VirtualKeyCode(VK::kNumpad8);
	case 73: return VirtualKeyCode(VK::kNumpad9);
	case 74: return VirtualKeyCode(VK::kSubtract);
	case 75: return VirtualKeyCode(VK::kNumpad4);
	case 76: return VirtualKeyCode(VK::kNumpad5);
	case 77: return VirtualKeyCode(VK::kNumpad6);
	case 78: return VirtualKeyCode(VK::kAdd);
	case 79: return VirtualKeyCode(VK::kNumpad1);
	case 80: return VirtualKeyCode(VK::kNumpad2);
	case 81: return VirtualKeyCode(VK::kNumpad3);
	case 82: return VirtualKeyCode(VK::kNumpad0);
	case 87: return VirtualKeyCode(VK::kF11);
	case 181: return VirtualKeyCode(VK::kDivide);
	case 197: return VirtualKeyCode(VK::kPause);
	case 199: return VirtualKeyCode(VK::kHome);
	case 200: return VirtualKeyCode(VK::kUp);
	case 201: return VirtualKeyCode(VK::kPrior);
	case 203: return VirtualKeyCode(VK::kLeft);
	case 205: return VirtualKeyCode(VK::kRight);
	case 207: return VirtualKeyCode(VK::kEnd);
	case 208: return VirtualKeyCode(VK::kDown);
	case 209: return VirtualKeyCode(VK::kNext);
	case 210: return VirtualKeyCode(VK::kInsert);
	case 211: return VirtualKeyCode(VK::kDelete);
	case 238: return VirtualKeyCode(VK::kSnapshot);
	default: return 0;
	}
}

bool MapKeyboardAction(uint32_t player, uint32_t action_id,
		uint32_t& host_player, keyboard_input::Action& action) {
	for (uint32_t shared_player = 0;
		 shared_player < keyboard_input::player_count; ++shared_player) {
		const uint32_t base_id =
			kSharedActionBaseId + shared_player * kSharedActionPlayerStride;
		if (action_id >= base_id && action_id < base_id + kSharedGameplayActionCount) {
			host_player = shared_player;
			action = static_cast<keyboard_input::Action>(action_id - base_id);
			return true;
		}
		if (action_id == base_id + SharedActionOffset::Start) {
			host_player = shared_player;
			action = keyboard_input::Action::Start;
			return true;
		}
		if (action_id == base_id + SharedActionOffset::Back) {
			host_player = shared_player;
			action = keyboard_input::Action::Back;
			return true;
		}
	}

	if (player < keyboard_input::player_count) {
		switch (action_id) {
		case kPlayerStartActionId:
			host_player = player;
			action = keyboard_input::Action::Start;
			return true;
		case kPlayerBackActionId:
			host_player = player;
			action = keyboard_input::Action::Back;
			return true;
		case kNativeKeyboardMenuConfirmActionId:
			host_player = player;
			action = keyboard_input::Action::Shot;
			return true;
		case kNativeKeyboardMenuBackActionId:
			host_player = player;
			action = keyboard_input::Action::Bomb;
			return true;
		default:
			break;
		}
	}
	if (player < keyboard_input::player_count && action_id <= GameActionId::Right) {
		host_player = player;
		action = static_cast<keyboard_input::Action>(action_id);
		return true;
	}
	return false;
}

bool NativeKeyboardMenuAction(uint32_t action_id,
		keyboard_input::MenuAction& action) {
	switch (action_id) {
	case kNativeKeyboardMenuUpActionId:
		action = keyboard_input::MenuAction::Up;
		return true;
	case kNativeKeyboardMenuDownActionId:
		action = keyboard_input::MenuAction::Down;
		return true;
	case kNativeKeyboardMenuLeftActionId:
		action = keyboard_input::MenuAction::Left;
		return true;
	case kNativeKeyboardMenuRightActionId:
		action = keyboard_input::MenuAction::Right;
		return true;
	case kNativeKeyboardMenuConfirmActionId:
		action = keyboard_input::MenuAction::Confirm;
		return true;
	case kNativeKeyboardMenuBackActionId:
		action = keyboard_input::MenuAction::Back;
		return true;
	default:
		return false;
	}
}

int32_t QueryKeyboard(PPCContext& ctx, uint8_t* base,
		uint32_t dispatch_flag, uint32_t player, uint32_t action_id,
		uint32_t binding, uint32_t return_address) {
	const PPCContext saved_ctx = ctx;
	ctx.r3.u32 = player;
	ctx.r4.u32 = dispatch_flag;
	ctx.r5.u32 = action_id;
	ctx.r6.u32 = binding;
	ctx.lr = return_address;
	REX_CALL_INDIRECT_FUNC(kGetInputHeldFunction);
	const int32_t value = ctx.r3.s32;
	ctx = saved_ctx;
	ctx.fpscr.setcsr(ctx.fpscr.csr);
	return value;
}

int32_t ReadGameplayKey(PPCContext& ctx, uint8_t* base,
		uint32_t player, uint32_t action_id, uint32_t return_address) {
	return QueryKeyboard(ctx, base, kPrivateKeyboardDispatchFlag,
		player, action_id, kKeyboardQueryBinding, return_address);
}

int32_t ReadKeyboardMenuAction(PPCContext& ctx, uint8_t* base,
		uint32_t player, uint32_t action_id, uint32_t return_address) {
	return QueryKeyboard(ctx, base, kPrivateKeyboardMenuDispatchFlag,
		player, action_id, kKeyboardQueryBinding, return_address);
}

struct SavedInputValue {
	uint32_t address;
	uint32_t value;
};

std::array<SavedInputValue, kSavedInputValueCapacity> g_saved_input_values{};
uint32_t g_saved_input_count = 0;

uint32_t KeyboardRawInputBit(uint32_t action_id) {
	switch (action_id) {
	case kNativeKeyboardMenuUpActionId: return 1u << 0;
	case kNativeKeyboardMenuDownActionId: return 1u << 1;
	case kNativeKeyboardMenuLeftActionId: return 1u << 2;
	case kNativeKeyboardMenuRightActionId: return 1u << 3;
	case kNativeKeyboardMenuConfirmActionId: return 1u << 4;
	case kNativeKeyboardMenuBackActionId: return 1u << 5;
	case kPlayerStartActionId: return 1u << 6;
	case kPlayerBackActionId: return 1u << 7;
	case kSharedActionBaseId + SharedActionOffset::Start: return 1u << 8;
	case kSharedActionBaseId + SharedActionOffset::Back: return 1u << 9;
	case kSharedActionBaseId + kSharedActionPlayerStride + SharedActionOffset::Start:
		return 1u << 10;
	case kSharedActionBaseId + kSharedActionPlayerStride + SharedActionOffset::Back:
		return 1u << 11;
	default: return 0;
	}
}

void RestoreSavedInputValues(uint8_t* base) {
	for (uint32_t i = 0; i < g_saved_input_count; ++i) {
		const SavedInputValue& saved = g_saved_input_values[i];
		REX_STORE_U32(saved.address, saved.value);
	}
	g_saved_input_count = 0;
}

void RefreshNativeKeyboardActions(PPCContext& ctx, uint8_t* base,
		uint32_t player_data, uint32_t player, uint32_t return_address) {
	RestoreSavedInputValues(base);
	if (player_data == 0) return;
	const uint32_t bindings = REX_LOAD_U32(player_data + kPlayerBindingsPointerOffset);
	const uint32_t binding_count = REX_LOAD_U32(player_data + kPlayerBindingCountOffset);
	const uint32_t action_count = REX_LOAD_U32(player_data + kPlayerActionCountOffset);
	const uint32_t raw_actions =
		REX_LOAD_U32(player_data + kPlayerRawActionsPointerOffset);
	if (bindings == 0 || raw_actions == 0 || action_count == 0) return;
	const uint32_t row_count =
		binding_count < action_count ? binding_count : action_count;
	uint32_t hid_action_mask = 0;
	uint32_t overridden_action_mask = 0;

	for (uint32_t i = 0; i < row_count; ++i) {
		const uint32_t binding_data = bindings + i * kBindingStride;
		const uint32_t action_id = REX_LOAD_U32(binding_data + kBindingActionIdOffset);
		const uint32_t binding = REX_LOAD_U32(binding_data + kBindingControlOffset);
		const uint32_t ui_bit = KeyboardRawInputBit(action_id);
		const bool has_keyboard_key = NativeKeyboardVirtualKey(binding) != 0;
		const bool p2_gameplay_row = player == 1 &&
			(action_id <= GameActionId::Right ||
			 action_id == kNativeKeyboardMenuConfirmActionId ||
			 action_id == kNativeKeyboardMenuBackActionId);
		keyboard_input::MenuAction menu_action = keyboard_input::MenuAction::Up;
		const bool menu_row = !p2_gameplay_row &&
			NativeKeyboardMenuAction(action_id, menu_action);

		if (!has_keyboard_key) {
			if (ui_bit == 0 || (hid_action_mask & ui_bit) != 0 ||
				(overridden_action_mask & ui_bit) != 0 ||
				g_saved_input_count >= kSavedInputValueCapacity) {
				continue;
			}
			const uint32_t raw_address = raw_actions + i * sizeof(uint32_t);
			const uint32_t original_raw = REX_LOAD_U32(raw_address);
			g_saved_input_values[g_saved_input_count++] = {raw_address, original_raw};
			overridden_action_mask |= ui_bit;

			const bool held =
				(menu_row
					? ReadKeyboardMenuAction(ctx, base, player, action_id, return_address)
					: ReadGameplayKey(ctx, base, player, action_id, return_address)) != 0;
			REX_STORE_U32(raw_address,
				original_raw != 0 ? original_raw : (held ? 1u : 0u));
			continue;
		}

		hid_action_mask |= ui_bit;
		bool held =
			(menu_row
				? ReadKeyboardMenuAction(ctx, base, player, action_id, return_address)
				: (p2_gameplay_row
					? ReadGameplayKey(ctx, base, player, action_id, return_address)
					: QueryKeyboard(ctx, base, kPrivateKeyboardDispatchFlag,
						player, action_id, binding, return_address))) != 0;
		for (uint32_t duplicate = 0; duplicate < i; ++duplicate) {
			const uint32_t duplicate_data = bindings + duplicate * kBindingStride;
			if (REX_LOAD_U32(duplicate_data + kBindingActionIdOffset) !=
				action_id) {
				continue;
			}
			const uint32_t duplicate_binding = REX_LOAD_U32(
				duplicate_data + kBindingControlOffset);
			if (NativeKeyboardVirtualKey(duplicate_binding) == 0) continue;
			if (REX_LOAD_U32(raw_actions + duplicate * sizeof(uint32_t)) != 0) {
				held = true;
			}
			if (held) {
				REX_STORE_U32(raw_actions + duplicate * sizeof(uint32_t), 1);
			}
		}
		REX_STORE_U32(raw_actions + i * sizeof(uint32_t), held ? 1u : 0u);
	}
}


bool PollGameplayPad(PPCContext& ctx, uint8_t* base, uint32_t port,
		uint32_t return_address, GameplayPad& pad) {
	if (pad.status >= 0) return pad.status != 0;

	const uint32_t xinput_state = (ctx.r1.u32 - kXInputStackFrameOffset) & ~15u;
	ctx.r3.u64 = port;
	ctx.r4.u64 = xinput_state;
	ctx.lr = return_address;
	REX_CALL_INDIRECT_FUNC(kXInputGetStateFunction);
	pad.status = ctx.r3.u32 == 0;
	if (!pad.status) return false;

	pad.buttons = REX_LOAD_U16(xinput_state + kXInputButtonsOffset);
	pad.triggers[0] = REX_LOAD_U8(xinput_state + kXInputLeftTriggerOffset);
	pad.triggers[1] = REX_LOAD_U8(xinput_state + kXInputRightTriggerOffset);
	for (uint32_t i = 0; i < 4; ++i) {
		pad.sticks[i] = static_cast<int16_t>(REX_LOAD_U16(
			xinput_state + kXInputThumbAxesOffset + i * sizeof(int16_t)));
	}
	return true;
}

int32_t ReadPadControl(uint32_t binding, const GameplayPad& pad) {
	const uint32_t control = binding & kBindingControlNumberMask;
	if (control == PhysicalPadControl::Unbound || control > PhysicalPadControl::Y) return 0;
	if (control <= PhysicalPadControl::RightShoulder)
		return (pad.buttons & (XInputButtonMask::DpadUp <<
			(control - PhysicalPadControl::Up))) != 0;
	if (control <= PhysicalPadControl::RightTrigger)
		return pad.triggers[control - PhysicalPadControl::LeftTrigger];
	if (control <= PhysicalPadControl::RightStickY)
		return pad.sticks[control - PhysicalPadControl::LeftStickX];
	return (pad.buttons & (XInputButtonMask::A <<
		(control - PhysicalPadControl::A))) != 0;
}

int32_t ReadGameplayAction(PPCContext& ctx, uint8_t* base,
		uint32_t input_manager, uint32_t player, uint32_t action_id,
		uint32_t return_address, std::array<GameplayPad, kControllerPortCount>& pads) {
	const uint32_t player_data = input_manager + kInputManagerPlayerDataOffset +
		player * kPlayerDataStride;
	const uint32_t bindings = REX_LOAD_U32(player_data + kPlayerBindingsPointerOffset);
	const uint32_t binding_count = REX_LOAD_U32(player_data + kPlayerBindingCountOffset);
	const uint32_t actions = REX_LOAD_U32(player_data + kPlayerActionsPointerOffset);
	if (bindings == 0 || actions == 0) return 0;

	for (uint32_t i = 0; i < binding_count; ++i) {
		const uint32_t binding_data = bindings + i * kBindingStride;
		if (REX_LOAD_U32(binding_data + kBindingActionIdOffset) != action_id) continue;
		const uint32_t binding = REX_LOAD_U32(binding_data + kBindingControlOffset);
		if (NativeKeyboardVirtualKey(binding) == 0) {
			if ((binding >> kBindingTypeShift) == kXInputBindingType &&
				player < pads.size() &&
				PollGameplayPad(ctx, base, player, return_address, pads[player])) {
				return ReadPadControl(binding, pads[player]);
			}
			return static_cast<int32_t>(REX_LOAD_U32(
				actions + i * kActionStride + kActionValueOffset));
		}
		if (QueryKeyboard(ctx, base, kPrivateKeyboardDispatchFlag,
				player, action_id, binding, return_address) != 0) return 1;

		for (uint32_t duplicate = i + 1; duplicate < binding_count; ++duplicate) {
			const uint32_t duplicate_data = bindings + duplicate * kBindingStride;
			if (REX_LOAD_U32(duplicate_data + kBindingActionIdOffset) != action_id)
				continue;
			const uint32_t duplicate_binding = REX_LOAD_U32(
				duplicate_data + kBindingControlOffset);
			if (NativeKeyboardVirtualKey(duplicate_binding) == 0) continue;
			if (QueryKeyboard(ctx, base, kPrivateKeyboardDispatchFlag,
					player, action_id, duplicate_binding, return_address) != 0) return 1;
		}
		return 0;
	}
	return 0;
}

void RefreshGameplayInput(PPCContext& ctx, uint8_t* base,
		uint32_t input_manager, uint32_t config_slot,
		uint32_t gameplay_input, uint32_t return_address) {
	const uint32_t config = REX_LOAD_U32(config_slot);
	if (input_manager == 0 || config == 0) return;

	const PPCContext saved_ctx = ctx;
	std::array<GameplayPad, kControllerPortCount> pads{};
	for (uint32_t gameplay_player = 0; gameplay_player < kGameplayPlayerCount; ++gameplay_player) {
		const uint32_t player = REX_LOAD_U32(config + kConfiguredPlayerMapOffset +
			gameplay_player * kConfiguredPlayerMapStride);
		if (player >= kControllerPortCount) continue;
		const uint32_t shared_actions =
			kSharedActionBaseId + gameplay_player * kSharedActionPlayerStride;
		const uint32_t player_config = config + gameplay_player * kPlayerConfigStride;
		const auto action = [&](uint32_t action_player, uint32_t id) {
			return ReadGameplayAction(ctx, base, input_manager, action_player,
				id, return_address, pads);
		};
		const auto configured = [&](ConfiguredBindingOffset::Offset offset) {
			return action(player, REX_LOAD_U32(player_config + offset)) != 0;
		};
		const auto button_pressed = [&](
			ConfiguredBindingOffset::Offset set2_offset, uint32_t shared_offset,
			ConfiguredBindingOffset::Offset set1_offset) {
			return configured(set2_offset) ||
				action(kSharedActionPort, shared_actions + shared_offset) ||
				configured(set1_offset);
		};

		uint32_t mask = 0;
		if (action(player, GameActionId::AlternateUp) ||
			action(kSharedActionPort, shared_actions + SharedActionOffset::Up) ||
			action(player, GameActionId::Up) ||
			action(player, GameActionId::LeftStickY) >= kDigitalStickThreshold) {
			mask |= GameplayMask::Up;
		}
		if (action(kSharedActionPort, shared_actions + SharedActionOffset::Down) ||
			action(player, GameActionId::Down) ||
			action(player, GameActionId::LeftStickY) <= -kDigitalStickThreshold) {
			mask |= GameplayMask::Down;
		}
		if (action(kSharedActionPort, shared_actions + SharedActionOffset::Left) ||
			action(player, GameActionId::Left) ||
			action(player, GameActionId::LeftStickX) <= -kDigitalStickThreshold) {
			mask |= GameplayMask::Left;
		}
		if (action(player, GameActionId::AlternateRight) ||
			action(kSharedActionPort, shared_actions + SharedActionOffset::Right) ||
			action(player, GameActionId::Right) ||
			action(player, GameActionId::LeftStickX) >= kDigitalStickThreshold) {
			mask |= GameplayMask::Right;
		}
		if (button_pressed(ConfiguredBindingOffset::Set2Button1,
				SharedActionOffset::Button1, ConfiguredBindingOffset::Set1Button1)) {
			mask |= GameplayMask::PrimaryAction;
		}
		if (button_pressed(ConfiguredBindingOffset::Set2Button2,
				SharedActionOffset::Button2, ConfiguredBindingOffset::Set1Button2)) {
			mask |= GameplayMask::Button2;
		}
		if (button_pressed(ConfiguredBindingOffset::Set2Button3,
				SharedActionOffset::Button3, ConfiguredBindingOffset::Set1Button3)) {
			mask |= GameplayMask::Button3;
		}
		if (button_pressed(ConfiguredBindingOffset::Set2Button4,
				SharedActionOffset::Button4, ConfiguredBindingOffset::Set1Button4) ||
			action(player, GameActionId::Button4Fallback)) {
			mask |= GameplayMask::Button4;
		}

		REX_STORE_U8(gameplay_input + kGameplayInputMaskOffset +
			gameplay_player * kGameplayInputPlayerStride, mask);
	}
	ctx = saved_ctx;
	ctx.fpscr.setcsr(ctx.fpscr.csr);
}

}
#endif

#if defined(LAG_PATCH_RENDER)
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace {

using render_patch::kStartRenderEarly;
using render_patch::kWaitForRender;
using render_patch::kEndRenderEarly;

using render_patch::kGameplayController;
constexpr uint32_t kGameplayModule = render_patch::GameplayModule(kArrangeRender);
constexpr uint32_t kGameFrame = 0x82633748;
constexpr uint32_t kRenderCallback = render_patch::RenderCallback(kArrangeRender);
constexpr uint32_t kRenderCallCount = kArrangeRender ? 0x886BC734 : 0x886B420C;
constexpr uint32_t kCountdown = kArrangeRender ? 0x888816CC : 0x888791A4;
constexpr uint32_t kHelperGate = kArrangeRender ? 0x888816D8 : 0x888791B0;
constexpr uint32_t kSwapState = kArrangeRender ? 0x888816E0 : 0x888791B8;
constexpr uint32_t kPublishedReady = kArrangeRender ? 0x888816E8 : 0x888791C0;
constexpr uint32_t kPublishedBuffer = kArrangeRender ? 0x888816EC : 0x888791C4;
constexpr uint32_t kPublishedStatus = kArrangeRender ? 0x888816F0 : 0x888791C8;
constexpr uint32_t kGraphics = kArrangeRender ? 0x88881724 : 0x888791FC;
constexpr uint32_t kSessionState = kArrangeRender ? 0x886B6620 : 0x886ACB68;
constexpr uint32_t kPlayState = kArrangeRender ? 0x886B6708 : 0x886ACC50;
constexpr uint32_t kSwapBusy = kArrangeRender ? 0x886FD214 : 0x886F4CEC;
constexpr uint32_t kSwapSerial = kArrangeRender ? 0x886FD218 : 0x886F4CF0;
constexpr uint32_t kActiveWriter = kArrangeRender ? 0x886FD258 : 0x886F4D30;
constexpr uint32_t kBufferA = kArrangeRender ? 0x886FD22C : 0x886F4D04;
constexpr uint32_t kBufferB = kArrangeRender ? 0x886FD238 : 0x886F4D10;

constexpr uint32_t kControllerStateOffset = 0x44;
using render_patch::kArrangeModeOffset;
constexpr uint32_t kGraphicsCurrentGateOffset = 0x1D8;
using render_patch::kModuleRenderReadyOffset;
constexpr uint32_t kModuleRenderFunctionOffset = 0x14;

constexpr uint32_t kRequiredControllerState = 13;
using render_patch::kRequiredArrangeValue;
constexpr uint32_t kRequiredSessionState = 7;
constexpr uint32_t kRequiredPlayState = 4;
constexpr uint32_t kPublishedStatusReadyMask = 0x10;
constexpr uint32_t kNoActiveWriter = 0xFFFFFFFF;
constexpr uint32_t kMinSlowdownDepth = 1;
constexpr uint32_t kMaxSlowdownDepth = 4;
constexpr auto kRenderWaitTimeout = std::chrono::milliseconds{8};
constexpr uint32_t kCancelCooldownFrames = 60;

constexpr uint32_t kExpectedRenderCallback = render_patch::ExpectedRenderCallback(kArrangeRender);
constexpr uint32_t kRenderHelperReturn = kArrangeRender ? 0x88051330 : 0x880513B8;

enum class EarlyRenderState : uint8_t {
	None,
	FirstSwap,
	FrameRequested,
	FrameStarted,
	FrameReady,
	Draining,
	Cancelled,
};

struct EarlyRenderFrame {
	uint64_t frame_id = 0;
	uint64_t module_version = 1;
	uint64_t scene_version = 0;
	uint64_t early_call_id = 0;
	uint32_t game_frame = 0;
	uint32_t module_handle = 0;
	uint32_t render_function = 0;
	uint32_t old_swap_count = 0;
	uint32_t finished_buffer = 0;
	uint32_t next_slowdown_depth = 0;
	bool first_swap = false;
};

struct MovedRenderCalls {
	uint64_t early_call_id = 0;
	uint32_t game_frame = 0;
	bool helper_done = false;
	bool wait_updated = false;
	bool render_call_added = false;
};

std::mutex early_render_lock;
std::condition_variable render_frame_ready;
EarlyRenderState early_render_state = EarlyRenderState::None;
EarlyRenderFrame early_render_frame;
MovedRenderCalls moved_render_calls;
uint64_t next_early_frame_id = 0;
uint64_t next_early_call_id = 0;
uint64_t module_version = 1;
uint64_t scene_version = 0;
bool was_in_gameplay = false;
bool buffers_synced = false;
bool game_is_waiting = false;
uint32_t cancel_frames_left = 0;
uint32_t game_wait_depth = 0;
uint32_t render_calls_before_wait = 0;

struct ActiveGameInfo {
	bool valid = false;
	uint32_t module_handle = 0;
	uint32_t render_function = 0;
};

ActiveGameInfo GetActiveGameInfo(uint8_t* base) {
	ActiveGameInfo result;
	if (REX_LOAD_U32(kGameplayController + kControllerStateOffset) != kRequiredControllerState ||
		(REX_LOAD_U32(kGameplayController + kArrangeModeOffset) == kRequiredArrangeValue) != kArrangeRender ||
		REX_LOAD_U32(kRenderCallback) != kExpectedRenderCallback ||
		REX_LOAD_U32(kSessionState) != kRequiredSessionState ||
		REX_LOAD_U32(kPlayState) != kRequiredPlayState ||
		(REX_LOAD_U32(kPublishedStatus) & kPublishedStatusReadyMask) == 0 ||
		REX_LOAD_U32(kActiveWriter) == 0 ||
		REX_LOAD_U32(kSwapBusy) != 0) {
		return result;
	}

	const uint32_t graphics = REX_LOAD_U32(kGraphics);
	if (graphics == 0 || REX_LOAD_U32(graphics + kGraphicsCurrentGateOffset) != 0) return result;

	result.module_handle = REX_LOAD_U32(kGameplayModule);
	if (result.module_handle == 0 || REX_LOAD_U32(result.module_handle + kModuleRenderReadyOffset) == 0) {
		return result;
	}
	result.render_function = REX_LOAD_U32(result.module_handle + kModuleRenderFunctionOffset);
	if (result.render_function == 0) return result;
	result.valid = true;
	return result;
}

bool IsSameGame(const ActiveGameInfo& game, uint32_t module_handle, uint32_t render_function) {
	return game.valid && game.module_handle == module_handle &&
		game.render_function == render_function;
}

bool IsSameEarlyCall(uint8_t* base, uint64_t early_call_id) {
	return moved_render_calls.early_call_id == early_call_id &&
		moved_render_calls.game_frame == REX_LOAD_U32(kGameFrame);
}

void UpdateGameplayState(bool in_gameplay) {
	if (in_gameplay == was_in_gameplay) return;
	was_in_gameplay = in_gameplay;
	++scene_version;
	buffers_synced = false;
	if (in_gameplay && early_render_state == EarlyRenderState::Cancelled) {
		early_render_state = EarlyRenderState::None;
	}
}

bool CanRunRenderEarly(uint8_t* base, uint32_t countdown) {
	if (!game_is_waiting || game_wait_depth < kMinSlowdownDepth ||
		game_wait_depth > kMaxSlowdownDepth || countdown < kMinSlowdownDepth ||
		countdown > game_wait_depth) {
		return false;
	}
	const uint32_t render_calls = REX_LOAD_U32(kRenderCallCount) - render_calls_before_wait;
	return render_calls == game_wait_depth - countdown;
}

uint32_t GetFinishedRenderBuffer(uint8_t* base, uint32_t old_swap_count) {
	return REX_LOAD_U32((old_swap_count & 1) != 0 ? kBufferA : kBufferB);
}

bool RenderBufferWasSwapped(uint8_t* base, uint32_t old_swap_count, uint32_t finished_buffer) {
	return REX_LOAD_U32(kSwapSerial) == old_swap_count + 1 &&
		REX_LOAD_U32(kPublishedBuffer) == finished_buffer &&
		REX_LOAD_U32(kActiveWriter) != finished_buffer &&
		REX_LOAD_U32(kActiveWriter) != 0 &&
		REX_LOAD_U32(kActiveWriter) != kNoActiveWriter &&
		REX_LOAD_U32(kPublishedReady) != 0 &&
		(REX_LOAD_U32(kPublishedStatus) & kPublishedStatusReadyMask) != 0;
}

void CancelEarlyRender() {
	early_render_state = EarlyRenderState::Cancelled;
	cancel_frames_left = kCancelCooldownFrames;
	buffers_synced = false;
	render_frame_ready.notify_all();
}

bool UseMovedRenderFlag(uint8_t* base, bool MovedRenderCalls::*flag) {
	std::lock_guard lock(early_render_lock);
	if (moved_render_calls.game_frame != REX_LOAD_U32(kGameFrame) ||
		!(moved_render_calls.*flag)) {
		return false;
	}
	moved_render_calls.*flag = false;
	return true;
}

bool StartRenderEarly(PPCContext& ctx, uint8_t* base) {
	const ActiveGameInfo active_game = GetActiveGameInfo(base);
	bool run_helper = false;
	uint64_t early_call_id = 0;
	{
		std::lock_guard lock(early_render_lock);
		UpdateGameplayState(active_game.valid);
		if (early_render_state == EarlyRenderState::Cancelled && cancel_frames_left != 0 &&
			--cancel_frames_left == 0) {
			early_render_state = EarlyRenderState::None;
		}
		moved_render_calls = {};
		early_call_id = ++next_early_call_id;
		if (!active_game.valid || early_render_state != EarlyRenderState::None) return false;

		const uint32_t countdown = REX_LOAD_U32(kCountdown);
		if (!CanRunRenderEarly(base, countdown)) return false;

		moved_render_calls.early_call_id = early_call_id;
		moved_render_calls.game_frame = REX_LOAD_U32(kGameFrame);
		run_helper = REX_LOAD_U32(kHelperGate) == 0;
		moved_render_calls.helper_done = run_helper;
	}

	if (run_helper) {
		ctx.lr = kRenderHelperReturn;
		sub_88036798(ctx, base);
	}

	std::unique_lock lock(early_render_lock);
	const ActiveGameInfo checked_game = GetActiveGameInfo(base);
	const uint32_t countdown = REX_LOAD_U32(kCountdown);
	if (!IsSameGame(checked_game, active_game.module_handle, active_game.render_function) ||
		!IsSameEarlyCall(base, early_call_id) ||
		early_render_state != EarlyRenderState::None || !CanRunRenderEarly(base, countdown)) {
		return moved_render_calls.helper_done;
	}

	if (countdown > kMinSlowdownDepth) {
		REX_STORE_U32(kCountdown, countdown - 1);
		moved_render_calls.wait_updated = true;
		moved_render_calls.render_call_added = true;
		REX_STORE_U32(kRenderCallCount, REX_LOAD_U32(kRenderCallCount) + 1);
		return true;
	}

	const bool first_swap = !buffers_synced;
	uint32_t first_swap_count = 0;
	uint32_t first_finished_buffer = 0;
	if (first_swap) {
		first_swap_count = REX_LOAD_U32(kSwapSerial);
		first_finished_buffer = GetFinishedRenderBuffer(base, first_swap_count);
		early_render_state = EarlyRenderState::FirstSwap;
		lock.unlock();
		ctx.lr = kStartRenderEarly;
		SwapRenderBuffer(ctx, base);
		lock.lock();
		if (early_render_state != EarlyRenderState::FirstSwap ||
			!RenderBufferWasSwapped(base, first_swap_count, first_finished_buffer)) {
			early_render_state = EarlyRenderState::None;
			return moved_render_calls.helper_done;
		}
	}

	const ActiveGameInfo current_game = GetActiveGameInfo(base);
	if (!IsSameGame(current_game, active_game.module_handle, active_game.render_function) ||
		!IsSameEarlyCall(base, early_call_id) ||
		!CanRunRenderEarly(base, kMinSlowdownDepth)) {
		early_render_state = EarlyRenderState::None;
		return moved_render_calls.helper_done;
	}

	REX_STORE_U32(kCountdown, 0);
	REX_STORE_U32(kSwapState, 0);
	early_render_frame = {};
	early_render_frame.frame_id = ++next_early_frame_id;
	early_render_frame.module_version = module_version;
	early_render_frame.scene_version = scene_version;
	early_render_frame.early_call_id = early_call_id;
	early_render_frame.game_frame = moved_render_calls.game_frame;
	early_render_frame.module_handle = active_game.module_handle;
	early_render_frame.render_function = active_game.render_function;
	early_render_frame.old_swap_count = REX_LOAD_U32(kSwapSerial);
	early_render_frame.first_swap = first_swap;
	early_render_state = EarlyRenderState::FrameRequested;
	moved_render_calls.wait_updated = true;
	moved_render_calls.render_call_added = true;
	REX_STORE_U32(kRenderCallCount, REX_LOAD_U32(kRenderCallCount) + 1);
	return true;
}

bool WaitForRenderAndSwapBuffer(PPCContext& ctx, uint8_t* base) {
	std::unique_lock lock(early_render_lock);
	if ((early_render_state != EarlyRenderState::FrameRequested &&
		early_render_state != EarlyRenderState::FrameStarted &&
		early_render_state != EarlyRenderState::FrameReady) ||
		early_render_frame.early_call_id != moved_render_calls.early_call_id ||
		early_render_frame.game_frame != REX_LOAD_U32(kGameFrame)) {
		return false;
	}

	const uint64_t frame_id = early_render_frame.frame_id;
	const bool frame_finished = render_frame_ready.wait_for(
		lock, kRenderWaitTimeout, [&] {
		return early_render_frame.frame_id != frame_id ||
			early_render_state == EarlyRenderState::FrameReady ||
			early_render_state == EarlyRenderState::Draining ||
			early_render_state == EarlyRenderState::Cancelled;
	});
	if (!frame_finished && early_render_frame.frame_id == frame_id &&
		(early_render_state == EarlyRenderState::FrameRequested ||
		 early_render_state == EarlyRenderState::FrameStarted)) {
		early_render_state = EarlyRenderState::Draining;
		buffers_synced = false;
	}
	if (early_render_frame.frame_id != frame_id ||
		early_render_state != EarlyRenderState::FrameReady) {
		return false;
	}

	const EarlyRenderFrame finished_frame = early_render_frame;
	const ActiveGameInfo active_game = GetActiveGameInfo(base);
	if (!IsSameGame(active_game, finished_frame.module_handle, finished_frame.render_function) ||
		finished_frame.module_version != module_version || finished_frame.scene_version != scene_version ||
		finished_frame.early_call_id != moved_render_calls.early_call_id ||
		finished_frame.game_frame != REX_LOAD_U32(kGameFrame) ||
		finished_frame.next_slowdown_depth < kMinSlowdownDepth ||
		finished_frame.next_slowdown_depth > kMaxSlowdownDepth ||
		REX_LOAD_U32(kActiveWriter) != kNoActiveWriter ||
		REX_LOAD_U32(kSwapSerial) != finished_frame.old_swap_count ||
		(!finished_frame.first_swap && REX_LOAD_U32(kPublishedReady) != 0)) {
		CancelEarlyRender();
		return false;
	}

	lock.unlock();
	ctx.lr = kWaitForRender;
	SwapRenderBuffer(ctx, base);
	lock.lock();
	const bool buffer_swapped = early_render_frame.frame_id == frame_id &&
		early_render_state == EarlyRenderState::FrameReady &&
		RenderBufferWasSwapped(base, finished_frame.old_swap_count, finished_frame.finished_buffer);
	if (buffer_swapped) {
		early_render_state = EarlyRenderState::None;
		buffers_synced = true;
	} else {
		CancelEarlyRender();
	}
	return buffer_swapped;
}

void EndEarlyRender() {
	std::lock_guard lock(early_render_lock);
	moved_render_calls = {};
	if (early_render_state == EarlyRenderState::FrameRequested ||
		early_render_state == EarlyRenderState::FrameStarted) {
		early_render_state = EarlyRenderState::Draining;
		buffers_synced = false;
	} else if (early_render_state == EarlyRenderState::FrameReady) {
		early_render_state = EarlyRenderState::None;
		buffers_synced = false;
	}
	render_frame_ready.notify_all();
}

void ResetEarlyRender() {
	if (!patch_flags::render_enabled()) return;
	std::lock_guard lock(early_render_lock);
	++module_version;
	++scene_version;
	was_in_gameplay = false;
	buffers_synced = false;
	game_is_waiting = false;
	game_wait_depth = 0;
	render_calls_before_wait = 0;
	moved_render_calls = {};
	early_render_state = EarlyRenderState::Cancelled;
	render_frame_ready.notify_all();
}

}

bool RenderHelperWasDoneEarly(uint8_t* base) {
	return UseMovedRenderFlag(base, &MovedRenderCalls::helper_done);
}

bool RenderWaitWasUpdatedEarly(uint8_t* base) {
	return UseMovedRenderFlag(base, &MovedRenderCalls::wait_updated);
}

bool RenderCallWasAddedEarly(uint8_t* base) {
	return UseMovedRenderFlag(base, &MovedRenderCalls::render_call_added);
}

uint64_t GameFrameStarted() {
	std::lock_guard lock(early_render_lock);
	if ((early_render_state != EarlyRenderState::FrameRequested &&
		early_render_state != EarlyRenderState::Draining) ||
		early_render_frame.module_version != module_version) return 0;
	if (early_render_state == EarlyRenderState::FrameRequested)
		early_render_state = EarlyRenderState::FrameStarted;
	return early_render_frame.frame_id;
}

void RenderBufferReady(uint8_t* base, uint64_t frame_id, uint32_t next_slowdown_depth) {
	std::lock_guard lock(early_render_lock);
	if (frame_id == 0 || frame_id != early_render_frame.frame_id ||
		early_render_frame.module_version != module_version) return;
	if (early_render_state == EarlyRenderState::Draining) {
		early_render_state = EarlyRenderState::None;
		buffers_synced = false;
		render_frame_ready.notify_all();
		return;
	}
	if (early_render_state != EarlyRenderState::FrameStarted) return;
	early_render_frame.finished_buffer =
		GetFinishedRenderBuffer(base, early_render_frame.old_swap_count);
	early_render_frame.next_slowdown_depth = next_slowdown_depth;
	early_render_state = EarlyRenderState::FrameReady;
	render_frame_ready.notify_all();
}

void GameWaitStarted(uint32_t depth, uint32_t render_call_count) {
	std::lock_guard lock(early_render_lock);
	game_is_waiting = true;
	game_wait_depth = depth;
	render_calls_before_wait = render_call_count;
}

void GameWaitEnded() {
	std::lock_guard lock(early_render_lock);
	game_is_waiting = false;
}
#endif
