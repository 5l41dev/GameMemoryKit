@echo off
setlocal EnableExtensions
cd /d "%~dp0"
rem Keep the window open until the user presses a key.
set "GMK_PAUSE=call pause"

rem ============================================================
rem  GameMemoryKit build helper (Windows)
rem
rem  Usage:
rem    build.bat                 debug build + run tests
rem    build.bat release         release build + run tests
rem    build.bat sanitizers      ASan/UBSan build + run tests
rem    build.bat tests-only      debug build, tests only
rem    build.bat --clean         delete build directories
rem
rem  Auto-detects the toolchain (MSYS2 clang64, then MSVC) and the
rem  generator (Ninja, then MinGW Makefiles). Requires CMake.
rem ============================================================

set "MODE=%~1"
if "%MODE%"=="" set "MODE=debug"

if "%MODE%"=="--clean" (
    rmdir /s /q build\debug build\release build\sanitizers build\tests-only 2>nul
    echo [build.bat] removed build directories.
    %GMK_PAUSE%
    exit /b 0
)

rem --- CMake configuration flags per mode ---------------------
set "CMAKE_ARGS="
if "%MODE%"=="debug"      set "CMAKE_ARGS=-DCMAKE_BUILD_TYPE=Debug"
if "%MODE%"=="release"    set "CMAKE_ARGS=-DCMAKE_BUILD_TYPE=Release"
if "%MODE%"=="sanitizers" set "CMAKE_ARGS=-DCMAKE_BUILD_TYPE=Debug -DGMK_ENABLE_SANITIZERS=ON"
if "%MODE%"=="tests-only" set "CMAKE_ARGS=-DCMAKE_BUILD_TYPE=Debug -DGMK_BUILD_EXAMPLES=OFF -DGMK_BUILD_CLI=OFF"

if not defined CMAKE_ARGS (
    echo [build.bat] unknown mode "%MODE%".
    echo [build.bat] valid modes: debug ^| release ^| sanitizers ^| tests-only ^| --clean
    %GMK_PAUSE%
    exit /b 1
)

set "BUILD_DIR=build\%MODE%"

rem --- Toolchain detection ------------------------------------
set "GENERATOR="

if exist "C:\msys64\clang64\bin\clang++.exe" (
    set "PATH=C:\msys64\clang64\bin;%PATH%"
    echo [build.bat] toolchain: MSYS2 clang64
    where ninja >nul 2>nul && set "GENERATOR=-G Ninja"
    if not defined GENERATOR (
        where mingw32-make >nul 2>nul && set "GENERATOR=-G MinGW Makefiles"
    )
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
    echo [build.bat] toolchain: MSVC (Visual Studio 2022)
) else (
    echo [build.bat] toolchain: compiler from PATH
    where ninja >nul 2>nul && set "GENERATOR=-G Ninja"
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo [build.bat] error: CMake was not found on PATH.
    %GMK_PAUSE%
    exit /b 1
)

rem --- Extra flags for not-yet-existing subprojects -----------
set "EXTRA_ARGS="
if not exist examples\CMakeLists.txt    set "EXTRA_ARGS=%EXTRA_ARGS% -DGMK_BUILD_EXAMPLES=OFF"
if not exist tools\gmk\CMakeLists.txt   set "EXTRA_ARGS=%EXTRA_ARGS% -DGMK_BUILD_CLI=OFF"

rem --- Configure ----------------------------------------------
cmake -S . -B "%BUILD_DIR%" %GENERATOR% %CMAKE_ARGS% %EXTRA_ARGS%
if errorlevel 1 (
    echo [build.bat] configure failed.
    %GMK_PAUSE%
    exit /b 1
)

rem --- Build ---------------------------------------------------
cmake --build "%BUILD_DIR%" -j
if errorlevel 1 (
    echo [build.bat] build failed.
    %GMK_PAUSE%
    exit /b 1
)

rem --- Test ----------------------------------------------------
ctest --test-dir "%BUILD_DIR%" --output-on-failure
set "GMK_TEST_RESULT=%errorlevel%"
%GMK_PAUSE%
exit /b %GMK_TEST_RESULT%
