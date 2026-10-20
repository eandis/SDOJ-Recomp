#pragma once

#include <atomic>
#include <cstdint>

#if defined(_WIN32)
#define MUTEKI_HIT_EXPORT extern "C" __declspec(dllexport)
#else
#define MUTEKI_HIT_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace muteki_hit {

// normal mode only for now
constexpr uint32_t kNoDamageState = 0x886ACA10;
constexpr uint32_t kGameState = 0x886B4300;
constexpr uint32_t kNoDamageOffset = 1256;
constexpr uint32_t kPlayerDeadOffset = 116;
constexpr uint32_t kPlayerControlOffset = 112;
constexpr uint32_t kInvincibleTimerOffset = 624;
constexpr uint32_t kTimerObjectOffset = 640;
constexpr uint32_t kTimerOffset = 136;
constexpr uint32_t kDamageFlags = 0x8C18;

inline std::atomic<uint32_t> hits{0};

inline void Check(uint8_t* base, uint32_t flags, uint32_t player) {
	if (REX_LOAD_U32(kNoDamageState) != 0 ||
		REX_LOAD_U32(kGameState + kNoDamageOffset) != 0) {
		return;
	}
	if (REX_LOAD_U8(player + kPlayerDeadOffset) != 0) return;
	const uint32_t control = REX_LOAD_U32(player + kPlayerControlOffset);
	const uint32_t timers = REX_LOAD_U32(control + kTimerObjectOffset);
	if (REX_LOAD_U32(timers + kTimerOffset) + REX_LOAD_U32(control + kInvincibleTimerOffset) != 0) {
		return;
	}
	if ((flags & kDamageFlags) == 0) return;
	hits.fetch_add(1, std::memory_order_relaxed);
}

}

MUTEKI_HIT_EXPORT uint32_t muteki_hits() {
	return muteki_hit::hits.load(std::memory_order_relaxed);
}
