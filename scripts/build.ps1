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
    # Re-run CMake before building. Implied by -Clean.
    [switch] $CMake,
    # Specifies the CMake generator to use. Ninja is the default. Use "MSBuild" to generate a
    # project for the latest instaled version of Visual Studio.
    [string] $Generator = "Ninja",
    # Specifies the CPU architecture for which the program should be built.
    [ValidateSet("Native", "x86", "x64", "arm")] [string] $Arch = "Native",
    # Use Clang-CL instead of the Microsoft compiler.
    [switch] $Clang,
    # Force the LLVM toolchain to be downloaded and unpacked again.
    [switch] $RedownloadLLVM,
    # Re-generate the OpenGL loader before building. Implied by -Clean.
    [switch] $GLLoader,
    # Generate a release build, i.e. enable optimizations.
    [switch] $Release,
    # Omit debug information for release builds.
    [switch] $NoDebugInfo,
    # Run the program after compilation.
    [switch] $Run,
    # Run the program under RenderDoc after compilation.
    [switch] $RenderDoc,
    # Run the program under WinDbg after compilation.
    [switch] $WinDbg,
    # Generate and open a Visual Studio project. Implies -Generator MSBuild.
    [switch] $VS
)

# Stop script if an error is encountered:
$ErrorActionPreference = "Stop"

# Process arguments:
if ($Help) {
    Get-Help $PSCommandPath -Detailed
    Exit
}
if ([bool]$Run + [bool]$VS + [bool]$RenderDoc + [bool]$WinDbg -gt 1) {
    LogWarn("Options -Run, -VS, -RenderDoc and -WinDbg are mutually exclusive.")
    Exit 1
}
if ($VS) {
    $Generator = "MSBuild"
}

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

