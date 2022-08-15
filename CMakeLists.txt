cmake_minimum_required(VERSION 3.12)

project(OpenGLAndVulkan)

set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_CXX_STANDARD 20)

# glad
include_directories(3rdparty/glad/include)
set(GLAD_SOURCES 3rdparty/glad/src/gl.c 3rdparty/glad/src/vulkan.c)
if(WIN32)
	set(GLAD_SOURCES ${GLAD_SOURCES} 3rdparty/glad/src/wgl.c)
endif()
add_library(glad ${GLAD_SOURCES})
set_property(TARGET glad          PROPERTY FOLDER "ThirdPartyLibraries")

#glfw
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
set(GLFW_INSTALL OFF CACHE BOOL "")
add_subdirectory(3rdparty/glfw)
include_directories(3rdparty/glfw/include)
set_property(TARGET glfw          PROPERTY FOLDER "ThirdPartyLibraries")

#glm
include_directories(3rdparty/glm)
add_subdirectory(3rdparty/glm)

#stb
include_directories(3rdparty/stb)

#imgui
include_directories(3rdparty/imgui)

#easy_profiler
option(BUILD_WITH_EASY_PROFILER "Enable EasyProfiler usage" ON)
set(EASY_PROFILER_NO_GUI ON CACHE BOOL "")
set(EASY_PROFILER_NO_SAMPLES ON CACHE BOOL "")

add_subdirectory(3rdparty/easy_profiler)

if(BUILD_WITH_EASY_PROFILER)
	message("Enabled EasyProfiler")
	add_definitions(-DBUILD_WITH_EASY_PROFILER=1)
	include_directories(3rdparty/easy_profiler_release/include)
	set_property(TARGET easy_profiler PROPERTY FOLDER "ThirdPartyLibraries")
	set_property(TARGET profiler_converter PROPERTY FOLDER "ThirdPartyLibraries")
endif()

#complier MSVC
if(MSVC)
    add_compile_options(-W4)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_CONSOLE)
    set_property(TARGET ${PROJ_NAME} PROPERTY
            VS_DEBUGGER_WORKING_DIRECTORY
            "${CMAKE_SOURCE_DIR}")
endif()


include_directories(src)
add_executable(first src/main.cpp )

target_link_libraries(first glad glfw easy_profiler)





