#!/usr/bin/env bash

set -Eeuo pipefail
shopt -s nullglob
if (( $# > 1 )); then
    printf 'error: unexpected arguments\n' >&2
    exit 1
fi

if (( $# == 1 )) && [[ "$1" != "--prepare-only" ]]; then
    printf 'error: unknown argument: %s\n' "$1" >&2
    exit 1
fi

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"

GAME_DATA="$ROOT/game_data"
USER_DATA="$ROOT/user_data"
EXTRACTING="$ROOT/game_data.extracting"
EXTRACTOR="$ROOT/tools/extract-xiso/extract-xiso"
EXECUTABLE="$ROOT/saidaioujou_recomp_tu1"

EXPECTED_TITLE_UPDATE_HASH='0A893048C761BA6CF8ED14637F326726BC616B5183CA1EE52DE02FC12B46B215'

BASE_FILES=(
    'default.xex|709EB9FD2E4446E5754461616693B3BBB8B78FEF5AA613FFCDE6D643DA0133F5'
    'CA022100.bin|A8454BB24A0C0F0A0E5C89C8885041DEE6411E2C688874A5E2A57966174939BE'
    'CA022110.bin|4C08B646140E4887AC2036208195E99DFDB4AE32FC7D1571D589B6657791BC75'
)

PATCH_FILES=(
    'CA022100.binp|53248|126976|830EE751922483115F735F442A23B0AB203468DE17E509DA6ED08B25BE25DC4F'
    'CA022110.binp|180224|118784|B0FD71B07A0DFAE8E834A08A2EF7BD064B44492A7B50854B2C9245AB48266C00'
    'default.xexp|299008|299008|79C6958470749931040BA2B68AED268A64CDAEED03F788EEAFFDABD45BA7EBBB'
)

TEMP_PAYLOAD=''

cleanup() {
    if [[ -n "$TEMP_PAYLOAD" && -e "$TEMP_PAYLOAD" ]]; then
        rm -f -- "$TEMP_PAYLOAD"
    fi
}

trap cleanup EXIT

die() {
    printf '\nerror: %s\n' "$*" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 ||
        die "Required command is missing: $1"
}

get_file_sha256() {
    local path="$1"

    sha256sum -- "$path" |
        awk '{ print toupper($1) }'
}

test_base_files() {
    local directory="$1"
    local entry
    local name
    local expected_hash
    local actual_hash

    for entry in "${BASE_FILES[@]}"; do
        IFS='|' read -r name expected_hash <<< "$entry"

        [[ -f "$directory/$name" ]] || return 1

        actual_hash="$(get_file_sha256 "$directory/$name")"
        [[ "$actual_hash" == "$expected_hash" ]] || return 1
    done

    return 0
}

test_patch_files() {
    local directory="$1"
    local entry
    local name
    local offset
    local length
    local expected_hash
    local actual_hash

    for entry in "${PATCH_FILES[@]}"; do
        IFS='|' read -r name offset length expected_hash <<< "$entry"

        [[ -f "$directory/$name" ]] || return 1

        actual_hash="$(get_file_sha256 "$directory/$name")"
        [[ "$actual_hash" == "$expected_hash" ]] || return 1
    done

    return 0
}

remove_extraction_temporary_directory() {
    local expected

    [[ -e "$EXTRACTING" ]] || return 0

    expected="$ROOT/game_data.extracting"

    if [[ "$EXTRACTING" != "$expected" || "$EXTRACTING" != "$ROOT/"* ]]; then
        die "Won't remove unexpected path: $EXTRACTING"
    fi

    rm -rf -- "$EXTRACTING"
}

printf '\033[36mSDOJ recomp setup\033[0m\n\n'

require_command sha256sum
require_command awk
require_command find
require_command stat
require_command dd
require_command mktemp

[[ -f "$EXTRACTOR" ]] ||
    die 'extract-xiso is missing. Re-extract the release and try again.'

if [[ ! -x "$EXTRACTOR" ]]; then
    chmod +x -- "$EXTRACTOR" ||
        die 'Could not make extract-xiso executable.'
fi

[[ -f "$EXECUTABLE" ]] ||
    die 'The Linux recomp executable is missing. Re-extract the release and try again.'

if [[ ! -x "$EXECUTABLE" ]]; then
    chmod +x -- "$EXECUTABLE" ||
        die 'Could not make the recomp executable executable.'
fi

if test_base_files "$GAME_DATA"; then
    printf '\033[32mgame_data looks good. Skipping ISO extraction.\033[0m\n'
else
    if [[ -e "$GAME_DATA" ]]; then
        [[ -d "$GAME_DATA" ]] ||
            die 'game_data exists but is not a directory.'

        existing_item="$(
            find "$GAME_DATA" -mindepth 1 -maxdepth 1 -print -quit
        )"

        if [[ -n "$existing_item" ]]; then
            die 'game_data is incomplete or from the wrong ISO. Delete it and try again.'
        fi

        rmdir -- "$GAME_DATA"
    fi

    iso_files=("$ROOT"/*.iso)

    if (( ${#iso_files[@]} == 0 )); then
        die 'No ISO found. Put your SDOJ ISO next to launch.sh and try again.'
    fi

    if (( ${#iso_files[@]} > 1 )); then
        die 'Found more than one ISO. Leave only the SDOJ ISO here and try again.'
    fi

    remove_extraction_temporary_directory
    mkdir -- "$EXTRACTING"

    printf '\033[33mExtracting %s. This only happens on the first launch...\033[0m\n' \
        "$(basename -- "${iso_files[0]}")"

    if ! "$EXTRACTOR" \
        -q \
        -x \
        -s \
        -d "$EXTRACTING" \
        "${iso_files[0]}"; then
        extractor_result=$?
        die "ISO extraction failed with error $extractor_result."
    fi

    if ! test_base_files "$EXTRACTING"; then
        die "That ISO doesn't match the supported SDOJ dump."
    fi

    mv -- "$EXTRACTING" "$GAME_DATA"

    printf '\033[32mISO extracted and verified.\033[0m\n'
fi

if test_patch_files "$GAME_DATA"; then
    printf '\033[32mTU1 is already set up.\033[0m\n'
else
    mapfile -d '' -t tu_candidates < <(
        find "$ROOT" \
            -maxdepth 1 \
            -type f \
            \( -name 'TU_*' -o -size 598016c \) \
            -print0
    )

    title_update=''

    for candidate in "${tu_candidates[@]}"; do
        if [[ "$(get_file_sha256 "$candidate")" == "$EXPECTED_TITLE_UPDATE_HASH" ]]; then
            title_update="$candidate"
            break
        fi
    done

    if [[ -z "$title_update" ]]; then
        die 'TU1 file not found. Put the TU_11LK1V7... file next to launch.sh and try again.'
    fi

    printf '\033[33mInstalling TU1 from %s...\033[0m\n' \
        "$(basename -- "$title_update")"

    title_update_size="$(stat -c '%s' -- "$title_update")"

    if [[ "$title_update_size" != '598016' ]]; then
        die 'That TU1 file is the wrong size.'
    fi

    for entry in "${PATCH_FILES[@]}"; do
        IFS='|' read -r name offset length expected_hash <<< "$entry"

        TEMP_PAYLOAD="$(mktemp "$ROOT/.sdoj-payload.XXXXXX")"

        dd \
            if="$title_update" \
            of="$TEMP_PAYLOAD" \
            bs=1 \
            skip="$offset" \
            count="$length" \
            status=none

        actual_hash="$(get_file_sha256 "$TEMP_PAYLOAD")"

        if [[ "$actual_hash" != "$expected_hash" ]]; then
            die "TU1 check failed for $name."
        fi

        mv -f -- "$TEMP_PAYLOAD" "$GAME_DATA/$name"
        TEMP_PAYLOAD=''
    done

    if ! test_patch_files "$GAME_DATA"; then
        die 'TU1 setup failed its final check.'
    fi

    printf '\033[32mTU1 installed.\033[0m\n'
fi

mkdir -p -- "$USER_DATA"

exit 0