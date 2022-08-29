cmake_minimum_required(VERSION 3.12)

project(OpenGLAndVulkan)

set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_CXX_STANDARD 20)

macro(SETUP_GROUPS src_files)
	foreach(FILE ${src_files})
		get_filename_component(PARENT_DIR "${FILE}" PATH)

		# skip src or include and changes /'s to \\'s
		set(GROUP "${PARENT_DIR}")
		string(REPLACE "/" "\\" GROUP "${GROUP}")

		source_group("${GROUP}" FILES "${FILE}")
	endforeach()
endmacro()

# main
file(GLOB_RECURSE SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.c??)
file(GLOB_RECURSE HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.h)
SETUP_GROUPS("${SRC_FILES}")
SETUP_GROUPS("${HEADER_FILES}")
include_directories(src)
include_directories(3rdparty)
add_executable(SampleTest ${SRC_FILES} ${HEADER_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glslang/StandAlone/ResourceLimits.cpp)

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

#etc2comp
# add_subdirectory(3rdparty/etc2comp)
# include_directories(3rdparty/etc2comp/EtcLib/Etc)
# include_directories(3rdparty/etc2comp/EtcLib/EtcCodec)
# include_directories(3rdparty/etc2comp/EtcTool)
# set_property(TARGET EtcLib        PROPERTY FOLDER "ThirdPartyLibraries")
# set_property(TARGET EtcTool       PROPERTY FOLDER "ThirdPartyLibraries")
# target_sources(first PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/../3rdparty/etc2comp/EtcTool/EtcFile.cpp)
# target_sources(first PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/../3rdparty/etc2comp/EtcTool/EtcFileHeader.cpp)

#taskflow
include_directories(3rdparty/taskflow)

#mesh optimizer
set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "")
set(MESHOPT_BUILD_TOOLS OFF CACHE BOOL "")
set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "")
add_subdirectory(3rdparty/meshoptimizer)
set_property(TARGET meshoptimizer PROPERTY FOLDER "ThirdPartyLibraries")

#vulkan
include_directories(3rdparty/vulkan/include)

#volk
add_subdirectory(3rdparty/volk)
set_property(TARGET volk          PROPERTY FOLDER "ThirdPartyLibraries")

#glslang
set(ENABLE_GLSLANG_BINARIES  OFF  CACHE BOOL "")
set(ENABLE_HLSL              OFF  CACHE BOOL "")
set(ENABLE_CTEST             OFF  CACHE BOOL "")
set(ENABLE_OPT               OFF  CACHE BOOL "")
set(SKIP_GLSLANG_INSTALL     ON   CACHE BOOL "")
add_subdirectory(3rdparty/glslang)
include_directories(3rdparty/glslang/glslang/Include)
include_directories(3rdparty/glslang)
include_directories(3rdparty/glslang/StandAlone)
include_directories(3rdparty/glslang/src)
set_property(TARGET glslang       PROPERTY FOLDER "ThirdPartyLibraries/glslang")
set_property(TARGET OGLCompiler   PROPERTY FOLDER "ThirdPartyLibraries/glslang")
set_property(TARGET OSDependent   PROPERTY FOLDER "ThirdPartyLibraries/glslang")
set_property(TARGET SPIRV         PROPERTY FOLDER "ThirdPartyLibraries/glslang")
set_property(TARGET SPVRemapper   PROPERTY FOLDER "ThirdPartyLibraries/glslang")

# other included 
set_property(TARGET IrrXML        PROPERTY FOLDER "ThirdPartyLibraries")
set_property(TARGET zlibstatic    PROPERTY FOLDER "ThirdPartyLibraries")
set_property(TARGET zlib          PROPERTY FOLDER "ThirdPartyLibraries")
set_property(TARGET uninstall     PROPERTY FOLDER "ThirdPartyLibraries")
set_property(TARGET GenericCodeGen     PROPERTY FOLDER "ThirdPartyLibraries")
set_property(TARGET MachineIndependent     PROPERTY FOLDER "ThirdPartyLibraries")

#complier MSVC
if(MSVC)
    add_compile_options(-W4)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_CONSOLE)
    # add_definitions(-DVULKAN)
    # add_definitions(-DOPENGL)
    set_property(TARGET SampleTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()



target_link_libraries(SampleTest glfw volk glslang SPIRV assimp)





