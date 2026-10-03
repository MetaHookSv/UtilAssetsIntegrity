@echo off
setlocal
set "Configuration=Release"
call "%~dp0build-UtilAssetsIntegrity-x86.bat" %*
exit /b %errorlevel%
