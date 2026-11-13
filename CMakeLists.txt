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
helix_require(ScriptCore)

# The player core: everything under Source/Helix but the entry points of the executables (PlayerMain*.cpp), which the
# executables and the Android library take.
helix_collect_sources(player_sources "${CMAKE_CURRENT_SOURCE_DIR}/Source/Helix")
set(player_main_sources ${player_sources})
list(FILTER player_sources EXCLUDE REGEX "/PlayerMain[A-Za-z]*\\.cpp$")
list(FILTER player_main_sources INCLUDE REGEX "/PlayerMain[A-Za-z]*\\.cpp$")
file(GLOB_RECURSE player_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Helix/*.h")
set(player_generated "${CMAKE_CURRENT_BINARY_DIR}/Generated")

# The game modules the build gives the player: targets made with helix_bindgen whose module name is their OUTPUT_NAME
# (or their name). Android packages them (shared libraries) into the APK; the Web links them (static libraries) into
# the player, which publishes their registration by name. Windows and Linux load the data folder's Modules/ instead.
set(HELIX_PLAYER_GAME_MODULES "" CACHE STRING "Game module targets packaged with (Android) or linked into (the Web) the player")
set(module_declarations "")
set(module_entries "")
set(module_count 0)
foreach(module_target IN LISTS HELIX_PLAYER_GAME_MODULES)
    get_target_property(module_name ${module_target} OUTPUT_NAME)
    if(NOT module_name)
        set(module_name ${module_target})
    endif()
    if(HELIX_PLATFORM STREQUAL "web")
        string(APPEND module_declarations "extern \"C\" void HxBindings_${module_name}_Publish();\n")
        string(APPEND module_entries "            {\"${module_name}\", &HxBindings_${module_name}_Publish},\n")
    else()
        string(APPEND module_entries "            {\"${module_name}\", nullptr},\n")
    endif()
    math(EXPR module_count "${module_count} + 1")
endforeach()
if(module_count EQUAL 0)
    set(module_body "        return {};")
else()
    set(module_body "        static const BuildGameModule modules[] = {\n${module_entries}        };\n        return Span<const BuildGameModule>(modules, ${module_count});")
endif()
file(CONFIGURE OUTPUT "${player_generated}/PlayerGameModules.cpp" CONTENT
"// Written by the build of Helix.Player: the game modules it gives the player.
#include <Helix/Player/Scripts/PlayerScripts.h>

${module_declarations}
namespace helix::player
{
    Span<const BuildGameModule> BuildGameModules()
    {
${module_body}
    }
}
" @ONLY)

add_library(helix_player_core STATIC ${player_sources} ${player_headers} "${player_generated}/PlayerGameModules.cpp")
add_library(Helix::PlayerCore ALIAS helix_player_core)
# What helix_require(Player) finds (repos that build the player beside theirs).
add_library(Helix::Player ALIAS helix_player_core)
target_include_directories(helix_player_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source>)
# The engine: the shared HelixEngine where game modules are loaded at run time (they use the player's engine), linked
# in on the Web.
if(TARGET helix_engine_shared)
    target_link_libraries(helix_player_core PUBLIC Helix::EngineShared)
else()
    target_link_libraries(helix_player_core PUBLIC Helix::Engine Helix::Platform Helix::RHI PRIVATE Helix::Jobs)
endif()
helix_apply_options(helix_player_core)

# The engine's assemblies of Managed/: HelixEngine.CoreModule and the managed side of the .NET host, with the
# runtime's files of its assembly.
get_filename_component(scripting_runtime_dir "${HELIX_SCRIPTING_RUNTIME}" DIRECTORY)
get_filename_component(scripting_runtime_name "${HELIX_SCRIPTING_RUNTIME}" NAME_WLE)
set(player_engine_assemblies "${HELIX_CORE_MODULE}" ${HELIX_CORE_MODULE_REFERENCES} "${HELIX_SCRIPTING_RUNTIME}" "${scripting_runtime_dir}/${scripting_runtime_name}.runtimeconfig.json"
    "${scripting_runtime_dir}/${scripting_runtime_name}.deps.json")

# Release players on the desktop take ReadyToRun images of the engine's assemblies (native code ready at start up).
option(HELIX_PLAYER_READY_TO_RUN "ReadyToRun images of the desktop player's managed assemblies (not in Debug builds)" ON)
set(player_il_assemblies ${player_engine_assemblies})
if(HELIX_PLAYER_READY_TO_RUN AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND (HELIX_PLATFORM STREQUAL "windows" OR HELIX_PLATFORM STREQUAL "linux"))
    set(r2r_inputs ${player_engine_assemblies})
    list(FILTER r2r_inputs INCLUDE REGEX "\.dll$")
    helix_dotnet_ready_to_run(r2r_images OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/ReadyToRun" ASSEMBLIES ${r2r_inputs})
    if(r2r_images)
        add_custom_target(helix_player_ready_to_run ALL DEPENDS ${r2r_images})
        add_dependencies(helix_player_ready_to_run helix_core_module)
        if(TARGET helix_scripting_runtime)
            add_dependencies(helix_player_ready_to_run helix_scripting_runtime)
        endif()
        list(FILTER player_engine_assemblies EXCLUDE REGEX "\.dll$")
        list(APPEND player_engine_assemblies ${r2r_images})
    endif()
endif()

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
    # A .NET web app (the runtime with the player linked into its module) with the engine's assemblies and the game's
    # (HELIX_PLAYER_GAME_ASSEMBLIES: the web app carries them, the data folder's Managed/ names them), published with the
    # page from the template, its main.js and the loader of Helix.Platform, and the data folder as Data/.
    set(HELIX_PLAYER_GAME_ASSEMBLIES "" CACHE STRING "C# game assemblies the Web player is built with")
    list(FILTER player_main_sources INCLUDE REGEX "/Platform/Posix/")
    helix_add_executable(helix_player DOTNET SOURCES ${player_main_sources} DEPS Helix::PlayerCore)
    set_target_properties(helix_player PROPERTIES OUTPUT_NAME HelixPlayer)
    set_property(TARGET helix_player APPEND PROPERTY HELIX_DOTNET_ASSEMBLIES "${HELIX_CORE_MODULE}" ${HELIX_CORE_MODULE_REFERENCES} ${HELIX_PLAYER_GAME_ASSEMBLIES})
    if(HELIX_PLAYER_GAME_MODULES)
        target_link_libraries(helix_player PRIVATE ${HELIX_PLAYER_GAME_MODULES})
    endif()
    if(TARGET helix_player_web)
        add_dependencies(helix_player_web helix_core_module)
        set(web_root "${CMAKE_CURRENT_BINARY_DIR}/helix_player.Web/publish/wwwroot")
        add_custom_command(TARGET helix_player_web POST_BUILD
                           COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/Web/Templates/Default.html" "${web_root}/index.html"
                           COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/Web/Templates/main.js" "${web_root}/main.js"
                           COMMAND "${CMAKE_COMMAND}" -E copy "${HELIX_PLATFORM_WEB_DIR}/Loader/HelixPreload.mjs" "${web_root}/HelixPreload.mjs")
        if(HELIX_PLAYER_DATA)
            add_custom_command(TARGET helix_player_web POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_directory "${HELIX_PLAYER_DATA}" "${web_root}/Data")
        endif()
    endif()
elseif(HELIX_PLATFORM STREQUAL "android")
    # libHelixPlayer.so with HelixMain, made an application (the activity and the APK) by Helix.Platform.
    list(FILTER player_main_sources INCLUDE REGEX "/Platform/Android/")
    add_library(helix_player SHARED ${player_main_sources})
    target_link_libraries(helix_player PRIVATE Helix::PlayerCore)
    helix_apply_options(helix_player)
    set_target_properties(helix_player PROPERTIES OUTPUT_NAME HelixPlayer)
    # The .NET runtime of the APK: the android-arm64 runtime pack's native libraries (the debugger's left out) next to
    # the player's, its framework and the engine's assemblies as the assets' Managed/ (the player extracts them).
    helix_dotnet_pack(android_pack Microsoft.NETCore.App.Runtime.android-arm64)
    set(runtime_libraries "")
    set(managed_assets "")
    set(managed_depends "")
    if(android_pack)
        set(pack_runtime "${android_pack}/runtimes/android-arm64")
        file(GLOB runtime_libraries "${pack_runtime}/native/*.so")
        list(FILTER runtime_libraries EXCLUDE REGEX "/libmscord(bi|accore)\.so$")
        file(GLOB framework_assemblies "${pack_runtime}/lib/net11.0/*.dll")
        set(managed_stage "${CMAKE_CURRENT_BINARY_DIR}/AndroidManaged")
        # The coreclr host takes no runtimeconfig.json or deps.json: the assemblies alone.
        set(android_engine_assemblies ${player_engine_assemblies})
        list(FILTER android_engine_assemblies INCLUDE REGEX "\.dll$")
        add_custom_command(
            OUTPUT "${managed_stage}/.staged"
            COMMAND "${CMAKE_COMMAND}" -E rm -rf "${managed_stage}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${managed_stage}"
            COMMAND "${CMAKE_COMMAND}" -E copy ${framework_assemblies} ${android_engine_assemblies} "${managed_stage}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${managed_stage}/.staged"
            DEPENDS ${android_engine_assemblies}
            COMMENT "Staging the managed assemblies of the APK")
        add_custom_target(helix_player_android_managed DEPENDS "${managed_stage}/.staged")
        add_dependencies(helix_player_android_managed helix_core_module)
        set(managed_assets "${managed_stage}=Managed")
        set(managed_depends helix_player_android_managed "${managed_stage}/.staged")
    else()
        message(WARNING "Helix.Player: no android-arm64 runtime pack; the APK runs C++ scripts only.")
    endif()
    # The shared engine, the runtime and the build's game modules ride along in the APK.
    helix_add_android_app(helix_player PACKAGE org.helix.player LABEL "Helix Player" ASSETS "${HELIX_PLAYER_DATA}" ASSETS_PATH Data
                          PLATFORM Helix::EngineShared LIBRARIES helix_engine_shared ${runtime_libraries} ${HELIX_PLAYER_GAME_MODULES}
                          MORE_ASSETS ${managed_assets} DEPENDS ${managed_depends})
endif()
if(TARGET helix_player_ready_to_run AND TARGET helix_player)
    add_dependencies(helix_player helix_player_ready_to_run)
endif()
if(HELIX_PLAYER_DATA AND (HELIX_PLATFORM STREQUAL "windows" OR HELIX_PLATFORM STREQUAL "linux"))
    # The data folder next to the player, with the engine's assemblies in its Managed/.
    add_dependencies(helix_player helix_core_module)
    add_custom_command(TARGET helix_player POST_BUILD
                       COMMAND "${CMAKE_COMMAND}" -E copy_directory "${HELIX_PLAYER_DATA}" "$<TARGET_FILE_DIR:helix_player>/HelixPlayer_Data"
                       COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:helix_player>/HelixPlayer_Data/Managed"
                       COMMAND "${CMAKE_COMMAND}" -E copy ${player_engine_assemblies} "$<TARGET_FILE_DIR:helix_player>/HelixPlayer_Data/Managed")
endif()

helix_add_test_suite(helix_player_tests DOTNET EXCLUDE "/Scripts/(Managed|Module)/" DEPS Helix::PlayerCore)

# The test shaders: a C# assembly the tests put into the data folders they make, built with the .NET SDK and embedded.
if(TARGET helix_player_tests)
    set(player_artifacts "${CMAKE_CURRENT_BINARY_DIR}/Dotnet")
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
    # The exit scene: its data folder written by hand and the corpus models it uses (files only, no CMake of the corpus).
    helix_dependency_dir(player_corpus_dir Corpus)
    target_compile_definitions(helix_player_tests PRIVATE "HELIX_PLAYER_EXIT_DATA=\"${CMAKE_CURRENT_SOURCE_DIR}/Tests/Exit/Data\""
                               "HELIX_PLAYER_CORPUS_MODELS=\"${player_corpus_dir}/Assets/Models\"")
    # The C# test scripts of the data folders the tests make.
    set(scripts_dir "${CMAKE_CURRENT_SOURCE_DIR}/Tests/Scripts")
    set(test_scripts_dll "${player_artifacts}/bin/HelixPlayer.TestScripts/release/HelixPlayer.TestScripts.dll")
    file(GLOB test_script_sources CONFIGURE_DEPENDS "${scripts_dir}/Managed/*.cs" "${scripts_dir}/Managed/*.csproj")
    add_custom_command(
        OUTPUT "${test_scripts_dll}"
        COMMAND "${HELIX_DOTNET}" build "${scripts_dir}/Managed/HelixPlayer.TestScripts.csproj" -c Release --artifacts-path "${player_artifacts}"
                --nologo -v quiet "-p:HelixCoreModule=${HELIX_CORE_MODULE}" "-p:HelixScriptingRuntime=${HELIX_SCRIPTING_RUNTIME}"
        DEPENDS ${test_script_sources} "${HELIX_CORE_MODULE}"
        COMMENT "Building the C# test scripts of the player")
    add_custom_target(helix_player_test_scripts DEPENDS "${test_scripts_dll}")
    add_dependencies(helix_player_test_scripts helix_core_module)
    if(HELIX_PLATFORM STREQUAL "web")
        # The web app of the tests carries the engine's assemblies and the test scripts (the data folder names them).
        set_property(TARGET helix_player_tests APPEND PROPERTY HELIX_DOTNET_ASSEMBLIES "${HELIX_CORE_MODULE}" ${HELIX_CORE_MODULE_REFERENCES} "${test_scripts_dll}")
        if(TARGET helix_player_tests_web)
            add_dependencies(helix_player_tests_web helix_player_test_scripts helix_core_module)
        endif()
        target_compile_definitions(helix_player_tests PRIVATE HELIX_PLAYER_WEB_SCRIPTS=1)
    endif()
    # The player executable itself, run by the tests as a process of its own.
    if(HELIX_PLATFORM STREQUAL "windows" OR HELIX_PLATFORM STREQUAL "linux")
        add_dependencies(helix_player_tests helix_player)
        target_compile_definitions(helix_player_tests PRIVATE "HELIX_PLAYER_EXECUTABLE=\"$<TARGET_FILE:helix_player>\"")

        # The scripts of a test data folder: a game module (a shared library against the shared engine) and a C#
        # assembly, with the engine's assemblies the player packages.
        include("${HELIX_BINDGEN_ROOT}/CMake/HelixBindgen.cmake")
        helix_dependency_dir(player_engine_dir Engine)
        add_library(helix_player_test_module SHARED "${scripts_dir}/Module/PlayerTestModule.h")
        set_target_properties(helix_player_test_module PROPERTIES OUTPUT_NAME PlayerTestModule PREFIX "" LINKER_LANGUAGE CXX)
        target_link_libraries(helix_player_test_module PRIVATE Helix::EngineShared)
        target_include_directories(helix_player_test_module PRIVATE "${scripts_dir}")
        helix_apply_options(helix_player_test_module)
        set(engine_source "${player_engine_dir}/Source")
        helix_bindgen(helix_player_test_module MODULE PlayerTestModule NAMESPACE PlayerTests KIND NativeGame
                      HEADERS "${scripts_dir}/Module/PlayerTestModule.h" INCLUDE_ROOTS "${scripts_dir}" "${engine_source}"
                      REFERENCE_NAMESPACE HelixEngine
                      REFERENCES "${engine_source}/Helix/Engine/Object/EntityId.h" "${engine_source}/Helix/Engine/Object/Object.h"
                                 "${engine_source}/Helix/Engine/GameObject/GameObject.h" "${engine_source}/Helix/Engine/Script/GameBehaviour.h")
        add_dependencies(helix_player_tests helix_player_test_module helix_player_test_scripts)
        list(JOIN player_engine_assemblies "|" engine_assembly_list)
        list(JOIN player_il_assemblies "|" il_assembly_list)
        target_compile_definitions(helix_player_tests PRIVATE "HELIX_PLAYER_IL_ASSEMBLIES=\"${il_assembly_list}\"")
        if(TARGET helix_player_ready_to_run)
            add_dependencies(helix_player_tests helix_player_ready_to_run)
            target_compile_definitions(helix_player_tests PRIVATE HELIX_PLAYER_READY_TO_RUN=1)
        endif()
        target_compile_definitions(helix_player_tests PRIVATE "HELIX_PLAYER_TEST_MODULE=\"$<TARGET_FILE:helix_player_test_module>\""
                                   "HELIX_PLAYER_TEST_SCRIPTS=\"${test_scripts_dll}\"" "HELIX_PLAYER_ENGINE_ASSEMBLIES=\"${engine_assembly_list}\"")
    endif()
endif()

# The .NET runtime is never unloaded: LeakSanitizer is told about its modules (as in Helix.Scripting's tests).
if(TEST helix_player_tests AND HELIX_ENABLE_ASAN AND HELIX_PLATFORM STREQUAL "linux")
    helix_dependency_dir(scripting_dir Scripting)
    set_tests_properties(helix_player_tests PROPERTIES ENVIRONMENT "LSAN_OPTIONS=suppressions=${scripting_dir}/Tests/Support/LeakSuppressions.txt")
endif()

# The APK carries the .NET runtime: its native libraries and the managed folder are checked once it is packaged.
if(HELIX_PLATFORM STREQUAL "android" AND TARGET helix_player_apk AND android_pack)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/HelixPlayer.apk.checked"
        COMMAND "${CMAKE_COMMAND}" "-DAPK=${CMAKE_CURRENT_BINARY_DIR}/HelixPlayer.apk" "-DSTAMP=${CMAKE_CURRENT_BINARY_DIR}/HelixPlayer.apk.checked"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/Android/CheckApk.cmake"
        DEPENDS helix_player_apk "${CMAKE_CURRENT_SOURCE_DIR}/Android/CheckApk.cmake" "${CMAKE_CURRENT_BINARY_DIR}/HelixPlayer.apk"
        COMMENT "Checking the APK's .NET runtime")
    add_custom_target(helix_player_apk_check ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/HelixPlayer.apk.checked")
endif()
