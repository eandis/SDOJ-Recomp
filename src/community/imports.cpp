#include "saidaioujou_recomp_tu1_init.h"
#include "community/guest_service.h"
#include "community/native_service.h"

#include <rex/hook.h>
#include <rex/string.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xenumerator.h>
#include <rex/system/xevent.h>
#include <rex/system/xnotifylistener.h>
#include <rex/system/xthread.h>
#include <algorithm>
#include <cstddef>
#include <cstring>

DECLARE_REX_FUNC(__imp__XamShowMessageBoxUI);

namespace {
using rex::X_RESULT;
using namespace sdoj::xam;
using namespace rex::system;

// Workspace for the stock Xbox message box. Its ninth PPC argument is read
// from the call frame at +0x54; a zero value makes the dialog synchronous.
struct CardDialog {
  uint8_t call_frame[0x80];
  rex::be<uint32_t> button;
  rex::be<uint32_t> result;
  rex::be<uint16_t> title[12];
  rex::be<uint16_t> close[6];
};
}  // namespace

// Sign-in and privilege checks use local profile state and issue no HTTP requests.
REX_HOOK_RAW(Sdoj_XamUserGetSigninState) {
  if (ctx.r3.u32 == 0 && GetCommunityService()) {
    ctx.r3.u64 = 2;  // XUserSigninState_SignedInToLive
    return;
  }
  __imp__XamUserGetSigninState(ctx, base);
}

REX_HOOK_RAW(Sdoj_XamUserGetSigninInfo) {
  const auto info = ctx.r5.u32;
  __imp__XamUserGetSigninInfo(ctx, base);
  if (ctx.r3.u32 != 0)
    return;
  if (auto service = GetCommunityService()) {
    rex::memory::store_and_swap<uint32_t>(base + info + 8, 1);  // LIVE_ENABLED
    rex::memory::store_and_swap<uint32_t>(base + info + 12, 2);
    auto name =
        cave::community::NativeLegacyName(service->DisplayName(cave::community::kNativeCommunityLocalXuid));
    name.resize(16, '\0');
    name.back() = '\0';
    std::memcpy(base + info + 24, name.data(), name.size());
  }
}

REX_HOOK_RAW(Sdoj_XamUserCheckPrivilege) {
  const auto privilege = ctx.r4.u32;
  const auto out = ctx.r5.u32;
  __imp__XamUserCheckPrivilege(ctx, base);
  if (ctx.r3.u32 == 0 && out) {
    auto service = GetCommunityService();
    if (service && service->AllowsPrivilege(privilege))
      rex::memory::store_and_swap<uint32_t>(base + out, 1);
  }
}

REX_HOOK_RAW(Sdoj_XamNotifyCreateListener) {
  const auto mask = ctx.r3.u64;
  __imp__XamNotifyCreateListener(ctx, base);
  if ((mask & 2) && GetCommunityService()) {
    auto listener = kernel_state()->object_table()->LookupObject<XNotifyListener>(ctx.r3.u32);
    if (listener)
      listener->EnqueueNotification(0x02000001, 0x001510F0);
  }
}

REX_HOOK_RAW(Sdoj_NetDll_XNetQosServiceLookup) {
  const auto out = ctx.r6.u32;
  if (!out || !GetCommunityService()) {
    __imp__NetDll_XNetQosServiceLookup(ctx, base);
    return;
  }
  // Signal only after the stock allocation contains our service result.
  const auto event = ctx.r5;
  ctx.r5.u64 = 0;
  __imp__NetDll_XNetQosServiceLookup(ctx, base);
  ctx.r5 = event;
  const auto qos = rex::memory::load_and_swap<uint32_t>(base + out);
  if (ctx.r3.u32 == 0 && qos) {
    rex::memory::store_and_swap<uint32_t>(base + qos, 1);
    base[qos + 8] = 3;  // XNQOSINFO: complete and contacted
  }
  if (event.u32) {
    auto signal = kernel_state()->object_table()->LookupObject<XEvent>(event.u32);
    if (signal)
      signal->Set(0, false);
  }
}

