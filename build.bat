@echo off

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
    )
)

if not EXIST bin mkdir bin

echo Build type : %BUILDTYPE%

cmake -S . -B build -DRUN_TESTS=%RUNTESTS% -A="%ARCH%"

cd build
cmake --build . --config %BUILDTYPE%

set TESTERR=0

if %RUNTESTS% equ 1 ctest --output-on-failure

if %errorlevel% neq 0 (
    echo "Error catched during testing"
    if %RUNTESTS% equ 1 type build\Testing\Temporary\LastTest.log

    set TESTERR=1
)

cd ..

dir "D:/a/libromano/libromano/build/tests/Release"

exit /B %TESTERR%

rem //////////////////////////////////
rem Little function to process args
:ParseArg

if "%~1" equ "--debug" set BUILDTYPE=Debug

if "%~1" equ "--tests" set RUNTESTS=1

if "%~1" equ "--clean" set REMOVEOLDDIR=1

exit /B 0

rem //////////////////////////////////
