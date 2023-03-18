@echo off

setlocal

:: Config
set "ParentDir=%~dp0"
set "BinDir=%ParentDir%bin\"
set "SourceDir=%ParentDir%examples\"

set "Warnings=-Werror"
set "CompilerFlags=-m64 %Warnings% -std=c11 -pedantic -I^"%ParentDir%\^""
set "LinkerFlags=-fuse-ld=lld"


if not exist "%BinDir%" (
    echo [*] Creating %BinDir%
    mkdir "%BinDir%"
)

pushd %BinDir%

clang %CompilerFlags% -O0 -g "%SourceDir%ntfs_example.c" -o "ntfs_example.exe" %LinkerFlags%

popd
