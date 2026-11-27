#include "community/hooks.h"
#include <rex/system.h>
#include "community/guest_service.h"

using namespace sdoj::xam;

bool CommunityEstimateRank(PPCRegister& r3, PPCRegister& r4, PPCRegister& r5, PPCRegister& r6,
                           PPCRegister& r7) {
  if (!GetCommunityService())
    return false;
  r3.u64 = StartCommunityRankIo(rex::system::kernel_state(), r3.u32, r4.u32, r5.u32, r6.u32, r7.u32);
  return true;
}

bool CommunityUploadReplay(PPCRegister& r3, PPCRegister& r4, PPCRegister& r5, PPCRegister& r6,
                           PPCRegister& r7) {
  if (!GetCommunityService())
    return false;
  r3.u64 =
      StartCommunityReplayIo(rex::system::kernel_state(), true, r3.u32, r4.u32, r5.u32, r6.u32, 0, 0, r7.u32);
  return true;
}

bool CommunityDownloadReplay(PPCRegister& r3, PPCRegister& r4, PPCRegister& r5, PPCRegister& r6,
                             PPCRegister& r7, PPCRegister& r8, PPCRegister& r9) {
  if (!GetCommunityService())
    return false;
  r3.u64 = StartCommunityReplayIo(rex::system::kernel_state(), false, r3.u32, r4.u32, r5.u32, r6.u32, r7.u32,
                                  r8.u32, r9.u32);
  return true;
}

bool CommunityLocalReplayPath(PPCRegister& r3, PPCRegister& r4, PPCRegister& r5, PPCRegister& r6,
                              PPCRegister& r7, PPCRegister& r8, PPCRegister& r9) {
  if (!GetCommunityService())
    return false;
  r3.u64 = BuildCommunityReplayPath(rex::system::kernel_state(), r3.u32, 0, r4.u32, r5.u32, r6.u32, r7.u32,
                                    r8.u32, r9.u32);
  return true;
}

bool CommunityRemoteReplayPath(PPCRegister& r3, PPCRegister& r4, PPCRegister& r5, PPCRegister& r6,
                               PPCRegister& r7, PPCRegister& r8, PPCRegister& r9) {
  if (!GetCommunityService())
    return false;
  r3.u64 = BuildCommunityReplayPath(rex::system::kernel_state(), 254, r3.u64, r4.u32, r5.u32, r6.u32, r7.u32,
                                    r8.u32, r9.u32);
  return true;
}

bool CommunityHandleLegacyAuth(PPCRegister& result) {
  if (!GetCommunityService())
    return false;
  // The title's HTTP worker handles a failed XAuth initialization and reports
  // it back to the caller. Team requests require an actual Xbox Live service.
  result.u64 = 0x80070032u;  // HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)
  return true;
}
