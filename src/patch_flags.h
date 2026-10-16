#pragma once

#include <rex/cvar.h>

namespace patch_flags {

inline bool input_enabled() {
  static const bool enabled = rex::cvar::GetStartupFlagByName("input_patch") == "true";
  return enabled;
}

inline bool render_enabled() {
  static const bool enabled = rex::cvar::GetStartupFlagByName("render_patch") == "true";
  return enabled;
}

}
