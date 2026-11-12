# The Web player: HelixPlayer.html, .js and .wasm from the template and the loader, which fetches the data folder (Data/
# next to the page, or ?data=<URL>) before main. A data folder given by HELIX_PLAYER_DATA is copied there.

list(FILTER player_main_sources INCLUDE REGEX "/Platform/Posix/")
helix_add_executable(helix_player SOURCES ${player_main_sources} DEPS Helix::PlayerCore)
set_target_properties(helix_player PROPERTIES OUTPUT_NAME HelixPlayer SUFFIX ".html")
target_link_options(helix_player PRIVATE -sENVIRONMENT=web -sEXIT_RUNTIME=0 -sFORCE_FILESYSTEM=1
                    "SHELL:--shell-file \"${CMAKE_CURRENT_SOURCE_DIR}/Web/Templates/Default.html\""
                    "SHELL:--pre-js \"${CMAKE_CURRENT_SOURCE_DIR}/Web/Loader/HelixLoader.js\"")
set_property(TARGET helix_player APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Web/Templates/Default.html"
             "${CMAKE_CURRENT_SOURCE_DIR}/Web/Loader/HelixLoader.js")
