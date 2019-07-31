#!/bin/bash
set -euo pipefail

Usage() {
    echo "Unix (Linux/macOS) build script for the VX Engine codebase."
    echo ""
    echo "Syntax: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -h, -?, --help, -Help"
    echo "    Show this message"
    echo "  -v, --verbose, -Verbose"
    echo "    Print build command lines and other verbose log data."
    echo "  -c, --clean, -Clean"
    echo "    Clean the build directory before building."
    echo "  --cmake, -CMake"
    echo "    Re-run CMake before building. Implied by -c."
    echo "  -g, --generator, -Generator [name]"
    echo "    Specify the CMake generator to use. Ninja is the default, if available."
    echo "  --gl-loader, -GLLoader"
    echo "    Re-generate the OpenGL loader before building. Implied by -c."
    echo "  --release, -Release"
    echo "    Generate a release build, i.e. enable optimizations."
    echo "  --no-debug-info, -NoDebugInfo"
    echo "    Omit debug information for release builds."
    echo "  -r, --run, -Run"
    echo "    Run the program after compilation."
    echo "  --xcode, -Xcode"
    echo "    Generate and open an Xcode project. Implies -g Xcode."
}

Verbose=false
Clean=false
CMake=false
GLLoader=false
Release=false
NoDebugInfo=false
Run=false
Xcode=false
Generator="Ninja"

set +u
while :; do
    case $1 in
        -h | -\? | --help | -Help       ) Usage; exit   ;;
        -v | --verbose    | -Verbose    ) Verbose=true  ;;
        -c | --clean      | -Clean      ) Clean=true    ;;
        --cmake           | -CMake      ) CMake=true    ;;
        --gl-loader       | -GLLoader   ) GLLoader=true ;;
        --release         | -Release    ) Release=true  ;;
        -r | --run        | -Run        ) Run=true      ;;
        --xcode           | -XCode      ) Generator="Xcode"; Run=false; Xcode=true ;;
        -g | --generator  | -Generator  ) Generator=$2; shift ;;
        *) break ;; # end of options
    esac
    shift
done
set -u

# Configuration:
cd "$(dirname "$0")"
cd ..
RepositoryRoot="$PWD"
BuildDN="build";    BuildRoot="$RepositoryRoot/$BuildDN"
SourceDN="src";     SourceRoot="$RepositoryRoot/$SourceDN"
LibraryDN="lib";    LibraryRoot="$RepositoryRoot/$LibraryDN"
WorkingDN="run";    WorkingRoot="$RepositoryRoot/$WorkingDN"

# Functions for logging a line of text.
SGR_RESET="\033[0m"
SGR_BOLD="\033[1m"
SGR_RED="\033[31m"
SGR_YELLOW="\033[33m"
SGR_MAGENTA="\033[35m"
LogInfo()  { printf "$SGR_MAGENTA$*$SGR_RESET\n"; }
LogWarn()  { printf "$SGR_BOLD$SGR_YELLOW$*$SGR_RESET\n"; }
LogError() { printf "$SGR_BOLD$SGR_RED$*$SGR_RESET\n"; }

# Quits the script with exit code 1.
Panic() { LogError "$@"; exit 1; }

# CMake build type:
BuildType="Debug"
if $Release; then BuildType="RelWithDebInfo"; fi
if $Release && $NoDebugInfo; then BuildType="Release"; fi

# Build directory:
BD1=$(uname | tr '[:upper:]' '[:lower:]')
BD2=$(uname -m | sed 's/x86_64/x64/' | sed 's/i.86/x86/')
BD3=$(echo "$Generator" | sed 's/ /-/g' | sed 's/-+/-/g' | tr '[:upper:]' '[:lower:]')
BD4=$(echo "$BuildType" | tr '[:upper:]' '[:lower:]')
BuildDir="$BuildRoot/$BD1-$BD2-$BD3-$BD4"

# Configuration:
BinaryName="Game";      BinaryPath="$BuildDir/$BinaryName"
XcProjectName="Game";   XcProjectFile="$BuildDir/$XcProjectName.xcodeproj"

