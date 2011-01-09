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
    endian.c
    fchmod.c
    fgets.c
    fnmatch.c
    fopen_uniq.c
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
    mro.c
    observer.c
    parallel.c
    parse.c
    printb.c
    process.c
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
    SET(WIN32LIBS ws2_32.lib)
    add_definitions(
	    -DBU_EXPORT_DLL
	 )    
ENDIF(MSVC)

BRLCAD_ADDLIB(libbu "${LIBBU_SOURCES}" "${CMAKE_THREAD_LIBS_INIT} ${PNG_LIBRARY} ${TCL_LIBRARY} ${WIN32LIBS}" STRICT)
SET_TARGET_PROPERTIES(libbu PROPERTIES VERSION 19.0.1 SOVERSION 19)
IF(BRLCAD_BUILD_LOCAL_TCL)
	ADD_DEPENDENCIES(libbu tcl)
ENDIF(BRLCAD_BUILD_LOCAL_TCL)

add_executable(htester htester.c)
target_link_libraries(htester ${CMAKE_THREAD_LIBS_INIT} ${PNG_LIBRARY} libbu)

set(bu_MAN3
  htond.3
  libbu.3
  redblack.3
)
install(FILES ${bu_MAN3} DESTINATION ${MAN_DIR}/man3)
