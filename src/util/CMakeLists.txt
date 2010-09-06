include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BRLCAD_PNG_INCLUDE_DIR}
  ${BRLCAD_UTAHRLE_INCLUDE_DIR}
  ${X11_INCLUDE_DIR}
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
)

add_definitions(
  ${BRLCAD_STD_DEFS}
)

IF(BRLCAD-ENABLE_X11)
  BRLCAD_ADDEXEC(pl-X pl-X.c "bu dm")
  IF(TK_LIBRARY)
    BRLCAD_ADDEXEC(pl-dm pl-dm.c "bu dm")
    BRLCAD_ADDEXEC(bombardier bombardier.c "bu tclcad ${TCL_LIBRARY} ${TK_LIBRARY}")
  ENDIF(TK_LIBRARY)
ENDIF(BRLCAD-ENABLE_X11)

# Don't have the libpc or boost stuff yet
# BRLCAD_ADDEXEC(pc_test pc_test.c "wdb pc")


BRLCAD_ADDEXEC(alias-pix alias-pix.c bu)
BRLCAD_ADDEXEC(ap-pix ap-pix.c bu)
BRLCAD_ADDEXEC(asc-pl asc-pl.c "bu bn")
BRLCAD_ADDEXEC(azel azel.c bu)
BRLCAD_ADDEXEC(bary bary.c bu)
BRLCAD_ADDEXEC(buffer buffer.c bu)
BRLCAD_ADDEXEC(bw-a bw-a.c "bu fb")
BRLCAD_ADDEXEC(bw-imp bw-imp.c bu)
BRLCAD_ADDEXEC(bw-pix bw-pix.c bu)
BRLCAD_ADDEXEC(bw-png bw-png.c "bu fb ${BRLCAD_PNG_LIBRARY}")
BRLCAD_ADDEXEC(bw-ps bw-ps.c bu)
BRLCAD_ADDEXEC(bw-rle bw-rle.c "bu ${UTAHRLE_LIBRARY} sysv")
BRLCAD_ADDEXEC(bw3-pix bw3-pix.c bu)
BRLCAD_ADDEXEC(bwcrop bwcrop.c bu)
BRLCAD_ADDEXEC(bwdiff bwdiff.c bu)
BRLCAD_ADDEXEC(bwfilter bwfilter.c bu)
BRLCAD_ADDEXEC(bwhist bwhist.c fb)
BRLCAD_ADDEXEC(bwhisteq bwhisteq.c bu)
BRLCAD_ADDEXEC(bwmod bwmod.c bu)
BRLCAD_ADDEXEC(bwrect bwrect.c bu)
BRLCAD_ADDEXEC(bwrot bwrot.c bu)
BRLCAD_ADDEXEC(bwscale bwscale.c bu)
BRLCAD_ADDEXEC(bwshrink bwshrink.c bu)
BRLCAD_ADDEXEC(bwstat bwstat.c "bu ${M_LIBRARY}")
BRLCAD_ADDEXEC(bwthresh bwthresh.c bu)
BRLCAD_ADDEXEC(cv cv.c bu)
BRLCAD_ADDEXEC(dbcp dbcp.c bu)
BRLCAD_ADDEXEC(decimate decimate.c bu)
BRLCAD_ADDEXEC(double-asc double-asc.c "bu fb")
BRLCAD_ADDEXEC(dpix-pix dpix-pix.c bu)
BRLCAD_ADDEXEC(dsp_add dsp_add.c bu)
BRLCAD_ADDEXEC(dunncolor "dunncolor.c dunncomm.c" bu)
BRLCAD_ADDEXEC(dunnsnap "dunnsnap.c dunncomm.c" "bu fb")
BRLCAD_ADDEXEC(fix_polysolids fix_polysolids.c "bu librt")
BRLCAD_ADDEXEC(gencolor gencolor.c bu)
BRLCAD_ADDEXEC(hex hex.c bu)
BRLCAD_ADDEXEC(imgdims imgdims.c "bu fb")
BRLCAD_ADDEXEC(loop loop.c bu)
BRLCAD_ADDEXEC(lowp lowp.c bu)
BRLCAD_ADDEXEC(mac-pix mac-pix.c bu)
BRLCAD_ADDEXEC(random random.c "bu bn")
BRLCAD_ADDEXEC(orle-pix orle-pix.c "bu orle fb")
BRLCAD_ADDEXEC(pix-alias pix-alias.c bu)
BRLCAD_ADDEXEC(pix-bw pix-bw.c bu)
BRLCAD_ADDEXEC(pix-bw3 pix-bw3.c bu)
BRLCAD_ADDEXEC(pix-orle pix-orle.c "bu orle fb")
BRLCAD_ADDEXEC(pix-png pix-png.c "bu fb")
BRLCAD_ADDEXEC(pix-ppm pix-ppm.c "bu fb")
BRLCAD_ADDEXEC(pix-ps pix-ps.c bu)
BRLCAD_ADDEXEC(pix-rle pix-rle.c "bu ${UTAHRLE_LIBRARY} sysv")
BRLCAD_ADDEXEC(pix-spm pix-spm.c bn)
BRLCAD_ADDEXEC(pix-sun pix-sun.c bu)
BRLCAD_ADDEXEC(pix-yuv pix-yuv.c "bu bn fb")
BRLCAD_ADDEXEC(pix3filter pix3filter.c bu)
BRLCAD_ADDEXEC(pixbackgnd pixbackgnd.c bu)
BRLCAD_ADDEXEC(pixbgstrip pixbgstrip.c "bu fb")
BRLCAD_ADDEXEC(pixblend pixblend.c bu)
BRLCAD_ADDEXEC(pixborder pixborder.c "bu bn fb")
BRLCAD_ADDEXEC(pixbustup pixbustup.c bu)
BRLCAD_ADDEXEC(pixclump pixclump.c "bu bn")
BRLCAD_ADDEXEC(pixcolors pixcolors.c bu)
BRLCAD_ADDEXEC(pixcount pixcount.c bu)
BRLCAD_ADDEXEC(pixcut pixcut.c "bu fb")
BRLCAD_ADDEXEC(pixdiff pixdiff.c bu)
BRLCAD_ADDEXEC(pixdsplit pixdsplit.c bu)
BRLCAD_ADDEXEC(pixelswap pixelswap.c bu)
BRLCAD_ADDEXEC(pixembed pixembed.c bu)
BRLCAD_ADDEXEC(pixfade pixfade.c "bu bn")
BRLCAD_ADDEXEC(pixfields pixfields.c bu)
BRLCAD_ADDEXEC(pixfieldsep pixfieldsep.c bu)
BRLCAD_ADDEXEC(pixfilter pixfilter.c bu)
BRLCAD_ADDEXEC(pixhalve pixhalve.c "bu fb")
BRLCAD_ADDEXEC(pixhist pixhist.c "bu fb")
BRLCAD_ADDEXEC(pixhist3d pixhist3d.c "bu fb")
BRLCAD_ADDEXEC(pixhist3d-pl pixhist3d-pl.c "bu bn")
BRLCAD_ADDEXEC(pixinterp2x pixinterp2x.c bu)
BRLCAD_ADDEXEC(pixmatte pixmatte.c bu)
BRLCAD_ADDEXEC(pixmerge pixmerge.c bu)
BRLCAD_ADDEXEC(pixmorph pixmorph.c "bu fb")
BRLCAD_ADDEXEC(pixpaste pixpaste.c "bu fb")
BRLCAD_ADDEXEC(pixrect pixrect.c bu)
BRLCAD_ADDEXEC(pixrot pixrot.c bu) 
BRLCAD_ADDEXEC(pixsaturate pixsaturate.c bu)
BRLCAD_ADDEXEC(pixscale pixscale.c bu)
BRLCAD_ADDEXEC(pixshrink pixshrink.c bu)
BRLCAD_ADDEXEC(pixstat pixstat.c bu)
BRLCAD_ADDEXEC(pixsubst pixsubst.c bu)
BRLCAD_ADDEXEC(pixtile pixtile.c bu)
BRLCAD_ADDEXEC(pixuntile pixuntile.c bu)
BRLCAD_ADDEXEC(pl-asc pl-asc.c bu)
BRLCAD_ADDEXEC(pl-hpgl pl-hpgl.c bu)
BRLCAD_ADDEXEC(pl-pl pl-pl.c bu)
BRLCAD_ADDEXEC(pl-ps pl-ps.c bu)
BRLCAD_ADDEXEC(pl-sgi pl-sgi.c bu)
BRLCAD_ADDEXEC(pl-tek pl-tek.c bu)
BRLCAD_ADDEXEC(plcolor plcolor.c bn)
BRLCAD_ADDEXEC(pldebug pldebug bu)
BRLCAD_ADDEXEC(plgetframe plgetframe.c bu)
BRLCAD_ADDEXEC(plline2 plline2.c "bu bn")
BRLCAD_ADDEXEC(plrot plrot.c "bu bn")
BRLCAD_ADDEXEC(plstat plstat.c bu)
BRLCAD_ADDEXEC(png-bw png-bw.c "bu bn")
BRLCAD_ADDEXEC(png-pix png-pix.c "bu bn")
BRLCAD_ADDEXEC(png_info png_info.c "bu bn")
BRLCAD_ADDEXEC(query query.c bu)
BRLCAD_ADDEXEC(rle-pix rle-pix.c "bu ${UTAHRLE_LIBRARY} sysv")
BRLCAD_ADDEXEC(sun-pix sun-pix.c bu)
BRLCAD_ADDEXEC(terrain_util terrain.c "bu bn")
BRLCAD_ADDEXEC(texturescale texturescale.c "bu bn fb")
BRLCAD_ADDEXEC(ttcp ttcp.c bu)
BRLCAD_ADDEXEC(wavelet wavelet.c "bu bn fb")
BRLCAD_ADDEXEC(xyz-pl xyz-pl.c "bu bn")
BRLCAD_ADDEXEC(yuv-pix yuv-pix.c "bu bn fb")