# Quits the script with exit code 1, performing any required cleanup.
function Panic {
    param([Parameter(Position=0)] $msg = "")
    LogError $msg
    Exit 1
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

# Functions for timing.
[System.Collections.ArrayList] $TimerNames = @()
$Timers = @{}
function StartTimer {
    param ([Parameter(Mandatory=$TRUE, Position=0)] $Name)
    $Stopwatch = $Timers[$Name]
    if ($Stopwatch) {
        $Stopwatch.Start()
    } else {
        $TimerNames.Add($Name) | Out-Null
        $Timers.Add($Name, [Diagnostics.Stopwatch]::StartNew())
    }
}
function StopTimer {
    param ([Parameter(Mandatory=$TRUE, Position=0)] $Name)
    $Stopwatch = $Timers[$Name]
    if ($Stopwatch) {
        $Stopwatch.Stop()
    }
}
function GetTimerMs {
    param ([Parameter(Mandatory=$TRUE, Position=0)] $Name)
    $Stopwatch = $Timers[$Name]
    if ($Stopwatch) {
        return [Math]::Round($Stopwatch.Elapsed.TotalMilliseconds)
    } else {
        return 0
    }
}

function LoadVisualStudio2017 {
    param([Parameter(Mandatory=$TRUE, Position=0)] [string] $HostArch,
          [Parameter(Mandatory=$TRUE, Position=1)] [string] $TargetArch)

    if (("${env:VSCMD_ARG_HOST_ARCH}" -eq $HostArch) -and
        ("${env:VSCMD_ARG_TGT_ARCH}"  -eq $TargetArch)) {
        return
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
    ForEach-Object {
        if ($_ -match "=") {
            $Var = $_.split("=")
            $VarName  = $Var[0]
            $VarValue = $Var[1]
            Set-Item -force -path "ENV:\$VarName" -value "$VarValue"
        }
    }

    # Sanity check:
    if (-not (
        ("${env:VisualStudioVersion}" -eq "15.0")       -and
        ("${env:VSCMD_ARG_HOST_ARCH}" -eq $HostArch)    -and
        ("${env:VSCMD_ARG_TGT_ARCH}"  -eq $TargetArch)))
    {
        Panic "VsDevCmd failed to load the correct environment."
    }
}

try {
    StartTimer("Main")

    # Configuration:
    $RepositoryRoot = (Resolve-Path "$PSScriptRoot/..").Path
    $BuildDN   = "build"    ; $BuildRoot   = "$RepositoryRoot/$BuildDN"
    $LibraryDN = "lib"      ; $LibraryRoot = "$RepositoryRoot/$LibraryDN"
    $SourceDN  = "src"      ; $SourceRoot  = "$RepositoryRoot/$SourceDN"
    $WorkingDN = "run"      ; $WorkingRoot = "$RepositoryRoot/$WorkingDN"
    $ProjectName = "VX"     # CMake project name
    $TargetName = "Game"    # CMake executable target name

    # Switch to repository root:
    Push-Location $RepositoryRoot

    # Log the build script's output to a file.
    # NOTE: External commands must be piped into Out-Host to show up in the log.
    $BuildLog     = "$BuildRoot/build.log"
    $BuildLastLog = "$BuildRoot/build.prev.log"
    if (Test-Path $BuildLog) {
        Move-Item -Force $BuildLog $BuildLastLog
    }
    Start-Transcript $BuildLog | Out-Null

    # Machine information:
    $HostArch = "x86"
    if ([System.Environment]::Is64BitOperatingSystem) { $HostArch = "x64" }

    # Build information:
    $TargetArch = $Arch.ToLower().Replace("native", $HostArch)
    $BuildType = "Debug"
    if ($Release) { $BuildType = "RelWithDebInfo" }
    if ($Release -and $NoDebugInfo) { $BuildType = "Release" }

    # Load Visual Studio:
    StartTimer "Load Visual Studio environment"
    LoadVisualStudio2017 $HostArch $TargetArch
    StopTimer "Load Visual Studio environment"

    # Get the correct generator names:
    if ($Generator.ToLower() -eq "msbuild") {
        switch ($env:VisualStudioVersion) {
            "15.0" {
                if ($TargetArch -eq "x64") { $Generator = "Visual Studio 15 2017 Win64" }
                if ($TargetArch -eq "x86") { $Generator = "Visual Studio 15 2017" }
                break
            }
        }
    }
    $ShortGenerator = $Generator.ToLower().Replace("-", "").Replace(" ", "-")
    if ($ShortGenerator.StartsWith("visual-studio")) {
        switch ($env:VisualStudioVersion) {
            "15.0" { $ShortGenerator = "msvc" }
        }
    }

    # Build directory:
    $BuildDir = "$BuildRoot/win-$TargetArch-$ShortGenerator"
    if ($Clang) { $BuildDir = "$BuildDir-clang" }
    if ($ShortGenerator -ne "msvc") { $BuildDir = "$BuildDir-$($BuildType.ToLower())"}

    # Build artifact paths:
    $BinaryPath = "$BuildDir/$TargetName.exe"
    if ($ShortGenerator -eq "msvc") { $BinaryPath = "$BuildDir/$BuildType/$TargetName.exe" }
    $VsSolutionFile = "$BuildDir/$ProjectName.sln"

    # Clean build directory:
    function CleanDir {
        param([Parameter(Mandatory=$TRUE, Position=0)] [string] $Dir)
        if (Test-Path -PathType Container $Dir) {
            LogInfo "Cleaning build directory: $Dir"
            Remove-Item -Recurse -Force "$Dir"
        }
    }
    function CleanFile {
        param([Parameter(Mandatory=$TRUE, Position=0)] [string] $File)
        if (Test-Path -PathType Leaf $File) {
            LogInfo "Cleaning build file: $File"
            Remove-Item "$File"
        }
    }
    if ($Clean) {
        StartTimer "Clean build directory"
        CleanDir "$BuildDir"
        CleanDir "$BuildRoot/include"
        CleanFile "$BuildRoot/tree.txt"
        StopTimer "Clean build directory"
    }

    # Determine whether or not CMake has to be run manually, using the following heuristics:
    # 1. Has the user passed -CMake to the script?
    # 2. Will we launch Visual Studio? (The project files should be up-to-date for that.)
    # 3. Does a valid makefile for the selected generator exist yet?
    # 4. Has the structure of the src directory changed since the last build?
    # Returns either an empty string or one containing the reason CMake should be run.
    if ($CMake) {
        $CMakeReason = "script -CMake flag set"
    } elseif ($VS) {
        $CMake = $TRUE
        $CMakeReason = "launching Visual Studio"
    } elseif ($ShortGenerator -eq "msvc" -and -not (Test-Path "$VsSolutionFile")) {
        $CMake = $TRUE
        $CMakeReason = "solution $ProjectName.sln not found"
    } elseif ($ShortGenerator -eq "ninja" -and -not (Test-Path "$BuildDir/build.ninja")) {
        $CMake = $TRUE
        $CMakeReason = "makefile build.ninja not found"
    } else {
        StartTimer "Check for source tree changes"
        $LastTree = (Get-Content -Raw "$BuildRoot/tree.txt" -ErrorAction Ignore)
        $ThisTree = (tree /f /a "$SourceRoot" | Out-String)
        CheckExitCode
        if ($ThisTree -ne $LastTree) {
            Set-Content -NoNewline $ThisTree "$BuildRoot/tree.txt"
            $CMake = $TRUE
            $CMakeReason = "source tree changed"
        }
        StopTimer "Check for source tree changes"
    }

    # Make directories:
    mkdir -Force "$BuildRoot"           | Out-Null
    mkdir -Force "$BuildRoot/include"   | Out-Null
    mkdir -Force "$BuildDir"            | Out-Null

    # Download and unpack LLVM/Clang if requested:
    if ($Clang) {
        $LLVMVersion = "8.0.0"
        $LLVMPlatform = $HostArch.Replace("x86", "win32").Replace("x64", "win64")
        $LLVMPackageName = "LLVM-$LLVMVersion-$LLVMPlatform"
        $LLVMDownloadURL = "https://releases.llvm.org/$LLVMVersion/$LLVMPackageName.exe"
        $LLVMDirectory = "$BuildRoot/$LLVMPackageName"
        $LLVMInstaller = "$BuildRoot/$LLVMPackageName.exe"

        $7z = (Get-Command "7z" -ErrorAction Ignore).Path
        if (-not $7z) { $7z = Resolve-Path "$env:PROGRAMFILES\7-Zip\7z.exe" -ErrorAction Ignore }
        if (-not $7z) { Panic "7-Zip is required to extract the LLVM toolchain." }

        if ($RedownloadLLVM -or -not ((Test-Path $LLVMDirectory) -or (Test-Path $LLVMInstaller))) {
            StartTimer "LLVM download"
            Set-Location "$BuildRoot"
            LogInfo "Downloading LLVM installer from $LLVMDownloadURL..."
            # NOTE: curl is quite a bit faster.
            # FIXME: if the URL is wrong this will just download a 404 page without complaining.
            if (CommandExists "curl.exe") {
                curl.exe -o $LLVMInstaller $LLVMDownloadURL
                CheckExitCode
            }
            else {
                Invoke-WebRequest -OutFile $LLVMInstaller $LLVMDownloadURL
            }
            StopTimer "LLVM download"
        }

        if ($RedownloadLLVM -or -not (Test-Path $LLVMDirectory)) {
            StartTimer "LLVM unpack"
            mkdir "$LLVMDirectory" | Out-Null
            Set-Location "$LLVMDirectory"
            & $7z x "$LLVMInstaller" | Out-Host
            CheckExitCode
            StopTimer "LLVM unpack"
        }
    }

    # Run CMake:
    if ($CMake) {
        StartTimer "Generate build files with CMake"
        Set-Location "$BuildDir"
        if ($Clang) {
            LogInfo "Running CMake ($CMakeReason) with generator `"$Generator`" for Clang..."
            cmake "$RepositoryRoot" -G "$Generator" -DCMAKE_BUILD_TYPE="$BuildType" `
                -DCMAKE_C_COMPILER:PATH="$LLVMDirectory/bin/clang-cl.exe" `
                -DCMAKE_CXX_COMPILER:PATH="$LLVMDirectory/bin/clang-cl.exe" `
                -DCMAKE_LINKER:PATH="$LLVMDirectory/bin/lld-link.exe" `
            CheckExitCode
        } else {
            LogInfo "Running CMake ($CMakeReason) with generator `"$Generator`"..."
            cmake "$RepositoryRoot" -G "$Generator" -DCMAKE_BUILD_TYPE="$BuildType"
            CheckExitCode
        }
        StopTimer "Generate build files with CMake"
    }

    # Build OpenGL loader:
    $GLLoaderPath = "$BuildRoot/include/glad"
    $GLADLocation = "$LibraryRoot/glad"
    if ($GLLoader -or -not (Test-Path $GLLoaderPath)) {
        StartTimer "Generate OpenGL loader with GLAD"
        if (-not (Test-Path "$GLADLocation/glad/__init__.py")) {
            Panic "Can't build OpenGL loader: GLAD not found at $GLADLocation."
        }
        if (-not (CommandExists "python")) {
            Panic "Can't build OpenGL loader: Python is not installed or not in PATH."
        }
        LogInfo "Building OpenGL loader..."
        Set-Location "$GLADLocation"
        python -m glad --out-path "$GLLoaderPath" --generator c-debug --local-files --api "gl=4.1" `
            --profile core --reproducible | Out-Host
        CheckExitCode
        StopTimer "Generate OpenGL loader with GLAD"
    }

    # Run build tool:
    if (-not $VS) {
        Set-Location $BuildDir
        switch ($ShortGenerator) {
            "msvc" {
                StartTimer "Build with MSBuild"
                LogInfo "Running MSBuild for configuration $BuildType..."
                msbuild -nologo ALL_BUILD.vcxproj -p:Configuration=$BuildType | Out-Host
                CheckExitCode
                StopTimer "Build with MSBuild"
            }
            "ninja" {
                StartTimer "Build with Ninja"
                LogInfo "Running Ninja for configuration $BuildType..."
                if ($Verbose) { ninja -v | Out-Host }
                else          { ninja    | Out-Host }
                CheckExitCode
                StopTimer "Build with Ninja"
            }
        }
    }

    # Write out performance info:
    StopTimer "Main"
    LogInfo "Build script timing information:"
    $TimerNameMaxLen = 0
    $TotalTimeTracked = 0
    $TimerNames.ForEach({
        if ($_.Length -gt $TimerNameMaxLen) { $TimerNameMaxLen = $_.Length }
    })
    $TimerNames.ForEach({
        if ($_ -ne "Main") {
            $Ms = GetTimerMs $_
            if ($Ms -gt 0) {
                Write-Host "* $($_.PadRight($TimerNameMaxLen)) :: $Ms ms"
                $TotalTimeTracked += $Ms
            }
        }
    })
    $Main = GetTimerMs "Main"
    LogInfo "Total: $Main ms ($($Main - $TotalTimeTracked) ms untracked)"

    # Run game:
    if ($Run) {
        Set-Location "$WorkingRoot"
        LogInfo "Set working directory to $PWD."
        LogInfo "Running $BinaryPath..."
        & $BinaryPath
        LogInfo "Program exited with return code $LASTEXITCODE."
    }

    # Launch Visual Studio:
    if ($VS) {
        LogInfo "Launching Visual Studio for $VsSolutionFile..."
        & devenv.exe $VsSolutionFile
    }

    # Launch RenderDoc:
    if ($RenderDoc) {
        $Rd = (Get-Command "qrenderdoc" -ErrorAction Ignore).Path
        if (-not $Rd) {
            $Rd = Resolve-Path "$env:PROGRAMFILES\RenderDoc\qrenderdoc.exe" -ErrorAction Ignore
        }
        if (-not $Rd) { Panic "RenderDoc is not installed or not in your PATH." }
        # Generate cap file:
        $CapFile = "$BuildRoot/renderdoc.cap"
        $Template = Get-Content -Raw "$PSScriptRoot/renderdoc.cap"
        $Template = $Template.Replace("__PLACEHOLDER__EXECUTABLE__", $BinaryPath.Replace("\", "\\"))
        $Template = $Template.Replace("__PLACEHOLDER__WORKINGDIR__", $WorkingRoot.Replace("\", "\\"))
        Set-Content -NoNewline $Template $CapFile
        # Launch:
        LogInfo "Launching RenderDoc ($Rd)..."
        & $Rd $CapFile
    }

    # Launch WinDbg:
    if ($WinDbg) {
        $Wd = (Get-Command "windbg" -ErrorAction Ignore).Path
        if (-not $Wd -and ($HostArch -eq "x64")) {
            $Wd = Resolve-Path "${env:PROGRAMFILES(X86)}/Windows Kits/10/Debuggers/x64/windbg.exe" `
                -ErrorAction Ignore
        }
        if (-not $Wd -and ($HostArch -eq "x86")) {
            $Wd = Resolve-Path "${env:PROGRAMFILES(X86)}/Windows Kits/10/Debuggers/x86/windbg.exe"`
                -ErrorAction Ignore
        }
        if (-not $Wd -and ($HostArch -eq "x86")) {
            $Wd = Resolve-Path "${env:PROGRAMFILES}/Windows Kits/10/Debuggers/x86/windbg.exe"`
                -ErrorAction Ignore
        }
        if (-not $Wd) { Panic "WinDbg is not installed or not in your PATH." }
        Set-Location "$WorkingRoot"
        LogInfo "Set working directory to $PWD."
        LogInfo "Launching WinDbg for $BinaryPath..."
        & $Wd $BinaryPath
    }

} finally {
    Pop-Location
    Stop-Transcript -ErrorAction Ignore | Out-Null
}