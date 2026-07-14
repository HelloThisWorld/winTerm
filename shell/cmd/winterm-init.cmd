@echo off
rem Copyright (c) winTerm contributors.
rem Licensed under the MIT license.

if defined WINTERM_CMD_INITIALIZED exit /b 0
set "WINTERM_CMD_INITIALIZED=1"
set "WINTERM_INTEGRATION_VERSION=1"
if not defined WINTERM_SESSION_ID set "WINTERM_SESSION_ID=cmd-%RANDOM%%RANDOM%"
if not defined WINTERM_COMPATIBILITY_MODE set "WINTERM_COMPATIBILITY_MODE=Safe"
set "WINTERM_SHELL_ASSET_ROOT=%~dp0.."
set "WINTERM_SHIM=%WINTERM_SHELL_ASSET_ROOT%\winterm-shim.exe"

call "%~dp0winterm-doskey.cmd"
call "%~dp0winterm-prompt.cmd"
exit /b 0