add_executable(roots_example roots_example.c)
target_link_libraries(roots_example bu librt)

set(util_MANS
  ap-pix.1 
  asc-pl.1 
  azel.1 
  bary.1 
  brlcad.1 
  buffer.1 
  bw-imp.1 
  bw-pix.1 
  bw-png.1 
  bw-ps.1 
  bw-rle.1 
  bw3-pix.1 
  bwcrop.1 
  bwdiff.1 
  bwfilter.1 
  bwhist.1 
  bwhisteq.1 
  bwmod.1 
  bwrect.1 
  bwrot.1 
  bwshrink.1 
  bwstat.1 
  bwthresh.1 
  cv.1 
  dbcp.1 
  decimate.1 
  dpix-pix.1 
  dunncolor.1 
  dunnsnap.1 
  gencolor.1 
  hex.1 
  imgdims.1 
  loop.1 
  mac-pix.1 
  morphedit.1 
  orle-pix.1 
  pix-alias.1 
  pix-bw.1 
  pix-bw3.1 
  pix-orle.1 
  pix-png.1 
  pix-ppm.1 
  pix-ps.1 
  pix-rle.1 
  pix-sun.1 
  pixbackgnd.1 
  pixbgstrip.1 
  pixblend.1 
  pixborder.1 
  pixbustup.1 
  pixclump.1 
  pixcolors.1 
  pixcut.1 
  pixdiff.1 
  pixdsplit.1 
  pixfade.1 
  pixfilter.1 
  pixhalve.1 
  pixhist.1 
  pixhist3d-pl.1 
  pixhist3d.1 
  pixinterp2x.1 
  pixmatte.1 
  pixmerge.1 
  pixmorph.1 
  pixrect.1 
  pixrot.1 
  pixsaturate.1 
  pixscale.1 
  pixshrink.1 
  pixstat.1 
  pixsubst.1 
  pixtile.1 
  pixuntile.1 
  pl-X.1 
  pl-asc.1 
  pl-hpgl.1 
  pl-pl.1 
  pl-ps.1 
  pl-sgi.1 
  pl-tek.1 
  plcolor.1 
  pldebug.1 
  plgetframe.1 
  plline2.1 
  plrot.1 
  png-bw.1 
  png-pix.1 
  png_info.1 
  query.1 
  random.1 
  rle-pix.1 
  sun-pix.1 
  terrain.1 
  wavelet.1
)
install(FILES ${util_MANS} DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man1)
