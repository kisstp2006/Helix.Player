# Builds and signs the player's APK (cmake -P): Java to dex, the manifest and assets through aapt2, the dex and the
# native library added with the JDK's jar tool, aligned and signed with a debug key made on first use.

function(run)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "BuildApk: ${ARGV0} failed (${result}):\n${out}\n${err}")
    endif()
endfunction()

if(CMAKE_HOST_WIN32)
    set(d8 "${BUILD_TOOLS}/d8.bat")
    set(apksigner "${BUILD_TOOLS}/apksigner.bat")
    set(aapt2 "${BUILD_TOOLS}/aapt2.exe")
    set(zipalign "${BUILD_TOOLS}/zipalign.exe")
else()
    set(d8 "${BUILD_TOOLS}/d8")
    set(apksigner "${BUILD_TOOLS}/apksigner")
    set(aapt2 "${BUILD_TOOLS}/aapt2")
    set(zipalign "${BUILD_TOOLS}/zipalign")
endif()

set(stage "${WORK}/stage")
file(REMOVE_RECURSE "${stage}")
file(MAKE_DIRECTORY "${stage}/classes" "${stage}/assets" "${stage}/lib/${ABI}")

# Java: classes, then dex (the sources come joined with |).
string(REPLACE "|" ";" JAVA_SOURCES "${JAVA_SOURCES}")
run("${JAVAC}" --release 11 -classpath "${ANDROID_JAR}" -d "${stage}/classes" ${JAVA_SOURCES})
file(GLOB_RECURSE classes "${stage}/classes/*.class")
run("${d8}" --min-api "${MIN_SDK}" --lib "${ANDROID_JAR}" --output "${stage}" ${classes})

# The data folder as the assets' Data/.
if(DATA AND EXISTS "${DATA}")
    file(COPY "${DATA}/" DESTINATION "${stage}/assets/Data")
endif()

# The manifest and the assets, then the dex and the library.
set(unsigned "${stage}/unsigned.apk")
run("${aapt2}" link -I "${ANDROID_JAR}" --manifest "${MANIFEST}" -A "${stage}/assets" -o "${unsigned}")
file(COPY "${LIBRARY}" DESTINATION "${stage}/lib/${ABI}")
run("${JAR}" --update --no-manifest --file "${unsigned}" -C "${stage}" classes.dex -C "${stage}" "lib/${ABI}")

set(aligned "${stage}/aligned.apk")
run("${zipalign}" -f -p 4 "${unsigned}" "${aligned}")
set(keystore "${WORK}/debug.keystore")
if(NOT EXISTS "${keystore}")
    run("${KEYTOOL}" -genkeypair -keystore "${keystore}" -storepass android -keypass android -alias debug -keyalg RSA -keysize 2048
        -validity 10000 -dname "CN=Helix Player Debug")
endif()
run("${apksigner}" sign --ks "${keystore}" --ks-pass pass:android --out "${OUTPUT}" "${aligned}")
file(SIZE "${OUTPUT}" size)
math(EXPR kb "${size} / 1024")
message(STATUS "HelixPlayer.apk: ${kb} KB")
