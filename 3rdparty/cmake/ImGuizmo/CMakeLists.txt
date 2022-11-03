cmake_minimum_required(VERSION 3.0)

project("ImGuizmo")

include_directories(../../imgui)
include_directories(../../ImGuizmo)

add_library(ImGuizmo ../../ImGuizmo/ImGuizmo.cpp ../../ImGuizmo/ImGuizmo.h)

set_property(TARGET ImGuizmo PROPERTY FOLDER "ThirdPartyLibraries")
