@echo off
powershell -ExecutionPolicy Bypass -File "%~dp0scripts\Package-Scrapbook.ps1" %*
exit /b %ERRORLEVEL%
