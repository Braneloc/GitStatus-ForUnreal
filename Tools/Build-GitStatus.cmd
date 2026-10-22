@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build-GitStatus.ps1" %*
set EXIT_CODE=%ERRORLEVEL%
echo.
if not "%EXIT_CODE%"=="0" echo BUILD FAILED - see messages above.
pause
exit /b %EXIT_CODE%
