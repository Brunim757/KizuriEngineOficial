if(WIN32)
    enable_language(RC)
endif()

add_executable(KizuriEditor
    src/EditorApp.cpp
    src/EditorLayer.cpp
    src/Updater.cpp
    src/AndroidExporter.cpp
    src/UI/Icons.cpp
    src/UI/Panels/AnimatorPanel.cpp
    src/UI/Panels/ParticleEditorPanel.cpp
    src/UI/Panels/ProfilerPanel.cpp
    src/UI/Panels/GameViewPanel.cpp
    src/UI/Panels/MaterialEditorPanel.cpp
    src/UI/Panels/ProjectSettingsPanel.cpp
    $<$<BOOL:${WIN32}>:resources/kizuri-editor.rc>
)


target_link_libraries(KizuriEditor PRIVATE KizuriEngine libcurl_static miniz)
if(WIN32)
    target_compile_definitions(KizuriEditor PRIVATE CURL_STATICLIB)
endif()
target_include_directories(KizuriEditor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)




add_library(imguizmo STATIC ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp)
target_include_directories(imguizmo PUBLIC ${imguizmo_SOURCE_DIR}/src)
target_link_libraries(imguizmo PUBLIC imgui)
target_link_libraries(KizuriEditor PRIVATE imguizmo)

set_target_properties(KizuriEditor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND COMMAND kz_copy_mingw_runtime)
    kz_copy_mingw_runtime(KizuriEditor)
endif()





if(NOT MSVC)
    target_compile_options(KizuriEditor PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unknown-warning-option)
endif()

if(WIN32)
    target_link_options(KizuriEditor PRIVATE $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-mwindows>)
    set_target_properties(KizuriEditor PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
endif()
