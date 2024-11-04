@echo off

rem SPDX-License-Identifier: BSD-3-Clause 
rem Copyright (c) 2023 - Present Romain Augier 
rem All rights reserved. 

rem Little utility batch script to build the library

call :LogInfo "Building libromano"

set BUILDTYPE=Release
set RUNTESTS=0
set REMOVEOLDDIR=0
set ARCH=x64
set VERSION="0.0.0"
set INSTALLDIR=%CD%\install
set SANITIZE=0

for %%x in (%*) do (
    call :ParseArg %%~x
)

if %REMOVEOLDDIR% equ 1 (
    if exist build (
        call :LogInfo "Removing old build directory"
        rmdir /s /q build
    )
    if exist bin (
        call :LogInfo "Removing old bin directory"
        rmdir /s /q bin
    )
    if exist lib (
        call :LogInfo "Removing old lib directory"
        rmdir /s /q lib
    )
    if exist %INSTALLDIR% (
        call :LogInfo "Removing old install directory"
        rmdir /s /q %INSTALLDIR%
    )
)

call :LogInfo "Build type: %BUILDTYPE%"
call :LogInfo "Build version: %VERSION%"

cmake -S . -B build -DRUN_TESTS=%RUNTESTS% -A="%ARCH%" -DVERSION=%VERSION%

if %errorlevel% neq 0 (
    call :LogError "Error caught during CMake configuration"
    exit /B 1
)

cd build
cmake --build . --config %BUILDTYPE% -j %NUMBER_OF_PROCESSORS%

if %errorlevel% neq 0 (
    call :LogError "Error caught during CMake compilation"
    cd ..
    exit /B 1
)

if %RUNTESTS% equ 1 (
    ctest --output-on-failure

    if %errorlevel% neq 0 (
        call :LogError "Error caught during CMake testing"
        type build\Testing\Temporary\LastTest.log

        cd ..
        exit /B 1
    )
)

cmake --install . --config %BUILDTYPE% --prefix %INSTALLDIR%

if %errorlevel% neq 0 (
    call :LogError "Error caught during CMake installation"
    cd ..
    exit /B 1
)

cd ..

exit /B 0

rem //////////////////////////////////
rem Little function to process args
:ParseArg

if "%~1" equ "--debug" set BUILDTYPE=Debug

if "%~1" equ "--reldebug" set BUILDTYPE=RelWithDebInfo

if "%~1" equ "--tests" set RUNTESTS=1

if "%~1" equ "--clean" set REMOVEOLDDIR=1

if "%~1" equ "--export-compile-commands" (
    call :LogWarning "Exporting compile commands is not supported on Windows for now"
)

echo "%~1" | find /I "version">nul && (
    call :ParseVersion %~1
)

echo "%~1" | find /I "installdir">nul && (
    call :ParseInstallDir %~1
)

exit /B 0
rem //////////////////////////////////

rem //////////////////////////////////
rem Little function to parse the version from the command line arg (ex: --version:0.1.3)
:ParseVersion

for /f "tokens=2 delims=:" %%a in ("%~1") do (
    set VERSION=%%a
    call :LogInfo "Version specified by the user: %%a"
)

exit /B 0
rem //////////////////////////////////

rem //////////////////////////////////
rem Little function to parse the install dir from the command line 
:ParseInstallDir

for /f "tokens=1* delims=:" %%a in ("%~1") do (
    set INSTALLDIR=%%b
    call :LogInfo "Install directory specified by the user: %%b"
)

exit /B 0
rem //////////////////////////////////

rem //////////////////////////////////
rem Little function to check for errors
:CheckErrors

if %errorlevel% neq 0 (
    echo %~1
    exit /B 1
)

exit /B 0
rem //////////////////////////////////

rem //////////////////////////////////
rem Little function to log errors
:LogError

echo [ERROR] : %~1

exit /B 0
rem //////////////////////////////////

rem //////////////////////////////////
rem Little function to log warnings 
:LogWarning

echo [WARNING] : %~1

exit /B 0
rem //////////////////////////////////

rem //////////////////////////////////
rem Little function to log infos 
:LogInfo

echo [INFO] : %~1

exit /B 0
rem //////////////////////////////////