REX_HOOK_RAW(Sdoj_XamUserCreateStatsEnumerator) {
  if (!GetCommunityService()) {
    __imp__XamUserCreateStatsEnumerator(ctx, base);
    return;
  }
  ctx.r3.u64 = CreateCommunityStatsEnumerator(kernel_state(), ctx.r3.u32, ctx.r4.u32, ctx.r5.u64, ctx.r6.u32,
                                              ctx.r7.u32, ctx.r8.u32, ctx.r9.u32, ctx.r10.u32);
}

REX_HOOK_RAW(Sdoj_XamEnumerate) {
  auto* kernel = kernel_state();
  if (GetCommunityService() && !ctx.r4.u32) {
    auto enumerator = kernel->object_table()->LookupObject<XEnumerator>(ctx.r3.u32);
    uint32_t count = 0;
    if (auto result = EnumerateCommunityStats(kernel, enumerator.get(), ctx.r5.u32, ctx.r6.u32, ctx.r8.u32,
                                              ctx.r8.u32 ? nullptr : &count)) {
      if (!ctx.r8.u32 && ctx.r7.u32)
        rex::memory::store_and_swap<uint32_t>(base + ctx.r7.u32, count);
      ctx.r3.u64 = *result;
      return;
    }
  }
  __imp__XamEnumerate(ctx, base);
}

REX_HOOK_RAW(Sdoj_XMsgStartIORequest) {
  if (auto result = DispatchCommunityMessage(kernel_state(), ctx.r3.u32, ctx.r4.u32, ctx.r6.u32, ctx.r7.u32,
                                             ctx.r5.u32)) {
    if (!ctx.r5.u32)
      XThread::SetLastError(*result & 0xffff);
    ctx.r3.u64 = *result;
    return;
  }
  __imp__XMsgStartIORequest(ctx, base);
}

REX_HOOK_RAW(Sdoj_XMsgInProcessCall) {
  if (auto result =
          DispatchCommunityMessage(kernel_state(), ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, 0)) {
    ctx.r3.u64 = *result;
    return;
  }
  __imp__XMsgInProcessCall(ctx, base);
}

REX_HOOK_RAW(Sdoj_XMsgCancelIORequest) {
  if (CancelCommunityIo(kernel_state(), ctx.r3.u32)) {
    ctx.r3.u64 = 0;
    return;
  }
  __imp__XMsgCancelIORequest(ctx, base);
}

REX_HOOK_RAW(Sdoj_XamShowGamerCardUIForXUID) {
  // Use the nickname from the local profile or the previously fetched leaderboard row.
  auto service = GetCommunityService();
  if (!service) {
    __imp__XamShowGamerCardUIForXUID(ctx, base);
    return;
  }
  auto name = service->DisplayName(ctx.r4.u64);
  if (ctx.r3.u32 || name.empty()) {
    ctx.r3.u64 = X_ERROR_NO_SUCH_USER;
    return;
  }
  const auto text = rex::string::to_utf16(name);
  auto* memory = kernel_state()->memory();
  const uint32_t size = sizeof(CardDialog) + (text.size() + 1) * sizeof(uint16_t);
  const auto address = memory->SystemHeapAlloc(size);
  if (!address) {
    ctx.r3.u64 = 8u;  // ERROR_NOT_ENOUGH_MEMORY
    return;
  }
  auto* layout = memory->TranslateVirtual<CardDialog*>(address);
  *layout = {};
  std::copy_n(u"PLAYER CARD", 11, layout->title);
  std::copy_n(u"CLOSE", 6, layout->close);
  auto* description = reinterpret_cast<rex::be<uint16_t>*>(layout + 1);
  std::copy(text.begin(), text.end(), description);
  description[text.size()] = 0;
  layout->button = address + offsetof(CardDialog, close);

  rex::CallFrame frame(ctx);
  auto& dialog = frame.ctx;
  dialog.r1.u64 = address;
  dialog.r3.u64 = 0;
  dialog.r4.u64 = address + offsetof(CardDialog, title);
  dialog.r5.u64 = address + sizeof(CardDialog);
  dialog.r6.u64 = 1;
  dialog.r7.u64 = address + offsetof(CardDialog, button);
  dialog.r8.u64 = 0;
  dialog.r9.u64 = 0;
  dialog.r10.u64 = address + offsetof(CardDialog, result);
  __imp__XamShowMessageBoxUI(dialog, base);
  memory->SystemHeapFree(address);
  ctx.r3 = dialog.r3;
}
