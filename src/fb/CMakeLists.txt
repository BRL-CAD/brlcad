set(FBBIN_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  ${DM_INCLUDE_DIRS}
  ${UTAHRLE_INCLUDE_DIR}
  ${PNG_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(FBBIN_INCLUDE_DIRS)

if(MSVC)
  add_definitions(-DIF_WGL)
endif(MSVC)
BRLCAD_ADDEXEC(bw-fb bw-fb.c libdm)
BRLCAD_ADDEXEC(fb-bw fb-bw.c libdm)
BRLCAD_ADDEXEC(fb-fb fb-fb.c libdm)
BRLCAD_ADDEXEC(fb-pix "fb-pix.c;cmap-crunch.c" libdm)
BRLCAD_ADDEXEC(fb-png "fb-png.c;cmap-crunch.c" "libdm;${PNG_LIBRARIES}")
BRLCAD_ADDEXEC(fbcbars fbcbars.c libdm)
BRLCAD_ADDEXEC(fbclear fbclear.c libdm)
BRLCAD_ADDEXEC(fbcmap fbcmap.c libdm)

if(TARGET libtermio)
  BRLCAD_ADDEXEC(fbcolor fbcolor.c "libdm;libtermio")
endif(TARGET libtermio)
BRLCAD_ADDEXEC(fbfade "fbfade.c;ioutil.c" libdm)
BRLCAD_ADDEXEC(fbframe fbframe.c libdm)
BRLCAD_ADDEXEC(fbfree fbfree.c libdm)
BRLCAD_ADDEXEC(fbgamma fbgamma.c "libdm;${M_LIBRARY}")
BRLCAD_ADDEXEC(fbgrid fbgrid.c libdm)
BRLCAD_ADDEXEC(fbhelp fbhelp.c libdm)
BRLCAD_ADDEXEC(fblabel fblabel.c libdm)
BRLCAD_ADDEXEC(fbline fbline.c libdm)
if(TARGET libtermio)
  BRLCAD_ADDEXEC(fbpoint fbpoint.c "libdm;libbu;libtermio;libpkg")
endif(TARGET libtermio)
BRLCAD_ADDEXEC(fbstretch "fbstretch.c;ioutil.c" libdm)
if(TARGET libtermio)
  BRLCAD_ADDEXEC(fbzoom fbzoom.c "libdm;libtermio")
endif(TARGET libtermio)
BRLCAD_ADDEXEC(gif-fb "gif-fb.c;ioutil.c" libdm)
BRLCAD_ADDEXEC(gif2fb gif2fb.c libdm)
BRLCAD_ADDEXEC(pix-fb pix-fb.c libdm)
BRLCAD_ADDEXEC(pixautosize pixautosize.c libdm)
BRLCAD_ADDEXEC(plot3-fb plot3-fb.c libdm)
BRLCAD_ADDEXEC(png-fb png-fb.c "libdm;${PNG_LIBRARIES}")

BRLCAD_ADDEXEC(spm-fb spm-fb.c "libdm;libbn")

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
