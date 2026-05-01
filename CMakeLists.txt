cmake_minimum_required(VERSION 3.10)

project(MeshSimplification)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    glad/src/gl.c
    main.cpp
    stb_image.cpp
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

add_executable(main ${SOURCES})
set_target_properties(main PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(main PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/vcglib
)

target_compile_definitions(main PRIVATE IMGUI_IMPL_GLFW_DISABLE_WAYLAND)
target_link_libraries(main PRIVATE glfw dl)

# --- Metro Tool Configuration ---
set(METRO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/vcglib/apps/metro/metro.cpp")
set(PLY_SRC "${CMAKE_CURRENT_SOURCE_DIR}/vcglib/wrap/ply/plylib.cpp")

if(EXISTS ${METRO_SRC})
    add_executable(metro_tool ${METRO_SRC} ${PLY_SRC})

    target_include_directories(metro_tool PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/vcglib
        ${CMAKE_CURRENT_SOURCE_DIR}/vcglib/eigenlib
    )

    set_target_properties(metro_tool PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_NAME "metro"
    )

    # Link OpenMP for multi-threaded performance
    if(OpenMP_CXX_FOUND)
        target_link_libraries(metro_tool PRIVATE OpenMP::OpenMP_CXX)
    endif()

    if(MSVC)
        target_compile_definitions(metro_tool PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
    else()
        target_link_libraries(metro_tool PRIVATE pthread)
    endif()

else()
    message(WARNING "Metro source not found at ${METRO_SRC}. Skipping metro_tool build.")
endif()
