@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\clear_logs.ps1" %*
exit /b %ERRORLEVEL%
