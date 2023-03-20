@echo off

setlocal

if "%ProjectDir%" == "" set "ProjectDir=%~dp0..\"
set "BinDir=%ProjectDir%bin\cl"
set "SourceDir=%ProjectDir%examples\"

set "Warnings=/W4 /WX /wd4204"
set "CompilerFlags=/nologo %Warnings% /I^"%ProjectDir%\^""
set "LinkerFlags=/link /subsystem:console"

if not exist "%BinDir%" mkdir "%BinDir%"

pushd %BinDir%

cl %CompilerFlags% /Od /Zi "%SourceDir%ntfs_example.c" /Fe"ntfs_example.exe" %LinkerFlags%

popd
