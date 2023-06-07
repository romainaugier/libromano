#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause 
# Copyright (c) 2023 - Present Romain Augier 
# All rights reserved. 

echo Building libromano

BUILDTYPE="Release"
RUNTESTS=0
REMOVEOLDDIR=0
EXPORTCOMPILECOMMANDS=0

parse_args()
{
    [ "$1" == "--debug" ] && BUILDTYPE="Debug"

    [ "$1" == "--tests" ] && RUNTESTS=1

    [ "$1" == "--clean" ] && REMOVEOLDDIR=1

    [ "$1" == "--export-compile-commands" ] && EXPORTCOMPILECOMMANDS=1
}

for arg in "$@"
do
    parse_args "$arg"
done

if [[ -d "build" && $REMOVEOLDDIR -eq 1 ]]; then
    rm -rf build
    rm -rf bin
    rm -rf static
fi

cmake -S . -B build -DRUN_TESTS=$RUNTESTS -DCMAKE_EXPORT_COMPILE_COMMANDS=$EXPORTCOMPILECOMMANDS -DCMAKE_BUILD_TYPE=$BUILDTYPE

if [[ $? -ne 0 ]]; then
    echo Error during CMake configuration
    exit 1 
fi

cd build

cmake --build . -- -j $(nproc)

if [[ $? -ne 0 ]]; then
    echo Error during compilation
    cd ..
    exit 1
fi

if [[ $RUNTESTS -eq 1 ]]; then
    ctest --output-on-failure
    
    if [[ $? -ne 0 ]]; then
        echo Error during testing
        cd ..
        exit 1
    fi
fi

cd ..

if [[ $EXPORTCOMPILECOMMANDS -eq 1 ]]
then
    cp ./build/compile_commands.json ./compile_commands.json
fi
