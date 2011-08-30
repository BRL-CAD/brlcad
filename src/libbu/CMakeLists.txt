include_directories(
	${CMAKE_SOURCE_DIR}/src/other/uce-dirent
	${PNG_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIR}
	${TCL_INCLUDE_DIRS}
)

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
    cmdhist_obj.c
    color.c
    convert.c
    crashreport.c
    dirent.c
    dirname.c
    dlfcn.c
    endian.c
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
    image.c
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
    semaphore.c
    simd.c
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

BRLCAD_ADDLIB(libbu "${LIBBU_SOURCES}" "${CMAKE_THREAD_LIBS_INIT} ${PNG_LIBRARY} ${TCL_LIBRARY} ${WINSOCK_LIB} ${PSAPI_LIB}")
SET_TARGET_PROPERTIES(libbu PROPERTIES VERSION 19.0.1 SOVERSION 19)
IF(BRLCAD_BUILD_LOCAL_TCL)
	ADD_DEPENDENCIES(libbu tcl)
ENDIF(BRLCAD_BUILD_LOCAL_TCL)

add_executable(test_htond test_htond.c)
target_link_libraries(test_htond ${CMAKE_THREAD_LIBS_INIT} ${PNG_LIBRARY} libbu)

add_executable(test_timer test_timer.c)
target_link_libraries(test_timer libbu)

add_executable(test_basename test_basename.c)
target_link_libraries(test_basename libbu)
IF(MSVC)
	SET_TARGET_PROPERTIES(test_basename PROPERTIES COMPILE_FLAGS "-DBRLCAD_DLL")
ENDIF(MSVC)

add_executable(test_progname test_progname.c)
target_link_libraries(test_progname libbu)

add_executable(test_vls test_vls.c)
target_link_libraries(test_vls libbu)

set(bu_MAN3
  htond.3
  libbu.3
  redblack.3
)
ADD_MAN_PAGES(3 bu_MAN3)

set(bu_ignore_files
	rb_internals.h
	)
CMAKEFILES(${bu_ignore_files})
