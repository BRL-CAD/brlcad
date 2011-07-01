include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${ZLIB_INCLUDE_DIR}
  ${PNG_INCLUDE_DIR}
  ${TCL_INCLUDE_DIRS}
  ${TK_INCLUDE_PATH}
  ${UTAHRLE_INCLUDE_DIR}
  ${X11_INCLUDE_DIR}
)
IF(MSVC)
		  add_definitions(
					 -DIF_WGL
					 )
ENDIF(MSVC)
BRLCAD_ADDEXEC(bw-fb bw-fb.c libfb)
BRLCAD_ADDEXEC(cell-fb cell-fb.c "libfb libbn")
BRLCAD_ADDEXEC(cmap-fb cmap-fb.c libfb)
BRLCAD_ADDEXEC(fb-bw fb-bw.c libfb)
BRLCAD_ADDEXEC(fb-cmap fb-cmap.c libfb)
BRLCAD_ADDEXEC(fb-fb fb-fb.c libfb)
BRLCAD_ADDEXEC(fb-orle "fb-orle.c cmap-crunch.c" "libfb liborle")
BRLCAD_ADDEXEC(fb-pix "fb-pix.c cmap-crunch.c" libfb)
BRLCAD_ADDEXEC(fb-png "fb-png.c cmap-crunch.c" libfb)
BRLCAD_ADDEXEC(fb-rle "fb-rle.c cmap-crunch.c" "libfb ${UTAHRLE_LIBRARY}")
BRLCAD_ADDEXEC(fbanim fbanim.c libfb)
BRLCAD_ADDEXEC(fbcbars fbcbars.c libfb)
BRLCAD_ADDEXEC(fbclear fbclear.c libfb)
BRLCAD_ADDEXEC(fbcmap fbcmap.c libfb)
BRLCAD_ADDEXEC(fbcmrot fbcmrot.c libfb)

IF(HAVE_TERMLIB)
	BRLCAD_ADDEXEC(fbcolor fbcolor.c "libfb libtermio")
ENDIF(HAVE_TERMLIB)
BRLCAD_ADDEXEC(fbfade "fbfade.c ioutil.c" libfb)
BRLCAD_ADDEXEC(fbframe fbframe.c libfb)
BRLCAD_ADDEXEC(fbfree fbfree.c libfb)
BRLCAD_ADDEXEC(fbgamma fbgamma.c libfb)
BRLCAD_ADDEXEC(fbgammamod fbgammamod.c libfb)
BRLCAD_ADDEXEC(fbgrid fbgrid.c libfb)
BRLCAD_ADDEXEC(fbhelp fbhelp.c libfb)
BRLCAD_ADDEXEC(fblabel fblabel.c libfb)
BRLCAD_ADDEXEC(fbline fbline.c libfb)
IF(HAVE_TERMLIB)
	BRLCAD_ADDEXEC(fbpoint fbpoint.c "libfb libbu libtermio libpkg")
ENDIF(HAVE_TERMLIB)
BRLCAD_ADDEXEC(fbscanplot fbscanplot.c libfb)
BRLCAD_ADDEXEC(fbstretch "fbstretch.c ioutil.c" libfb)
IF(BRLCAD-ENABLE_TK)
	BRLCAD_ADDEXEC(fbthreadtest fbthreadtest "libfb ${TCL_TK_LIBRARY}")
	SET_TARGET_PROPERTIES(fbthreadtest PROPERTIES COMPILE_DEFINITIONS "IF_TK")
	SET_TARGET_PROPERTIES(fbthreadtest PROPERTIES LINKER_LANGUAGE C)
ENDIF(BRLCAD-ENABLE_TK)
IF(HAVE_TERMLIB)
	BRLCAD_ADDEXEC(fbzoom fbzoom.c "libfb libtermio")
ENDIF(HAVE_TERMLIB)
BRLCAD_ADDEXEC(gif-fb "gif-fb.c ioutil.c" libfb)
BRLCAD_ADDEXEC(gif2fb gif2fb.c libfb)
BRLCAD_ADDEXEC(orle-fb orle-fb.c "libfb liborle")
BRLCAD_ADDEXEC(pix-fb pix-fb.c libfb)
BRLCAD_ADDEXEC(pixautosize pixautosize.c libfb)
BRLCAD_ADDEXEC(pixflip-fb pixflip-fb.c libfb)
BRLCAD_ADDEXEC(pl-fb pl-fb.c libfb)
BRLCAD_ADDEXEC(png-fb png-fb.c libfb)
BRLCAD_ADDEXEC(polar-fb polar-fb.c libfb)
IF(HAVE_TERMLIB)
	BRLCAD_ADDEXEC(pp-fb pp-fb.c "libfb libtermio")
ENDIF(HAVE_TERMLIB)
BRLCAD_ADDEXEC(rle-fb rle-fb.c "libfb ${UTAHRLE_LIBRARY}")
BRLCAD_ADDEXEC(spm-fb spm-fb.c "libfb libbn")

SET(fb_MANS
  bw-fb.1 
  cell-fb.1 
  cmap-fb.1 
  fb-bw.1 
  fb-cmap.1 
  fb-fb.1 
  fb-orle.1 
  fb-pix.1 
  fb-png.1 
  fb-rle.1 
  fbanim.1 
  fbcbars.1 
  fbclear.1 
  fbcmap.1 
  fbcmrot.1 
  fbcolor.1 
  fbfade.1 
  fbframe.1 
  fbfree.1 
  fbgamma.1 
  fbgrid.1 
  fbhelp.1 
  fblabel.1 
  fbline.1 
  fbpoint.1 
  fbscanplot.1 
  fbstretch.1 
  fbzoom.1 
  gif-fb.1 
  gif2fb.1 
  orle-fb.1 
  pix-fb.1 
  pixautosize.1 
  pl-fb.1 
  png-fb.1 
  polar-fb.1 
  pp-fb.1 
  rle-fb.1
)
ADD_MAN_PAGES(1 fb_MANS)
CMAKEFILES(fbthreadtest.c)
