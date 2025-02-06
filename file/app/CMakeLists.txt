
cmake_minimum_required(VERSION 3.20.0)

# Get the version from Git
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
    COMMAND git describe --tags --long --dirty
    WORKING_DIRECTORY                ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE                  version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE                   stderr
    RESULT_VARIABLE                  return_code
  )
  if(return_code)
    message(STATUS "git describe failed: ${stderr}; ${KERNEL_VERSION_STRING} will be used instead")
  elseif(CMAKE_VERBOSE_MAKEFILE)
    message(STATUS "git describe stderr: ${stderr}")
  endif()
endif()


include_guard(GLOBAL)

# Usage:
#   nrfxlib_calculate_lib_path(<dir> [SOC_MODE] [BASE_DIR <dir> [SOFT_FLOAT_FALLBACK]])
#
# Calculate a library path based on current SoC architecture or SoC name and
# FPU ABI mode.
# The calculated library path is returned in the '<dir>' argument.
#
# If BASE_DIR is specified the calculated path will be converted to an absolute
# path starting from BASE_DIR. The absolute path will be checked for existence
# and if the absolute path does not exists, the returned value will be
# '<dir>-NOTFOUND'.
#
# SOC_MODE:            Use the SoC name instead of the SoC architecture when calculating the path.
#                      For example 'nrf52840' will be used for the path instead of 'cortex-m4'
# BASE_DIR:            Base path from where the calculated path should start from.
#                      When specifying BASE_DIR the path returned will be absolute, or
#                      '<dir>-NOTFOUND' if the path does not exists
# SOFT_FLOAT_FALLBACK: Allow soft float library path fallback if softfp-float is the FPU ABI in use
#                      and no 'softfp-float' is found.
#                      This flag requires 'BASE_DIR'
#
function(nrfxlib_calculate_lib_path lib_path)
  cmake_parse_arguments(CALC_LIB_PATH "SOFT_FLOAT_FALLBACK;SOC_MODE" "BASE_DIR" "" ${ARGN})

  if(CALC_LIB_PATH_SOFT_FLOAT_FALLBACK AND NOT DEFINED CALC_LIB_PATH_BASE_DIR)
    message(WARNING "nrfxlib_calculate_lib_path(SOFT_FLOAT_FALLBACK ...) "
      "depends on argument: BASE_DIR."
    )
  endif()
  if(${CALC_LIB_PATH_SOC_MODE})
    # CMake regex does not support {4}
    string(REGEX REPLACE "_[a-zA-Z][a-zA-Z][a-zA-Z][a-zA-Z]$" "" arch_soc_dir ${CONFIG_SOC})
  elseif(CONFIG_SOC_SERIES_BSIM_NRFXX)
    set(arch_soc_dir "bsim_nrfxx")
  else()
    # Add Arch type
    assert(GCC_M_CPU "GCC_M_CPU must be set to find correct lib.")
    set(arch_soc_dir ${GCC_M_CPU})
  endif()

  # Set floating ABI
  if(CONFIG_FPU)
    if(CONFIG_FP_HARDABI)
      set(float_dir hard-float)
    elseif(CONFIG_FP_SOFTABI)
      set(float_dir softfp-float)
      if(CALC_LIB_PATH_SOFT_FLOAT_FALLBACK AND DEFINED CALC_LIB_PATH_BASE_DIR)
        list(APPEND float_dir soft-float)
      endif()
    else()
      assert(0 "Unreachable code")
    endif()
  else()
    set(float_dir soft-float)
  endif()
  list(FIND COMPILER_OPT_AS_LIST "-fshort-wchar" SHORT_WCHAR_INDEX)
  if (NOT (SHORT_WCHAR_INDEX EQUAL -1))
    set(short_wchar "/short-wchar")
  endif()

  if(DEFINED CALC_LIB_PATH_BASE_DIR)
    foreach(dir ${float_dir})
      set(${lib_path} "${CALC_LIB_PATH_BASE_DIR}/lib/${arch_soc_dir}/${dir}${short_wchar}")
      if(EXISTS ${${lib_path}})
        set(${lib_path} ${${lib_path}} PARENT_SCOPE)
        return()
      endif()
    endforeach()
    set(${lib_path} ${lib_path}-NOTFOUND PARENT_SCOPE)
  else()
    set(${lib_path} "lib/${arch_soc_dir}/${float_dir}${short_wchar}" PARENT_SCOPE)
  endif()
endfunction()

if(version)
    string(FIND ${version} "-dirty" status REVERSE)
endif()


# Add option to select release type
set(RELEASE_TYPE "FINAL" CACHE STRING "Set release type to ALPHA or BETA or FINAL or FEATURE")
set_property(CACHE RELEASE_TYPE PROPERTY STRINGS "ALPHA" "BETA" "FINAL" "FEATURE")

# Optional: Set a default feature string for the FEATURE release type
set(FEATURE_STRING "" CACHE STRING "Custom feature string to append to the version for FEATURE release type")

