function(ctui_copy_asset ASSET_PATH)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${ASSET_PATH}" 
               "${CMAKE_BINARY_DIR}/examples/${ASSET_PATH}" COPYONLY)
endfunction(ctui_copy_asset)

ctui_copy_asset(spartan_16x16_cp437.ctuifont)
ctui_copy_asset(spartan_16x16_cp437.png)