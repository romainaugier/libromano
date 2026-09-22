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
set INSTALL=0
set ADDRSAN=0
set BENCHMARK=0
set BENCHMARKARGS=

for %%x in (%*) do (
    call :ParseArg "%%~x"
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

cmake -S . -B build -DRUN_TESTS=%RUNTESTS% -A="%ARCH%" -DVERSION=%VERSION% -DADDRSAN=%ADDRSAN% -DBUILD_BENCHMARKS=%BENCHMARK%

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

rem Inside a parenthesized block %errorlevel% is expanded before the block runs: use "if errorlevel 1"
if %RUNTESTS% equ 1 (
    ctest --output-on-failure -C %BUILDTYPE%

    if errorlevel 1 (
        call :LogError "Error caught during CMake testing"
        type Testing\Temporary\LastTest.log

        cd ..
        exit /B 1
    )
)

if %INSTALL% equ 1 (
    cmake --install . --config %BUILDTYPE% --prefix %INSTALLDIR%

    if errorlevel 1 (
        call :LogError "Error caught during CMake installation"
        cd ..
        exit /B 1
    )
)

if %BENCHMARK% equ 1 (
    call :LogInfo "Running the matrix multiplication benchmark"

    if /I not "%BUILDTYPE%" == "Release" (
        call :LogWarning "Benchmarking a %BUILDTYPE% build, numbers will not be representative"
    )

    benchmarks\%BUILDTYPE%\bench_matmul.exe --check --csv benchmarks\bench_matmul.csv %BENCHMARKARGS%

    if errorlevel 1 (
        call :LogError "Error caught during the benchmark (wrong results or invalid arguments)"
        cd ..
        exit /B 1
    )

    call :LogInfo "Benchmark results written to build\benchmarks\bench_matmul.csv"
)

cd ..

exit /B 0

rem //////////////////////////////////
rem Little function to process args
:ParseArg

if "%~1" equ "--debug" set BUILDTYPE=Debug

if "%~1" equ "--reldebug" set BUILDTYPE=RelWithDebInfo

if "%~1" equ "--tests" set RUNTESTS=1

if "%~1" equ "--install" set INSTALL=1

if "%~1" equ "--clean" set REMOVEOLDDIR=1

if "%~1" equ "--addrsan" set ADDRSAN=1

if "%~1" equ "--benchmark" set BENCHMARK=1

if "%~1" equ "--export-compile-commands" (
    call :LogWarning "Exporting compile commands is not supported on Windows for now"
)

rem Prefix checks with substrings (piping into find is slow and matched every argument on some shells)
set "ARG=%~1"

if /I "%ARG:~0,17%" equ "--benchmark-args:" call :ParseBenchmarkArgs "%ARG%"

if /I "%ARG:~0,10%" equ "--version:" call :ParseVersion "%ARG%"

if /I "%ARG:~0,13%" equ "--installdir:" call :ParseInstallDir "%ARG%"

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
rem Little function to parse the arguments forwarded to the benchmark
rem (ex: "--benchmark-args:--suite full --threads 1,max", quotes around the whole argument)
:ParseBenchmarkArgs

set BENCHMARK=1

for /f "tokens=1* delims=:" %%a in ("%~1") do (
    set BENCHMARKARGS=%%b
)

call :LogInfo "Benchmark arguments specified by the user: %BENCHMARKARGS%"

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
