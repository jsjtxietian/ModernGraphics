cmake_minimum_required(VERSION 3.0)

project("Bullet")

if(MSVC)
	add_definitions(/wd4018)
	add_definitions(/wd4244)
	add_definitions(/wd4267)
	add_definitions(/wd4305)
	add_definitions(/wd4316)
	add_definitions(/wd4359)
	add_definitions(-DB3_USE_CLEW)
	if(CMAKE_SIZEOF_VOID_P LESS 8)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")
	endif()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GS- /fp:fast")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ox /Ob2 /Oy /Ot")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Ox /Ob2 /Oy /Ot")
	STRING (REGEX REPLACE "/RTC[^ ]*" "" CMAKE_CXX_FLAGS_DEBUG  "${CMAKE_CXX_FLAGS_DEBUG}")
endif(MSVC)

include_directories(../../bullet/src)

file(GLOB CPP_FILES
	../../bullet/src/BulletCollision/BroadphaseCollision/*.cpp
	../../bullet/src/BulletCollision/CollisionDispatch/*.cpp
	../../bullet/src/BulletCollision/CollisionShapes/*.cpp
	../../bullet/src/BulletCollision/Gimpact/*.cpp
	../../bullet/src/BulletCollision/NarrowPhaseCollision/*.cpp
	../../bullet/src/BulletDynamics/Character/*.cpp
	../../bullet/src/BulletDynamics/ConstraintSolver/*.cpp
	../../bullet/src/BulletDynamics/Dynamics/*.cpp
	../../bullet/src/BulletDynamics/Featherstone/*.cpp
	../../bullet/src/BulletDynamics/MLCPSolvers/*.cpp
	../../bullet/src/BulletDynamics/Vehicle/*.cpp
	../../bullet/src/BulletSoftBody/*.cpp
	../../bullet/src/LinearMath/*.cpp
# OpenCL support
#	../../bullet/src/clew/*.cpp
#	../../bullet/src/Bullet3OpenCL/BroadphaseCollision/*.cpp
#	../../bullet/src/Bullet3OpenCL/Initialize/*.cpp
#	../../bullet/src/Bullet3OpenCL/NarrowphaseCollision/*.cpp
#	../../bullet/src/Bullet3OpenCL/ParallelPrimitives/*.cpp
#	../../bullet/src/Bullet3OpenCL/Raycast/*.cpp
#	../../bullet/src/Bullet3OpenCL/RigidBody/*.cpp
)

file(GLOB H_FILES
	../../bullet/*.h
	../../bullet/src/BulletCollision/BroadphaseCollision/*.h
	../../bullet/src/BulletCollision/CollisionDispatch/*.h
	../../bullet/src/BulletCollision/CollisionShapes/*.h
	../../bullet/src/BulletCollision/Gimpact/*.h
	../../bullet/src/BulletCollision/NarrowPhaseCollision/*.h
	../../bullet/src/BulletDynamics/Character/*.h
	../../bullet/src/BulletDynamics/ConstraintSolver/*.h
	../../bullet/src/BulletDynamics/Dynamics/*.h
	../../bullet/src/BulletDynamics/Featherstone/*.h
	../../bullet/src/BulletDynamics/MLCPSolvers/*.h
	../../bullet/src/BulletDynamics/Vehicle/*.h
	../../bullet/src/BulletSoftBody/*.h
	../../bullet/src/LinearMath/*.h
)

set(SRC_FILES ${CPP_FILES} ${H_FILES})

add_library(Bullet ${SRC_FILES})

set_property(TARGET Bullet PROPERTY FOLDER "ThirdPartyLibraries")
