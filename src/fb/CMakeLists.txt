set(FBBIN_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${ORLE_INCLUDE_DIRS}
  ${TERMINFO_INCLUDE_DIRS}
  ${UTAHRLE_INCLUDE_DIR}
  ${PNG_INCLUDE_DIR}
  ${ZLIB_INCLUDE_DIR}
  )
BRLCAD_SORT_INCLUDE_DIRS(FBBIN_INCLUDE_DIRS)
include_directories(${FBBIN_INCLUDE_DIRS})

if(MSVC)
  add_definitions(-DIF_WGL)
endif(MSVC)
BRLCAD_ADDEXEC(bw-fb bw-fb.c libfb)
BRLCAD_ADDEXEC(cell-fb cell-fb.c "libfb;libbn")
BRLCAD_ADDEXEC(cmap-fb cmap-fb.c libfb)
BRLCAD_ADDEXEC(fb-bw fb-bw.c libfb)
BRLCAD_ADDEXEC(fb-cmap fb-cmap.c libfb)
BRLCAD_ADDEXEC(fb-fb fb-fb.c libfb)
BRLCAD_ADDEXEC(fb-pix "fb-pix.c;cmap-crunch.c" libfb)
BRLCAD_ADDEXEC(fb-png "fb-png.c;cmap-crunch.c" "libfb;${PNG_LIBRARY}")
BRLCAD_ADDEXEC(fb-rle "fb-rle.c;cmap-crunch.c" "libfb;${UTAHRLE_LIBRARY}")
BRLCAD_ADDEXEC(fbanim fbanim.c libfb)
BRLCAD_ADDEXEC(fbcbars fbcbars.c libfb)
BRLCAD_ADDEXEC(fbclear fbclear.c libfb)
BRLCAD_ADDEXEC(fbcmap fbcmap.c libfb)
BRLCAD_ADDEXEC(fbcmrot fbcmrot.c libfb)

if(HAVE_TERMLIB)
  BRLCAD_ADDEXEC(fbcolor fbcolor.c "libfb;libtermio")
endif(HAVE_TERMLIB)
BRLCAD_ADDEXEC(fbfade "fbfade.c;ioutil.c" libfb)
BRLCAD_ADDEXEC(fbframe fbframe.c libfb)
BRLCAD_ADDEXEC(fbfree fbfree.c libfb)
BRLCAD_ADDEXEC(fbgamma fbgamma.c "libfb;${M_LIBRARY}")
BRLCAD_ADDEXEC(fbgammamod fbgammamod.c "libfb;${M_LIBRARY}")
BRLCAD_ADDEXEC(fbgrid fbgrid.c libfb)
BRLCAD_ADDEXEC(fbhelp fbhelp.c libfb)
BRLCAD_ADDEXEC(fblabel fblabel.c libfb)
BRLCAD_ADDEXEC(fbline fbline.c libfb)
if(HAVE_TERMLIB)
  BRLCAD_ADDEXEC(fbpoint fbpoint.c "libfb;libbu;libtermio;libpkg")
endif(HAVE_TERMLIB)
BRLCAD_ADDEXEC(fbscanplot fbscanplot.c libfb)
BRLCAD_ADDEXEC(fbstretch "fbstretch.c;ioutil.c" libfb)
if(HAVE_TERMLIB)
  BRLCAD_ADDEXEC(fbzoom fbzoom.c "libfb;libtermio")
endif(HAVE_TERMLIB)
BRLCAD_ADDEXEC(gif-fb "gif-fb.c;ioutil.c" libfb)
BRLCAD_ADDEXEC(gif2fb gif2fb.c libfb)
BRLCAD_ADDEXEC(pix-fb pix-fb.c libfb)
BRLCAD_ADDEXEC(pixautosize pixautosize.c libfb)
BRLCAD_ADDEXEC(pixflip-fb pixflip-fb.c libfb)
BRLCAD_ADDEXEC(plot3-fb plot3-fb.c libfb)
BRLCAD_ADDEXEC(png-fb png-fb.c "libfb;${PNG_LIBRARY}")
BRLCAD_ADDEXEC(polar-fb polar-fb.c "libfb;${M_LIBRARY}")
if(HAVE_TERMLIB)
  BRLCAD_ADDEXEC(pp-fb pp-fb.c "libfb;libtermio")
endif(HAVE_TERMLIB)
BRLCAD_ADDEXEC(rle-fb rle-fb.c "libfb;${UTAHRLE_LIBRARY}")
if(CPP_DLL_DEFINES)
  SET_PROPERTY(TARGET rle-fb APPEND PROPERTY COMPILE_DEFINITIONS "RLE_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)

BRLCAD_ADDEXEC(spm-fb spm-fb.c "libfb;libbn")

add_subdirectory(tests)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