if(version)
  string(REGEX REPLACE "^([(A-z)|(a-z)|(0-9)]+).*$" "\\1" version_major ${version})
  string(REGEX REPLACE "^[(A-z)|(a-z)|(0-9)]+\.([0-9]+).*$" "\\1" version_minor "${version}")
  string(REGEX REPLACE "^[(A-z)|(a-z)|(0-9)]+\.[0-9]+\.([0-9]+).*$" "\\1" version_patch "${version}")

  # Configure settings based on release type
  if(RELEASE_TYPE STREQUAL "ALPHA")
    set(rel_type "-a")
  elseif(RELEASE_TYPE STREQUAL "BETA")
    set(rel_type "-b")
  elseif(RELEASE_TYPE STREQUAL "FEATURE")
    set(rel_type "-${FEATURE_STRING}")
  endif()

    # Create version.conf
  file(WRITE version.conf "CONFIG_APP_VERSION_MAJOR=${version_major}\n")
  file(APPEND version.conf "CONFIG_APP_VERSION_MINOR=${version_minor}\n")
  file(APPEND version.conf "CONFIG_APP_VERSION_PATCH=${version_patch}\n")
  file(APPEND version.conf "CONFIG_APP_VERSION=\"${version_major}.${version_minor}.${version_patch}${rel_type}\"\n")
  file(APPEND version.conf "CONFIG_BT_DIS_FW_REV_STR=\"${version_major}.${version_minor}.${version_patch}${rel_type}\"\n")
  file(APPEND version.conf "CONFIG_MEMFAULT_NCS_FW_VERSION=\"${version_major}.${version_minor}.${version_patch}\"\n")

  message(STATUS "Version: ${version_major}.${version_minor}.${version_patch}${rel_type}")

endif()

# list(APPEND SYSCALL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../../sh_sdk_2_0/drivers/sensor/max30001)
# set(SYSCALL_INCLUDE_DIRS ${SYSCALL_INCLUDE_DIRS} PARENT_SCOPE)

set(EXTRA_CONF_FILE  ${CMAKE_CURRENT_SOURCE_DIR}/version.conf)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app)

zephyr_include_directories(memfault_config)

target_include_directories(app PRIVATE ../../modules/hal/nordic/nrfx/hal/)
target_include_directories(app PRIVATE ../../nrf/include/)
target_include_directories(app PRIVATE ../../sh_sdk_2_0/sh_modules/)
target_include_directories(app PRIVATE ../../sh_sdk_2_0/drivers/sensor/)
target_include_directories(app PRIVATE ../../sh_sdk_2_0/drivers/haptic)
target_include_directories(app PRIVATE ../../sh_sdk_2_0/drivers/ktd2026)
target_include_directories(app PRIVATE ../../sh_sdk_2_0/drivers/sensor/lp5813)
target_include_directories(app PRIVATE src/data_storage/)
target_include_directories(app PRIVATE src/)
target_include_directories(app PRIVATE ../../signal/core/)
target_include_directories(app PRIVATE ../../signal/core/accl/)
target_include_directories(app PRIVATE ../../signal/core/analyzer/)
target_include_directories(app PRIVATE ../../signal/core/config_common/)
target_include_directories(app PRIVATE ../../signal/core/config_embedded/)
target_include_directories(app PRIVATE ../../signal/core/data/)
target_include_directories(app PRIVATE ../../signal/core/ecg/)
target_include_directories(app PRIVATE ../../signal/core/fall/)
target_include_directories(app PRIVATE ../../signal/core/filter/)
target_include_directories(app PRIVATE ../../signal/core/frequency/)
target_include_directories(app PRIVATE ../../signal/core/interface/)
target_include_directories(app PRIVATE ../../signal/core/message_log/)
target_include_directories(app PRIVATE ../../signal/core/models/)
target_include_directories(app PRIVATE ../../signal/core/neural/)
target_include_directories(app PRIVATE ../../signal/core/number/)
target_include_directories(app PRIVATE ../../signal/core/pat/)
target_include_directories(app PRIVATE ../../signal/core/ppg/)
target_include_directories(app PRIVATE ../../signal/core/process/)
target_include_directories(app PRIVATE ../../signal/core/respiration/)
target_include_directories(app PRIVATE ../../signal/core/runtime/)
target_include_directories(app PRIVATE ../../signal/core/scg/)
target_include_directories(app PRIVATE ../../signal/core/shrd/)
target_include_directories(app PRIVATE ../../signal/core/temp/)
target_include_directories(app PRIVATE ../../signal/core/limb_sleep/)

include(../../sh_sdk_2_0/drivers/charger/PTX30/CMakeLists.txt)
target_link_libraries(app PRIVATE ptx30)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/)
target_include_directories(app PRIVATE ../../sh_sdk_2_0/sh_ble_service/)

FILE(GLOB app_library_c ../../sh_sdk_2_0/sh_modules/src/*.c)
FILE(GLOB app_library_cpp ../../sh_sdk_2_0/sh_modules/src/*.cpp)
FILE(GLOB app_sources_cpp src/*.cpp)
FILE(GLOB app_data_storage_cpp src/data_storage/*.cpp)
FILE(GLOB app_sources_c src/*.c)
FILE(GLOB app_ble_service_c ../../sh_sdk_2_0/sh_ble_service/*/*.c)
FILE(GLOB app_ble_service_cpp ../../sh_sdk_2_0/sh_ble_service/*/*.cpp)
FILE(GLOB app_segno_c ../../signal/core/*/*.c)

add_definitions( -DM_PI=3.141592653589793238462643383279502884197163993751 )
add_definitions( -DM_PI_2=1.57079632679489661923)

add_definitions( -Dasm=__asm)
add_definitions( -DSEGNO_COMPILE_ENABLE_ASM=0)

if (CONFIG_SEGNO_SCRATCH_PROGRAM)
#Enable the scratch program related configurations from segno
add_definitions( -DARIA_SCRATCH_PROGRAM=1)
endif()

set_source_files_properties(${app_segno_c}
                            PROPERTIES
                            COMPILE_FLAGS "-w")

target_sources(app PRIVATE
    ${app_library_c}
    ${app_library_cpp}
    ${app_sources_cpp}
    ${app_data_storage_cpp}
    ${app_sources_c}
    ${app_ble_service_c}
    ${app_ble_service_cpp}
    ${app_segno_c}
    )
