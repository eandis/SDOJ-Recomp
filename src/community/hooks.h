#pragma once
#include <rex/ppc.h>

bool CommunityEstimateRank(PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&);
bool CommunityUploadReplay(PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&);
bool CommunityDownloadReplay(PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&,
                             PPCRegister&, PPCRegister&);
bool CommunityLocalReplayPath(PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&,
                              PPCRegister&, PPCRegister&);
bool CommunityRemoteReplayPath(PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&,
                               PPCRegister&, PPCRegister&);
bool CommunityHandleLegacyAuth(PPCRegister& result);
