@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Launch-U59.ps1" %*
if errorlevel 1 pause
