@echo off
powershell -ExecutionPolicy Bypass -File "%~dp0scripts\Run-Scrapbook.ps1" %*
exit /b %ERRORLEVEL%
