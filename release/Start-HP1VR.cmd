@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0HP1VR\Launcher.ps1"
if errorlevel 1 pause
