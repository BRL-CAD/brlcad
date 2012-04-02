# Include directories needed by libbu users
set(BU_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${TCL_INCLUDE_DIRS}
  )

# locally used but not needed by users of the library
set(BU_LOCAL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

BRLCAD_LIB_INCLUDE_DIRS(bu BU_INCLUDE_DIRS BU_LOCAL_INCLUDE_DIRS)

set(LIBBU_SOURCES
  argv.c
  avs.c
  backtrace.c
  badmagic.c
  basename.c
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
  dirname.c
  dlfcn.c
  endian.c
  escape.c
  fchmod.c
  fgets.c
  file.c
  fnmatch.c
  fopen_uniq.c
  getcwd.c
  getopt.c
  globals.c
  hash.c
  hist.c
  hook.c
  htond.c
  htonf.c
  interrupt.c
  ispar.c
  kill.c
  lex.c
  linebuf.c
  list.c
  log.c
  magic.c
  malloc.c
  mappedfile.c
  mread.c
  observer.c
  parallel.c
  parse.c
  pool.c
  printb.c
  process.c
  progname.c
  ptbl.c
  quote.c
  rb_create.c
  rb_delete.c
  rb_diag.c
  rb_extreme.c
  rb_free.c
  rb_insert.c
  rb_order_stats.c
  rb_rotate.c
  rb_search.c
  rb_walk.c
  realpath.c
  semaphore.c
  simd.c
  sscanf.c
  str.c
  tcl.c
  temp.c
  timer.c
  units.c
  vers.c
  vfont.c
  vlb.c
  vls.c
  whereis.c
  which.c
  xdr.c
  )

BRLCAD_ADDLIB(libbu "${LIBBU_SOURCES}" "${CMAKE_THREAD_LIBS_INIT};${TCL_LIBRARY};${WINSOCK_LIB};${PSAPI_LIB};${M_LIBRARY}")
set_target_properties(libbu PROPERTIES VERSION 19.0.1 SOVERSION 19)
get_directory_property(BRLCAD_TCL_BUILD DIRECTORY ${BRLCAD_SOURCE_DIR}/src/other DEFINITION BRLCAD_TCL_BUILD)
if("${BRLCAD_TCL_BUILD}" STREQUAL "ON")
  add_dependencies(libbu tcl)
endif("${BRLCAD_TCL_BUILD}" STREQUAL "ON")

BRLCAD_ADDEXEC(test_basename test_basename.c libbu NO_INSTALL)
BRLCAD_ADDEXEC(test_dirname test_dirname.c libbu NO_INSTALL)
BRLCAD_ADDEXEC(test_escape test_escape.c libbu NO_INSTALL)
BRLCAD_ADDEXEC(test_progname test_progname.c libbu NO_INSTALL)
BRLCAD_ADDEXEC(test_quote test_quote.c libbu NO_INSTALL)
BRLCAD_ADDEXEC(test_sscanf test_sscanf.c libbu NO_INSTALL)
BRLCAD_ADDEXEC(test_timer test_timer.c libbu NO_INSTALL)
BRLCAD_ADDEXEC(test_vls test_vls.c libbu NO_INSTALL)

set(bu_MAN3
  htond.3
  redblack.3
  )
ADD_MAN_PAGES(3 bu_MAN3)

set(bu_ignore_files
  rb_internals.h
  uce-dirent.h
  )
CMAKEFILES(${bu_ignore_files})
CMAKEFILES(Makefile.am)
