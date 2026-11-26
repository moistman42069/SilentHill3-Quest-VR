@echo off
rem SH3 Quest VR: builds tools\movie_check.exe (the movies off the game; see tools\movie_check.c). Run from anywhere.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
"C:\Program Files\LLVM\bin\clang.exe" -O2 -D_CRT_SECURE_NO_WARNINGS -w "%~dp0movie_check.c" -o "%~dp0movie_check.exe"
