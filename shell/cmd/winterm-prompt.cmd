@echo off
rem Copyright (c) winTerm contributors.
rem Licensed under the MIT license.

if defined WINTERM_CMD_PROMPT_INITIALIZED exit /b 0
set "WINTERM_CMD_PROMPT_INITIALIZED=1"
if not defined WINTERM_ORIGINAL_PROMPT set "WINTERM_ORIGINAL_PROMPT=%PROMPT%"

rem cmd.exe has no reliable pre-execution hook. autoMarkPrompts supplies the
rem command-executed transition while this prompt supplies CWD and prompt marks.
prompt $E]133;D$E\$E]133;A$E\$E]9;9;"$P"$E\$E]133;B$E\%WINTERM_ORIGINAL_PROMPT%
exit /b 0
