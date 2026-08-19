file(GLOB_RECURSE KIZURI_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)






if(ANDROID)
    list(FILTER KIZURI_SOURCES EXCLUDE REGEX
        "src/core/ImGuiLayer\\.cpp$|src/core/FileDialog\\.cpp$|src/scripting/CoreCLRHost\\.cpp$")
else()
    
    list(FILTER KIZURI_SOURCES EXCLUDE REGEX
        "src/core/AndroidPlatform\\.cpp$|src/scripting/CoreCLRHostAndroid\\.cpp$")
endif()

file(GLOB_RECURSE KIZURI_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
)





get_filename_component(KIZURI_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)







if(ANDROID)
    add_library(glad STATIC ${KIZURI_REPO_ROOT}/third_party_generated/glad_gles/src/glad.c)
    target_include_directories(glad PUBLIC ${KIZURI_REPO_ROOT}/third_party_generated/glad_gles/include)
else()
    add_library(glad STATIC ${KIZURI_REPO_ROOT}/third_party_generated/glad/src/gl.c)
    target_include_directories(glad PUBLIC ${KIZURI_REPO_ROOT}/third_party_generated/glad/include)
endif()






set(KZ_EMBEDDED_FONTS_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/kizuri)
set(KZ_FONT_REGULAR_TTF ${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/JetBrainsMono-Regular.ttf)
set(KZ_FONT_BOLD_TTF    ${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/JetBrainsMono-Bold.ttf)
set(KZ_FONT_REGULAR_HPP ${KZ_EMBEDDED_FONTS_DIR}/EmbeddedFontRegular.hpp)
set(KZ_FONT_BOLD_HPP    ${KZ_EMBEDDED_FONTS_DIR}/EmbeddedFontBold.hpp)

add_custom_command(
    OUTPUT ${KZ_FONT_REGULAR_HPP}
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE=${KZ_FONT_REGULAR_TTF}
        -DOUTPUT_FILE=${KZ_FONT_REGULAR_HPP}
        -DVAR_NAME=kFontRegularTTF
        -P ${KIZURI_REPO_ROOT}/cmake/EmbedResource.cmake
    DEPENDS ${KZ_FONT_REGULAR_TTF} ${KIZURI_REPO_ROOT}/cmake/EmbedResource.cmake
    COMMENT "Embutindo JetBrainsMono-Regular.ttf no executável"
)
add_custom_command(
    OUTPUT ${KZ_FONT_BOLD_HPP}
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE=${KZ_FONT_BOLD_TTF}
        -DOUTPUT_FILE=${KZ_FONT_BOLD_HPP}
        -DVAR_NAME=kFontBoldTTF
        -P ${KIZURI_REPO_ROOT}/cmake/EmbedResource.cmake
    DEPENDS ${KZ_FONT_BOLD_TTF} ${KIZURI_REPO_ROOT}/cmake/EmbedResource.cmake
    COMMENT "Embutindo JetBrainsMono-Bold.ttf no executável"
)
add_custom_target(KizuriEmbeddedFonts DEPENDS ${KZ_FONT_REGULAR_HPP} ${KZ_FONT_BOLD_HPP})








set(KZ_EMBEDDED_CONTENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources/embedded_content)
set(KZ_EMBEDDED_CONTENT_HPP ${CMAKE_CURRENT_BINARY_DIR}/generated/kizuri/EmbeddedContent.gen.hpp)
set(KZ_EMBED_CONTENT_MANIFEST ${CMAKE_CURRENT_BINARY_DIR}/generated/kizuri/embedded_content.manifest)
file(WRITE ${KZ_EMBED_CONTENT_MANIFEST}
    "models/Cube.glb\t${KZ_EMBEDDED_CONTENT_DIR}/models/Cube.glb\n"
)
file(APPEND ${KZ_EMBED_CONTENT_MANIFEST}
    "skies/sky_gradient.hdr\t${KZ_EMBEDDED_CONTENT_DIR}/skies/sky_gradient.hdr\n"
)




set(KZ_EMBED_DEMO_DEPENDS "")
if(DEFINED KZ_EMBED_DEMO_DIR)
    if(EXISTS "${KZ_EMBED_DEMO_DIR}/models/Fox.glb")
        file(APPEND ${KZ_EMBED_CONTENT_MANIFEST}
            "models/Fox.glb\t${KZ_EMBED_DEMO_DIR}/models/Fox.glb\n"
        )
        list(APPEND KZ_EMBED_DEMO_DEPENDS "${KZ_EMBED_DEMO_DIR}/models/Fox.glb")
    endif()
    if(EXISTS "${KZ_EMBED_DEMO_DIR}/models/DamagedHelmet.glb")
        file(APPEND ${KZ_EMBED_CONTENT_MANIFEST}
            "models/DamagedHelmet.glb\t${KZ_EMBED_DEMO_DIR}/models/DamagedHelmet.glb\n"
        )
        list(APPEND KZ_EMBED_DEMO_DEPENDS "${KZ_EMBED_DEMO_DIR}/models/DamagedHelmet.glb")
    endif()
endif()
add_custom_command(
    OUTPUT ${KZ_EMBEDDED_CONTENT_HPP}
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_FILE=${KZ_EMBEDDED_CONTENT_HPP}
        -DMANIFEST_FILE=${KZ_EMBED_CONTENT_MANIFEST}
        -P ${KIZURI_REPO_ROOT}/cmake/EmbedContent.cmake
    DEPENDS
        ${KZ_EMBED_CONTENT_MANIFEST}
        ${KZ_EMBEDDED_CONTENT_DIR}/models/Cube.glb
        ${KZ_EMBEDDED_CONTENT_DIR}/skies/sky_gradient.hdr
        ${KZ_EMBED_DEMO_DEPENDS}
        ${KIZURI_REPO_ROOT}/cmake/EmbedContent.cmake
    COMMENT "Gerando registro de conteúdo embutido (kzres://)"
)
add_custom_target(KizuriEmbeddedContent DEPENDS ${KZ_EMBEDDED_CONTENT_HPP})






















add_library(KizuriEngine SHARED ${KIZURI_SOURCES} ${KIZURI_HEADERS})
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND COMMAND kz_copy_mingw_runtime)
    kz_copy_mingw_runtime(KizuriEngine)
endif()








set_target_properties(KizuriEngine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)






if(WIN32)
    set_target_properties(KizuriEngine PROPERTIES PREFIX "")
endif()




















if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(KizuriEngine PRIVATE "-Wl,--exclude-libs,ALL")
endif()

add_dependencies(KizuriEngine KizuriEmbeddedFonts)
add_dependencies(KizuriEngine KizuriEmbeddedContent)

target_include_directories(KizuriEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${KZ_EMBEDDED_FONTS_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/generated/kizuri
)




target_include_directories(KizuriEngine SYSTEM PUBLIC
    ${miniaudio_SOURCE_DIR}
    ${stb_SOURCE_DIR}
    ${tinyobjloader_SOURCE_DIR}
    ${cgltf_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${bullet3_SOURCE_DIR}/src
    ${box2d_SOURCE_DIR}/include
)





if(NOT ANDROID)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(imgui PUBLIC glfw glad)
    
    
    target_compile_definitions(imgui PUBLIC GLFW_INCLUDE_NONE)
    
    
    
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD2)
endif()

if(ANDROID)
    target_link_libraries(KizuriEngine
        PUBLIC
            glad
            glm::glm
            EnTT::EnTT
            spdlog::spdlog
            nlohmann_json::nlohmann_json
            BulletDynamics
            BulletCollision
            LinearMath
            box2d
            ${CMAKE_DL_LIBS}
            
            
            EGL
            android
            log
    )
else()
    target_link_libraries(KizuriEngine
        PUBLIC
            glfw
            glad
            glm::glm
            EnTT::EnTT
            spdlog::spdlog
            nlohmann_json::nlohmann_json
            imgui
            BulletDynamics
            BulletCollision
            LinearMath
            box2d
            ${CMAKE_DL_LIBS}
    )
endif()

if(WIN32)
    
    
    
    target_link_libraries(KizuriEngine PUBLIC ole32 ws2_32)
endif()

target_compile_definitions(KizuriEngine PUBLIC
    KZ_PLATFORM_$<UPPER_CASE:$<PLATFORM_ID>>
    GLFW_INCLUDE_NONE
)




if(ANDROID)
    foreach(dep glad glm::glm EnTT::EnTT spdlog::spdlog nlohmann_json::nlohmann_json)
        target_include_directories(KizuriEngine SYSTEM PRIVATE $<TARGET_PROPERTY:${dep},INTERFACE_INCLUDE_DIRECTORIES>)
    endforeach()
else()
    foreach(dep glfw glad glm::glm EnTT::EnTT spdlog::spdlog nlohmann_json::nlohmann_json)
        target_include_directories(KizuriEngine SYSTEM PRIVATE $<TARGET_PROPERTY:${dep},INTERFACE_INCLUDE_DIRECTORIES>)
    endforeach()
endif()

if(MSVC)
    target_compile_options(KizuriEngine PUBLIC /W4 /permissive- /WX)
else()
    
    
    
    target_compile_options(KizuriEngine PUBLIC -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unknown-warning-option)
endif()

target_compile_definitions(KizuriEngine PUBLIC
    $<$<CONFIG:Debug>:KZ_DEBUG>
    $<$<CONFIG:Release>:KZ_RELEASE>
)
