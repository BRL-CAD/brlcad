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
  ${CMAKE_CURRENT_SOURCE_DIR}/y2038
  )
if (UUID_INCLUDE_DIR)
  set(BU_LOCAL_INCLUDE_DIRS ${BU_LOCAL_INCLUDE_DIRS} ${UUID_INCLUDE_DIR})
endif (UUID_INCLUDE_DIR)

BRLCAD_LIB_INCLUDE_DIRS(bu BU_INCLUDE_DIRS BU_LOCAL_INCLUDE_DIRS)


# Use configure_file to make copies of the mime input files,
# so CMake will get notified for the need for a re-configure
# if they are edited.
configure_file(${BRLCAD_SOURCE_DIR}/misc/mime.types ${CMAKE_CURRENT_BINARY_DIR}/mime.tmp COPYONLY)
configure_file(${BRLCAD_SOURCE_DIR}/misc/mime_cad.types ${CMAKE_CURRENT_BINARY_DIR}/mime.tmp COPYONLY)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/mime.tmp)

# Run the generation process
include(${CMAKE_CURRENT_SOURCE_DIR}/mime.cmake)

# We do want to install the mime header
set_source_files_properties("${BU_MIME_TYPES_H_FILE}" PROPERTIES GENERATED TRUE)
BUILD_CFG_HDR("${BU_MIME_TYPES_H_FILE}" ${INCLUDE_DIR}/brlcad/bu)

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
  bu_init.cpp
  cmd.c
  color.cpp
  convert.c
  crashreport.c
  ctype.c
  dir.c
  dirent.c
  damlevlim.cpp
  datetime.c
  dylib.c
  encode.c
  endian.c
  env.c
  escape.c
  fchmod.c
  fgets.c
  file.c
  fort.c
  fnmatch.c
  getcwd.c
  gethostname.c
  getopt.c
  glob.c
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
  num.c
  observer.c
  opt.c
  parallel.c
  parse.c
  path.c
  path_normalize.c
  printb.c
  process.c
  progname.c
  ptbl.c
  redblack.c
  realpath.c
  semaphore.c
  semaphore_register.cpp
  sha1.c
  simd.c
  sort.c
  sscanf.c
  scan.c
  snooze.c
  str.c
  tc.c
  tcllist.c
  tbl.c
  temp.c
  thread.cpp
  units.c
  units_dehumanize.c
  units_humanize.c
  uuid.c
  vers.c
  vfont.c
  vlb.c
  vls.c
  vls_incr.cpp
  vls_vprintf.c
  whereami.c
  whereis.c
  which.c
  xdr.c
  )

if(NOT CMAKE_CXX_STANDARD EQUAL 98)
  set(LIBBU_SOURCES ${LIBBU_SOURCES} parallel_cpp11thread.cpp)
else(NOT CMAKE_CXX_STANDARD EQUAL 98)
  CMAKEFILES(parallel_cpp11thread.cpp)
endif(NOT CMAKE_CXX_STANDARD EQUAL 98)

BRLCAD_ADDLIB(libbu "${LIBBU_SOURCES}" "${Foundation_LIBRARIES};Threads::Threads;${DL_LIBRARY};${WINSOCK_LIB};${PSAPI_LIB};${BSON_LIBRARIES};${UUID_LIBRARIES};${M_LIBRARY}")
set_target_properties(libbu PROPERTIES VERSION 20.0.1 SOVERSION 20)

# Define a pre-build test for libbu to check the semaphores in BRL-CAD's headers for
# ordering issues
#set(BCAD_HDRS_FILE "${CMAKE_CURRENT_BINARY_DIR}/brlcad_headers.txt")
#file(REMOVE "${BCAD_HDRS_FILE}")
#file(GLOB_RECURSE BCAD_HDRS "${CMAKE_SOURCE_DIR}/include/*.h*")
#foreach(HDRF ${BCAD_HDRS})
#  if (NOT "${HDRF}" MATCHES ".*.svn.*")
#    file(APPEND "${BCAD_HDRS_FILE}" "${HDRF}\n")
#  endif (NOT "${HDRF}" MATCHES ".*.svn.*")
#endforeach(HDRF ${BCAD_HDRS})
#DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/brlcad_headers.txt")
#BRLCAD_ADDEXEC(semchk "tests/semchk.cpp" "" NO_INSTALL)
#add_custom_command(
#  TARGET libbu PRE_BUILD
#  COMMAND $<TARGET_FILE:semchk> "${BCAD_HDRS_FILE}"
#  COMMENT "Validating BRL-CAD library semaphore definitions"
#  DEPENDS semchk
#  )

BRLCAD_ADDDATA(fix.6r vfont)
BRLCAD_ADDDATA(nonie.r.12 vfont)

add_subdirectory(tests)

set(bu_ignore_files
  CMakeLists.txt
  bitv.h
  charclass.h
  fort.h
  mime.cmake
  parallel.h
  process.h
  tests/semchk.cpp
  sha1.h
  vfont.h
  vls_vprintf.h
  TODO.binary_attributes
  uce-dirent.h
  whereami.h
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
