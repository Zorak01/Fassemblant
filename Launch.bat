@echo off
setlocal
title Fassemblant Development
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Dev.ps1" %*
set "result=%errorlevel%"
if not "%result%"=="0" (
  echo.
  echo Launcher stopped. See the message above and game\shooter\Saved\Launcher\Logs.
  pause
)
exit /b %result%
