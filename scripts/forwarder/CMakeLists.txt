cmake_minimum_required(VERSION 3.13)

if (NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif ()
set(DEVKITPRO $ENV{DEVKITPRO} CACHE BOOL "DEVKITPRO")
include(${DEVKITPRO}/cmake/Switch.cmake REQUIRED)
set(ARCH "-march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE")
set(CMAKE_C_FLAGS "${ARCH} -g -Wall -O2 -ffunction-sections")
set(CMAKE_ASM_FLAGS "-g ${ARCH}")
set(CMAKE_EXE_LINKER_FLAGS "-L${DEVKITPRO}/libnx/lib -specs=${DEVKITPRO}/libnx/switch.specs -g ${ARCH}")
enable_language(ASM)

find_program(HACBREWPACK_EXE NAMES hacbrewpack)
if (NOT HACBREWPACK_EXE)
	message(FATAL_ERROR "Could not find hacbrewpack: try installing https://github.com/The-4n/hacBrewPack")
endif()

project(forwarder)

# NPDM: the program_id comes from PROJECT_TITLEID (root CMakeLists), the
# single source of truth for the title ID — do not hardcode it here.
configure_file("${PROJECT_SOURCE_DIR}/config.json.in" "${PROJECT_BINARY_DIR}/config.json" @ONLY)

set(FORWARDER_SRC "${PROJECT_SOURCE_DIR}/source/main.c" "${PROJECT_SOURCE_DIR}/source/trampoline.s")
add_executable(${PROJECT_NAME} ${FORWARDER_SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE nx)
target_compile_definitions(${PROJECT_NAME} PRIVATE VERSION="2.4.3")

if (NOT PROJECT_KEYSET)
    SET(PROJECT_KEYSET "${PROJECT_BINARY_DIR}/prod.keys")
    file(DOWNLOAD https://raw.githubusercontent.com/externalEmu/s/master/prod.keys ${PROJECT_KEYSET})
endif ()

add_custom_target(${PROJECT_NAME}.pfs0 DEPENDS ${PROJECT_NAME}
    COMMAND ${CMAKE_COMMAND} -E make_directory "exefs"
    COMMAND ${NX_ELF2NSO_EXE} "${PROJECT_NAME}.elf" "exefs/main"
    COMMAND ${NX_NPDMTOOL_EXE} "${PROJECT_BINARY_DIR}/config.json" "exefs/main.npdm"
)

# hacbrewpack writes the NSP with a lower-case title-ID filename, so the rename
# source below must be lower-cased. No-op for all-digit IDs (old pleNx
# 0104201312000000) but required for SMCA 0500474D43410000 (474D4341 = "SMCA"):
# on case-sensitive Linux the upper-case source path is not found.
string(TOLOWER "${PROJECT_TITLEID}" PROJECT_TITLEID_LOWER)

# The tile's NACP version is the HOME menu's cache key for its artwork. Stamped
# 0.1.0 for every build, the console can keep the icon of the FIRST install, and a
# reinstall then changes nothing — which is exactly the "always the old icon, even
# after delete and reinstall" report. Stamp the git build count instead, so every
# build is a version the console has not seen (one byte per component, hence % 256).
set(TILE_VERSION_MICRO 0)
if (VERSION_BUILD MATCHES "^[0-9]+$")
    math(EXPR TILE_VERSION_MICRO "${VERSION_BUILD} % 256")
endif ()

# The HOME tile's icon is baked in here, from the project icon (not from the
# NRO's own --icon): hacbrewpack packs control/icon_AmericanEnglish.dat into the
# NSP, and HintActivity installs that NSP from romfs. Depending on the icon makes
# the intent explicit and keeps a rebuild from reusing a NSP built before the
# artwork changed (the target is always re-run; the remove guards a stale file
# left behind by an interrupted build).
add_custom_target(${PROJECT_NAME}.nsp DEPENDS ${PROJECT_NAME}.pfs0 ${PROJECT_ICON}
    COMMAND ${CMAKE_COMMAND} -E remove "${PROJECT_RESOURCES}/${PROJECT_NAME}.nsp"
    COMMAND ${CMAKE_COMMAND} -E make_directory "control" "logo"
    COMMAND ${NX_NACPTOOL_EXE} --create "${CMAKE_PROJECT_NAME}" "${PROJECT_AUTHOR}" "${VERSION_MAJOR}.${VERSION_MINOR}.${TILE_VERSION_MICRO}" "control/control.nacp"
    COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_SOURCE_DIR}/icon/logo.png" "logo/NintendoLogo.png"
    COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_SOURCE_DIR}/icon/logo.gif" "logo/StartupMovie.gif"
    COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_ICON}" "control/icon_AmericanEnglish.dat"
    COMMAND ${HACBREWPACK_EXE} -k "${PROJECT_KEYSET}" --titleid ${PROJECT_TITLEID} --titlename ${CMAKE_PROJECT_NAME} --noromfs --nostartupuser
    COMMAND ${CMAKE_COMMAND} -E rename "hacbrewpack_nsp/${PROJECT_TITLEID_LOWER}.nsp" "${PROJECT_RESOURCES}/${PROJECT_NAME}.nsp"
)
