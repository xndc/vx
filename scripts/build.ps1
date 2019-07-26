<#
.SYNOPSIS
    Windows build script for the VX Engine codebase.
.DESCRIPTION
    This is the main Windows/PowerShell build script for VX. It performs the following tasks:
    * Sets up the correct environment for building software with the Microsoft Visual C++ tools.
    * Downloads and unpacks the LLVM Clang compiler if requested.
    * Uses CMake to generate a Ninja or MSBuild project that can be used to build VX.
    * Uses GLAD to generate an OpenGL loader for use with the GLFW library.
    * Performs the actual build process.
    * Runs the VX Engine or launches an external debugging tool, if requested.
#>

param (
    # Show the help message for this script.
    [switch] $Help,
    # Print build command lines and other verbose log data.
    [switch] $Verbose,
    # Clean the build directory before building.
    [switch] $Clean,
    # Re-run CMake before building.
    [switch] $CMake,
    # Re-generate the OpenGL loader before building.
    [switch] $GLLoader,
    # Generate a release build, i.e. enable optimizations.
    [switch] $Release,
    # Omit debug information for release builds.
    [switch] $NoDebugInfo,
    # Use MSBuild instead of the Ninja build system.
    [switch] $MSBuild,
    # Use Clang-CL instead of the Microsoft compiler.
    [switch] $Clang,
    # Run the program after compilation.
    [switch] $Run,
    # Run the program under RenderDoc after compilation.
    [switch] $RenderDoc,
    # Run the program under WinDbg after compilation.
    [switch] $WinDbg,
    # Generate an MSBuild project and open it in Visual Studio.
    [switch] $VS,
    # The architecture for which the program should be built.
    [ValidateSet("Native", "x86", "x64", "arm")] [string] $Arch = "Native"
)

# Process arguments:

if ($Help) {
    Get-Help $PSCommandPath -Detailed
    Exit
}
if ($VS) {
    $MSBuild = $TRUE
}

# Stop script if an error is encountered:
$ErrorActionPreference = "Stop"

# Configuration:
$RepositoryRoot = (Resolve-Path "$PSScriptRoot/..").Path
$BuildDir   = "build"   ; $BuildRoot   = "$RepositoryRoot/$BuildDir"
$LibraryDir = "lib"     ; $LibraryRoot = "$RepositoryRoot/$LibraryDir"
$SourceDir  = "source"  ; $SourceRoot  = "$RepositoryRoot/$SourceDir"
$WorkingDir = "run"     ; $WorkingRoot = "$RepositoryRoot/$WorkingDir"
$BinaryName  = "Game"
$ProjectName = "Game"

# Switch to repository root:
pushd $RepositoryRoot

# Log the build script's output to a file.
# NOTE: External commands must be piped into Out-Host to show up in the log.
$BuildLog     = "$BuildRoot/build.log"
$BuildLastLog = "$BuildRoot/build.lastlog"
if (Test-Path $BuildLog) {
    Move -Force $BuildLog $BuildLastLog
}
Start-Transcript $BuildLog | Out-Null

## *************************************************************************************************
## Helper functions:

# Functions for logging a line of text.
function LogInfo {
    param([Parameter(Position=0)] $msg = "")
    Write-Host -ForegroundColor Magenta $msg
}
function LogWarn {
    param([Parameter(Position=0)] $msg = "")
    Write-Host -ForegroundColor Yellow $msg
}
function LogError {
    param([Parameter(Position=0)] $msg = "")
    Write-Host -ForegroundColor Red $msg
}

# Functions for timing.
$_Timers = @{}
function StartTimer {
    param ([Parameter(Mandatory=$TRUE, Position=0)] $Name)
    $Stopwatch = $_Timers[$Name]
    if ($Stopwatch) {
        $Stopwatch.Start()
    } else {
        $_Timers.Add($Name, [Diagnostics.Stopwatch]::StartNew())
    }
}
function StopTimer {
    param ([Parameter(Mandatory=$TRUE, Position=0)] $Name)
    $Stopwatch = $_Timers[$Name]
    if ($Stopwatch) {
        $Stopwatch.Stop()
    }
}
function GetTimerMs {
    param ([Parameter(Mandatory=$TRUE, Position=0)] $Name)
    $Stopwatch = $_Timers[$Name]
    if ($Stopwatch) {
        return [Math]::Round($Stopwatch.Elapsed.TotalMilliseconds)
    } else {
        return 0
    }
}

