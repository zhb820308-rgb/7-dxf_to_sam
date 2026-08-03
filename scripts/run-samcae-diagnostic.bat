@echo off
setlocal

rem Capture SAMCAE stdout and stderr without changing its Qt settings.
rem Launcher lookup order: first argument, SAMCAE_BAT environment variable, PATH.
set "SAMCAE_LAUNCHER=%~1"
if not defined SAMCAE_LAUNCHER if defined SAMCAE_BAT set "SAMCAE_LAUNCHER=%SAMCAE_BAT%"
if not defined SAMCAE_LAUNCHER for %%I in (samCAE.bat) do set "SAMCAE_LAUNCHER=%%~$PATH:I"
set "LOG_DIR=%~dp0..\logs\diagnostics"
set "LOG_FILE=%LOG_DIR%\samcae-console-latest.log"

if not defined SAMCAE_LAUNCHER goto launcher_missing
if not exist "%SAMCAE_LAUNCHER%" goto launcher_missing

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

echo ============================================================
echo Starting SAMCAE. Reproduce the DXF import crash now.
echo Output file: %LOG_FILE%
echo ============================================================

call "%SAMCAE_LAUNCHER%" > "%LOG_FILE%" 2>&1
set "SAMCAE_EXIT_CODE=%ERRORLEVEL%"

echo.
echo ============================================================
echo SAMCAE exited with code: %SAMCAE_EXIT_CODE%
echo Captured console output:
echo ============================================================
type "%LOG_FILE%"
echo.
echo ============================================================
echo Log file: %LOG_FILE%
echo Keep this window open or send the log file for analysis.
echo ============================================================
pause

exit /b %SAMCAE_EXIT_CODE%

:launcher_missing
echo SAMCAE launcher not found.
echo Usage: %~nx0 "D:\path\to\samCAE.bat"
echo Or set the SAMCAE_BAT environment variable before running this script.
exit /b 2
