@echo off

setlocal

if "%ProjectDir%" == "" set "ProjectDir=%~dp0..\"
set "BinDir=%ProjectDir%bin\"
set "SourceDir=%ProjectDir%examples\"

set "Warnings=-Werror -Wall -pedantic-errors -D_CRT_SECURE_NO_WARNINGS"
set "CompilerFlags=-m64 %Warnings% -std=c11 -I^"%ProjectDir%\^""
set "LinkerFlags=-fuse-ld=lld -Wl,-subsystem:console"

if not exist "%BinDir%" mkdir "%BinDir%"

pushd %BinDir%

clang %CompilerFlags% -O0 -g "%SourceDir%dump_record.c" -o "dump_record.exe" %LinkerFlags%
clang %CompilerFlags% -O0 -g "%SourceDir%dump_attrdef.c" -o "dump_attrdef.exe" %LinkerFlags%

popd
