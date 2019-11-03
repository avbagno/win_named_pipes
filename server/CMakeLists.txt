# CMakeList.txt : CMake project for client-server, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif ()

add_definitions( -DBOOST_ALL_NO_LIB )
FIND_PACKAGE(Boost 1.71 COMPONENTS serialization log)
include_directories(${Boost_INCLUDE_DIRS})
LINK_DIRECTORIES(${Boost_LIBRARIES})

LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/custom_objects)
include_directories(${CMAKE_SOURCE_DIR}/custom_objects)


add_executable (server "Server.cpp" "Server.hpp")
target_link_libraries(server custom_objects)
target_link_libraries(server ${Boost_LIBRARIES})