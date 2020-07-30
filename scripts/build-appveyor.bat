@ECHO OFF
SETLOCAL
SET EL=0

ECHO ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ %~f0 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~

IF /I "%msvs_toolset%"=="" ECHO msvs_toolset unset, defaulting to 14 && SET msvs_toolset=14
IF /I "%msvs_version%"=="" ECHO msvs_version unset, defaulting to 2019 && SET msvs_version=2019

SET PATH=%CD%;%PATH%
IF "%msvs_toolset%"=="12" SET msvs_version=2013
IF NOT "%NODE_RUNTIME%"=="" SET "TOOLSET_ARGS=%TOOLSET_ARGS% --runtime=%NODE_RUNTIME%"
IF NOT "%NODE_RUNTIME_VERSION%"=="" SET "TOOLSET_ARGS=%TOOLSET_ARGS% --target=%NODE_RUNTIME_VERSION%"

ECHO APPVEYOR^: %APPVEYOR%
ECHO nodejs_version^: %nodejs_version%
ECHO platform^: %platform%
ECHO msvs_toolset^: %msvs_toolset%
ECHO msvs_version^: %msvs_version%
ECHO TOOLSET_ARGS^: %TOOLSET_ARGS%

ECHO activating VS command prompt
:: NOTE this call makes the x64 -> X64
IF /I "%platform%"=="x64" ECHO x64 && CALL "C:\Program Files (x86)\Microsoft Visual Studio\%msvs_version%\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
IF /I "%platform%"=="x86" ECHO x86 && CALL "C:\Program Files (x86)\Microsoft Visual Studio\%msvs_version%\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

ECHO using compiler^: && CALL cl
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

ECHO using MSBuild^: && CALL msbuild /version && ECHO.
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

ECHO downloading/installing node
powershell Update-NodeJsInstallation (Get-NodeJsLatestBuild $env:nodejs_version) $env:PLATFORM
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

powershell Set-ExecutionPolicy Unrestricted -Scope CurrentUser -Force
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

ECHO available node.exe^:
call where node
ECHO available npm^:
call where npm

ECHO node^: && call node -v
call node -e "console.log('  - arch:',process.arch,'\n  - argv:',process.argv,'\n  - execPath:',process.execPath)"
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

ECHO npm^: && CALL npm -v
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

ECHO ===== where npm puts stuff START ============
ECHO npm root && CALL npm root
IF %ERRORLEVEL% NEQ 0 GOTO ERROR
ECHO npm root -g && CALL npm root -g
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

ECHO npm bin && CALL npm bin
IF %ERRORLEVEL% NEQ 0 GOTO ERROR
ECHO npm bin -g && CALL npm bin -g
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

SET NPM_BIN_DIR=
FOR /F "tokens=*" %%i in ('CALL npm bin -g') DO SET NPM_BIN_DIR=%%i
IF %ERRORLEVEL% NEQ 0 GOTO ERROR
IF /I "%NPM_BIN_DIR%"=="%CD%" ECHO ERROR npm bin -g equals local directory && SET ERRORLEVEL=1 && GOTO ERROR
ECHO ===== where npm puts stuff END ============

IF "%nodejs_version:~0,1%"=="4" CALL npm install node-gyp@3.x
IF %ERRORLEVEL% NEQ 0 GOTO ERROR
IF "%nodejs_version:~0,1%"=="5" CALL npm install node-gyp@3.x
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

::Need to force update node-gyp to v6+ for electron v6 and v5
ECHO ===== conditional node-gyp upgrade START ============
:: Find the folder to install the node-gyp in
SET npm_in_nodejs_dir="%ProgramFiles%\nodejs\node_modules\npm"
ECHO npm_in_nodejs_dir^: %npm_in_nodejs_dir%
IF /I "%platform%"=="x86" SET npm_in_nodejs_dir="%ProgramFiles(x86)%\nodejs\node_modules\npm"
ECHO npm_in_nodejs_dir^: %npm_in_nodejs_dir%
:: Set boolean whether the update has to happen
SET "needs_patch="
IF DEFINED NODE_RUNTIME_VERSION (
  ECHO NODE_RUNTIME_VERSION_REDUCED^: %NODE_RUNTIME_VERSION:~0,1%
  IF "%NODE_RUNTIME_VERSION:~0,1%"=="5" SET "needs_patch=y"
  IF "%NODE_RUNTIME_VERSION:~0,1%"=="6" SET "needs_patch=y"
)
:: Check if electron and install
ECHO NODE_RUNTIME^: %NODE_RUNTIME%
IF DEFINED needs_patch CALL npm install --prefix %npm_in_nodejs_dir% node-gyp@6.x
IF %ERRORLEVEL% NEQ 0 GOTO ERROR
ECHO ===== conditional node-gyp upgrade END ============

