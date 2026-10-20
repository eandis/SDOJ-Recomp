// saidaioujou_recomp_tu1 - ReXGlue Recompiled Project

#include "generated/default/saidaioujou_recomp_tu1_init.h"

#include "saidaioujou_recomp_tu1_app.h"

REXCVAR_DEFINE_BOOL(input_patch, true, "SDOJ",
                    "enable the input latency patch")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_BOOL(render_patch, true, "SDOJ",
                    "enable the render latency patch")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_BOOL(debug, false, "SDOJ",
                    "enable the original in-game debug menu")
    .debug_only();
REXCVAR_DEFINE_BOOL(muteki_flash, true, "SDOJ",
                    "flash the screen red when you get hit with muteki on");
REXCVAR_DEFINE_BOOL(check_for_updates, true, "SDOJ",
                    "check GitHub for newer releases at startup");
REXCVAR_DEFINE_COMMAND(check_for_updates_now,
                       [] { update_checker::CheckNowFromSettings(); }, "SDOJ",
                       "check GitHub for a newer release right now");

REX_DEFINE_APP(saidaioujou_recomp_tu1, SaidaioujouRecompTu1App::Create)
