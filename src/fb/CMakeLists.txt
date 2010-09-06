include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_STD_INCLUDES}
  ${BRLCAD_PNG_INCLUDE_DIR}
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
  ${UTAHRLE_INCLUDE_DIR}
)

add_definitions(
  ${BRLCAD_STD_DEFS}
)


BRLCAD_ADDEXEC(bw-fb bw-fb.c fb)
BRLCAD_ADDEXEC(cat-fb cat-fb.c fb)
BRLCAD_ADDEXEC(cell-fb cell-fb.c "bn fb")
BRLCAD_ADDEXEC(cmap-fb cmap-fb.c fb)
BRLCAD_ADDEXEC(fb-bw fb-bw.c fb)
BRLCAD_ADDEXEC(fb-cmap fb-cmap.c fb)
BRLCAD_ADDEXEC(fb-fb fb-fb.c fb)
BRLCAD_ADDEXEC(fb-orle "fb-orle.c cmap-crunch.c" "fb orle")
BRLCAD_ADDEXEC(fb-pix "fb-pix.c cmap-crunch.c" fb)
BRLCAD_ADDEXEC(fb-png "fb-png.c cmap-crunch.c" fb)
BRLCAD_ADDEXEC(fb-rle "fb-rle.c cmap-crunch.c" "fb ${UTAHRLE_LIBRARY}")
BRLCAD_ADDEXEC(fbanim fbanim.c fb)
BRLCAD_ADDEXEC(fbcbars fbcbars.c fb)
BRLCAD_ADDEXEC(fbclear fbclear.c fb)
BRLCAD_ADDEXEC(fbcmap fbcmap.c fb)
BRLCAD_ADDEXEC(fbcmrot fbcmrot.c fb)
BRLCAD_ADDEXEC(fbcolor fbcolor.c "fb termio")
BRLCAD_ADDEXEC(fbfade "fbfade.c ioutil.c" fb)
BRLCAD_ADDEXEC(fbframe fbframe.c fb)
BRLCAD_ADDEXEC(fbfree fbfree.c fb)
BRLCAD_ADDEXEC(fbgamma fbgamma.c fb)
BRLCAD_ADDEXEC(fbgammamod fbgammamod.c fb)
BRLCAD_ADDEXEC(fbgrid fbgrid.c fb)
BRLCAD_ADDEXEC(fbhelp fbhelp.c fb)
BRLCAD_ADDEXEC(fblabel fblabel.c fb)
BRLCAD_ADDEXEC(fbline fbline.c fb)
BRLCAD_ADDEXEC(fbpoint fbpoint.c "bu fb termio pkg")
BRLCAD_ADDEXEC(fbscanplot fbscanplot.c fb)
BRLCAD_ADDEXEC(fbstretch "fbstretch.c ioutil.c" fb)
BRLCAD_ADDEXEC(fbthreadtest fbthreadtest "fb ${TK_LIBRARY}")
SET_TARGET_PROPERTIES(fbthreadtest PROPERTIES COMPILE_DEFINITIONS "IF_TK")
BRLCAD_ADDEXEC(fbzoom fbzoom.c "fb termio")
BRLCAD_ADDEXEC(gif-fb "gif-fb.c ioutil.c" fb)
BRLCAD_ADDEXEC(gif2fb gif2fb.c fb)
BRLCAD_ADDEXEC(orle-fb orle-fb.c "fb orle")
BRLCAD_ADDEXEC(pix-fb pix-fb.c fb)
BRLCAD_ADDEXEC(pixautosize pixautosize.c fb)
BRLCAD_ADDEXEC(pixflip-fb pixflip-fb.c fb)
BRLCAD_ADDEXEC(pl-fb pl-fb.c fb)
BRLCAD_ADDEXEC(png-fb png-fb.c fb)
BRLCAD_ADDEXEC(polar-fb polar-fb.c fb)
BRLCAD_ADDEXEC(pp-fb pp-fb.c "fb termio")
BRLCAD_ADDEXEC(rle-fb rle-fb.c "fb ${UTAHRLE_LIBRARY}")
BRLCAD_ADDEXEC(spm-fb spm-fb.c "bn fb")

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
install(FILES ${fb_MANS} DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man1)
