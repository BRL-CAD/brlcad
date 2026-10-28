if(NOT DEFINED CREO_BUNDLE_BIN_DIR OR NOT IS_DIRECTORY "${CREO_BUNDLE_BIN_DIR}")
  message(FATAL_ERROR "Creo-BRL bundle bin directory is missing: ${CREO_BUNDLE_BIN_DIR}")
endif()

get_filename_component(CREO_BUNDLE_DIR "${CREO_BUNDLE_BIN_DIR}" DIRECTORY)
set(required_bundle_files
  bin/creo-brl.dll
  bin/creo-brl-core.dll
  config/Creo_to_BRL.g
  creo-brl.dat
  README.txt
  text/resource/creo_brl.res
  text/usascii/creo-brl-msg.txt
  text/usascii/ptc_materials.mtl
  )
if(CREO_EXPECT_EXEC_PLUGIN)
  list(APPEND required_bundle_files
    bin/creo-brl-exe.exe
    creo-brl-debug.dat)
endif()

foreach(required_file ${required_bundle_files})
  if(NOT EXISTS "${CREO_BUNDLE_DIR}/${required_file}")
    message(FATAL_ERROR "Creo-BRL bundle is missing ${required_file}")
  endif()
endforeach()

function(require_dat_entry dat_file key expected_value)
  file(STRINGS "${dat_file}" matching_entries REGEX "^${key} ")
  list(LENGTH matching_entries entry_count)
  if(NOT entry_count EQUAL 1 OR
     NOT matching_entries STREQUAL "${key} ${expected_value}")
    message(FATAL_ERROR
      "${dat_file} must contain '${key} ${expected_value}', found '${matching_entries}'")
  endif()
endfunction()

require_dat_entry("${CREO_BUNDLE_DIR}/creo-brl.dat" exec_file
  "${CREO_BUNDLE_DIR}/bin/creo-brl.dll")
require_dat_entry("${CREO_BUNDLE_DIR}/creo-brl.dat" text_dir
  "${CREO_BUNDLE_DIR}/text")

if(CREO_EXPECT_EXEC_PLUGIN)
  require_dat_entry("${CREO_BUNDLE_DIR}/creo-brl-debug.dat" exec_file
    "${CREO_BUNDLE_DIR}/bin/creo-brl-exe.exe")
  require_dat_entry("${CREO_BUNDLE_DIR}/creo-brl-debug.dat" text_dir
    "${CREO_BUNDLE_DIR}/text")
elseif(EXISTS "${CREO_BUNDLE_DIR}/creo-brl-debug.dat" OR
       EXISTS "${CREO_BUNDLE_DIR}/bin/creo-brl-exe.exe")
  message(FATAL_ERROR "Disabled diagnostic plugin leaked into the Creo-BRL bundle")
endif()

file(GLOB_RECURSE bundle_files RELATIVE "${CREO_BUNDLE_DIR}" "${CREO_BUNDLE_DIR}/*")
foreach(bundle_file ${bundle_files})
  if(bundle_file MATCHES "(^|/)(creo_[^/]*_test\\.(exe|pdb)|creo-brl-compile-shim\\.(dll|pdb))$")
    message(FATAL_ERROR "Build-only artifact leaked into Creo-BRL bundle: ${bundle_file}")
  endif()
endforeach()
