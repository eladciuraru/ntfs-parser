@echo off

if /i "%1"==""      call build_clang.bat
if /i "%1"=="clang" call build_clang.bat
if /i "%1"=="cl"    call build_cl.bat