# Performs pre-exit cleanup. Should be run before the script exits.
function PreExitCleanup {
    popd
    Stop-Transcript | Out-Null
}

# Quits the script with exit code 1, performing any required cleanup.
function Panic {
    param([Parameter(Position=0)] $msg = "")
    LogError $msg
    Exit $1
}

# Asserts that a condition is true. Quite the script otherwise.
function Assert {
    param([Parameter(Position=0, Mandatory=$TRUE)] [bool] $cond,
          [Parameter(Position=1)] [string] $msg = "Unknown assertion failed.")
    if ($cond) { Panic $msg }
}

# Asserts that the last command exited correctly. Quits the script otherwise.
function CheckExitCode {
    if ($LASTEXITCODE -ne 0) { Panic "Command exited with exit code $LASTEXITCODE." }
}

# Returns true if the specified command exists.
function CommandExists {
    param ([Parameter(Mandatory=$TRUE, Position=0)] $cmd)
    return ((Get-Command $cmd -ErrorAction Ignore).length -ne 0)
}

# Returns the name of this machine's CPU architecture (x86 or x64).
function GetHostArchitecture {
    if ([System.Environment]::Is64BitOperatingSystem) { return "x64" }
    else { return "x86" }
}

# Returns the name of the host architecture that should be used for this build.
function GetTargetArchitecture {
    if ($Arch -eq "Native") { return GetHostArchitecture }
    else { return $Arch }
}

## *************************************************************************************************
## Function to locate and load the Visual Studio command line environment:

function LoadVisualStudio {
    param([Parameter(Position=0)] [string] $HostArch   = "Native",
          [Parameter(Position=1)] [string] $TargetArch = "Native")
    $HostArch = $HostArch.Replace("Native", (GetHostArchitecture))
    $TargetArch = $TargetArch.Replace("Native", (GetTargetArchitecture))

    if (FindVisualStudio2017 $HostArch $TargetArch) { return }

    Panic "Failed to find any supported version of Visual Studio installed."
}

