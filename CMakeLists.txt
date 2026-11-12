cmake_minimum_required(VERSION 3.28)
project(Helix.Player LANGUAGES CXX)

if(NOT COMMAND helix_add_library)
    include("${CMAKE_CURRENT_SOURCE_DIR}/External/Helix.CMake/Helix.cmake")
endif()

helix_require(Core)
helix_require(Platform)
helix_require(Jobs)
helix_require(RHI)
helix_require(Engine)

# The player core: everything under Source/Helix but the entry points of the executables (PlayerMain*.cpp), which the
# executables and the Android library take.
helix_collect_sources(player_sources "${CMAKE_CURRENT_SOURCE_DIR}/Source/Helix")
set(player_main_sources ${player_sources})
list(FILTER player_sources EXCLUDE REGEX "/PlayerMain[A-Za-z]*\\.cpp$")
list(FILTER player_main_sources INCLUDE REGEX "/PlayerMain[A-Za-z]*\\.cpp$")
file(GLOB_RECURSE player_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Helix/*.h")
add_library(helix_player_core STATIC ${player_sources} ${player_headers})
add_library(Helix::PlayerCore ALIAS helix_player_core)
target_include_directories(helix_player_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source>)
target_link_libraries(helix_player_core PUBLIC Helix::Engine Helix::Platform Helix::RHI PRIVATE Helix::Jobs)
helix_apply_options(helix_player_core)

# A data folder to run: copied next to the player as <name>_Data (Data/ on the Web, the assets' Data/ in the APK).
set(HELIX_PLAYER_DATA "" CACHE PATH "A data folder to package with the player")

if(HELIX_PLATFORM STREQUAL "windows")
    helix_add_executable(helix_player SOURCES ${player_main_sources} DEPS Helix::PlayerCore)
    set_target_properties(helix_player PROPERTIES WIN32_EXECUTABLE TRUE OUTPUT_NAME HelixPlayer)
    target_link_libraries(helix_player PRIVATE shell32 user32)
elseif(HELIX_PLATFORM STREQUAL "linux")
    helix_add_executable(helix_player SOURCES ${player_main_sources} DEPS Helix::PlayerCore)
    set_target_properties(helix_player PROPERTIES OUTPUT_NAME HelixPlayer)
elseif(HELIX_PLATFORM STREQUAL "web")
    include("${CMAKE_CURRENT_SOURCE_DIR}/Web/HelixPlayerWeb.cmake")
elseif(HELIX_PLATFORM STREQUAL "android")
    include("${CMAKE_CURRENT_SOURCE_DIR}/Android/HelixPlayerAndroid.cmake")
endif()
if(HELIX_PLAYER_DATA AND NOT HELIX_PLATFORM STREQUAL "android")
    set(data_name "HelixPlayer_Data")
    if(HELIX_PLATFORM STREQUAL "web")
        set(data_name "Data")
    endif()
    add_custom_command(TARGET helix_player POST_BUILD
                       COMMAND "${CMAKE_COMMAND}" -E copy_directory "${HELIX_PLAYER_DATA}" "$<TARGET_FILE_DIR:helix_player>/${data_name}")
endif()

helix_add_test_suite(helix_player_tests DEPS Helix::PlayerCore Helix::Jobs)

# The test shaders: a C# assembly the tests put into the data folders they make, built with the .NET SDK and embedded.
if(TARGET helix_player_tests)
    set(player_artifacts "${CMAKE_CURRENT_BINARY_DIR}/Dotnet")
    set(player_generated "${CMAKE_CURRENT_BINARY_DIR}/Generated")
    set(test_shaders_dir "${player_artifacts}/bin/HelixPlayer.TestShaders/release")
    helix_dependency_dir(player_engine_dir Engine)
    file(GLOB_RECURSE test_shader_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/Shaders/*.cs" "${CMAKE_CURRENT_SOURCE_DIR}/Tests/Shaders/*.csproj"
         "${player_engine_dir}/Shaders/*.cs" "${HELIX_SHADERS_DIR}/Source/*.cs")
    list(FILTER test_shader_sources EXCLUDE REGEX "/(bin|obj)/")
    add_custom_command(
        OUTPUT "${test_shaders_dir}/HelixPlayer.TestShaders.dll" "${test_shaders_dir}/HelixPlayer.TestShaders.pdb"
        COMMAND "${HELIX_DOTNET}" build "${CMAKE_CURRENT_SOURCE_DIR}/Tests/Shaders/HelixPlayer.TestShaders.csproj" -c Release
                --artifacts-path "${player_artifacts}" --nologo -v quiet "-p:HelixShadersDir=${HELIX_SHADERS_DIR}" "-p:HelixEngineDir=${player_engine_dir}"
        DEPENDS ${test_shader_sources}
        COMMENT "Building the C# test shaders of the player")
    set(test_embedded "${player_generated}/PlayerTestShadersAssembly.h" "${player_generated}/PlayerTestShadersSymbols.h")
    add_custom_command(
        OUTPUT ${test_embedded}
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${player_generated}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${test_shaders_dir}/HelixPlayer.TestShaders.dll" "-DOUTPUT=${player_generated}/PlayerTestShadersAssembly.h"
                -DNAME=kPlayerTestShadersAssembly -P "${HELIX_EMBED_FILE}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${test_shaders_dir}/HelixPlayer.TestShaders.pdb" "-DOUTPUT=${player_generated}/PlayerTestShadersSymbols.h"
                -DNAME=kPlayerTestShadersSymbols -P "${HELIX_EMBED_FILE}"
        DEPENDS "${test_shaders_dir}/HelixPlayer.TestShaders.dll" "${HELIX_EMBED_FILE}"
        COMMENT "Embedding the C# test shaders of the player")
    target_sources(helix_player_tests PRIVATE ${test_embedded})
    target_include_directories(helix_player_tests PRIVATE "${player_generated}")
    target_compile_definitions(helix_player_tests PRIVATE "HELIX_PLAYER_GOLDEN_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/Tests/Golden\"")
    # The player executable itself, run by the tests as a process of its own.
    if(HELIX_PLATFORM STREQUAL "windows" OR HELIX_PLATFORM STREQUAL "linux")
        add_dependencies(helix_player_tests helix_player)
        target_compile_definitions(helix_player_tests PRIVATE "HELIX_PLAYER_EXECUTABLE=\"$<TARGET_FILE:helix_player>\"")
    endif()
endif()
