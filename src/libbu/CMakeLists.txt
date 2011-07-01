include_directories(
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
    stat.c
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

IF(MSVC)
	 SET(WIN32LIBS ${WINSOCK_LIB})
ENDIF(MSVC)

BRLCAD_ADDLIB(libbu "${LIBBU_SOURCES}" "${CMAKE_THREAD_LIBS_INIT} ${PNG_LIBRARY} ${TCL_LIBRARY} ${WIN32LIBS}")
SET_TARGET_PROPERTIES(libbu PROPERTIES VERSION 19.0.1 SOVERSION 19)
IF(BRLCAD_BUILD_LOCAL_TCL)
	ADD_DEPENDENCIES(libbu tcl)
ENDIF(BRLCAD_BUILD_LOCAL_TCL)

add_executable(htester htester.c)
target_link_libraries(htester ${CMAKE_THREAD_LIBS_INIT} ${PNG_LIBRARY} libbu)

add_executable(timetester timetester.c)
target_link_libraries(timetester libbu)

add_executable(basenametester  basenametester.c)
target_link_libraries(basenametester libbu)
IF(MSVC)
	SET_TARGET_PROPERTIES(basenametester PROPERTIES COMPILE_FLAGS "-DBRLCAD_DLL")
ENDIF(MSVC)

add_executable(prognametester  prognametester.c)
target_link_libraries(prognametester libbu)

set(bu_MAN3
  htond.3
  libbu.3
  redblack.3
)
ADD_MAN_PAGES(3 bu_MAN3)

set(bu_ignore_files
	rb_internals.h
	uce-dirent.h
	)
CMAKEFILES(${bu_ignore_files})
