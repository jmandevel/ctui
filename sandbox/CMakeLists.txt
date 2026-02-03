add_executable(sandbox main.c)
target_link_libraries(sandbox PRIVATE ctui)

# Copy font files to build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/spartan_16x16_cp437.ctuifont 
               ${CMAKE_CURRENT_BINARY_DIR}/spartan_16x16_cp437.ctuifont COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/spartan_16x16_cp437.png 
               ${CMAKE_CURRENT_BINARY_DIR}/spartan_16x16_cp437.png COPYONLY)