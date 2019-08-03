cmake_minimum_required(VERSION 3.10)
project(VX)

# Require builds to be out-of-tree (CMake is not designed for in-tree builds).
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "Please run CMake from a build directory.")
endif()

# Strip out compile options we don't need.
# MSVC unfortunately cannot handle two versions of a flag being passed on a single command line.
# Passing e.g. /W3 and /W4 will produce a warning, which is awful enough, but passing e.g. /O2 and
# /RTC1 (which is a default flag in CMake) will result in an error.
# This should affect all targets defined in this file. It's a bit heavy-handed but as far as I can
# tell there's no other way to do this (as of CMake 3.12).
# Details:
# * https://lists.llvm.org/pipermail/llvm-commits/Week-of-Mon-20150413/271182.html
# * https://docs.microsoft.com/en-us/cpp/build/reference/compiler-options?view=vs-2017

function(StripCompileFlags regex)
    string(REGEX REPLACE "${regex}" "" CF "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE "${regex}" "" CFD "${CMAKE_C_FLAGS_DEBUG}")
    string(REGEX REPLACE "${regex}" "" CFR "${CMAKE_C_FLAGS_RELEASE}")
    string(REGEX REPLACE "${regex}" "" CXXF "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "${regex}" "" CXXFD "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REGEX REPLACE "${regex}" "" CXXFR "${CMAKE_CXX_FLAGS_RELEASE}")
    set(CMAKE_C_FLAGS "${CF}" PARENT_SCOPE)
    set(CMAKE_C_FLAGS_DEBUG "${CFD}" PARENT_SCOPE)
    set(CMAKE_C_FLAGS_RELEASE "${CFR}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS "${CXXF}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS_DEBUG "${CXXFD}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS_RELEASE "${CXXFR}" PARENT_SCOPE)
endfunction()

if (MSVC)
    StripCompileFlags(" [-/]W[0-4X]")               # Warning level
    StripCompileFlags(" [-/]O[bdgistxy]?[0-2\-]?")  # Optimization level
    StripCompileFlags(" [-/]Z[7iI]")                # PDB generation
    StripCompileFlags(" [-/]RTC[1csu\-]+")          # Runtime security checks
    StripCompileFlags(" [-/]EH[sc]+")               # Exception handling
endif()

# Libraries:
# * EASTL       portable C++ STL implementation
# * GLFW        window and OpenGL context management
# * GLM         vector and matrix math library
# * dear imgui  user interface

# add_subdirectory(lib/eastl)
# add_subdirectory(lib/eastl/test/packages/EAAssert)
# add_subdirectory(lib/eastl/test/packages/EABase)
# add_subdirectory(lib/eastl/test/packages/EAStdC)
# add_subdirectory(lib/eastl/test/packages/EAThread)

set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/glfw)
if (MSVC AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    # GLFW trips some really stupid warnings with clang-cl, so let's just disable all of them.
    target_compile_options(glfw PRIVATE /W0)
endif()

# set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
# add_subdirectory(lib/glm)

# file(GLOB_RECURSE cglm_sources RELATIVE ${CMAKE_SOURCE_DIR} "lib/cglm/src/*.c")
# add_library(cglm ${cglm_sources})
# target_include_directories(cglm PUBLIC "lib/cglm/include")

add_library(imgui
    "lib/imgui/imgui.cpp"
    "lib/imgui/imgui_demo.cpp"
    "lib/imgui/imgui_draw.cpp"
    "lib/imgui/imgui_widgets.cpp"
    "lib/imgui/examples/imgui_impl_opengl3.cpp"
    "lib/imgui/examples/imgui_impl_glfw.cpp")
target_include_directories(imgui PUBLIC "lib/imgui")
target_compile_definitions(imgui PRIVATE "IMGUI_IMPL_OPENGL_LOADER_GLAD")
target_include_directories(imgui PRIVATE "build/include")
target_link_libraries(imgui PUBLIC glfw)

# Main executable target:

file(GLOB_RECURSE GameSourcesC   RELATIVE ${CMAKE_SOURCE_DIR} "src/*.c")
file(GLOB_RECURSE GameSourcesCXX RELATIVE ${CMAKE_SOURCE_DIR} "src/*.cc")
file(GLOB_RECURSE GameHeaders    RELATIVE ${CMAKE_SOURCE_DIR} "src/*.h")
add_executable(Game ${GameSourcesC} ${GameSourcesCXX} ${GameHeaders} "src/misc/main.rc")

# Headers:

target_include_directories(Game PRIVATE "src")
target_include_directories(Game PRIVATE "lib/etc")          # header-only libraries
target_include_directories(Game PRIVATE "build/include")    # auto-generated, e.g. OpenGL loader
target_include_directories(Game PRIVATE "lib/cglm/include") # cglm library

# Libraries:

target_link_libraries(Game PRIVATE glfw)
target_link_libraries(Game PRIVATE imgui)

# Compiler and linker options:

set_target_properties(Game PROPERTIES C_STANDARD 11)
set_target_properties(Game PROPERTIES C_EXTENSIONS ON)
set_target_properties(Game PROPERTIES CXX_STANDARD 14)
set_target_properties(Game PROPERTIES CXX_EXTENSIONS ON)

# Microsoft Visual C++
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Common flags:
    # * W4      more warnings
    # * Zi      generate PDB files
    # * Zo      generate extra debug information for optimized code
    # * EHsc-   disable C++ exceptions
    target_compile_options(Game PRIVATE /W4 /Zi /Zo /EHsc- /wd4100 /wd4201 /wd4127)
    # Suppressed warnings:
    target_compile_options(Game PRIVATE
        /wd4100  # unreferenced function parameters
        /wd4201  # gnu-anonymous-struct and nested-anon-types
        /wd4204  # non-constant aggregate initializer - struct x y = {a, b}
        /wd4127) # constant conditional expressions - while(1)
    # Prevent MSVCRT from nagging us to use Microsoft's non-standard "secure" functions:
    target_compile_definitions(Game PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Generate PDB files only if required:
    target_compile_options(Game PRIVATE $<$<CONFIG:Debug>:/Zi /Zo>)
    target_compile_options(Game PRIVATE $<$<CONFIG:RelWithDebInfo>:/Zi /Zo>)
    # Enable optimizations only in release mode:
    target_compile_options(Game PRIVATE $<$<CONFIG:Debug>:/Od>)
    target_compile_options(Game PRIVATE $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:/O2>)
    # Compile as console application:
    set_target_properties(Game PROPERTIES LINK_FLAGS_DEBUG
        "${LINK_FLAGS_DEBUG} /ENTRY:mainCRTStartup /SUBSYSTEM:console")
    set_target_properties(Game PROPERTIES LINK_FLAGS_RELEASE
        "${LINK_FLAGS_RELEASE} /ENTRY:mainCRTStartup /SUBSYSTEM:console")
    set_target_properties(Game PROPERTIES LINK_FLAGS_RELWITHDEBINFO
        "${LINK_FLAGS_RELWITHDEBINFO} /ENTRY:mainCRTStartup /SUBSYSTEM:console")

# Microsoft Visual C++ with Clang-CL
elseif (MSVC AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    # Common flags:
    # * W3      more warnings (but not W4, which translates to -Wextra for Clang)
    # * Zi      generate PDB files
    # * Zo      generate extra debug information for optimized code
    # * EHsc-   disable C++ exceptions
    target_compile_options(Game PRIVATE /W3 /Zi /Zo /EHsc-)
    # Suppressed warnings:
    target_compile_options(Game PRIVATE
        -Wno-nonportable-system-include-path    # trips on lots of Windows.h includes
        -Wno-nonportable-include-path           # pointless, trips on a few things
        -Wno-documentation          # trips on most documentation comments in GLFW3.h
        -Wno-reserved-id-macro      # macro names starting with _
        -Wno-writable-strings       # passing const char* to char* arguments [1]
        -Wno-unused-parameter       # unreferenced function parameters
        -Wno-gnu-anonymous-struct   # useful well-supported extension
        -Wno-nested-anon-types)     # useful well-supported extension
    # Prevent MSVCRT from nagging us to use Microsoft's non-standard "secure" functions:
    target_compile_definitions(Game PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Generate PDB files only if required:
    target_compile_options(Game PRIVATE $<$<CONFIG:Debug>:/Zi /Zo>)
    target_compile_options(Game PRIVATE $<$<CONFIG:RelWithDebInfo>:/Zi /Zo>)
    # Enable optimizations only in release mode:
    target_compile_options(Game PRIVATE $<$<CONFIG:Debug>:/Od>)
    target_compile_options(Game PRIVATE $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:/O2>)
    # Compile as console application:
    set_target_properties(Game PROPERTIES LINK_FLAGS_DEBUG
        "${LINK_FLAGS_DEBUG} /ENTRY:mainCRTStartup /SUBSYSTEM:console")
    set_target_properties(Game PROPERTIES LINK_FLAGS_RELEASE
        "${LINK_FLAGS_RELEASE} /ENTRY:mainCRTStartup /SUBSYSTEM:console")
    set_target_properties(Game PROPERTIES LINK_FLAGS_RELWITHDEBINFO
        "${LINK_FLAGS_RELWITHDEBINFO} /ENTRY:mainCRTStartup /SUBSYSTEM:console")

# Clang on Linux/Mac, GCC and probably other compilers
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Common flags:
    # * Wall    more warnings
    target_compile_options(Game PRIVATE -Wall)
    # Suppressed warnings:
    target_compile_options(Game PRIVATE
        -Wno-nonportable-include-path   # pointless, trips on a few things
        -Wno-documentation          # trips on most documentation comments in GLFW3.h
        -Wno-reserved-id-macro      # macro names starting with _
        -Wno-writable-strings       # passing const char* to char* arguments [1]
        -Wno-unused-parameter       # unreferenced function parameters
        -Wno-gnu-anonymous-struct   # useful well-supported extension
        -Wno-nested-anon-types)     # useful well-supported extension
    # Enable optimization only in release mode:
    target_compile_options(Game PRIVATE $<$<CONFIG:Debug>:-Og>)
    target_compile_options(Game PRIVATE $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-O3>)

# Fallback configuration for GCC and other GCC-like compilers
else()
    # Common flags:
    # * Wall    more warnings
    target_compile_options(Game PRIVATE -Wall)
    # Enable optimization only in release mode:
    target_compile_options(Game PRIVATE $<$<CONFIG:Debug>:-Og>)
    target_compile_options(Game PRIVATE $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-O3>)
endif()

# Enable Xcode scheme file generation. We need this in order to set the working directory before starting Xcode.
set(CMAKE_XCODE_GENERATE_SCHEME ON)

# Set the working directory and startup project for Visual Studio:
# * https://stackoverflow.com/questions/23950887/
# * https://stackoverflow.com/questions/7304625/
set_target_properties(Game PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/run")
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Game)