function FindVisualStudio2017 {
    param([Parameter(Mandatory=$TRUE, Position=0)] [string] $HostArch,
          [Parameter(Mandatory=$TRUE, Position=1)] [string] $TargetArch)

    if (("${env:VisualStudioVersion}" -eq "15.0") -and
        ("${env:VSCMD_ARG_HOST_ARCH}" -eq $HostArch) -and
        ("${env:VSCMD_ARG_TGT_ARCH}"  -eq $TargetArch)) {
        LogInfo "Visual Studio 2017 is already loaded for host $HostArch and target $TargetArch."
        return $TRUE
    }

    $VSDir = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2017\Community\Common7\Tools"
    if (-not (test-path $VSDir)) { return $FALSE }

    # VsDevCmd expects "amd64" instead of "x64":
    $VDCHostArch = $HostArch.Replace("x64", "amd64")
    $VDCTargetArch = $TargetArch.Replace("x64", "amd64")

    # Run the VsDevCmd script and update our environment variables according to what it does.
    LogInfo "Loading Visual Studio 2017 environment for host $HostArch and target $TargetArch..."
    if ("${env:VisualStudioVersion}" -ne "") {
        LogWarn "WARNING: Another MSVC environment was previously loaded."
        LogWarn "         Repeating this will eventually lead to an `"input line is too long`" error."
        LogWarn "         If you get this error, restart PowerShell and try again."
    }
    cmd.exe /c "`"$VSDir\VsDevCmd.bat`" -host_arch=$VDCHostArch -arch=$VDCTargetArch && set" |
    foreach {
        if ($_ -match "=") {
            $Var = $_.split("=")
            $VarName  = $Var[0]
            $VarValue = $Var[1]
            Set-Item -force -path "ENV:\$VarName" -value "$VarValue"
        }
    }

    # Sanity check:
    if (("${env:VisualStudioVersion}" -eq "15.0") -and
        ("${env:VSCMD_ARG_HOST_ARCH}" -eq $HostArch) -and
        ("${env:VSCMD_ARG_TGT_ARCH}"  -eq $TargetArch)) {
        return $TRUE
    } else {
        Panic "VsDevCmd failed to load the correct environment."
    }
}

## *************************************************************************************************
## Function to download and unpack the LLVM/Clang toolchain:

# Configuration:
$ClangVersion = "8.0.0"
$ClangInstaller = "LLVM-$ClangVersion-$Platform.exe"
$ClangDirectory = "LLVM-$ClangVersion-$Platform"
$ClangURL = "https://releases.llvm.org/$ClangVersion/$ClangInstaller"

function DownloadAndUnpackClang {
    param([Parameter(Position=0)] [string] $HostArch = "Native")
    $HostArch = $HostArch.Replace("Native", (GetHostArchitecture))
    switch ($HostArch) {
        "x86" { $Platform = "win32"; break }
        "x64" { $Platform = "win64"; break }
        default { Panic "Unknown host architecture $HostArch" }
    }

    $7ZPath = "$env:PROGRAMFILES\7-Zip\7z.exe"
    if (CommandExists "7z") {
        $7Z = (Get-Command "7z")[0].Path
    } elseif (Test-Path $7ZPath) {
        $7Z = $7ZPath
    } else {
        Panic "7-Zip is required to extract the LLVM/Clang toolchain."
    }

    pushd $BuildRoot
    if (-not (Test-Path $ClangDirectory)) {
        # Download the installer:
        if (-not (Test-Path $ClangInstaller)) {
            StartTimer "DownloadClang"
            LogInfo "Downloading LLVM/Clang installer from $ClangURL..."
            # We use curl if available, since it's a lot faster than Invoke-WebRequest.
            if (CommandExists "curl.exe") {
                curl.exe -o $ClangInstaller $ClangURL
                CheckExitCode
            } else {
                Invoke-WebRequest -OutFile $ClangInstaller $ClangURL
            }
            StopTimer "DownloadClang"
        }
        # Unpack:
        # NOTE: What happens if the download is incomplete?
        StartTimer "UnpackClang"
        LogInfo "Extracting LLVM/Clang toolchain into $BuildDir/$ClangDirectory..."
        mkdir $ClangDirectory
        pushd $ClangDirectory
        7z x "$BuildRoot/$ClangInstaller" | Out-Host
        popd
        StopTimer "UnpackClang"
    }
    popd

    return (Resolve-Path "$BuildRoot/$ClangDirectory")
}

## *************************************************************************************************
## Helper functions for use with CMake:

# Returns a generator name for the script's given command line arguments.
function GetGeneratorName {
    if ($MSBuild) { return "msbuild" }
    else { return "ninja" }
}

# Returns the CMake build name corresponding to this script's command line arguments.
function GetBuildType {
    if ($Release) {
        if ($NoDebugInfo) { return "Release" }
        else { return "RelWithDebInfo" }
    } else {
        return "Debug"
    }
}

# Returns the CMake generator name corresponding to the currently loaded Visual Studio environment.
function GetVisualStudioGeneratorName {
    switch ($env:VisualStudioVersion) {
        "15.0" {
            if ("${env:VSCMD_ARG_TGT_ARCH}" -eq "x64") { return "Visual Studio 15 2017 Win64" }
            if ("${env:VSCMD_ARG_TGT_ARCH}" -eq "x86") { return "Visual Studio 15 2017" }
            break
        }
    }
    Panic "Visual Studio environment not loaded."
}

# Returns the correct (absolute) build directory for the script's given command line arguments.
function GetBuildDirectory {
    $BuildType = (GetBuildType).ToLower()
    if ($Clang) { $Suffix = "-clang" }
    else        { $Suffix = "" }
    switch (GetGeneratorName) {
        "msbuild" { return "$BuildRoot/windows-$(GetTargetArchitecture)-msbuild$Suffix" }
        "ninja"   { return "$BuildRoot/windows-$(GetTargetArchitecture)-ninja-$BuildType$Suffix"}
    }
}

# Determines whether or not CMake has to be run manually, using the following heuristics:
# 1. Has the user passed -CMake to the script?
# 2. Will we launch Visual Studio? (The project files should be up-to-date for that.)
# 3. Does a valid makefile for the selected generator exist yet?
# 4. Has the structure of the src directory changed since this function was last run?
# Returns either an empty string or one containing the reason CMake should be run.
function ShouldRunCMake? {
    if ($CMake) {
        return "script -CMake flag set"
    }

    if ($VS) {
        return "launching Visual Studio"
    }

    switch (GetGeneratorName) {
        "msbuild" {
            if (-not (Test-Path "$(GetBuildDirectory)/ALL_BUILD.vcxproj")) {
                return "project file ALL_BUILD.vcxproj not found"
            }
            break
        }
        "ninja" {
            if (-not (Test-Path "$(GetBuildDirectory)/build.ninja")) {
                return "makefile build.ninja not found"
            }
            break
        }
    }

    $CurrentSourceTree = (tree /f /a Source | out-string)
    $LastSourceTree = (get-content -Raw "$BuildRoot/tree.txt" -ErrorAction Ignore)
    if ($CurrentSourceTree -ne $LastSourceTree) {
        set-content -NoNewline $CurrentSourceTree "$BuildRoot/tree.txt"
        if (-not $ShouldRunCMake) {
            return "source tree changed"
        }
    }

    return ""
}

## *************************************************************************************************
## Function to display timing statistics:

# NOTE: this should be the length of the longest description string used by ShowTimingStats
$TimingMaxDescLength = 30
$TotalTimeTracked = 0

function ShowTimingStat {
    param ([Parameter(Position=0, Mandatory=$TRUE)] [string] $TimerName,
           [Parameter(Position=0, Mandatory=$TRUE)] [string] $Description)
    $ms = GetTimerMs $TimerName
    if ($ms -gt 0) {
        Write-Host "* $($Description.PadRight($TimingMaxDescLength)) :: $ms ms"
        $TotalTimeTracked += $ms
    }
}

function ShowTimingStats {
    switch (GetGeneratorName) {
        "msbuild" { $Builder = "MSBuild"; break }
        "ninja"   { $Builder = "Ninja";   break }
        default   { $Builder = GetGeneratorName }
    }
    ShowTimingStat "CleanBuildDir"      "Clean build directory"
    ShowTimingStat "LoadVisualStudio"   "Load Visual Studio environment"
    ShowTimingStat "DownloadClang"      "Download LLVM/Clang installer"
    ShowTimingStat "UnpackClang"        "Unpack LLVM/Clang toolchain"
    ShowTimingStat "SourceTreeCompare"  "Check for source tree changes"
    ShowTimingStat "CMake"              "Generate makefile (CMake)"
    ShowTimingStat "GLAD"               "Generate OpenGL loader (GLAD)"
    ShowTimingStat "Build"              "Build program ($Builder)"
}

## *************************************************************************************************
## Main function for this script:

function Main {
    StartTimer "Main"

    mkdir -Force "$BuildRoot"           | Out-Null
    mkdir -Force "$BuildRoot/include"   | Out-Null
    mkdir -Force "$(GetBuildDirectory)" | Out-Null

    if ($Clean) {
        StartTimer "CleanBuildDir"
        LogInfo "Cleaning build directory: $(GetBuildDirectory)..."
        rm -Recurse -Force "$(GetBuildDirectory)"
        LogInfo "Cleaning build directory: $BuildRoot/include..."
        rm -Recurse -Force "$BuildRoot/include/*"
        LogInfo "Cleaning other build files in $BuildRoot..."
        rm "$BuildRoot/tree.txt"
        StopTimer "CleanBuildDir"
    }

    StartTimer "LoadVisualStudio"
    LoadVisualStudio
    StopTimer "LoadVisualStudio"

    if ($Clang) {
        DownloadAndUnpackClang
    }

    $Reason = ShouldRunCMake?
    if ($Reason) {
        StartTimer "CMake"
        pushd (GetBuildDirectory)
        switch (GetGeneratorName) {
            "ninja" {
                if ($Clang) {
                    LogInfo "Running CMake ($Reason) with generator Ninja and compiler Clang..."
                    cmake $RepositoryRoot -G Ninja `
                        -DCMAKE_BUILD_TYPE="$(GetBuildType)" `
                        -DCMAKE_C_COMPILER:PATH="$ClangDirectory/bin/clang-cl.exe" `
                        -DCMAKE_CXX_COMPILER:PATH="$ClangDirectory/bin/clang-cl.exe" `
                        -DCMAKE_LINKER:PATH="$ClangDirectory/bin/lld-link.exe" `
                    | Out-Host
                    CheckExitCode
                } else {
                    LogInfo "Running CMake ($Reason) with generator Ninja and compiler MSVC..."
                    cmake $RepositoryRoot -G Ninja -DCMAKE_BUILD_TYPE="$(GetBuildType)" | Out-Host
                    CheckExitCode
                }
                break
            }
            "msbuild" {
                $Generator = GetVisualStudioGeneratorName
                LogInfo "Running CMake ($Reason) with generator `"$Generator`"..."
                cmake $RepositoryRoot -G $Generator -DCMAKE_BUILD_TYPE="$(GetBuildType)" | Out-Host
                CheckExitCode
                break
            }
        }
        popd
        StopTimer "CMake"
    }

    $GLLoaderPath = "$BuildRoot/include/glad"
    $GLADLocation = "$LibraryRoot/glad"
    if ($GLLoader -or -not (Test-Path $GLLoaderPath)) {
        StartTimer "GLAD"
        Assert (Test-Path "$GLADLocation/glad/__init__.py") `
            "Can't build OpenGL loader: GLAD not found at $GLADLocation."
        Assert (CommandExists "python") `
            "Can't build OpenGL loader: Python is not installed or not in PATH."
        LogInfo "Building OpenGL loader..."
        pushd $GLADLocation
        python -m glad --out-path $GLLoaderPath --generator c --local-files --api "gl=4.1" `
            --profile core | Out-Host
        CheckExitCode
        popd
        StopTimer "GLAD"
    }

    if (-not $VS) {
        StartTimer "Build"
        pushd $(GetBuildDirectory)
        switch (GetGeneratorName) {
            "msbuild" {
                LogInfo "Running MSBuild for configuration $(GetBuildType)..."
                msbuild -nologo ALL_BUILD.vcxproj -p:Configuration=$(GetBuildType) | Out-Host
                CheckExitCode
                break
            }
            "ninja" {
                LogInfo "Running Ninja for configuration $(GetBuildType)..."
                if ($Verbose) { ninja -v | Out-Host }
                else { ninja | Out-Host }
                CheckExitCode
                break
            }
        }
        StopTimer "Build"
    }

    switch (GetGeneratorName) {
        "msbuild" { $BinaryPath = "$(GetBuildDirectory)/$BinaryName.exe"; break }
        "ninja"   { $BinaryPath = "$(GetBuildDirectory)/$(GetBuildType)/$BinaryName.exe"; break }
    }

    StopTimer "Main"
    $MainMs = GetTimerMs Main
    LogInfo "Build script timing information:"
    ShowTimingStats
    Write-Host "Total: $MainMs ms ($(MainMs - $TotalTimeTracked) ms untracked)"

    if ($Run) {
        cd "$WorkingRoot"
        LogInfo "Set working directory to $PWD."
        LogInfo "Running $BinaryPath..."
        & $BinaryPath
    }

    elseif ($VS) {
        LogInfo "Launching Visual Studio for $(GetBuildDirectory)/$ProjectName.vcxproj..."
        start devenv.exe "$(GetBuildDirectory)/$ProjectName.vcxproj"
    }

    elseif ($RenderDoc) {
        # Find RenderDoc:
        $RenderDoc1 = "${env:PROGRAMFILES}/RenderDoc/qrenderdoc.exe"
        if (CommandExists qrenderdoc)   { $RenderDocPath = (Get-Command qrenderdoc)[0].Path }
        elseif (Test-Path $RenderDoc1) { $RenderDocPath = $RenderDoc1 }
        else { Panic "Can't find RenderDoc." }
        # Generate cap file:
        $CapFile = "$BuildRoot/renderdoc.cap"
        $Template = Get-Content -Raw "$PSScriptRoot/renderdoc.cap"
        $Template = $Template.Replace("__PLACEHOLDER_EXECUTABLE__", $BinaryPath.Replace("\", "\\"))
        $Template = $Template.Replace("__PLACEHOLDER_WORKINGDIR__", $WorkingRoot.Replace("\", "\\"))
        Set-Content -NoNewline $Template $CapFile
        # Launch:
        LogInfo "Launching RenderDoc ($RenderDocPath)..."
        start $RenderDocPath $CapFile
    }

    elseif ($WinDbg) {
        cd "$WorkingRoot"
        LogInfo "Set working directory to $PWD."
        LogInfo "Launching WinDbg..."
        # Find WinDbg:
        $WinDbg1 = "${env:PROGRAMFILES(X86)}/Windows Kits/10/Debuggers/x64/WinDbg.exe"
        if (CommandExists windbg)   { $WinDbgPath = (Get-Command windbg)[0].Path }
        elseif (Test-Path $WinDbg1) { $WinDbgPath = $WinDbg1 }
        else { Panic "Can't find WinDbg." }
        # Launch:
        start $WinDbgPath $BinaryPath
    }
}

# Run the Main function:
try { Main }
finally { PreExitCleanup }