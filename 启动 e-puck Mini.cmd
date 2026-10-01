@echo off
setlocal
set "APP=%~dp0release\epuck_mini_control\epuck_mini_control.exe"
if not exist "%APP%" set "APP=%~dp0epuck_mini_control.exe"
if not exist "%APP%" (
  echo Release package not found. Run tools\package.ps1 or extract the GitHub Windows release.
  pause
  exit /b 1
)
for %%I in ("%APP%") do set "APPDIR=%%~dpI"
start "e-puck Mini Control" /d "%APPDIR%" "%APP%"
