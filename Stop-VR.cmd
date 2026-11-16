@echo off
setlocal
cd /d "%~dp0"
python -B "%~dp0scripts\stop_probe.py"
if errorlevel 1 pause
endlocal
