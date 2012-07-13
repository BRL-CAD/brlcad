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

macro(MD5_FILES)
  foreach(filename ${ARGN})
    FILEVAR(${filename} filevar)
    MD5("${filename}" ${filevar}_md5)
    message("${filename} md5: ${${filevar}_md5}")
  endforeach(filename ${ARGN})
endmacro(MD5_FILES)

if(LEMON_EXECUTABLE AND PERPLEX_EXECUTABLE AND RE2C_EXECUTABLE)
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

  message("versions:  lemon: ${lemon_version}; re2c: ${re2c_version}; perplex: ${perplex_version}")

  # Calculate the MD5 sums of the template files used for lemon and perplex
  MD5_FILES("${LEMON_TEMPLATE}" "${PERPLEX_TEMPLATE}")

  # Calculate the MD5 sums of the parser and scanner input files
  FILEVAR("${CMAKE_CURRENT_SOURCE_DIR}/expparse.y" expparse_y)
  MD5("${CMAKE_CURRENT_SOURCE_DIR}/expparse.y" ${expparse_y}_md5)
  message("expparse.y md5: ${${expparse_y}_md5}")
  FILEVAR("${CMAKE_CURRENT_SOURCE_DIR}/expparse.l" expscan_l)
  MD5("${CMAKE_CURRENT_SOURCE_DIR}/expscan.l" ${expscan_l}_md5)
  message("expscan.l md5: ${${expscan_l}_md5}")

endif(LEMON_EXECUTABLE AND PERPLEX_EXECUTABLE AND RE2C_EXECUTABLE)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
