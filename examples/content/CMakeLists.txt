function(ctui_copy_asset ASSET_PATH)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${ASSET_PATH}" 
               "${CMAKE_BINARY_DIR}/examples/${ASSET_PATH}" COPYONLY)
endfunction(ctui_copy_asset)

ctui_copy_asset(cp437_8x16.ctuifont)
ctui_copy_asset(cp437_8x16.png)
ctui_copy_asset(cp437_16x16.ctuifont)
ctui_copy_asset(cp437_16x16.png)