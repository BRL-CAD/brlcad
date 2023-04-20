# helper functions for setting up gcv conversion tests

function(GCV_to_g_util fmt INPUT_FILES)
  # INPUT_OPTIONS: for gcv run flags
  # EXPECTED_FILE_SIZE: for conversions resulting in file sizes outside of 150-850 bytes
  # DEPS: for converters using 3rd party build dependencies (i.e. gcv-gdal, gcv-assetimport)
  # CONVERTERS: for file types with multiple converters (i.e. obj). runs 'default' and then specifies --input-format(s)
  cmake_parse_arguments("${fmt}" "" "DEPS;EXPECTED_FILE_SIZE" "INPUT_OPTIONS;CONVERTERS" ${ARGN})

  # BRLCAD_REGRESSION_TEST is expecting a cmake.in file - create for this test
  set(cmake_in_path "${CMAKE_CURRENT_BINARY_DIR}/regress-${fmt}-g.cmake.in")
  file(WRITE "${cmake_in_path}" "# cmake file for ${fmt}\n\n")

  # Use a default file size check for converted .g's to make sure we have
  # a reasonable output. This doesn't ensure the data is correct.
  # most test files are small, set default expected converted file size range 
  # 150-850 bytes
  if(${fmt}_EXPECTED_FILE_SIZE)
    set(EXPECTED_FILE_SIZE ${${fmt}_EXPECTED_FILE_SIZE})
    # set the delta to 3% of file size
    math(EXPR FILE_SIZE_DELTA "${EXPECTED_FILE_SIZE} * 3 / 100")
  else()
    set(EXPECTED_FILE_SIZE 500)
    set(FILE_SIZE_DELTA 350)
  endif(${fmt}_EXPECTED_FILE_SIZE)

  # setup logfile and gcv EXEC call in cmake.in
  file(APPEND "${cmake_in_path}"
  "
# Values set at CMake configure time
set(CSDIR \"@CMAKE_CURRENT_SOURCE_DIR@\")
set(CBDIR \"@CMAKE_CURRENT_BINARY_DIR@\")
set(LOGFILE \"\${CBDIR}/regress-${fmt}-g.log\")

file(WRITE \"\${LOGFILE}\" \"Starting ${fmt} testing\\n\\n\")

# The executable locations aren't know at CMake configure time, so one of them
# is passed in via the EXEC variable at runtime.  De-quote it and assign it to
# the appropriate variable.
string(REPLACE \"\\\\\" \"\" GCV_EXEC \"\${EXEC}\")
if(NOT EXISTS \"\${GCV_EXEC}\")
  file(WRITE \"\${LOGFILE}\" \"gcv not found at location \\\"\${GCV_EXEC}\\\" - aborting\\n\")
  message(FATAL_ERROR \"Unable to find gcv, aborting.\\nSee \${LOGFILE} for more details.\")
endif(NOT EXISTS \"\${GCV_EXEC}\")\n"
  )

  # setup conversion tests
  foreach(in_file ${INPUT_FILES})
    # create .g from input file basename
    get_filename_component(curr_in_basename ${in_file} NAME_WLE)
    set(out_path "${CMAKE_CURRENT_BINARY_DIR}/${curr_in_basename}.g")
    # keep track for distclean
    set(OUTPUT_FILES "${OUTPUT_FILES} ${out_path}")

    # command string can't have empty "" - have to build it explicitly with cmd line flags if supplied
    if(${fmt}_INPUT_OPTIONS)
      set(CMD_STR "COMMAND \"\${GCV_EXEC}\" \"${${fmt}_INPUT_OPTIONS}\" \"\${in_path}\" \"${out_path}\"")
    else()
      set(CMD_STR "COMMAND \"\${GCV_EXEC}\" \"\${in_path}\" \"${out_path}\"")
    endif(${fmt}_INPUT_OPTIONS)

    foreach(out_fmt ${${fmt}_CONVERTERS})
      # command string can't have empty "" - have to build it explicitly with cmd line flags if supplied
      if(${fmt}_INPUT_OPTIONS)
        set(CMD_STR "${CMD_STR} COMMAND \"\${GCV_EXEC}\" \"${${fmt}_INPUT_OPTIONS}\" \"--input-format=${out_fmt}\" \"--output-format=brlcad\" \"\${in_path}\" \"${out_path}.${out_fmt}\"")
      else()
        set(CMD_STR "${CMD_STR} COMMAND \"\${GCV_EXEC}\" \"--input-format=${out_fmt}\" \"--output-format=brlcad\" \"\${in_path}\" \"${out_path}.${out_fmt}\"")
      endif(${fmt}_INPUT_OPTIONS)

      set(OUTPUT_FILES "${OUTPUT_FILES} ${out_path}.${out_fmt}")
    endforeach(out_fmt ${${fmt}_CONVERTERS})

    # make command string pretty for printing to log file
    string(REPLACE "\"" "\\\"" cleaned_CMD_STR ${CMD_STR})
    string(REPLACE "COMMAND" "\n  COMMAND" cleaned_CMD_STR ${cleaned_CMD_STR})

    # setup tests in cmake.in
    file(APPEND "${cmake_in_path}"
    "
# check for input file in source and binary dir - priority of src
if(EXISTS \${CSDIR}/${in_file})
  set(in_path \${CSDIR}/${in_file})
elseif(EXISTS \${CBDIR}/${in_file})
  set(in_path \${CBDIR}/${in_file})
else()
  message(FATAL_ERROR \"[gcv] could not find ${in_file}, searched \${CSDIR} and \${CBDIR}. aborting.\")
endif(EXISTS \${CSDIR}/${in_file})

# blow away expected output file if it exists for some reason
foreach(of ${OUTPUT_FILES})
  file(REMOVE \"\${of}\")
endforeach(of ${OUTPUT_FILES})

# Run the converter for ${in_file}
set(log_msg \"Running:${cleaned_CMD_STR}\\n\\n\")
file(APPEND \"\${LOGFILE}\" \"\${log_msg}\")

execute_process(
  ${CMD_STR}
  RESULT_VARIABLE curr_result OUTPUT_VARIABLE log_msg ERROR_VARIABLE log_msg
  )

file(APPEND \"\${LOGFILE}\" \"\${log_msg}\\n\")
set(log_msg)

# check output files exists and are a reasonable size
")

    # setup output files size check in cmake.in
    foreach(out_fmt "" ${${fmt}_CONVERTERS})
      if("${out_fmt}" STREQUAL "")
        set(final_out ${out_path})
      else()
        set(final_out ${out_path}.${out_fmt})
      endif("${out_fmt}" STREQUAL "")

      file(APPEND "${cmake_in_path}"
      "
if(NOT EXISTS \"${final_out}\")
  file(APPEND \"\${LOGFILE}\" \"FAILURE: \${curr_result}\")
  message(FATAL_ERROR \"[gcv] Unable to convert \${in_path} to ${final_out} with \${GCV_EXEC}, aborting.\\nSee \${LOGFILE} for more details.\")
else()
  file(SIZE \"${final_out}\" file_size)	# size in bytes
  if(\"\${file_size}\" GREATER \"${EXPECTED_FILE_SIZE}\")
    math(EXPR size_delta \"\${file_size} - ${EXPECTED_FILE_SIZE}\")
  else(\"\${file_size}\" GREATER \"${EXPECTED_FILE_SIZE}\")
    math(EXPR size_delta \"${EXPECTED_FILE_SIZE} - \${file_size}\")
  endif(\"\${file_size}\" GREATER \"${EXPECTED_FILE_SIZE}\")

  if(\"\${size_delta}\" GREATER \"${FILE_SIZE_DELTA}\")
    file(APPEND \"\${LOGFILE}\" \"FAILURE: file size is \${file_size} for ${final_out}\\n    Expecting ${EXPECTED_FILE_SIZE} with delta ${FILE_SIZE_DELTA}\")
    message(FATAL_ERROR \"[gcv] Failure while converting ${in_file}, see \${LOGFILE} for more info.\")
  endif(\"\${size_delta}\" GREATER \"${FILE_SIZE_DELTA}\")
endif(NOT EXISTS \"${final_out}\")\n
      ")
    endforeach(out_fmt "" ${${fmt}_CONVERTERS})
  endforeach(in_file ${INPUT_FILES})

  # setup cleanup loop for output files in cmake.in
  file(APPEND "${cmake_in_path}"
  "
# Cleanup
foreach(of ${OUTPUT_FILES})
  file(REMOVE \"\${of}\")
endforeach(of ${OUTPUT_FILES})"
  )

  # assume the fmt is the gcv dependency unless specified
  if(${fmt}_DEPS)
    set(DEPS ${${fmt}_DEPS})
  else()
    set(DEPS "gcv;gcv-${fmt}")
  endif(${fmt}_DEPS)

  # create actual regression target
  BRLCAD_REGRESSION_TEST(regress-${fmt}-g ${DEPS} EXEC gcv TEST_SCRIPT "${cmake_in_path}")

  # BRLCAD_REGRESSION_TEST copies and evalutates the cmake.in - safe to get rid of it now
  file(REMOVE ${cmake_in_path})

  DISTCLEAN(
    "${cmake_in_path}"                                  # cmake.in file
    "${CMAKE_CURRENT_BINARY_DIR}/regress-${fmt}-g.log"  # logfile
    "${OUTPUT_FILES}"                                   # any expected output files
  )
endfunction()

# each file type to be tested should have its own directory
add_subdirectory(3ds)
add_subdirectory(3mf)
add_subdirectory(dae)
add_subdirectory(dem)
add_subdirectory(dxf)
add_subdirectory(fastgen)
add_subdirectory(gltf)
add_subdirectory(lwo)
add_subdirectory(obj)
add_subdirectory(ply)
add_subdirectory(stl)
add_subdirectory(vrml)
add_subdirectory(x)
add_subdirectory(x3d)

CMAKEFILES(
  CMakeLists.txt
  README
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
