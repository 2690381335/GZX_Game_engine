@echo off
rem ============================================================
rem  Windows build script for the gzx engine demo
rem  Toolchain : LLVM clang++ 23.1.2 (MSVC ABI) + Ninja + CMake
rem  Usage     : build_windows.bat [Release^|Debug]   (default: Release)
rem  Note      : nothing here is installed into the system PATH
rem ============================================================
setlocal

if "%1"=="" (set BUILD_TYPE=Release) else (set BUILD_TYPE=%1)
set BUILD_DIR=build-%BUILD_TYPE%

set CMAKE=D:\kaifa\tools\cmake\bin\cmake.exe
set NINJA=D:\kaifa\tools\ninja\ninja.exe
set CLANG=D:\kaifa\tools\downloads\bin\clang++.exe
set RC=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\rc.exe
set THREELIB=D:\kaifa\tools\threelib
set VULKAN=D:\kaifa\tools\downloads\vulkan

echo [1/3] Configuring (%BUILD_TYPE%) ...
"%CMAKE%" -B %BUILD_DIR% -G Ninja ^
  -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
  -DCMAKE_CXX_COMPILER="%CLANG%" ^
  -DCMAKE_RC_COMPILER="%RC%" ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DSDL3_DIR="%THREELIB%\SDL3-3.4.16\cmake" ^
  -DSDL3_image_DIR="%THREELIB%\SDL3_image-3.4.6\cmake" ^
  -DSDL3_mixer_DIR="%THREELIB%\SDL3_mixer-3.2.4\cmake" ^
  -DSDL3_ttf_DIR="%THREELIB%\SDL3_ttf-3.2.2\cmake" ^
  -DVulkan_INCLUDE_DIR="%VULKAN%\Include" ^
  -DVulkan_LIBRARY="%VULKAN%\Lib\vulkan-1.lib"
if errorlevel 1 goto :fail

echo [2/3] Building ...
"%CMAKE%" --build %BUILD_DIR%
if errorlevel 1 goto :fail

echo [3/3] Copying runtime DLLs next to main.exe ...
for %%P in ("SDL3-3.4.16:SDL3" "SDL3_image-3.4.6:SDL3_image" "SDL3_mixer-3.2.4:SDL3_mixer" "SDL3_ttf-3.2.2:SDL3_ttf") do (
  for /f "tokens=1,2 delims=:" %%A in (%%P) do (
    copy /y "%THREELIB%\%%A\lib\x64\%%B.dll" "%BUILD_DIR%\" >nul
  )
)
copy /y "%THREELIB%\SDL3_image-3.4.6\lib\x64\optional\*.dll" "%BUILD_DIR%\" >nul 2>&1
copy /y "%THREELIB%\SDL3_mixer-3.2.4\lib\x64\optional\*.dll" "%BUILD_DIR%\" >nul 2>&1

echo.
echo Done: %BUILD_DIR%\main.exe
echo Run it from that folder (assets are loaded as ../Sprites/...).
exit /b 0

:fail
echo Build FAILED
exit /b 1
