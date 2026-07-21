#pragma once

#include <rex/cvar.h>

namespace sdoj_patch_flags {
  
inline bool input_enabled() {
  static const bool enabled = REXCVAR_QUERY(bool, input_patch);
  return enabled;
}

inline bool render_enabled() {
  static const bool enabled = REXCVAR_QUERY(bool, render_patch);
  return enabled;
}

} 
