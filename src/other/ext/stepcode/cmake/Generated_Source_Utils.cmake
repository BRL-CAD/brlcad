# Utility routines for managing generated files with CMake

macro(MD5 filename md5sum)
  file(READ "${filename}" RAW_MD5_FILE)
  string(REGEX REPLACE "\r" "" STRIPPED_MD5_FILE "${RAW_MD5_FILE}")
  string(MD5 ${md5sum} "${STRIPPED_MD5_FILE}")
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

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
