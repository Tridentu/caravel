cmake_minimum_required(VERSION 3.10)
project(caravel-pm)

find_package(PkgConfig REQUIRED)



find_library(CARAVEL_LIBRARY caravel)

include_directories(${CMAKE_INSTALL_PREFIX}/include/caravel)
include_directories(${CMAKE_INSTALL_PREFIX}/include/tridentu_tk/core)

include_directories("./src")
file (GLOB mw_SOURCES src/main.cpp)


add_executable(caravel-pm ${mw_SOURCES})

target_link_libraries(caravel-pm CaravelLib dl tridentu_tk_core)

install(TARGETS caravel-pm RUNTIME DESTINATION ${BIN_DIR})

install(FILES "./extras/caravel-operator" DESTINATION /usr/bin)

install(FILES "./extras/config/main.ini" DESTINATION /etc/caravel-repos)
install(FILES "./extras/config/cmr.ini" DESTINATION /etc/caravel-repos)
