#!/usr/bin/env bash

set -u

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PREPARE_SCRIPT="$ROOT/prepare_game.sh"
EXECUTABLE="$ROOT/saidaioujou_recomp_tu1"

if [[ -t 1 ]]; then
    printf '\033]0;DoDonPachi Saidaioujou TU1 Recomp\007'
fi

"$PREPARE_SCRIPT" --prepare-only
setup_result=$?

if (( setup_result != 0 )); then
    printf '\nsetup failed. Fix the error above and try again.\n'

    if [[ -t 0 ]]; then
        read -r -p "Press Enter to close..."
    fi

    exit "$setup_result"
fi

cd -- "$ROOT" || exit 1

exec "$EXECUTABLE" \
    --input_backend=sdl \
    --vsync=false \
    --fullscreen=true \
    --video_mode_refresh_rate=60 \
    --xex_apply_patches=true \
    --game_data_root="$ROOT/game_data" \
    --user_data_root="$ROOT/user_data" \
    --input_patch=true \
    --render_patch=true