cmake_minimum_required(VERSION 3.20)

project(ctui)

find_package(Curses REQUIRED)
#find_package(PkgConfig REQUIRED)
#pkg_search_module(GLFW REQUIRED glfw3)

add_library(ctui)
target_include_directories(ctui PUBLIC include ${CURSES_INCLUDE_DIRS} 
#${GLFW_INCLUDE_DIRS}
)
target_link_libraries(ctui PUBLIC ${CURSES_LIBRARIES}
# ${GLFW_LIBRARIES} 
m)

add_subdirectory(src)
add_subdirectory(examples)