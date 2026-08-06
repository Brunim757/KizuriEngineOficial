add_executable(KizuriEditor
    src/EditorApp.cpp
    src/EditorLayer.cpp
    src/UI/Icons.cpp
    src/UI/Panels/ProfilerPanel.cpp
    src/UI/Panels/GameViewPanel.cpp
    src/UI/Panels/MaterialEditorPanel.cpp
    src/UI/Panels/AnimatorPanel.cpp
    src/UI/Panels/ProjectSettingsPanel.cpp
)
target_link_libraries(KizuriEditor PRIVATE KizuriEngine)
target_include_directories(KizuriEditor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ImGuizmo — gizmo de transformação no viewport. É só uma UI de edição, não
# tem lugar no runtime do jogo, então fica no editor e não na engine.
# O código-fonte vive em src/ dentro do repo (não na raiz).
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

# As fontes da UI (JetBrains Mono) agora são embutidas direto no executável
# em tempo de build (ver engine/CMakeLists.txt + cmake/EmbedResource.cmake),
# então não há mais uma pasta assets/ pra copiar pra junto do binário.

if(WIN32)
    target_link_options(KizuriEditor PRIVATE $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-mwindows>)
    set_target_properties(KizuriEditor PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
endif()
