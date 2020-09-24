@echo off
setlocal

call :copyinc SavedGameManager
call :copylib SavedGameManager
call :copydll SavedGameManager

exit /b

REM %1 - Name
:copyinc
call :copyfile %1.h inc

exit /b


REM %1 - Name
:copylib

call :copybinary %1.lib lib

exit /B


REM %1 - Name
:copydll

call :copybinary %1.dll bin
call :copybinary %1.pdb bin

exit /B


REM %1 - Filename
REM %2 - Relative Destination Base Path
:copybinary

call :copybinarywitharch %1 x64 %2\desktop
call :copybinarywitharch %1 x64 %2\emulator uwp
call :copybinarywitharch %1 arm64 %2\hololens uwp


exit /b


REM %1 - Name
REM %2 - Relative Source Base Path With Arch
REM %3 - Relative Destination Base Path With Arch
REM %4 - (Optional) uwp for uwp projects
:copybinarywitharch

if /i "%4" EQU "uwp" (
	set DEBUG_SOURCE_DIR=debug-uwp
        set RELEASE_SOURCE_DIR=release-uwp
) else (
	set DEBUG_SOURCE_DIR=debug
        set RELEASE_SOURCE_DIR=release
)

call :copyfile bld\bin\%2\%RELEASE_SOURCE_DIR%\%1 %3

exit /b


REM %1 - Relative Source Path
REM %2 - Relative Destination Directory Path
:copyfile

set DEST_DIR=..\pipes\thirdparty\savedgamemanager\%2

if EXIST %1 (
	if NOT EXIST %DEST_DIR% (
		echo Creating directory %DEST_DIR%
		mkdir %DEST_DIR%
	)

	echo Copying %1 to %DEST_DIR%
	copy /y %1 %DEST_DIR%
)

exit /b