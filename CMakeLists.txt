cmake_minimum_required(VERSION 3.24)
project(KizuriEngine VERSION 0.38.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()





if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

option(KZ_BUILD_SANDBOX "Build the sandbox example game" ON)
option(KZ_BUILD_EDITOR  "Build the Kizuri Editor"        ON)
option(KZ_BUILD_GAME    "Build the standalone game exe"   ON)
option(KZ_BUILD_TESTS   "Build unit tests"                OFF)


option(KZ_BUILD_V2      "Build the Kizuri Engine v2 foundation" OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)








set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)






































if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_link_options(-Wl,--no-insert-timestamp)

    
    
    
    
    get_filename_component(KZ_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

    function(kz_copy_mingw_runtime target)
        foreach(dll IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
            if(EXISTS "${KZ_MINGW_BIN_DIR}/${dll}")
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${KZ_MINGW_BIN_DIR}/${dll}"
                        "$<TARGET_FILE_DIR:${target}>/${dll}"
                    COMMENT "Copiando ${dll} do runtime MinGW pra junto de ${target}"
                )
            endif()
        endforeach()
    endfunction()
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)








if(NOT ANDROID)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    
    
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
endif()


FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1)







FetchContent_Declare(entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.13.2)


FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1)


FetchContent_Declare(bullet3
    GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
    GIT_TAG        3.25)
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(USE_GRAPHICAL_BENCHMARK OFF CACHE BOOL "" FORCE)


FetchContent_Declare(box2d
    GIT_REPOSITORY https://github.com/erincatto/box2d.git
    GIT_TAG        v2.4.2)
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)


FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        0.11.21)


FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master)


FetchContent_Declare(tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13)



FetchContent_Declare(cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG        v1.15)


FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.9-docking)






FetchContent_Declare(imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG        master)


FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3)




if(NOT ANDROID)
    FetchContent_Declare(curl
        GIT_REPOSITORY https://github.com/curl/curl.git
        GIT_TAG        curl-8_11_0)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
    if(WIN32)
        
        set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
        set(CURL_WINDOWS_SSPI ON CACHE BOOL "" FORCE)
    else()
        set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(miniz
        GIT_REPOSITORY https://github.com/richgel999/miniz.git
        GIT_TAG        3.0.2)
endif()

if(ANDROID)
    FetchContent_MakeAvailable(glm entt spdlog miniaudio stb tinyobjloader cgltf json)
else()
    FetchContent_MakeAvailable(glfw glm entt spdlog miniaudio stb tinyobjloader cgltf json)
    FetchContent_MakeAvailable(curl miniz)
    if(NOT TARGET miniz)
        add_library(miniz STATIC ${miniz_SOURCE_DIR}/miniz.c)
        target_include_directories(miniz PUBLIC ${miniz_SOURCE_DIR})
    endif()
endif()


FetchContent_MakeAvailable(bullet3 box2d)

FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

FetchContent_GetProperties(imguizmo)
if(NOT imguizmo_POPULATED)
    FetchContent_Populate(imguizmo)
endif()

add_subdirectory(engine)




if(KZ_BUILD_V2 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/kizuri_v2")
    add_subdirectory(kizuri_v2)
endif()




if(ANDROID)
    if(KZ_BUILD_EDITOR)
        message(WARNING "KZ_BUILD_EDITOR não é suportado em Android — desligado.")
        set(KZ_BUILD_EDITOR OFF)
    endif()
    if(KZ_BUILD_SANDBOX)
        message(WARNING "KZ_BUILD_SANDBOX não é suportado em Android — desligado.")
        set(KZ_BUILD_SANDBOX OFF)
    endif()
endif()

if(KZ_BUILD_SANDBOX)
    add_subdirectory(examples/sandbox)
endif()

if(KZ_BUILD_GAME)
    add_subdirectory(examples/KizuriGame)
endif()

if(KZ_BUILD_EDITOR)
    add_subdirectory(editor)
endif()
