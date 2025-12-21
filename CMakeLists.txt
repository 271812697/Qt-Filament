cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
project(TestFilament)
find_package(QT NAMES Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui OpenGL Svg)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if (WIN32)
    # Link statically against c/c++ lib to avoid missing redistriburable such as
    # "VCRUNTIME140.dll not found. Try reinstalling the app.", but give users
    # a choice to opt for the shared runtime if they want.
    option(USE_STATIC_CRT "Link against the static runtime libraries." ON)

    # On Windows we need to instruct cmake to generate the .def in order to get the .lib required
    # when linking against dlls. CL.EXE will not generate .lib without .def file (or without pragma
    # __declspec(dllexport) in front of each functions).
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

    # The CMAKE_CXX_FLAGS vars can be overriden by some Visual Studio generators, so we use an alternative
    # global method here:
    if (${USE_STATIC_CRT})
        add_compile_options(
            $<$<CONFIG:>:/MT>
            $<$<CONFIG:Debug>:/MTd>
            $<$<CONFIG:Release>:/MT>
        )
    else()
        add_compile_options(
            $<$<CONFIG:>:/MD>
            $<$<CONFIG:Debug>:/MDd>
            $<$<CONFIG:Release>:/MD>
        )
    endif()

    # TODO: Figure out why pdb generation messes with incremental compilaton.
    # IN RELEASE_WITH_DEBUG_INFO, generate debug info in .obj, no in pdb.
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Z7")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} /Z7")

    # In RELEASE, also generate PDBs.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zi")

    # In DEBUG, avoid generating a PDB file which seems to mess with incremental compilation.
    # Instead generate debug info directly inside obj files.
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Z7")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Z7")

    # Special settings when building on CI.
    if (${FILAMENT_WINDOWS_CI_BUILD})
        set(LinkerFlags
            CMAKE_SHARED_LINKER_FLAGS_DEBUG
            CMAKE_EXE_LINKER_FLAGS_DEBUG
            CMAKE_MODULE_LINKER_FLAGS_DEBUG
            )
        foreach(LinkerFlag ${LinkerFlags})
            # The /debug flag outputs .pdb files, which we don't need on CI.
            string(REPLACE "/debug" "" ${LinkerFlag} ${${LinkerFlag}})

            # The /INCREMENTAL flag outputs .ilk files for incremental linking. These are huge, and
            # we don't need them on CI.
            string(REPLACE "/INCREMENTAL" "/INCREMENTAL:NO" ${LinkerFlag} ${${LinkerFlag}})
        endforeach()

        # We turn off compile-time optimizations for CI, as options that speed up the compile-time
        # (e.g. /MP) might increase memory usage, leading to instabilities on limited CI machines.
        option(FILAMENT_SHORTEN_MSVC_COMPILATION "Shorten compile-time in Visual Studio" OFF)
    else()
        option(FILAMENT_SHORTEN_MSVC_COMPILATION "Shorten compile-time in Visual Studio" ON)
    endif()

    if (MSVC)
        if (FILAMENT_SHORTEN_MSVC_COMPILATION)
            # enable multi-processor compilation
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
            # disable run-time STL checks to improve tools (e.g. matc) performance
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_ITERATOR_DEBUG_LEVEL=0")
        endif()
    endif()
endif()
add_subdirectory(filament)
add_subdirectory(Extern)
add_subdirectory(test)
