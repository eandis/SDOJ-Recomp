# SDOJ-Recomp

Dodonpachi Saidaioujou Recompilation using [rexglue](https://github.com/rexglue/rexglue-sdk).

# Requirements

- Your own DoDonPachi Saidaioujou Xbox 360 ISO dump (MD5 366517C07EA2B1912F9B33F208C866BB)
- Title Update 1.01 file
  
# Building

### Windows

You need CMake 3.25 or newer, Ninja, LLVM/Clang with C++23 support, and the Visual Studio C++ build tools with a Windows SDK.

Make sure cmake, ninja, clang and clang++ are in PATH.

ReXGlue and all required dependencies are already included in the repo.

Run:

`cmake --preset win-amd64-release`
`cmake --build --preset win-amd64-release --parallel`

### Linux

You need CMake 3.25 or newer, Ninja, and Clang with C++23 support. The linux-amd64-release preset looks for clang-20/clang++-20 specifically.

You'll also need a few system packages for the native UI, input and audio backends. On Debian/Ubuntu:

`sudo apt install libgtk-3-dev libx11-xcb-dev libvulkan-dev libasound2-dev libpulse-dev libpipewire-0.3-dev`

ReXGlue and all required dependencies are already included in the repo.

Run:

`cmake --preset linux-amd64-release`
`cmake --build --preset linux-amd64-release --parallel`

After building, copy librexruntime.so and libTracyClient.so from thirdparty/rexglue-sdk/out/linux-amd64/ into the build output folder (out/build/linux-amd64-release/), next to the saidaioujou_recomp_tu1 executable. They aren't copied there automatically, and the executable won't start without them.

# First launch

1. Put SDOJ iso file in this folder, beside the recomp exe.
2. Put the TU_11LK1V7... TU1 file in the same folder.
3. Run "launch.bat".

The first launch extracts the ISO to game_data and installs the three TU1 patch
files.

For the later launches it verifies the existing game_data and
skips both extraction steps when they have already completed. After successful
setup, the ISO and TU container may be removed.

# Settings 

There are some hardcoded arguments in the launch.cmd. You can edit them as you wish, but don't remove  --xex_apply_patches=true.

You can open the settings by pressing F4 in game. To turn on keyboard inputs, go to the Input tab and turn on mnk_mode. The recommended input backend is xinput, the input lag should be around ~1 frame during gameplay with it. To achieve more consistent results, running the game through Special K is recommended. If you turn off VSync (it is disabled by default), you might have to cap the FPS manually to 60 via Special K or any other way.

The saved settings are stored in the saidaioujou_recomp_tu1.toml config file.

# Patches

The input lag fix (reduction from 3f to 1f) consists of 2 patches:

- The first patch polls the controller again and rebuilds the gameplay input mask right before the game copies it, instead of using the stale mask.
- The second patch runs the game's render callback batch earlier, waits for the buffer swap, and skips the original late calls. This saves another frame.

You can turn them off or on via editing the launch arguments in launch.bat/launch.sh. The arguments are `--render_patch` and `--input_patch`

The patches for both Normal and Arrange modes are in:

- `generated/default/saidaioujou_recomp_tu1_recomp.3.cpp`
- `generated/CA022100/saidaioujou_recomp_tu1_recomp.0.cpp`
- `generated/CA022100/saidaioujou_recomp_tu1_recomp.1.cpp`
- `generated/CA022110/saidaioujou_recomp_tu1_recomp.0.cpp`
- `generated/CA022110/saidaioujou_recomp_tu1_recomp.1.cpp`