:: install node-gyp v6.1
ECHO ===== install node-gyp v6.1 ============
CALL npm install -g node-gyp
REM SET npm_config_node_gyp=C:\Users\appveyor\AppData\Roaming\npm\node-gyp
SET npm_config_node_gyp=C:\Users\appveyor\AppData\Roaming\npm\node_modules\node-gyp\bin\node-gyp.js
ECHO ===== install node-gyp v6.1 completed ============

:: build Ziti C-SDK
CALL git submodule update --init --recursive
IF %ERRORLEVEL% NEQ 0 GOTO ERROR
CALL cd deps
CALL cd ziti-sdk-c
CALL git submodule update --init --recursive
ECHO ===== starting make sequence... ============
CALL mkdir build
CALL cd build

ECHO ===== calling cmake -G Ninja ============
CALL cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_VERBOSE_MAKEFILE=OFF -G "NMake Makefiles" ..

IF %ERRORLEVEL% NEQ 0 GOTO ERROR
ECHO ===== calling cmake --build ============
CALL nmake ziti
IF %ERRORLEVEL% NEQ 0 GOTO ERROR
CALL cd ..
CALL cd ..
CALL cd ..

:: build Ziti NodeJS-SDK
ECHO ===== calling npm install --build-from-source ============
CALL npm install --build-from-source --cd=%CD% --msvs_version=%msvs_version% %TOOLSET_ARGS%
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

:: test our module
CALL node tests/hello.js
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

FOR /F "tokens=*" %%i in ('"CALL node_modules\.bin\node-pre-gyp reveal module %TOOLSET_ARGS% --silent"') DO SET MODULE=%%i
IF %ERRORLEVEL% NEQ 0 GOTO ERROR
FOR /F "tokens=*" %%i in ('node -e "console.log(process.execPath)"') DO SET NODE_EXE=%%i
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

dumpbin /DEPENDENTS "%NODE_EXE%"
IF %ERRORLEVEL% NEQ 0 GOTO ERROR
dumpbin /DEPENDENTS "%MODULE%"
IF %ERRORLEVEL% NEQ 0 GOTO ERROR


IF "%NODE_RUNTIME%"=="electron" GOTO CHECK_ELECTRON_TEST_ERRORLEVEL

::skipping check for errorlevel npm test result when using io.js
::@springmeyer: how to proceed?
IF NOT "%nodejs_version%"=="1.8.1" IF NOT "%nodejs_version%"=="2.0.0" GOTO CHECK_NPM_TEST_ERRORLEVEL

ECHO test our module
CALL node tests/hello.js
ECHO ==========================================
ECHO ==========================================
ECHO ==========================================

GOTO NPM_TEST_FINISHED


:CHECK_ELECTRON_TEST_ERRORLEVEL
ECHO installing electron
CALL npm install -g "electron@%NODE_RUNTIME_VERSION%"
ECHO installing electron-mocha
IF "%nodejs_version%" LEQ 6 CALL npm install -g "electron-mocha@7"
IF "%nodejs_version%" GTR 6 CALL npm install -g "electron-mocha"
ECHO preparing tests
CALL electron "test/support/createdb-electron.js"
DEL "test\support\createdb-electron.js"
ECHO calling electron-mocha
CALL electron-mocha -R spec --timeout 480000
IF %ERRORLEVEL% NEQ 0 GOTO ERROR
GOTO NPM_TEST_FINISHED


:CHECK_NPM_TEST_ERRORLEVEL
ECHO calling npm test
CALL npm test
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

:NPM_TEST_FINISHED
ECHO packaging for node-gyp
CALL node_modules\.bin\node-pre-gyp package %TOOLSET_ARGS%
::make commit message env var shorter
SET CM=%APPVEYOR_REPO_COMMIT_MESSAGE%
IF NOT "%CM%" == "%CM:[publish binary]=%" (ECHO publishing && CALL node_modules\.bin\node-pre-gyp --msvs_version=%msvs_version% publish %TOOLSET_ARGS%) ELSE (ECHO not publishing)
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

GOTO DONE



:ERROR
ECHO ~~~~~~~~~~~~~~~~~~~~~~ ERROR %~f0 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ECHO ERRORLEVEL^: %ERRORLEVEL%
SET EL=%ERRORLEVEL%

:DONE
ECHO ~~~~~~~~~~~~~~~~~~~~~~ DONE %~f0 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~

EXIT /b %EL%
