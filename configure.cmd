@echo off
@echo configuring windows build

GOTO start
:find_dp0
SET dp0=%~dp0
EXIT /b
:start
SETLOCAL
CALL :find_dp0

echo %dp0%

if defined TARGET_ARCH (
   set arch_arg=-a %TARGET_ARCH%
)

if NOT defined PRESET (
   set PRESET=base-win64
)

echo arch =  %arch_arg%
echo preset = %PRESET%

%dp0%\node_modules\.bin\cmake-js configure %arch_arg% -- --preset=%PRESET%