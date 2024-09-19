BRLCAD_Find_Package(LMDB REQUIRED)

if (BRLCAD_ENABLE_BINARY_ATTRIBUTES)
  # If this becomes a default we'll need to bundle libbson to ensure it is
  # available - while it is not on by default, require a system version
  find_package(BSON REQUIRED)
endif (BRLCAD_ENABLE_BINARY_ATTRIBUTES)

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
  cache.cpp
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
  fchmod.cpp
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
endif(NOT CMAKE_CXX_STANDARD EQUAL 98)

# Note - libbu_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
set(BU_LIBS
  ${Foundation_LIBRARIES}
  Threads::Threads
  ${libbu_deps}
  ${DL_LIBRARY}
  ${WINSOCK_LIB}
  ${PSAPI_LIB}
  ${BSON_LIBRARIES}
  ${LMDB_LIBRARIES}
  ${UUID_LIBRARIES}
  ${M_LIBRARY}
  )

# Include directories needed by libbu users
set(BU_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  )

# locally used but not needed by users of the library
set(BU_LOCAL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/y2038
  ${BSON_INCLUDE_DIR}
  ${LMDB_INCLUDE_DIR}
  ${UUID_INCLUDE_DIR}
  )

BRLCAD_ADDLIB(libbu "${LIBBU_SOURCES}" "${BU_LIBS}" "${BU_INCLUDE_DIRS}" "${BU_LOCAL_INCLUDE_DIRS}")
set_target_properties(libbu PROPERTIES VERSION 20.0.1 SOVERSION 20)

set(bu_atgts)
set(bu_targets libbu-obj libbu libbu-static)
foreach(bt ${bu_targets})
  if (TARGET ${bt})
    list(APPEND bu_atgts ${bt})
  endif ()
endforeach()

foreach(bt ${bu_atgts})
  # LIBBU is where we record some information about the OS environment
  target_compile_definitions(${bt} PRIVATE BRLCAD_ROOT=\"${CMAKE_INSTALL_PREFIX}\")
  target_compile_definitions(${bt} PRIVATE EXECUTABLE_SUFFIX=\"${CMAKE_EXECUTABLE_SUFFIX}\")
  target_compile_definitions(${bt} PRIVATE SHARED_LIBRARY_SUFFIX=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\")
  target_compile_definitions(${bt} PRIVATE SIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P})

  # OpenBSD doesn't define __WORD_SIZE, used by y2038/time64.h.  Remove this
  # once we can replace y2038 with a C++ based solution
  if(${CMAKE_WORD_SIZE} MATCHES "32BIT")
    target_compile_definitions(${bt} PRIVATE __OS_WORDSIZE=32)
  endif(${CMAKE_WORD_SIZE} MATCHES "32BIT")
  if(${CMAKE_WORD_SIZE} MATCHES "64BIT")
    target_compile_definitions(${bt} PRIVATE __OS_WORDSIZE=64)
  endif(${CMAKE_WORD_SIZE} MATCHES "64BIT")

  # Define the various relative paths for bu_dir
  include(Path_Setup)
  target_compile_definitions(${bt} PRIVATE BRLCAD_BIN_DIR=\"${BIN_DIR}\")
  target_compile_definitions(${bt} PRIVATE BRLCAD_LIB_DIR=\"${LIB_DIR}\")
  target_compile_definitions(${bt} PRIVATE BRLCAD_LIBEXEC_DIR=\"${LIBEXEC_DIR}\")
  target_compile_definitions(${bt} PRIVATE BRLCAD_INCLUDE_DIR=\"${INCLUDE_DIR}\")
  target_compile_definitions(${bt} PRIVATE BRLCAD_DATA_DIR=\"${DATA_DIR}\")
  target_compile_definitions(${bt} PRIVATE BRLCAD_DOC_DIR=\"${DOC_DIR}\")
  target_compile_definitions(${bt} PRIVATE BRLCAD_MAN_DIR=\"${MAN_DIR}\")
endforeach(bt ${bu_atgts})

# With newer GCC, our bitv structure triggers a compiler
# warning about writing 1 byte into a region of size 0.
if(${BRLCAD_OPTIMIZED} MATCHES "ON")
  check_c_compiler_flag(-Wno-error=stringop-overflow= HAVE_NO_STRINGOP_OVERFLOW)
  if (HAVE_NO_STRINGOP_OVERFLOW)
    target_link_options(libbu PRIVATE -Wno-error=stringop-overflow=)
  endif (HAVE_NO_STRINGOP_OVERFLOW)
endif(${BRLCAD_OPTIMIZED} MATCHES "ON")

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
  parallel_cpp11thread.cpp
  process.h
  tests/semchk.cpp
  sha1.h
  vfont.h
  vls_vprintf.h
  TODO.binary_attributes
  uce-dirent.h
  whereami.h
  xxhash.h
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
