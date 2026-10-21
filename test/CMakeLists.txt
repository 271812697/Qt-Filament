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
# 项目自带的多个材质：用同一棵 Filament 树里的 matc 编译，再用 resgen 嵌入。
# 远端机器上的 Filament 版本无论是什么，材质包版本都与链接的 library 匹配。
# ---------------------------------------------------------------------------
set(MATERIAL_ASSET_IDS
    material_preview
    lit_opaque
    cloth
    unlit
    subsurface
)
set(MATERIAL_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/materials")
set(MATERIAL_PACKAGE_OUTPUTS)
set(MATERIAL_PACKAGE_SOURCES)

# Debug 版 matc 在复杂材质（纹理/clearCoat/refraction）上会卡住，
# 材质统一用 Release 版 matc 编译；若还没有就先构建一次 Release matc。
if(CMAKE_CONFIGURATION_TYPES)
    set(MATC_RELEASE "${CMAKE_BINARY_DIR}/filament/tools/matc/Release/matc.exe")
    if(EXISTS "${MATC_RELEASE}")
        set(MATC_COMMAND "${MATC_RELEASE}")
        set(MATC_DEPENDENCY "")
    else()
        set(MATC_RELEASE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/matc_release.stamp")
        add_custom_command(
            OUTPUT "${MATC_RELEASE_STAMP}"
            COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}"
                    --config Release --target matc
            COMMAND ${CMAKE_COMMAND} -E touch "${MATC_RELEASE_STAMP}"
            COMMENT "Building release matc for material compilation"
            VERBATIM
        )
        add_custom_target(matc_release DEPENDS "${MATC_RELEASE_STAMP}")
        # 先让当前配置的 Debug Filament 构建完，避免 MSBuild 同时构建
        # Debug/Release filamat 时争用同一个 combined lib 临时文件。
        add_dependencies(matc_release filament)
        add_dependencies(${TARGET_NAME} matc_release)
        set(MATC_COMMAND "${MATC_RELEASE}")
        set(MATC_DEPENDENCY "${MATC_RELEASE_STAMP}")
    endif()
else()
    set(MATC_COMMAND "$<TARGET_FILE:matc>")
    set(MATC_DEPENDENCY matc)
endif()

foreach(MATERIAL_ID IN LISTS MATERIAL_ASSET_IDS)
    set(MATERIAL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/materials/${MATERIAL_ID}.mat")
    set(MATERIAL_FILAMAT "${MATERIAL_GEN_DIR}/${MATERIAL_ID}.filamat")
    set(MATERIAL_PACKAGE "MaterialPackage_${MATERIAL_ID}")
    set(MATERIAL_HEADER "${MATERIAL_GEN_DIR}/${MATERIAL_PACKAGE}.h")
    set(MATERIAL_SOURCE_C "${MATERIAL_GEN_DIR}/${MATERIAL_PACKAGE}.c")

    add_custom_command(
        OUTPUT
            "${MATERIAL_FILAMAT}"
            "${MATERIAL_HEADER}"
            "${MATERIAL_SOURCE_C}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MATERIAL_GEN_DIR}"
        COMMAND "${MATC_COMMAND}" -a opengl -p desktop
                -o "${MATERIAL_FILAMAT}" "${MATERIAL_SOURCE}"
        COMMAND $<TARGET_FILE:resgen> -c -p "${MATERIAL_PACKAGE}" "${MATERIAL_FILAMAT}"
        WORKING_DIRECTORY "${MATERIAL_GEN_DIR}"
        DEPENDS ${MATC_DEPENDENCY} resgen "${MATERIAL_SOURCE}"
        COMMENT "Compiling project material ${MATERIAL_ID}.mat with matc"
        VERBATIM
    )

    list(APPEND MATERIAL_PACKAGE_OUTPUTS
        "${MATERIAL_FILAMAT}" "${MATERIAL_HEADER}" "${MATERIAL_SOURCE_C}")
    list(APPEND MATERIAL_PACKAGE_SOURCES "${MATERIAL_SOURCE_C}")
endforeach()

add_custom_target(material_preview_package DEPENDS ${MATERIAL_PACKAGE_OUTPUTS})
add_dependencies(${TARGET_NAME} material_preview_package)
target_sources(${TARGET_NAME} PRIVATE ${MATERIAL_PACKAGE_SOURCES})
target_include_directories(${TARGET_NAME} PRIVATE "${MATERIAL_GEN_DIR}")

# 运行时 .mat 编辑需要用同一版本的 matc；把它放到 exe 旁边。
if(DEFINED MATC_RELEASE)
    target_compile_definitions(${TARGET_NAME} PRIVATE
        TEST_MATC_PATH="${MATC_RELEASE}")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MATC_RELEASE}" "$<TARGET_FILE_DIR:${TARGET_NAME}>/matc.exe"
        COMMENT "Copying matc for runtime .mat compilation")
endif()

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
