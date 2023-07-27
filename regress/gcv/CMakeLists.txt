# helper function for setting up gcv conversion tests

function(GCV_regress_util fmt INPUT_FILES)
  # EXPORTER            : for testing export from .g file (g->fmt)
  #     note- the EXPORTER opt creates a different regression target 'regress-g-{fmt} vs regress-{fmt}-g'
  # GCV_OPTIONS         : for gcv run flags
  # EXPECTED_FILE_SIZE  : for conversions resulting in file sizes outside of 150-850 bytes
  # DEPS                : for converters using 3rd party build dependencies (i.e. gcv-gdal, gcv-assetimport)
  # CONVERTERS          : for file types with multiple converters (i.e. obj). runs 'default' and then specified converter(s)
  cmake_parse_arguments("${fmt}" "EXPORTER" "DEPS;EXPECTED_FILE_SIZE" "GCV_OPTIONS;CONVERTERS" ${ARGN})

  if(${fmt}_EXPORTER)
    set(TARGET_DIRECTION "regress-g-${fmt}")
  else()
    set(TARGET_DIRECTION "regress-${fmt}-g")
  endif(${fmt}_EXPORTER)

  # BRLCAD_REGRESSION_TEST is expecting a cmake.in file - create for this test
  set(cmake_in_path "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_DIRECTION}.cmake.in")
  file(WRITE "${cmake_in_path}" "# cmake file for ${fmt}\n\n")

  # Use a default file size check for converted .g's to make sure we have
  # a reasonable output. This doesn't ensure the data is correct.
  # most test files are small, set default expected converted file size range 
  # 150-850 bytes
  if(${fmt}_EXPECTED_FILE_SIZE)
    set(EXPECTED_FILE_SIZE ${${fmt}_EXPECTED_FILE_SIZE})
    # set the delta to 3% of file size
    math(EXPR FILE_SIZE_DELTA "${EXPECTED_FILE_SIZE} * 3 / 100")
    if(FILE_SIZE_DELTA LESS 350)
      set(FILE_SIZE_DELTA 350)
    endif(FILE_SIZE_DELTA LESS 350)
  else()
    set(EXPECTED_FILE_SIZE 500)
    set(FILE_SIZE_DELTA 350)
  endif(${fmt}_EXPECTED_FILE_SIZE)

  # setup logfile and gcv EXEC call in cmake.in
  file(APPEND "${cmake_in_path}"
  "
# lets cmake if() recognize numbers and boolean constants.
if (POLICY CMP0012)
  cmake_policy(SET CMP0012 NEW)
endif (POLICY CMP0012)

# Values set at CMake configure time
set(CSDIR \"@CMAKE_CURRENT_SOURCE_DIR@\")
set(CBDIR \"@CMAKE_CURRENT_BINARY_DIR@\")
set(LOGFILE \"\${CBDIR}/${TARGET_DIRECTION}.log\")

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
    get_filename_component(curr_in_basename ${in_file} NAME_WLE)
    set(out_path "${CMAKE_CURRENT_BINARY_DIR}/conv-${curr_in_basename}")

    # need to split suffix so we can distinguish from different converters
    if(${fmt}_EXPORTER)
      # create fmt file from input's basename
      set(out_suffix ".${fmt}")
    else()
      # create .g from input file basename
      set(out_suffix ".g")
    endif(${fmt}_EXPORTER)
    # keep track for distclean
    set(expected_output_files "${expected_output_files} ${out_path}${out_suffix}")

    # command string can't have empty "" - have to build it explicitly with cmd line flags if supplied
    if(${fmt}_GCV_OPTIONS)
      set(CMD_STR "COMMAND \"\${GCV_EXEC}\" \"${${fmt}_GCV_OPTIONS}\" \"\${in_path}\" \"${out_path}${out_suffix}\"")
    else()
      set(CMD_STR "COMMAND \"\${GCV_EXEC}\" \"\${in_path}\" \"${out_path}${out_suffix}\"")
    endif(${fmt}_GCV_OPTIONS)

    foreach(out_fmt ${${fmt}_CONVERTERS})
      if(${fmt}_EXPORTER)
        set(fmt_input_output "--input-format=brlcad\" \"--output-format=${out_fmt}")
      else()
        set(fmt_input_output "--input-format=${out_fmt}\" \"--output-format=brlcad")
      endif(${fmt}_EXPORTER)

      # command string can't have empty "" - have to build it explicitly with cmd line flags if supplied
      if(${fmt}_GCV_OPTIONS)
        set(CMD_STR "${CMD_STR} COMMAND \"\${GCV_EXEC}\" \"${${fmt}_GCV_OPTIONS}\" \"${fmt_input_output}\" \"\${in_path}\" \"$${out_path}-${out_fmt}${out_suffix}\"")
      else()
        set(CMD_STR "${CMD_STR} COMMAND \"\${GCV_EXEC}\" \"${fmt_input_output}\" \"\${in_path}\" \"${out_path}-${out_fmt}${out_suffix}\"")
      endif(${fmt}_GCV_OPTIONS)

      set(expected_output_files "${expected_output_files} ${out_path}-${out_fmt}${out_suffix}")
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
elseif(${${fmt}_EXPORTER} AND EXISTS \${CSDIR}/../${in_file})
  # if this is an exporter, check regress/gcv for .g files used to test multiple converters
  set(in_path \${CSDIR}/../${in_file})
else()
  message(FATAL_ERROR \"[gcv] could not find ${in_file}, searched \${CSDIR} and \${CBDIR}. aborting.\")
endif(EXISTS \${CSDIR}/${in_file})

# blow away expected output file if it exists for some reason
foreach(of ${expected_output_files})
  file(REMOVE \"\${of}\")
endforeach(of ${expected_output_files})

# Run the converter for ${in_file}
set(log_msg \"Running:${cleaned_CMD_STR}\\n\\n\")
file(APPEND \"\${LOGFILE}\" \"\${log_msg}\")

execute_process(
  ${CMD_STR}
  RESULTS_VARIABLE curr_result OUTPUT_VARIABLE log_msg ERROR_VARIABLE log_msg
  )

file(APPEND \"\${LOGFILE}\" \"\${log_msg}\\n\")
unset(log_msg)
  ")

  set(OUTPUT_FILES "${OUTPUT_FILES} ${expected_output_files}")
  unset(expected_output_files)
  endforeach(in_file ${INPUT_FILES})

  file(APPEND ${cmake_in_path} 
  "
# check output files exists and are a reasonable size
foreach(out_file ${OUTPUT_FILES})
  if(NOT EXISTS \"\${out_file}\")
    file(APPEND \"\${LOGFILE}\" \"FAILURE: \${curr_result}, \${out_file} not found.\")
    message(FATAL_ERROR \"[gcv] Unable to convert \${in_path} to \${out_file} with \${GCV_EXEC}, aborting.\\nSee \${LOGFILE} for more details.\")
  else()
    file(SIZE \"\${out_file}\" file_size)	# size in bytes
    if(\"\${file_size}\" GREATER \"${EXPECTED_FILE_SIZE}\")
      math(EXPR size_delta \"\${file_size} - ${EXPECTED_FILE_SIZE}\")
    else(\"\${file_size}\" GREATER \"${EXPECTED_FILE_SIZE}\")
      math(EXPR size_delta \"${EXPECTED_FILE_SIZE} - \${file_size}\")
    endif(\"\${file_size}\" GREATER \"${EXPECTED_FILE_SIZE}\")

    if(\"\${size_delta}\" GREATER \"${FILE_SIZE_DELTA}\")
      file(APPEND \"\${LOGFILE}\" \"FAILURE: \${curr_result} file size is \${file_size} for \${out_file}\\n    Expecting ${EXPECTED_FILE_SIZE} with delta ${FILE_SIZE_DELTA}\")
      message(FATAL_ERROR \"[gcv] Failure while converting ${in_file}, see \${LOGFILE} for more info.\")
    endif(\"\${size_delta}\" GREATER \"${FILE_SIZE_DELTA}\")
  endif(NOT EXISTS \"\${out_file}\")\n

  # cleanup file from build dir
  file(REMOVE \"\${out_file}\")
endforeach(out_file ${OUTPUT_FILES})
    ")

  # assume the fmt is the gcv dependency unless specified
  if(${fmt}_DEPS)
    set(DEPS ${${fmt}_DEPS})
  else()
    set(DEPS "gcv;gcv-${fmt}")
  endif(${fmt}_DEPS)

  # create actual regression target
  BRLCAD_REGRESSION_TEST(${TARGET_DIRECTION} ${DEPS} EXEC gcv TEST_SCRIPT "${cmake_in_path}")

  # BRLCAD_REGRESSION_TEST copies and evalutates the cmake.in - safe to get rid of it now
  file(REMOVE ${cmake_in_path})

  DISTCLEAN(
    "${cmake_in_path}"                                      # cmake.in file
    "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_DIRECTION}.log"   # logfile
    "${OUTPUT_FILES}"                                       # any expected output files
  )
endfunction()

# each file type to be tested should have its own directory
add_subdirectory(3ds)
add_subdirectory(3mf)
add_subdirectory(assetimport-binary)
add_subdirectory(assetimport-xml)
add_subdirectory(dae)
add_subdirectory(dem)
add_subdirectory(fastgen)
add_subdirectory(fbx)
add_subdirectory(gltf)
add_subdirectory(json)
add_subdirectory(lwo)
add_subdirectory(obj)
add_subdirectory(off)
add_subdirectory(ply)
add_subdirectory(rhino)
add_subdirectory(stl)
add_subdirectory(stp)
add_subdirectory(vrml)
add_subdirectory(x)
add_subdirectory(x3d)

CMAKEFILES(
  CMakeLists.txt
  README
  TESTING_STATUS
  tetra.g           # used for exporters - keep in root to eliminate copying duplicate file to all subdirectories
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
