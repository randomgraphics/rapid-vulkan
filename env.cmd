@echo off
where.exe pwsh.exe >nul 2>&1 || (
    echo.
    echo Install PowerShell 7.0+ to get the lastest PowerShell features.
    echo.
)
if %ERRORLEVEL% EQU 0 (
    rem Run powershell 7
    pwsh.exe -nologo -noexit %~dp0dev\\env\\env.ps1 %*
) else (
    rem Run powershell 5.1
    powershell.exe -nologo -noexit %~dp0dev\\env\\env.ps1 %*
)
