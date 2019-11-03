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


add_executable (client "Client.cpp" "Client.hpp")

target_link_libraries(client custom_objects)
target_link_libraries(client ${Boost_LIBRARIES})