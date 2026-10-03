@echo off
setlocal
if not "%Configuration%"=="Debug" if not "%Configuration%"=="Release" exit /b 2
for %%I in ("%~dp0..") do set "ProjectRoot=%%~fI"
set "BuildDirectory=%ProjectRoot%\build\x86\%Configuration%"
set "InstallDirectory=%ProjectRoot%\install\x86\%Configuration%"
set "FreeImageDLL=FreeImage.dll"
if "%Configuration%"=="Debug" set "FreeImageDLL=FreeImaged.dll"

cmake -G "Visual Studio 17 2022" -A Win32 -S "%ProjectRoot%" -B "%BuildDirectory%" -DCMAKE_INSTALL_PREFIX="%InstallDirectory%" -DBUILD_TESTING=ON %*
if errorlevel 1 exit /b %errorlevel%
cmake --build "%BuildDirectory%" --config %Configuration% --parallel
if errorlevel 1 exit /b %errorlevel%
ctest --test-dir "%BuildDirectory%" -C %Configuration% --output-on-failure --no-tests=error
if errorlevel 1 exit /b %errorlevel%
cmake --install "%BuildDirectory%" --config %Configuration%
if errorlevel 1 exit /b %errorlevel%
"%BuildDirectory%\tests\%Configuration%\UtilAssetsIntegritySmokeTests.exe" "%InstallDirectory%\svencoop\metahook\dlls\UtilAssetsIntegrity.dll" "%InstallDirectory%\svencoop\metahook\dlls\FreeImage\%FreeImageDLL%"
exit /b %errorlevel%
