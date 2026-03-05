@echo off
setlocal

:: Find MSBuild via vswhere.exe (ships with Visual Studio 2017+ Installer)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found. Install Visual Studio 2019 or later.
    exit /b 1
)

for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"

if not defined MSBUILD (
    echo ERROR: MSBuild not found. Install Visual Studio with the "Desktop development with C++" workload.
    exit /b 1
)

"%MSBUILD%" "%~dp0SuiteSpot.sln" /p:Configuration=Release /p:Platform=x64 /v:minimal
