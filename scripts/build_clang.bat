@echo off

setlocal

if "%ProjectDir%" == "" set "ProjectDir=%~dp0..\"
set "BinDir=%ProjectDir%bin\"
set "SourceDir=%ProjectDir%examples\"

set "Warnings=-Werror"
set "CompilerFlags=-m64 %Warnings% -std=c11 -pedantic -I^"%ProjectDir%\^""
set "LinkerFlags=-fuse-ld=lld -Wl,-subsystem:console"

if not exist "%BinDir%" mkdir "%BinDir%"

pushd %BinDir%

clang %CompilerFlags% -O0 -g "%SourceDir%ntfs_example.c" -o "ntfs_example.exe" %LinkerFlags%

popd
