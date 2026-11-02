if(NOT DEFINED CREO_BUNDLE_BIN_DIR OR NOT IS_DIRECTORY "${CREO_BUNDLE_BIN_DIR}")
  message(FATAL_ERROR "Creo-BRL bundle bin directory is missing: ${CREO_BUNDLE_BIN_DIR}")
endif()

get_filename_component(CREO_BUNDLE_DIR "${CREO_BUNDLE_BIN_DIR}" DIRECTORY)
set(required_bundle_files
  bin/creo-brl.dll
  bin/creo-brl-core.dll
  config/Creo_to_BRL.g
  creo-brl.dat
  relocate.bat
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

if(WIN32)
  get_filename_component(CREO_BUNDLE_PARENT_DIR "${CREO_BUNDLE_DIR}" DIRECTORY)
  set(relocate_test_dir "${CREO_BUNDLE_PARENT_DIR}/relocate_test")
  file(REMOVE_RECURSE "${relocate_test_dir}")
  file(MAKE_DIRECTORY "${relocate_test_dir}/bundle")
  file(COPY "${CREO_BUNDLE_DIR}/relocate.bat" DESTINATION "${relocate_test_dir}/bundle")
  file(WRITE "${relocate_test_dir}/bundle/creo-brl.dat"
    "name creo-brl\nstartup dll\nallow_stop TRUE\nexec_file C:/old/bin/creo-brl.dll\ntext_dir C:/old/text\ndescription CREO to BRL-CAD\nend\n")
  file(WRITE "${relocate_test_dir}/config.pro"
    "pro_unit_length unit_inch\nprotkdat C:\\old\\creo-brl.dat\n")
  file(TO_NATIVE_PATH "${relocate_test_dir}/config.pro" native_config_pro)
  file(TO_NATIVE_PATH "${relocate_test_dir}/bundle/creo-brl.dat" native_dat_file)
  file(TO_NATIVE_PATH "${relocate_test_dir}/bundle/relocate.bat" native_relocate_bat)
  file(WRITE "${relocate_test_dir}/decline.txt" "n\n")
  file(WRITE "${relocate_test_dir}/confirm.txt" "y\n")

  execute_process(
    COMMAND "${native_relocate_bat}" "${native_config_pro}"
    INPUT_FILE "${relocate_test_dir}/decline.txt"
    RESULT_VARIABLE decline_result
    OUTPUT_VARIABLE decline_output
    ERROR_VARIABLE decline_error
    )
  if(NOT decline_result EQUAL 0)
    message(FATAL_ERROR "relocate.bat (decline) failed: ${decline_error}")
  endif()
  set(expected_prompt "Update ${native_config_pro} to ${native_dat_file} [Y/n]")
  string(FIND "${decline_output}" "${expected_prompt}" prompt_pos)
  if(prompt_pos EQUAL -1)
    message(FATAL_ERROR "Expected prompt '${expected_prompt}', got '${decline_output}'")
  endif()
  require_dat_entry("${relocate_test_dir}/bundle/creo-brl.dat" exec_file
    "${relocate_test_dir}/bundle/bin/creo-brl.dll")
  require_dat_entry("${relocate_test_dir}/bundle/creo-brl.dat" text_dir
    "${relocate_test_dir}/bundle/text")
  file(READ "${relocate_test_dir}/config.pro" declined_config)
  string(FIND "${declined_config}" "C:\\old\\creo-brl.dat" old_pos)
  if(old_pos EQUAL -1)
    message(FATAL_ERROR "config.pro was modified despite declining confirmation")
  endif()

  execute_process(
    COMMAND "${native_relocate_bat}" "${native_config_pro}"
    INPUT_FILE "${relocate_test_dir}/confirm.txt"
    RESULT_VARIABLE confirm_result
    OUTPUT_VARIABLE confirm_output
    ERROR_VARIABLE confirm_error
    )
  if(NOT confirm_result EQUAL 0)
    message(FATAL_ERROR "relocate.bat (confirm) failed: ${confirm_error}")
  endif()
  file(STRINGS "${relocate_test_dir}/config.pro" updated_config_lines)
  list(FIND updated_config_lines "pro_unit_length unit_inch" unit_line_index)
  list(FIND updated_config_lines "protkdat ${native_dat_file}" dat_line_index)
  if(unit_line_index EQUAL -1 OR dat_line_index EQUAL -1)
    message(FATAL_ERROR "config.pro was not updated as expected: ${updated_config_lines}")
  endif()
  file(REMOVE_RECURSE "${relocate_test_dir}")
endif()
