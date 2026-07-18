# SDOJ-Recomp

Dodonpachi Saidaioujou Recompilation using [rexglue](https://github.com/rexglue/rexglue-sdk).

# Requirements

- Your own DoDonPachi Saidaioujou Xbox 360 ISO dump (MD5 366517C07EA2B1912F9B33F208C866BB)
- Title Update 1.01 file

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

Changelog:

1.5:
- Input lag reduced from 3f to 1f via a game patch. Everything else in the game is unmodifed.
- Reverted back to default rexglue directx12 settings
