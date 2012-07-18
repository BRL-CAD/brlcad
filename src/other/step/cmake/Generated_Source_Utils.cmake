# Utility routines for managing generated files with CMake

macro(MD5 filename md5sum)
  file(READ "${filename}" RAW_MD5_FILE)
  string(REGEX REPLACE "\r?\n" "" STRIPPED_MD5_FILE "${RAW_MD5_FILE}")
  file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/md5_file "${STRIPPED_MD5_FILE}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E md5sum ${CMAKE_BINARY_DIR}/CMakeTmp/md5_file OUTPUT_VARIABLE md5string)
  file(REMOVE ${CMAKE_BINARY_DIR}/CMakeTmp/md5_file)
  string(REPLACE " ${CMAKE_BINARY_DIR}/CMakeTmp/md5_file" "" md5string "${md5string}")
  string(STRIP "${md5string}" ${md5sum})
endmacro(MD5)

macro(FILEVAR filename var)
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" ${var} ${filename})
endmacro(FILEVAR)

macro(VERIFY_FILES filelist warn resultvar)
  set(${resultvar} 1)
  foreach(fileitem ${filelist})
    # Deal with absolute and relative paths a bit differently
    get_filename_component(ITEM_ABS_PATH "${fileitem}" ABSOLUTE)
    if("${fileitem}" STREQUAL "${ITEM_ABS_PATH}")
      set(filefullname "${fileitem}")
    else("${fileitem}" STREQUAL "${ITEM_ABS_PATH}")
      set(filefullname "${CURRENT_SOURCE_DIR}/${fileitem}")
    endif("${fileitem}" STREQUAL "${ITEM_ABS_PATH}")
    get_filename_component(filename "${fileitem}" NAME)
    # Got filename components sorted - proceed
    if(NOT EXISTS ${filefullname})
      message(FATAL_ERROR "Attempted to verify non-existant file ${filefullname}")
    endif(NOT EXISTS ${filefullname})
    FILEVAR(${filename} filevar)
    if(NOT baseline_${filevar}_md5)
      message(FATAL_ERROR "No baseline MD5 available for ${filename} - baseline_${filevar}_md5 is not defined")
    endif(NOT baseline_${filevar}_md5)
    MD5(${filefullname} ${filevar}_md5)
    if(NOT "${${filevar}_md5}" STREQUAL "${baseline_${filevar}_md5}")
      if("${warn}" STREQUAL "1")
	message("\n${filename} differs from baseline: baseline md5 hash is ${baseline_${filevar}_md5} and current hash is ${${filevar}_md5}\n")
      endif("${warn}" STREQUAL "1")
      set(${resultvar} 0)
    else(NOT "${${filevar}_md5}" STREQUAL "${baseline_${filevar}_md5}")
      if("${warn}" STREQUAL "1")
	message("${fileitem} OK")
      endif("${warn}" STREQUAL "1")
    endif(NOT "${${filevar}_md5}" STREQUAL "${baseline_${filevar}_md5}")
  endforeach(fileitem ${filelist})
endmacro(VERIFY_FILES filelist resultvar)

macro(WRITE_MD5_SUMS filelist outfile)
  foreach(fileitem ${filelist})
    # Deal with absolute and relative paths a bit differently
    get_filename_component(ITEM_ABS_PATH "${fileitem}" ABSOLUTE)
    if("${fileitem}" STREQUAL "${ITEM_ABS_PATH}")
      set(filefullname "${fileitem}")
    else("${fileitem}" STREQUAL "${ITEM_ABS_PATH}")
      set(filefullname "${CURRENT_SOURCE_DIR}/${fileitem}")
    endif("${fileitem}" STREQUAL "${ITEM_ABS_PATH}")
    get_filename_component(filename "${fileitem}" NAME)
    # Got filename components sorted - proceed
    if(NOT EXISTS ${filefullname})
      message(FATAL_ERROR "Attempted to get MD5 sum of non-existant file ${filefullname}")
    endif(NOT EXISTS ${filefullname})
    FILEVAR(${filename} filevar)
    MD5(${filefullname} ${filevar}_md5)
    file(APPEND ${outfile} "set(baseline_${filevar}_md5 ${${filevar}_md5})\n")
  endforeach(fileitem ${filelist})
endmacro(WRITE_MD5_SUMS)


macro(GET_GENERATOR_EXEC_VERSIONS)
  # Read lemon version
  execute_process(COMMAND ${LEMON_EXECUTABLE} -x OUTPUT_VARIABLE lemon_version)
  string(REPLACE "Lemon version " "" lemon_version "${lemon_version}")
  string(STRIP "${lemon_version}" lemon_version)
  # Read re2c version
  execute_process(COMMAND ${RE2C_EXECUTABLE} -V OUTPUT_VARIABLE re2c_version)
  string(STRIP "${re2c_version}" re2c_version)
  # Read perplex version
  execute_process(COMMAND ${PERPLEX_EXECUTABLE} -v OUTPUT_VARIABLE perplex_version)
  string(STRIP "${perplex_version}" perplex_version)
endmacro(GET_GENERATOR_EXEC_VERSIONS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
