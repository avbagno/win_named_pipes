﻿# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("client-server")

# Include sub-projects.
add_subdirectory ("custom_objects")
add_subdirectory ("server")
add_subdirectory ("client")
