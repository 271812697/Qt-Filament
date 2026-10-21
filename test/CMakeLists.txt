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
target_link_libraries(${TARGET_NAME} PUBLIC filaflat)
target_link_libraries(${TARGET_NAME} PUBLIC bluegl)
target_link_libraries(${TARGET_NAME} PUBLIC filameshio)
target_link_libraries(${TARGET_NAME} PUBLIC filament-iblprefilter)
target_link_libraries(${TARGET_NAME} PUBLIC imageio)
target_link_libraries(${TARGET_NAME} PUBLIC image)
target_link_libraries(${TARGET_NAME} PUBLIC stb)
target_link_libraries(${TARGET_NAME} PUBLIC camutils
)

target_include_directories(${TARGET_NAME} PUBLIC $<BUILD_INTERFACE: ${Extern_DIR}> $<BUILD_INTERFACE: ${CMAKE_CURRENT_SOURCE_DIR}/include>)
# 材质预览的 IBL 直接读取仓库里的 studio HDR，避免依赖启动时的工作目录
target_compile_definitions(${TARGET_NAME} PRIVATE
    QT_FILAMENT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_property(TARGET ${TARGET_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
set_target_properties(${TARGET_NAME} PROPERTIES CXX_STANDARD 20)

# ---------------------------------------------------------------------------
# 项目自带的材质：用同一棵 Filament 树里的 matc 编译，再用 resgen 嵌入。
# 这样远端机器上的 Filament 版本无论是什么，材质包版本都与链接的 library 匹配。
# ---------------------------------------------------------------------------
set(MATERIAL_PREVIEW_MAT "${CMAKE_CURRENT_SOURCE_DIR}/materials/material_preview.mat")
set(MATERIAL_PREVIEW_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/material_preview")
set(MATERIAL_PREVIEW_FILAMAT "${MATERIAL_PREVIEW_GEN_DIR}/material_preview.filamat")
set(MATERIAL_PREVIEW_HEADER "${MATERIAL_PREVIEW_GEN_DIR}/MaterialPreviewPackage.h")
set(MATERIAL_PREVIEW_SOURCE "${MATERIAL_PREVIEW_GEN_DIR}/MaterialPreviewPackage.c")

add_custom_command(
    OUTPUT
        "${MATERIAL_PREVIEW_FILAMAT}"
        "${MATERIAL_PREVIEW_HEADER}"
        "${MATERIAL_PREVIEW_SOURCE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MATERIAL_PREVIEW_GEN_DIR}"
    COMMAND $<TARGET_FILE:matc> -a opengl -p desktop
            -o "${MATERIAL_PREVIEW_FILAMAT}" "${MATERIAL_PREVIEW_MAT}"
    COMMAND $<TARGET_FILE:resgen> -c -p MaterialPreviewPackage "${MATERIAL_PREVIEW_FILAMAT}"
    WORKING_DIRECTORY "${MATERIAL_PREVIEW_GEN_DIR}"
    DEPENDS matc resgen "${MATERIAL_PREVIEW_MAT}"
    COMMENT "Compiling project material_preview.mat with matc"
    VERBATIM
)
add_custom_target(material_preview_package
    DEPENDS "${MATERIAL_PREVIEW_FILAMAT}" "${MATERIAL_PREVIEW_HEADER}" "${MATERIAL_PREVIEW_SOURCE}")
add_dependencies(${TARGET_NAME} material_preview_package)
target_sources(${TARGET_NAME} PRIVATE "${MATERIAL_PREVIEW_SOURCE}")
target_include_directories(${TARGET_NAME} PRIVATE "${MATERIAL_PREVIEW_GEN_DIR}")

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