# Clean build directory:
CleanDir() {
    if [ -d "$1" ]; then
        LogInfo "Cleaning build directory: $1"
        rm -r "$1"
    fi
}
CleanFile() {
    if [ -f "$1" ]; then
        LogInfo "Cleaning build file: $1"
        rm "$1"
    fi
}
if $Clean; then
    CleanDir "$BuildDir"
    CleanDir "$BuildRoot/include"
    CleanFile "$BuildRoot/tree.txt"
fi

# Determines whether or not CMake has to be run manually, using the following heuristics:
# 1. Has the user passed -CMake to the script?
# 2. Does a valid makefile for the selected generator exist yet?
# 3. Has the structure of the src directory changed since this function was last run?
# Returns either an empty string or one containing the reason CMake should be run.
GetCMakeReason() {
    if $CMake; then echo "script -CMake flag set"; return; fi
    case $Generator in
        "Ninja")
            if ! [ -f "$BuildDir/build.ninja" ]; then
                echo "makefile build.ninja not found"
                return
            fi ;;
        "Unix Makefiles")
            if ! [ -f "$BuildDir/Makefile" ]; then
                echo "makefile not found"
                return
            fi ;;
        "Xcode")
            if ! [ -d "$BuildDir/$XcProjectName.xcodeproj" ]; then
                echo "project file $XcProjectName.xcodeproj not found"
                return
            fi ;;
    esac
    ls $SourceRoot/** > "$BuildRoot/tree.lst.new"
    if ! [ -f "$BuildRoot/tree.lst" ] ||
         [ -n "$(diff -q "$BuildRoot/tree.lst" "$BuildRoot/tree.lst.new")" ]; then
        mv -f "$BuildRoot/tree.lst.new" "$BuildRoot/tree.lst"
        echo "source tree changed"
    fi
    if [ -f "$BuildRoot/tree.lst.new" ]; then
        mv -f "$BuildRoot/tree.lst.new" "$BuildRoot/tree.lst"
    fi
}
CMakeReason="$(GetCMakeReason)"
if [ "$CMakeReason" ]; then CMake=true; else CMake=false; fi

# Make directories:
mkdir -p "$BuildRoot"
mkdir -p "$BuildRoot/include"
mkdir -p "$BuildDir"

# Run CMake:
if $CMake; then
    cd "$BuildDir"
    LogInfo "Running CMake ($CMakeReason) with generator \"$Generator\"..."
    cmake "$RepositoryRoot" -G "$Generator" -DCMAKE_BUILD_TYPE="$BuildType"
fi

# Build OpenGL loader:
GLLoaderPath="$BuildRoot/include/glad"
GLADLocation="$LibraryRoot/glad"
if ! [ -d "$GLLoaderPath" ]; then
    GLLoader=true
fi
if $GLLoader; then
    if ! [ -f "$GLADLocation/glad/__init__.py" ]; then
        Panic "Can't build OpenGL loader: GLAD not found at $GLADLocation."
    fi
    if ! command -v python > /dev/null; then
        Panic "Can't build OpenGL loader: Python is not insalled or not in PATH."
    fi
    LogInfo "Building OpenGL loader..."
    cd "$GLADLocation"
    python -m glad --out-path "$GLLoaderPath" --generator c-debug --local-files --api "gl=4.1" \
        --profile core --reproducible
fi

# Run build tool:
cd "$BuildDir"
case $Generator in
    "Ninja")
        LogInfo "Running Ninja for configuration $BuildType..."
        if $Verbose; then ninja -v; else ninja; fi
        ;;
    "Unix Makefiles")
        LogInfo "Running Make for configuration $BuildType..."
        make
        ;;
    *)
        Run=false
        ;;
esac

# Run game:
BinaryPath="$BuildDir/$BinaryName"
if $Run; then
    cd "$WorkingRoot"
    if [ -x "$BinaryPath" ]; then
        LogInfo "Set working directory to $PWD."
        LogInfo "Running $BinaryPath..."
        set +e
        $BinaryPath
        set -e
        LogInfo "Program exited with return code $?."
    fi
fi

# Start XCode:
if $Xcode && [ "$Generator" = "Xcode" ]; then
    LogInfo "Opening Xcode project $XcProjectFile"
    open -a Xcode "$XcProjectFile"
fi
