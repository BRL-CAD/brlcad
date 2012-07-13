# lots of TODO items for this stuff...
# 1.  gather in-use version nums for re2c, perplex and lemon
# 2.  gather md5sum results for PERPLEX_TEMPLATE and LEMON_TEMPLATE files
# 2a.  gather md5sum results for input files
# 3.  read numbers from gen_info.cmake to identify baseline for all inputs.

# There are a variety of situations that may result - need to outline them and deal with them appropriately

# define a special build target that will copy generated outputs back into their proper places and generated and updated gen_info.cmake, to automate the syncing process.

macro(MD5 filename md5sum)
  execute_process(COMMAND ${CMAKE_COMMAND} -E md5sum ${filename} OUTPUT_VARIABLE md5string)
  string(REPLACE " ${filename}" "" md5string "${md5string}")
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
      get_filename_component(filename "${fileitem}" NAME)
    else("${fileitem}" STREQUAL "${ITEM_ABS_PATH}")
      set(filefullname "${CURRENT_SOURCE_DIR}/${fileitem}")
      set(filename "${fileitem}")
    endif("${fileitem}" STREQUAL "${ITEM_ABS_PATH}")
    # Got filename components sorted - proceed
    if(NOT EXISTS ${filefullname})
      message(FATAL_ERROR "Attempted to verify non-existant file ${filefullname}")
    endif(NOT EXISTS ${filefullname})
    FILEVAR(${filename} filevar)
    if(NOT baseline_${filevar}_md5)
      message(FATAL_ERROR "No baseline MD5 available for ${filename} - baseline_${filevar}_md5 is not defined.")
    endif(NOT baseline_${filevar}_md5)
    MD5(${filefullname} ${filevar}_md5)
    if(NOT "${${filevar}_md5}" STREQUAL "${baseline_${filevar}_md5}")
      if("${warn}" STREQUAL "1")
	message("\n${filename} differs from baseline: baseline md5 hash is ${baseline_${filevar}_md5} and current hash is ${${filevar}_md5}.\n")
      endif("${warn}" STREQUAL "1")
      set(${resultvar} 0)
    endif(NOT "${${filevar}_md5}" STREQUAL "${baseline_${filevar}_md5}")
  endforeach(fileitem ${filelist})
endmacro(VERIFY_FILES filelist resultvar)


macro(TOOL_VERSIONS)
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
endmacro(TOOL_VERSIONS)

  # Calculate the MD5 sums of the template files used for lemon and perplex
  #MD5_FILES("${LEMON_TEMPLATE}" "${PERPLEX_TEMPLATE}")
if(0)
  # Calculate the MD5 sums of the parser and scanner input files
  FILEVAR("${CMAKE_CURRENT_SOURCE_DIR}/expparse.y" expparse_y)
  MD5("${CMAKE_CURRENT_SOURCE_DIR}/expparse.y" ${expparse_y}_md5)
  message("expparse.y md5: ${${expparse_y}_md5}")
  FILEVAR("${CMAKE_CURRENT_SOURCE_DIR}/expparse.l" expscan_l)
  MD5("${CMAKE_CURRENT_SOURCE_DIR}/expscan.l" ${expscan_l}_md5)
  message("expscan.l md5: ${${expscan_l}_md5}")
endif(0)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
