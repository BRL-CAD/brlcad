if(HAVE_TERMLIB)
  set(FBED_INCLUDE_DIRS
    ${FB_INCLUDE_DIRS}
    ${VFONT_INCLUDE_DIRS}
    ${TERMIO_INCLUDE_DIRS}
    ${CURSOR_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    )
  LIST(REMOVE_DUPLICATES FBED_INCLUDE_DIRS)
  include_directories(${FBED_INCLUDE_DIRS})

  set(fbed_SOURCES
    char.c
    empty.c
    execshell.c
    fbed.c
    fill_buf.c
    getinput.c
    glob.c
    pos_pad.c
    prnt.c
    squash.c
    try.c
    )

  BRLCAD_ADDEXEC(fbed "${fbed_SOURCES}" "libfb;libvfont;libtermio;libcursor;${TERMLIB_LIBRARY}")

  ADD_MAN_PAGES(1 fbed.1)
  CMAKEFILES(ascii.h cursorbits.h extern.h fb_ik.h popup.h std.h try.h)
endif(HAVE_TERMLIB)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
