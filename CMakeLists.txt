cmake_minimum_required(VERSION 3.23)
project(SotN_Editor)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})
link_libraries(${GLEW_LIBRARIES})
message(STATUS "${GLEW_VERSION}")

# Utilize include directories
include_directories(include imgui imgui/backends)

# Add NFD
add_subdirectory(external/nativefiledialog-extended)

add_executable(
        SotN_Editor
        # ImGui sources
        imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp
        # ImGui backends
        imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp
        # Main files
        src/main.cpp
        src/gte.cpp
        src/mips.cpp
        src/compression.cpp
        src/map.cpp
        src/sprites.cpp
        src/utils.cpp)

target_compile_features(SotN_Editor PUBLIC cxx_std_17)

target_link_libraries(SotN_Editor
    PUBLIC
        GL # Might need to remove this line on MacOS
        glfw
        GLEW::GLEW
    PRIVATE
        nfd
)
