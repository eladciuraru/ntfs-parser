@echo off
setlocal

:: Config
set "ParentDir=%~dp0"
set "BuildDir=%ParentDir%build\"
set "SourceDir=%ParentDir%src\"

set "Warnings=-Werror=all -Wno-unused"
set "CompilerFlags=-m64 %Warnings% -pedantic -I^"%ParentDir%\^""
set "LinkerFlags=-fuse-ld=lld -Wl,-subsystem:console"


if not exist "%BuildDir%" (
    echo [*] Creating %BuildDir%
    mkdir "%BuildDir%"
)

pushd %BuildDir%

clang %CompilerFlags% -O0 -g "%SourceDir%example.c" -o "ntfs_parser_example.exe" %LinkerFlags%

popd
