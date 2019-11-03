cmake_minimum_required (VERSION 3.8)

if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif ()

add_definitions( -DBOOST_ALL_NO_LIB )
FIND_PACKAGE(BOOST 1.71 COMPONENTS serialization)
include_directories(${Boost_INCLUDE_DIRS})
LINK_DIRECTORIES(${Boost_LIBRARIES})

set(Boost_USE_STATIC_LIBS        OFF)
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME    OFF)

message("message")
message(${Boost_INCLUDE_DIRS})
add_library(custom_objects STATIC "Objects.cpp" "Objects.hpp")

target_link_libraries(custom_objects ${Boost_LIBRARIES})