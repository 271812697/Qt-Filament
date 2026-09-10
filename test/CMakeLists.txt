set(TARGET_NAME TestFilament)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE ${TARGET_NAME}_HEAD ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE ${TARGET_NAME}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${${TARGET_NAME}_HEAD} ${${TARGET_NAME}_SRC})
qt5_add_resources(RESOURCES ./resource/resource.qrc)
add_executable(${TARGET_NAME} ${${TARGET_NAME}_HEAD} ${${TARGET_NAME}_SRC} ${RESOURCES})
target_compile_options(${TARGET_NAME} PRIVATE /bigobj)
target_compile_options(${TARGET_NAME} PRIVATE /utf-8)
# Link dependencies    
target_link_libraries(${TARGET_NAME} PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::OpenGL
    Qt${QT_VERSION_MAJOR}::Svg
)
target_link_libraries(${TARGET_NAME} PUBLIC glad)
target_link_libraries(${TARGET_NAME} PUBLIC spdlog)
target_link_libraries(${TARGET_NAME} PUBLIC filament)
target_link_libraries(${TARGET_NAME} PUBLIC bluegl)
target_link_libraries(${TARGET_NAME} PUBLIC filameshio)
target_link_libraries(${TARGET_NAME} PUBLIC filament-iblprefilter)
target_link_libraries(${TARGET_NAME} PUBLIC imageio)
target_link_libraries(${TARGET_NAME} PUBLIC image)
target_link_libraries(${TARGET_NAME} PUBLIC stb)
target_link_libraries(${TARGET_NAME} PUBLIC camutils
suzanne-resources
sample-resources
)

target_include_directories(${TARGET_NAME} PUBLIC $<BUILD_INTERFACE: ${Extern_DIR}> $<BUILD_INTERFACE: ${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_BINARY_DIR}/filament/samples")
set_property(TARGET ${TARGET_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
set_target_properties(${TARGET_NAME} PROPERTIES CXX_STANDARD 20)

set(QtPluginsDir "${QT_DIR}/../../../plugins")
file(GLOB QtPluginIconEnginesDLLs  "${QtPluginsDir}/iconengines/qsvgicon*.dll")
file(GLOB QtPluginImageFormatsDLLs "${QtPluginsDir}/imageformats/qsvg*.dll")
file(GLOB QtPluginPlatformsDLLs    "${QtPluginsDir}/platforms/qwindows*.dll")
set(QtPluginsDLLs ${QtPluginIconEnginesDLLs} ${QtPluginImageFormatsDLLs} ${QtPluginPlatformsDLLs})
foreach(QtPluginDLL ${QtPluginsDLLs})
    cmake_path(GET QtPluginDLL PARENT_PATH QtPluginDLL_Path)
    cmake_path(GET QtPluginDLL_Path FILENAME QtPluginDLL_PathName)
    add_custom_command(
        TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET_NAME}>/plugins/${QtPluginDLL_PathName}"
    )
    add_custom_command(
        TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QtPluginDLL}" "$<TARGET_FILE_DIR:${TARGET_NAME}>/plugins/${QtPluginDLL_PathName}"
    )
endforeach()

# 拷贝运行所需的 Qt DLL 到 exe 目录（Release/Debug 各自对应）
set(QtBinDir "${QT_DIR}/../../../bin")
set(QtRuntimeDLLs
    Qt5Core.dll
    Qt5Gui.dll
    Qt5Widgets.dll
    Qt5OpenGL.dll
    Qt5Svg.dll
)
foreach(QtRuntimeDLL ${QtRuntimeDLLs})
    add_custom_command(
        TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QtBinDir}/${QtRuntimeDLL}"
                "$<TARGET_FILE_DIR:${TARGET_NAME}>/${QtRuntimeDLL}"
    )
endforeach()
