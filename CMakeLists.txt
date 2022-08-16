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

#optick
option(BUILD_WITH_OPTICK "Enable Optick usage" OFF)
if(BUILD_WITH_OPTICK)
    message("Enabled Optick")
    include_directories(3rdparty/optick/src)
	add_definitions(-DBUILD_WITH_OPTICK=1)
    add_subdirectory(3rdparty/optick)
	set_property(TARGET OptickCore PROPERTY FOLDER "ThirdPartyLibraries")
endif()

#assimp
set(ASSIMP_NO_EXPORT ON CACHE BOOL "")
set(ASSIMP_BUILD_DRACO OFF CACHE BOOL "")
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "")
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
set(ASSIMP_INSTALL_PDB OFF CACHE BOOL "")
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "")
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "")

add_subdirectory(3rdparty/assimp)
include_directories(3rdparty/assimp/include)
set_property(TARGET assimp        PROPERTY FOLDER "ThirdPartyLibraries")
set_property(TARGET UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTY FOLDER "ThirdPartyLibraries")

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

target_link_libraries(first glad glfw assimp)





