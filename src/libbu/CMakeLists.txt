# Include directories needed by libbu users
set(BU_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  )

if(BRLCAD_ENABLE_BINARY_ATTRIBUTES)
  set(BU_INCLUDE_DIRS ${BU_INCLUDE_DIRS} ${BSON_INCLUDE_DIR})
endif(BRLCAD_ENABLE_BINARY_ATTRIBUTES)

# locally used but not needed by users of the library
set(BU_LOCAL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/y2038
  ${REGEX_INCLUDE_DIR}
  )
if (${UUID_INCLUDE_DIR})
  set(BU_LOCAL_INCLUDE_DIRS ${BU_LOCAL_INCLUDE_DIRS} ${UUID_INCLUDE_DIR})
endif (${UUID_INCLUDE_DIR})

BRLCAD_LIB_INCLUDE_DIRS(bu BU_INCLUDE_DIRS BU_LOCAL_INCLUDE_DIRS)


# Use configure_file to make copies of the mime input files,
# so CMake will get notified for the need for a re-configure
# if they are edited.
configure_file(${BRLCAD_SOURCE_DIR}/misc/mime.types ${CMAKE_CURRENT_BINARY_DIR}/tmp COPYONLY)
configure_file(${BRLCAD_SOURCE_DIR}/misc/mime_cad.types ${CMAKE_CURRENT_BINARY_DIR}/tmp COPYONLY)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/tmp)

# Run the generation process
include(${CMAKE_CURRENT_SOURCE_DIR}/mime.cmake)

# We do want to install the mime header
install(FILES ${BU_MIME_H_FILE} DESTINATION ${INCLUDE_DIR}/brlcad/bu)

# Year 2038+ support
set_property(SOURCE y2038/time64.c PROPERTY EXTERNAL 1)
include(CheckCCompilerFlag)
check_c_compiler_flag(-fPIC HAVE_FPIC)
if(HAVE_FPIC)
  set_property(SOURCE y2038/time64.c APPEND PROPERTY COMPILE_FLAGS "-fPIC")
endif(HAVE_FPIC)

set(LIBBU_SOURCES
  y2038/time64.c
  affinity.c
  argv.c
  avs.c
  b64.c
  backtrace.c
  badmagic.c
  bitv.c
  bomb.c
  booleanize.c
  brlcad_path.c
  cmd.c
  cmdhist.c
  color.c
  convert.c
  crashreport.c
  ctype.c
  dirent.c
  datetime.c
  dlfcn.c
  encode.c
  endian.c
  env.c
  escape.c
  fchmod.c
  fgets.c
  file.c
  fnmatch.c
  getcwd.c
  gethostname.c
  getopt.c
  globals.c
  hash.c
  heap.c
  hist.c
  hook.c
  htond.c
  htonf.c
  interrupt.c
  kill.c
  lex.c
  linebuf.c
  list.c
  log.c
  magic.c
  malloc.c
  mappedfile.c
  ${BU_MIME_C_FILE}
  mread.c
  observer.c
  opt.c
  parallel.c
  parallel_cpp11thread.cpp
  parse.c
  path.c
  printb.c
  process.c
  progname.c
  ptbl.c
  redblack.c
  realpath.c
  semaphore.c
  sha1.c
  simd.c
  sort.c
  sscanf.c
  scan.c
  str.c
  temp.c
  thread.cpp
  units.c
  uuid.c
  vers.c
  vfont.c
  vlb.c
  vls.c
  vls_vprintf.c
  whereis.c
  which.c
  xdr.c
  )

BRLCAD_ADDLIB(libbu "${LIBBU_SOURCES}" "${CMAKE_THREAD_LIBS_INIT};${DL_LIBRARY};${WINSOCK_LIB};${PSAPI_LIB};${BSON_LIBRARIES};${UUID_LIBRARIES};${REGEX_LIBRARY};${M_LIBRARY}")
set_target_properties(libbu PROPERTIES VERSION 20.0.1 SOVERSION 20 CXX_STANDARD 11)

BRLCAD_ADDDATA(fix.6r vfont)
BRLCAD_ADDDATA(nonie.r.12 vfont)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  add_subdirectory(tests)
endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)

set(bu_ignore_files
  bitv.h
  charclass.h
  mime.cmake
  parallel.h
  sha1.h
  vfont.h
  vls_vprintf.h
  TODO.binary_attributes
  uce-dirent.h
  y2038/LICENSE
  y2038/README.txt
  y2038/time64.h
  y2038/time64_config.h
  y2038/time64_limits.h
  )
CMAKEFILES(${bu_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
