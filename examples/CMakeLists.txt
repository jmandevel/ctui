add_executable(ctui_example_glfw_opengl33_matrix "backend_glfw_opengl33.c" "frontend_matrix_rain.c")
target_link_libraries(ctui_example_glfw_opengl33_matrix PRIVATE ctui)

add_executable(ctui_example_ncurses_matrix "backend_ncurses.c" "frontend_matrix_rain.c")
target_link_libraries(ctui_example_ncurses_matrix PRIVATE ctui)

add_subdirectory(content)