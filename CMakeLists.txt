cmake_minimum_required(VERSION 3.23)
project(SotN_Editor)

# Uncomment to generate debug info
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})
link_libraries(${GLEW_LIBRARIES})

# Utilize include directories
include_directories(include imgui imgui/backends)

# Add NFD
add_subdirectory(external/nativefiledialog-extended)

# Set target executable
add_executable(${PROJECT_NAME})

# Populate target sources
target_sources(
    ${PROJECT_NAME}
    PRIVATE

        # ImGui sources
        imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp

        # ImGui backends
        imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp

        # Main files
        src/main.cpp
        src/gte.cpp
        src/mips.cpp
        src/compression.cpp
        src/map.cpp
        src/sprites.cpp
        src/utils.cpp
        src/log.cpp
)

# Set icon for Windows builds
if(MSVC)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.rc")
    target_sources(${PROJECT_NAME} PRIVATE ${APP_ICON_RESOURCE_WINDOWS})
endif()

# Set icon for macOS
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE ON)
    set(MACOSX_BUNDLE_ICON_FILE icon.icns)
    set(SOTN_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.icns)
    set_source_files_properties(${SOTN_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(${PROJECT_NAME} PRIVATE ${SOTN_ICON})
endif()

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
        glfw
        GLEW::GLEW
    PRIVATE
        nfd
)

# Include GL for Linux targets
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PUBLIC GL)
endif()

# Include OpenGL32 for Windows targets
if(MSVC)
    target_link_libraries( ${PROJECT_NAME} PUBLIC OpenGL32)
endif()
