@echo off
rem Copyright (c) winTerm contributors.
rem Licensed under the MIT license.

if /i "%WINTERM_COMPATIBILITY_MODE%"=="Off" exit /b 0

call :AddMacro ll "dir /a $*"
call :AddMacro la "dir /a $*"
call :AddMacro clear "cls"
call :AddMacro pwd "cd"
call :AddMacro which "where $*"
call :AddMacro cat "type $*"
call :AddShimMacro touch touch
call :AddShimMacro open open
call :AddMacro ls "dir $*"
exit /b 0

:AddMacro
call :HasMacro "%~1"
if not errorlevel 1 exit /b 0

call :HasApplication "%~1"
if not errorlevel 1 exit /b 0

doskey %~1=%~2
exit /b 0

:AddShimMacro
call :HasMacro "%~1"
if not errorlevel 1 exit /b 0

call :HasApplication "%~1"
if not errorlevel 1 exit /b 0

doskey %~1="%WINTERM_SHIM%" %~2 $*
exit /b 0

:HasMacro
doskey /macros | findstr /r /i /c:"^%~1=" >nul
exit /b %ERRORLEVEL%

:HasApplication
where "%~1.exe" >nul 2>nul
exit /b %ERRORLEVEL%
