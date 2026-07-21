// saidaioujou_recomp_tu1 - ReXGlue Recompiled Project

#include "generated/default/saidaioujou_recomp_tu1_init.h"

#include "saidaioujou_recomp_tu1_app.h"

REXCVAR_DEFINE_BOOL(input_patch, true, "SDOJ",
                    "enable the input latency patch")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_BOOL(render_patch, true, "SDOJ",
                    "enable the render latency patch")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

REX_DEFINE_APP(saidaioujou_recomp_tu1, SaidaioujouRecompTu1App::Create)
