@echo off
setlocal
set "Configuration=Debug"
call "%~dp0build-UtilAssetsIntegrity-x86.bat" %*
exit /b %errorlevel%
