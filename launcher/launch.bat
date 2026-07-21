@echo off
setlocal
title DoDonPachi Saidaioujou TU1 Recomp

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0prepare_game.ps1" -PrepareOnly
set "setup_result=%ERRORLEVEL%"

if not "%setup_result%"=="0" (
  echo.
  echo setup failed. fix the error above and try again.
  pause
  exit /b %setup_result%
)

start "" /D "%~dp0" "%~dp0saidaioujou_recomp_tu1.exe" ^
  --input_backend=xinput ^
  --vsync=false ^
  --fullscreen=true ^
  --video_mode_refresh_rate=60 ^
  --xex_apply_patches=true ^
  --game_data_root="%~dp0game_data" ^
  --user_data_root="%~dp0user_data" ^
  --render_patch=true ^
  --input_patch=true
exit /b 0
