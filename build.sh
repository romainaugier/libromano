#!/bin/bash

echo Building libromano

BUILDTYPE="Release"
RUNTESTS=0
REMOVEOLDDIR=0

parse_args()
{
    [ "$1" == "Debug" ] && BUILDTYPE="Debug"
    [ "$1" == "debug" ] && BUILDTYPE="Debug"
    [ "$1" == "dbg" ] && BUILDTYPE="Debug"

    [ "$1" == "RunTests" ] && RUNTESTS=1
    [ "$1" == "Tests" ] && RUNTESTS=1
    [ "$1" == "Test" ] && RUNTESTS=1
    [ "$1" == "runtests" ] && RUNTESTS=1
    [ "$1" == "tests" ] && RUNTESTS=1
    [ "$1" == "test" ] && RUNTESTS=1

    [ "$1" == "RemoveDir" ] && REMOVEOLDDIR=1
    [ "$1" == "Remove" ] && REMOVEOLDDIR=1
    [ "$1" == "remove" ] && REMOVEOLDDIR=1
    [ "$1" == "removedir" ] && REMOVEOLDDIR=1
    [ "$1" == "rmdir" ] && REMOVEOLDDIR=1
    [ "$1" == "rdir" ] && REMOVEOLDDIR=1
}

for arg in "$@"
do
    parse_args "$arg"
done

if [[ -d "build" && $REMOVEOLDDIR -eq 1 ]]
then
    rm -rf build
fi

if [[ $RUNTESTS -eq 1 ]]
then
    cmake -S . -B build -DRUN_TESTS=1 
else
    cmake -S . -B build
fi

cd build

cmake --build . --config "$BUILDTYPE" 

if [[ $RUNTESTS -eq 1 ]]
then
    ctest --output-on-failure
fi

cd ..