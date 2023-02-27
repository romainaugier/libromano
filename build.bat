@echo off

rem Little utility batch script to build the library

echo Building libromano

set BUILDTYPE=Release
set RUNTESTS=0
set REMOVEOLDDIR=0

for %%x in (%*) do (
    call :ParseArg %%~x
)

if %REMOVEOLDDIR% equ 1 (
    if EXIST build (
        echo Removing old build directory
        rmdir /s /q build
    )
)

if not EXIST bin mkdir bin

echo Build type : %BUILDTYPE%

if %RUNTESTS% equ 1 (
    cmake -S . -B build -DRUN_TESTS=1
)
else (
    cmake -S . -B build
)

cd build
cmake --build . --config %BUILDTYPE%

if %RUNTESTS% equ 1 ctest --output-on-failure

cd ..

rem //////////////////////////////////
rem Little function to process args
:ParseArg

if "%~1" equ "--debug" set BUILDTYPE=Debug

if "%~1" equ "--tests" set RUNTESTS=1

if "%~1" equ "--clean" set REMOVEOLDDIR=1

exit /B 0

rem //////////////////////////////////