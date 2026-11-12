# The Android player: libHelixPlayer.so with the native methods of the activity, and the APK built from it with the
# tools of the Android SDK and a JDK (when both are found; the native library builds either way). A data folder given
# by HELIX_PLAYER_DATA goes into the APK's assets as Data/.

list(FILTER player_main_sources INCLUDE REGEX "/Platform/Android/")
add_library(helix_player SHARED ${player_main_sources})
target_link_libraries(helix_player PRIVATE Helix::PlayerCore android log)
helix_apply_options(helix_player)
set_target_properties(helix_player PROPERTIES OUTPUT_NAME HelixPlayer)

set(HELIX_ANDROID_PACKAGE "org.helix.player" CACHE STRING "The package name of the player's APK")
set(HELIX_ANDROID_LABEL "Helix Player" CACHE STRING "The name the launcher shows")

# The SDK's newest build tools and platform.
set(android_sdk "$ENV{ANDROID_HOME}")
if(NOT android_sdk)
    set(android_sdk "$ENV{ANDROID_SDK_ROOT}")
endif()
file(GLOB build_tools_dirs "${android_sdk}/build-tools/*")
list(SORT build_tools_dirs COMPARE NATURAL ORDER DESCENDING)
file(GLOB platform_jars "${android_sdk}/platforms/android-*/android.jar")
list(SORT platform_jars COMPARE NATURAL ORDER DESCENDING)
find_program(HELIX_JAVAC NAMES javac HINTS "$ENV{JAVA_HOME}/bin")
find_program(HELIX_JAR NAMES jar HINTS "$ENV{JAVA_HOME}/bin")
find_program(HELIX_KEYTOOL NAMES keytool HINTS "$ENV{JAVA_HOME}/bin")

if(build_tools_dirs AND platform_jars AND HELIX_JAVAC AND HELIX_JAR AND HELIX_KEYTOOL)
    list(GET build_tools_dirs 0 build_tools)
    list(GET platform_jars 0 android_jar)
    get_filename_component(platform_dir "${android_jar}" DIRECTORY)
    get_filename_component(platform_name "${platform_dir}" NAME)
    string(REPLACE "android-" "" target_sdk "${platform_name}")
    set(HELIX_ANDROID_MIN_SDK "${HELIX_ANDROID_API}")
    set(HELIX_ANDROID_TARGET_SDK "${target_sdk}")
    set(apk_dir "${CMAKE_CURRENT_BINARY_DIR}/Apk")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Android/Templates/AndroidManifest.xml" "${apk_dir}/AndroidManifest.xml" @ONLY)
    file(GLOB_RECURSE java_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Android/HelixActivity/src/main/java/*.java")
    set(apk "${CMAKE_CURRENT_BINARY_DIR}/HelixPlayer.apk")
    add_custom_command(
        OUTPUT "${apk}"
        COMMAND "${CMAKE_COMMAND}" "-DBUILD_TOOLS=${build_tools}" "-DANDROID_JAR=${android_jar}" "-DJAVAC=${HELIX_JAVAC}" "-DJAR=${HELIX_JAR}"
                "-DKEYTOOL=${HELIX_KEYTOOL}" "-DMANIFEST=${apk_dir}/AndroidManifest.xml" "-DJAVA_SOURCES=$<JOIN:${java_sources},|>"
                "-DLIBRARY=$<TARGET_FILE:helix_player>" "-DABI=${HELIX_ANDROID_ABI}" "-DMIN_SDK=${HELIX_ANDROID_MIN_SDK}"
                "-DDATA=${HELIX_PLAYER_DATA}" "-DWORK=${apk_dir}" "-DOUTPUT=${apk}" -P "${CMAKE_CURRENT_SOURCE_DIR}/Android/BuildApk.cmake"
        DEPENDS helix_player ${java_sources} "${apk_dir}/AndroidManifest.xml" "${CMAKE_CURRENT_SOURCE_DIR}/Android/BuildApk.cmake"
        COMMENT "Packaging HelixPlayer.apk")
    add_custom_target(helix_player_apk ALL DEPENDS "${apk}")
else()
    message(STATUS "Helix.Player: no Android SDK build tools, platform or JDK found; the APK is not built (libHelixPlayer.so is).")
endif()
