add_executable(Sandbox src/SandboxApp.cpp src/PlayerController.cpp)
target_link_libraries(Sandbox PRIVATE KizuriEngine)
target_include_directories(Sandbox PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

set_target_properties(Sandbox PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND COMMAND kz_copy_mingw_runtime)
    kz_copy_mingw_runtime(Sandbox)
endif()

if(WIN32)
    target_link_options(Sandbox PRIVATE $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-mwindows>)
    set_target_properties(Sandbox PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
endif()
