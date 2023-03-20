:: Helper script to auto setup environment variable for the build tools
:: using vswhere.exe
@echo off

setlocal EnableDelayedExpansion

set "ProjectDir=%~dp0"
set "PATH=%ProjectDir%scripts;%ProjectDir%bin;%PATH%"

set "_VsWhereExe=%ProjectDir%\vendors\microsoft\vswhere.exe"
set "_FindToolsCmd=%_VsWhereExe% -nologo -latest -legacy -property installationPath"

for /f "usebackq delims=" %%i in (`%_FindToolsCmd%`) do (
    set "_VsVarsCmd=%%i\VC\Auxiliary\Build\vcvars64.bat"
    if not exist "!_VsVarsCmd!" (
        echo [^^!] Failed to locate script to setup build tools
        echo [^^!]   Should have been at: "%_VsVarsCmd%"
    )
)

%comspec% /k "%_VsVarsCmd%"
