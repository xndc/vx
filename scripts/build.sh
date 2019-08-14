#!/bin/bash
set -euo pipefail

Usage() {
    echo "Linux and macOS build script for the VX Engine codebase."
    echo "Syntax: $0 [options]"
    echo ""
    echo "  -h, -?, --help  Show this message."
    echo "  -v, -Verbose    Print build command lines and other verbose log data."
    echo "  -c, -Clean      Clean the build directory before building."
    echo "  -CMake          Re-run CMake before building. Implied by -c."
    echo "  -GLLoader       Re-generate the OpenGL loader before building. Implied by -c."
    echo "  -Release        Generate a release build, i.e. enable optimizations."
    echo "  -NoDebugInfo    Omit debug information for release builds."
    echo "  -r, -Run        Run the program after compilation."
    echo "  -Xcode          Generate and open an Xcode project. Implies -g Xcode."
    echo "  -g, -Generator [Ninja|\"Unix Makefiles\"|...]"
    echo "    Specifies the CMake generator to use. Ninja is the default."
    echo ""
    echo "All options are case-insensitive, and --option can be used instead of -option."
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
    arg="$(echo "$1" | tr '[:upper:]' '[:lower:]' | sed 's/-//g')"
    case $arg in
        h | \? | help   ) Usage; exit   ;;
        v | verbose     ) Verbose=true  ;;
        c | clean       ) Clean=true    ;;
        cmake           ) CMake=true    ;;
        glloader        ) GLLoader=true ;;
        release         ) Release=true  ;;
        r | run         ) Run=true      ;;
        xcode           ) Generator="Xcode"; Run=false; Xcode=true ;;
        g | generator   ) Generator=$2; shift ;;
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
ProjectName="VX"    # CMake project name
TargetName="Game"   # CMake executable target name

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
BD1=$(uname | tr '[:upper:]' '[:lower:]' | sed 's/darwin/mac/')
BD2=$(uname -m | sed 's/x86_64/x64/' | sed 's/i.86/x86/')
BD3=$(echo "$Generator" | sed 's/ /-/g' | sed 's/-+/-/g' | tr '[:upper:]' '[:lower:]')
BD4=$(echo "$BuildType" | tr '[:upper:]' '[:lower:]')
BuildDir="$BuildRoot/$BD1-$BD2-$BD3-$BD4"

# Build artifact paths:
BinaryPath="$BuildDir/$TargetName"
XcProjectDir="$BuildDir/$ProjectName.xcodeproj"
XcSchemeFile="$XcProjectDir/xcshareddata/xcschemes/$TargetName.xcscheme"

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
    if $Xcode; then echo "launching Xcode"; return; fi
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
            if ! [ -d "$XcProjectDir" ]; then
                echo "project $ProjectName.xcodeproj not found"
                return
            fi ;;
    esac
    ls "$SourceRoot/" ** > "$BuildRoot/tree.txt.new"
    if ! [ -f "$BuildRoot/tree.txt" ] ||
         [ -n "$(diff -q "$BuildRoot/tree.txt" "$BuildRoot/tree.txt.new")" ]; then
        mv -f "$BuildRoot/tree.txt.new" "$BuildRoot/tree.txt"
        echo "source tree changed"
    fi
    if [ -f "$BuildRoot/tree.txt.new" ]; then
        mv -f "$BuildRoot/tree.txt.new" "$BuildRoot/tree.txt"
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
        Panic "Can't build OpenGL loader: Python is not installed or not in PATH."
    fi
    LogInfo "Building OpenGL loader..."
    cd "$GLADLocation"
    python -m glad --out-path "$GLLoaderPath" --generator c-debug --local-files --api "gl=3.3" \
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
        make -j "$(getconf _NPROCESSORS_ONLN)"
        ;;
    *)
        Run=false
        ;;
esac

# Run game:
BinaryPath="$BuildDir/$TargetName"
if $Run; then
    cd "$WorkingRoot"
    if [ -x "$BinaryPath" ]; then
        LogInfo "Set working directory to $PWD."
        LogInfo "Running $BinaryPath..."
        set +e
        "$BinaryPath"
        set -e
        LogInfo "Program exited with return code $?."
    fi
fi

# Start XCode:
if [ "$Generator" = "Xcode" ]; then
    if [ -f "$XcSchemeFile" ]; then
        # NOTE: CMake generates scheme files with XML attributes that look like "name=value", but
        #       Xcode rewrites them as "name = value" when changing any settings.
        if grep -E 'useCustomWorkingDirectory ?= ?"NO"' "$XcSchemeFile" >/dev/null; then
            LogInfo "Updating working directory for Xcode project..."
            awk '{gsub(/useCustomWorkingDirectory ?= ?"NO"/, "useCustomWorkingDirectory=\"YES\"\n      customWorkingDirectory=\"$PROJECT_DIR/run\"")}1' \
                "$XcSchemeFile" > "$XcSchemeFile.new"
            mv -f "$XcSchemeFile.new" "$XcSchemeFile"
        fi
    else
        LogWarn "Xcode scheme file $XcSchemeFile not found."
    fi
fi
if $Xcode && [ "$Generator" = "Xcode" ]; then
    LogInfo "Opening Xcode project $XcProjectDir"
    open -a Xcode "$XcProjectDir"
fi
