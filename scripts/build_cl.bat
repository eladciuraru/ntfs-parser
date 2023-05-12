@echo off

setlocal

if "%ProjectDir%" == "" set "ProjectDir=%~dp0..\"
set "BinDir=%ProjectDir%bin\cl"
set "SourceDir=%ProjectDir%examples\"

set "Warnings=/W4 /WX /wd4204 -D_CRT_SECURE_NO_WARNINGS"
set "CompilerFlags=/nologo %Warnings% /I^"%ProjectDir%\^""
set "LinkerFlags=/link /subsystem:console"

if not exist "%BinDir%" mkdir "%BinDir%"

pushd %BinDir%

cl %CompilerFlags% /Od /Zi "%SourceDir%dump_record.c" /Fe"dump_record.exe" %LinkerFlags%
cl %CompilerFlags% /Od /Zi "%SourceDir%dump_attrdef.c" /Fe"dump_attrdef.exe" %LinkerFlags%

popd
