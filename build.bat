@echo off

rem SPDX-License-Identifier: BSD-3-Clause 
rem Copyright (c) 2023 - Present Romain Augier 
rem All rights reserved. 

rem Little utility batch script to build the library

echo Building libromano

set BUILDTYPE=Release
set RUNTESTS=0
set REMOVEOLDDIR=0
set ARCH=x64

for %%x in (%*) do (
    call :ParseArg %%~x
)

if %REMOVEOLDDIR% equ 1 (
    if EXIST build (
        echo Removing old build directory
        rmdir /s /q build
        rmdir /s /q bin
        rmdir /s /q static
    )
)

if not EXIST bin mkdir bin

echo Build type : %BUILDTYPE%

cmake -S . -B build -DRUN_TESTS=%RUNTESTS% -A="%ARCH%"

if %errorlevel% neq 0 (
    echo Error catched during CMake configuration
    exit /B 1
)

cd build
cmake --build . --config %BUILDTYPE%

if %errorlevel% neq 0 (
    echo Error catched during compilation
    cd ..
    exit /B 1
)

if %RUNTESTS% equ 1 (
    ctest --output-on-failure

    if %errorlevel% neq 0 (
        echo Error catched during testing
        type build\Testing\Temporary\LastTest.log

        cd ..
        exit /B 1
    )
)

cd ..

exit /B 0

rem //////////////////////////////////
rem Little function to process args
:ParseArg

if "%~1" equ "--debug" set BUILDTYPE=Debug

if "%~1" equ "--tests" set RUNTESTS=1

if "%~1" equ "--clean" set REMOVEOLDDIR=1

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
