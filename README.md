# SDOJ-Recomp

Dodonpachi Saidaioujou Recompilation using [ReXGlue](https://github.com/rexglue/rexglue-sdk).

# Requirements

- DoDonPachi Saidaioujou Xbox 360 dump
- Title Update 1.01 file

# First launch

For the disc release:

1. Put the SDOJ iso beside the recomp executable.
2. Put the TU_11LK1V7... TU1 file in the same folder.
3. Run the executable.

For the Games on Demand release:

1. Put a converted GoD iso beside the recomp executable.
2. Extract `CA022100.binp`, `CA022110.binp`, and `default.xexp` from the matching TU_...0085 file and put them beside the recomp executable.
3. Run the executable.

The first launch extracts the ISO and installs the TU1 files into `game_data`. After that, the ISO and TU file can be removed.

# Features

### Input lag

Input lag is reduced from 3f to 1f with 2 patches:

- The first patch polls the controller again and rebuilds the gameplay input mask right before the game copies it, instead of using the stale mask.
- The second patch runs the game's render callback batch earlier, waits for the buffer swap, and skips the original late calls. This saves another frame.

Both can be turned off in the General screen or F4 (needs a restart).

### Slowdown

Slowdown is heavily tweaked compared to Xbox 360. The game is tuned to mostly match what you'd see playing on shmupmame 5.3.1, for both expert and regular modes. Xbox Arrange mode does **not** have any slowdown, as all the slowdown comes from the 360 lagging itself, thus, the slowdown will not be tuned for this mode.

### Training mode

You can start from any boss phase, or the final attacks of Hibachi/Inbachi. Their intros are skipped in training mode.

### Settings

VSync, fullscreen and the input/render patches are in the game's General screen settings, and keyboard controls can be remapped in the Keyboard screen. Press F4 in game for the rest of the ReXGlue settings. Everything is saved in `saidaioujou_recomp_tu1.toml`.

### Debug

Launching the game with `--debug` unlocks the game's original hidden debug menu. It only applies to that launch. Some of its features are:
- Payment (adds coins)
- Cheat open (unlocks everything)
- Muteki (invincibility)

### Updates

The game checks GitHub for new releases on launch and lets you know if there's one. You can turn it off with `check_for_updates` in F4 under SDOJ.

# Building

### Windows

You need CMake 3.25 or newer, Ninja, LLVM/Clang with C++23 support, and the Visual Studio C++ build tools with a Windows SDK and Visual C++ redist files.

Make sure cmake, ninja, clang and clang++ are in PATH. If `VCToolsRedistDir` isn't set, pass `-DSDOJ_VC_REDIST_DIR=<path>`. The first configure needs internet to fetch extract-xiso.

Run:

```
cmake --preset win-amd64-release
cmake --build --preset win-amd64-release --parallel
```

### Linux

You need CMake 3.25 or newer, Ninja, and Clang with C++23 support.

You'll also need a few system packages for the native UI, input and audio backends. On Debian/Ubuntu:

```
sudo apt install libgtk-3-dev libx11-xcb-dev libvulkan-dev libasound2-dev libpulse-dev libpipewire-0.3-dev libcurl4-openssl-dev
```

The first configure needs internet to fetch extract-xiso.

Run:

```
cmake --preset linux-amd64-release
cmake --build --preset linux-amd64-release --parallel
```

### Release versions

`SDOJ_RELEASE_VERSION` (currently `v1.51`) should match the GitHub release tag. Pass `-DSDOJ_RELEASE_VERSION=` for dev builds to skip the update check.

