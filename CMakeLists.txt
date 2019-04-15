cmake_minimum_required (VERSION 3.0)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

project (testFS)

set_property(GLOBAL PROPERTY C_STANDARD 11)

include(TestFSSPDK)

add_subdirectory(src)
