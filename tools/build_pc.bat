@echo off
rem SH3 Quest VR: the PC test build (Windows, LLVM clang + the MSVC libraries of VS 2022 Build Tools, Ninja).
rem Usage: tools\build_pc.bat [Release|RelWithDebInfo|Debug]   ->  build\pc\sh3.exe (the port, not the game's exe)
setlocal
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=RelWithDebInfo
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set ROOT=%~dp0..
cmake -S "%ROOT%" -B "%ROOT%\build\pc" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% ^
  -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang.exe" ^
  -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang++.exe" || exit /b 1
cmake --build "%ROOT%\build\pc" -- -k 0 || exit /b 1
echo BUILD OK
