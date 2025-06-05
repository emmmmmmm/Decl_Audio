@echo off
REM Build DeclarativeSoundEngine + CLI tool

REM Set up Visual Studio environment (adjust path if needed)
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

REM Build solution in Debug mode
msbuild DeclarativeSoundEngine.sln /p:Configuration=Debug

REM Optional: run the CLI test tool
if exist x64\Debug\AudioTestCLI.exe (
    echo Running tests...
    x64\Debug\AudioTestCLI.exe --test all
) else (
    echo Build failed or CLI tool not found.
)