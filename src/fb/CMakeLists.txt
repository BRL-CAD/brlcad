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
BRLCAD_ADDEXEC(bw-fb bw-fb.c libfb STRICT)
BRLCAD_ADDEXEC(cat-fb cat-fb.c libfb STRICT)
BRLCAD_ADDEXEC(cell-fb cell-fb.c "libfb libbn" STRICT)
BRLCAD_ADDEXEC(cmap-fb cmap-fb.c libfb STRICT)
BRLCAD_ADDEXEC(fb-bw fb-bw.c libfb STRICT)
BRLCAD_ADDEXEC(fb-cmap fb-cmap.c libfb STRICT)
BRLCAD_ADDEXEC(fb-fb fb-fb.c libfb STRICT)
BRLCAD_ADDEXEC(fb-orle "fb-orle.c cmap-crunch.c" "libfb liborle" STRICT)
BRLCAD_ADDEXEC(fb-pix "fb-pix.c cmap-crunch.c" libfb STRICT)
BRLCAD_ADDEXEC(fb-png "fb-png.c cmap-crunch.c" libfb STRICT)
BRLCAD_ADDEXEC(fb-rle "fb-rle.c cmap-crunch.c" "libfb ${UTAHRLE_LIBRARY}" STRICT)
BRLCAD_ADDEXEC(fbanim fbanim.c libfb STRICT)
BRLCAD_ADDEXEC(fbcbars fbcbars.c libfb STRICT)
BRLCAD_ADDEXEC(fbclear fbclear.c libfb STRICT)
BRLCAD_ADDEXEC(fbcmap fbcmap.c libfb STRICT)
BRLCAD_ADDEXEC(fbcmrot fbcmrot.c libfb STRICT)

IF(HAVE_TERMLIB)
	BRLCAD_ADDEXEC(fbcolor fbcolor.c "libfb libtermio" STRICT)
ENDIF(HAVE_TERMLIB)
BRLCAD_ADDEXEC(fbfade "fbfade.c ioutil.c" libfb STRICT)
BRLCAD_ADDEXEC(fbframe fbframe.c libfb STRICT)
BRLCAD_ADDEXEC(fbfree fbfree.c libfb STRICT)
BRLCAD_ADDEXEC(fbgamma fbgamma.c libfb STRICT)
BRLCAD_ADDEXEC(fbgammamod fbgammamod.c libfb STRICT)
BRLCAD_ADDEXEC(fbgrid fbgrid.c libfb STRICT)
BRLCAD_ADDEXEC(fbhelp fbhelp.c libfb STRICT)
BRLCAD_ADDEXEC(fblabel fblabel.c libfb STRICT)
BRLCAD_ADDEXEC(fbline fbline.c libfb STRICT)
IF(HAVE_TERMLIB)
	BRLCAD_ADDEXEC(fbpoint fbpoint.c "libfb libbu libtermio libpkg" STRICT)
ENDIF(HAVE_TERMLIB)
BRLCAD_ADDEXEC(fbscanplot fbscanplot.c libfb STRICT)
BRLCAD_ADDEXEC(fbstretch "fbstretch.c ioutil.c" libfb STRICT)
IF(BRLCAD-ENABLE_TK)
	BRLCAD_ADDEXEC(fbthreadtest fbthreadtest "libfb ${TCL_TK_LIBRARY}" STRICT)
	SET_TARGET_PROPERTIES(fbthreadtest PROPERTIES COMPILE_DEFINITIONS "IF_TK")
ENDIF(BRLCAD-ENABLE_TK)
IF(HAVE_TERMLIB)
	BRLCAD_ADDEXEC(fbzoom fbzoom.c "libfb libtermio" STRICT)
ENDIF(HAVE_TERMLIB)
BRLCAD_ADDEXEC(gif-fb "gif-fb.c ioutil.c" libfb STRICT)
BRLCAD_ADDEXEC(gif2fb gif2fb.c libfb STRICT)
BRLCAD_ADDEXEC(orle-fb orle-fb.c "libfb liborle" STRICT)
BRLCAD_ADDEXEC(pix-fb pix-fb.c libfb STRICT)
BRLCAD_ADDEXEC(pixautosize pixautosize.c libfb STRICT)
BRLCAD_ADDEXEC(pixflip-fb pixflip-fb.c libfb STRICT)
BRLCAD_ADDEXEC(pl-fb pl-fb.c libfb STRICT)
BRLCAD_ADDEXEC(png-fb png-fb.c libfb STRICT)
BRLCAD_ADDEXEC(polar-fb polar-fb.c libfb STRICT)
IF(HAVE_TERMLIB)
	BRLCAD_ADDEXEC(pp-fb pp-fb.c "libfb libtermio" STRICT)
ENDIF(HAVE_TERMLIB)
BRLCAD_ADDEXEC(rle-fb rle-fb.c "libfb ${UTAHRLE_LIBRARY}" STRICT)
BRLCAD_ADDEXEC(spm-fb spm-fb.c "libfb libbn" STRICT)

SET(fb_MANS
  bw-fb.1 
  cat-fb.1 
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
