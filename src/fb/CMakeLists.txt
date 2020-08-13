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
ADD_TARGET_DEPS(bw-fb dm-ogl dm-wgl)
BRLCAD_ADDEXEC(fb-bw fb-bw.c libdm)
ADD_TARGET_DEPS(fb-bw dm-ogl dm-wgl)
BRLCAD_ADDEXEC(fb-fb fb-fb.c libdm)
ADD_TARGET_DEPS(fb-fb dm-ogl dm-wgl)
BRLCAD_ADDEXEC(fb-pix "fb-pix.c;cmap-crunch.c" libdm)
ADD_TARGET_DEPS(fb-pix dm-ogl dm-wgl)
BRLCAD_ADDEXEC(fb-png "fb-png.c;cmap-crunch.c" "libdm;${PNG_LIBRARIES}")
ADD_TARGET_DEPS(fb-png dm-ogl dm-wgl)
BRLCAD_ADDEXEC(fbcbars fbcbars.c libdm)
ADD_TARGET_DEPS(fbcbars dm-ogl dm-wgl)
BRLCAD_ADDEXEC(fbclear fbclear.c libdm)
ADD_TARGET_DEPS(fbclear dm-ogl dm-wgl)
BRLCAD_ADDEXEC(fbcmap fbcmap.c libdm)
ADD_TARGET_DEPS(fbcmap dm-ogl dm-wgl)

if(TARGET libtermio)
  BRLCAD_ADDEXEC(fbcolor fbcolor.c "libdm;libtermio")
  ADD_TARGET_DEPS(fbcolor dm-ogl dm-wgl)
else(TARGET libtermio)
  CMAKEFILES(fbcolor.c)
endif(TARGET libtermio)
BRLCAD_ADDEXEC(fbfade "fbfade.c;ioutil.c" libdm)
ADD_TARGET_DEPS(fbfade dm-ogl dm-wgl)
BRLCAD_ADDEXEC(fbframe fbframe.c libdm)
ADD_TARGET_DEPS(fbframe dm-ogl dm-wgl)
BRLCAD_ADDEXEC(fbfree fbfree.c libdm)
ADD_TARGET_DEPS(fbfree dm-ogl dm-wgl)
BRLCAD_ADDEXEC(fbgamma fbgamma.c "libdm;${M_LIBRARY}")
ADD_TARGET_DEPS(fbgamma dm-ogl dm-wgl)
BRLCAD_ADDEXEC(fbgrid fbgrid.c libdm)
ADD_TARGET_DEPS(fbgrid dm-ogl dm-wgl)
BRLCAD_ADDEXEC(fbhelp fbhelp.c libdm)
ADD_TARGET_DEPS(fbhelp dm-ogl dm-wgl)
BRLCAD_ADDEXEC(fblabel fblabel.c libdm)
ADD_TARGET_DEPS(fblabel dm-ogl dm-wgl)
BRLCAD_ADDEXEC(fbline fbline.c libdm)
ADD_TARGET_DEPS(fbline dm-ogl dm-wgl)
if(TARGET libtermio)
  BRLCAD_ADDEXEC(fbpoint fbpoint.c "libdm;libbu;libtermio;libpkg")
  ADD_TARGET_DEPS(fbpoint dm-ogl dm-wgl)
else(TARGET libtermio)
  CMAKEFILES(fbpoint.c)
endif(TARGET libtermio)
BRLCAD_ADDEXEC(fbstretch "fbstretch.c;ioutil.c" libdm)
ADD_TARGET_DEPS(fbstretch dm-ogl dm-wgl)
if(TARGET libtermio)
  BRLCAD_ADDEXEC(fbzoom fbzoom.c "libdm;libtermio")
  ADD_TARGET_DEPS(fbzoom dm-ogl dm-wgl)
else(TARGET libtermio)
  CMAKEFILES(fbzoom.c)
endif(TARGET libtermio)
BRLCAD_ADDEXEC(gif-fb "gif-fb.c;ioutil.c" libdm)
ADD_TARGET_DEPS(gif-fb dm-ogl dm-wgl)
BRLCAD_ADDEXEC(gif2fb gif2fb.c libdm)
ADD_TARGET_DEPS(gif2fb dm-ogl dm-wgl)
BRLCAD_ADDEXEC(pix-fb pix-fb.c libdm)
ADD_TARGET_DEPS(pix-fb dm-ogl dm-wgl)
BRLCAD_ADDEXEC(pixautosize pixautosize.c libdm)
ADD_TARGET_DEPS(pixautosize dm-ogl dm-wgl)
BRLCAD_ADDEXEC(plot3-fb plot3-fb.c libdm)
ADD_TARGET_DEPS(plot3-fb dm-ogl dm-wgl)
BRLCAD_ADDEXEC(png-fb png-fb.c "libdm;${PNG_LIBRARIES}")
ADD_TARGET_DEPS(png-fb dm-ogl dm-wgl)

BRLCAD_ADDEXEC(spm-fb spm-fb.c "libdm;libbn")
ADD_TARGET_DEPS(spm-fb dm-ogl dm-wgl)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
