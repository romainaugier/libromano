#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause 
# Copyright (c) 2023 - Present Romain Augier 
# All rights reserved. 

log_info "Building libromano"

BUILDTYPE="Release"
RUNTESTS=0
REMOVEOLDDIR=0
EXPORTCOMPILECOMMANDS=0
VERSION="dev"
INSTALLDIR="$PWD/install"

# Little function to parse command line arguments
parse_args()
{
    [ "$1" == "--debug" ] && BUILDTYPE="Debug"

    [ "$1" == "--tests" ] && RUNTESTS=1

    [ "$1" == "--clean" ] && REMOVEOLDDIR=1

    [ "$1" == "--export-compile-commands" ] && EXPORTCOMPILECOMMANDS=1

    [ "$1" == *"version"* ] && parse_version $1

    [ "$1" == *"installdir"* ] && parse_install_dir $1
}

# Little function to parse the version from a command line argument
parse_version()
{
    arr=(${$1//:/ })

    [ ${#arr[@]} == 2 ] && VERSION=${arr[1]}
}

# Little function to parse the installation dir from a command line argument
parse_install_dir()
{
    INSTALLDIR="$( cut -d ':' -f 2- <<< "$s" )"
}

# Little function to log an information message to the console
log_info()
{
    echo "[INFO] : $1"
}

# Little function to log a warning message to the console
log_warning()
{
    echo "[WARNING] : $1"
}

# Little function to log an error message to the console
log_error()
{
    echo "[ERROR] : $1"
}

for arg in "$@"
do
    parse_args "$arg"
done

log_info "Build type: $BUILDTYPE"
log_info "Build version: $VERSION"

if [[ -d "build" && $REMOVEOLDDIR -eq 1 ]]; then
    log_info "Removing old build directory"
    rm -rf build
fi

if [[ -d "install" && $REMOVEOLDDIR -eq 1 ]]; then
    log_info "Removing old install directory"
    rm -rf install
fi

cmake -S . -B build -DRUN_TESTS=$RUNTESTS -DCMAKE_EXPORT_COMPILE_COMMANDS=$EXPORTCOMPILECOMMANDS -DCMAKE_BUILD_TYPE=$BUILDTYPE -DVERSION=$VERSION

if [[ $? -ne 0 ]]; then
    log_error "Error during CMake configuration"
    exit 1 
fi

cd build

cmake --build . -- -j $(nproc)

if [[ $? -ne 0 ]]; then
    log_error "Error during CMake build"
    cd ..
    exit 1
fi

if [[ $RUNTESTS -eq 1 ]]; then
    ctest --output-on-failure
    
    if [[ $? -ne 0 ]]; then
        log_error "Error during CMake testing"
        cd ..
        exit 1
    fi
fi

cmake --install . --config %BUILDTYPE% --prefix $INSTALLDIR

if [[ $? -ne 0 ]]; then
    log_error "Error during CMake installation"
    cd ..
    exit 1
fi

cd ..

if [[ $EXPORTCOMPILECOMMANDS -eq 1 ]]
then
    cp ./build/compile_commands.json ./compile_commands.json
    log_info "Copied compile_commands.json to root directory"
fi
