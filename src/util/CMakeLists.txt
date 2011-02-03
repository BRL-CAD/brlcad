include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${ZLIB_INCLUDE_DIR}
  ${PNG_INCLUDE_DIR}
  ${UTAHRLE_INCLUDE_DIR}
  ${X11_INCLUDE_DIR}
  ${TCL_INCLUDE_DIRS}
  ${TK_INCLUDE_PATH}
)

IF(MSVC)
	add_definitions(
	-D_CONSOLE
	-DIF_WGL
	)
ENDIF(MSVC)


IF(BRLCAD-ENABLE_X11)
	BRLCAD_ADDEXEC(pl-X pl-X.c "libdm libbu" STRICT)
	IF(BRLCAD-ENABLE_TK)
		BRLCAD_ADDEXEC(pl-dm pl-dm.c "libdm libbu" STRICT)
		BRLCAD_ADDEXEC(bombardier bombardier.c "libtclcad libbu" STRICT)
	ENDIF(BRLCAD-ENABLE_TK)
ENDIF(BRLCAD-ENABLE_X11)

BRLCAD_ADDFILE(pl-dm.c sample_applications)

IF(NOT MSVC)
	BRLCAD_ADDEXEC(pc_test pc_test.c "libwdb libpc")
ENDIF(NOT MSVC)

BRLCAD_ADDEXEC(alias-pix alias-pix.c libbu STRICT)
BRLCAD_ADDEXEC(ap-pix ap-pix.c libbu STRICT)
BRLCAD_ADDEXEC(asc-pl asc-pl.c "libbn libbu" STRICT)
BRLCAD_ADDEXEC(azel azel.c libbu STRICT)
BRLCAD_ADDEXEC(bary bary.c libbu STRICT)
BRLCAD_ADDEXEC(buffer buffer.c libbu STRICT)
BRLCAD_ADDEXEC(bw-a bw-a.c "libfb libbu" STRICT)
BRLCAD_ADDEXEC(bw-imp bw-imp.c libbu STRICT)
BRLCAD_ADDEXEC(bw-pix bw-pix.c libbu STRICT)
BRLCAD_ADDEXEC(bw-png bw-png.c "libfb libbu ${PNG_LIBRARY}" STRICT)
BRLCAD_ADDEXEC(bw-ps bw-ps.c libbu STRICT)
BRLCAD_ADDEXEC(bw-rle bw-rle.c "libbu libsysv ${UTAHRLE_LIBRARY}" STRICT)
BRLCAD_ADDEXEC(bw3-pix bw3-pix.c libbu STRICT)
BRLCAD_ADDEXEC(bwcrop bwcrop.c libbu STRICT)
BRLCAD_ADDEXEC(bwdiff bwdiff.c libbu STRICT)
BRLCAD_ADDEXEC(bwfilter bwfilter.c libbu STRICT)
BRLCAD_ADDEXEC(bwhist bwhist.c libfb STRICT)
BRLCAD_ADDEXEC(bwhisteq bwhisteq.c libbu STRICT)
BRLCAD_ADDEXEC(bwmod bwmod.c libbu STRICT)
BRLCAD_ADDEXEC(bwrect bwrect.c libbu STRICT)
BRLCAD_ADDEXEC(bwrot bwrot.c libbu STRICT)
BRLCAD_ADDEXEC(bwscale bwscale.c libbu STRICT)
BRLCAD_ADDEXEC(bwshrink bwshrink.c libbu STRICT)
BRLCAD_ADDEXEC(bwstat bwstat.c "libbu ${M_LIBRARY}" STRICT)
BRLCAD_ADDEXEC(bwthresh bwthresh.c libbu STRICT)
BRLCAD_ADDEXEC(cv cv.c libbu STRICT)
# FIXME dbcp is a "copy program for UNIX" - will need some porting
# work for Windows
IF(NOT WIN32)
	BRLCAD_ADDEXEC(dbcp dbcp.c libbu STRICT)
ENDIF(NOT WIN32)
BRLCAD_ADDEXEC(decimate decimate.c libbu STRICT)
BRLCAD_ADDEXEC(double-asc double-asc.c "libfb libbu" STRICT)
BRLCAD_ADDEXEC(dpix-pix dpix-pix.c libbu STRICT)
BRLCAD_ADDEXEC(dsp_add dsp_add.c libbu STRICT)
# Not fixing these for Windows - they're on their way out (deprecated)
IF(NOT WIN32)
	BRLCAD_ADDEXEC(dunncolor "dunncolor.c dunncomm.c" libbu STRICT)
	BRLCAD_ADDEXEC(dunnsnap "dunnsnap.c dunncomm.c" "libfb libbu" STRICT)
ENDIF(NOT WIN32)
BRLCAD_ADDEXEC(fix_polysolids fix_polysolids.c "librt libbu" STRICT)
BRLCAD_ADDEXEC(gencolor gencolor.c libbu STRICT)
BRLCAD_ADDEXEC(hex hex.c libbu STRICT)
BRLCAD_ADDEXEC(imgdims imgdims.c "libfb libbu" STRICT)
BRLCAD_ADDEXEC(loop loop.c libbu STRICT)
BRLCAD_ADDEXEC(lowp lowp.c libbu STRICT)
BRLCAD_ADDEXEC(mac-pix mac-pix.c libbu STRICT)
BRLCAD_ADDEXEC(random random.c "libbn libbu" STRICT)
BRLCAD_ADDEXEC(orle-pix orle-pix.c "libfb liborle libbu" STRICT)
BRLCAD_ADDEXEC(pix-alias pix-alias.c libbu STRICT)
BRLCAD_ADDEXEC(pix-bw pix-bw.c libbu STRICT)
BRLCAD_ADDEXEC(pix-bw3 pix-bw3.c libbu STRICT)
BRLCAD_ADDEXEC(pix-orle pix-orle.c "libfb liborle libbu" STRICT)
BRLCAD_ADDEXEC(pix-png pix-png.c "libfb libbu" STRICT)
BRLCAD_ADDEXEC(pix-ppm pix-ppm.c "libfb libbu" STRICT)
BRLCAD_ADDEXEC(pix-ps pix-ps.c libbu STRICT)
BRLCAD_ADDEXEC(pix-rle pix-rle.c "libbu libsysv ${UTAHRLE_LIBRARY}" STRICT)
BRLCAD_ADDEXEC(pix-spm pix-spm.c libbn STRICT)
BRLCAD_ADDEXEC(pix-sun pix-sun.c libbu STRICT)
BRLCAD_ADDEXEC(pix-yuv pix-yuv.c "libfb libbn libbu" STRICT)
BRLCAD_ADDEXEC(pix3filter pix3filter.c libbu STRICT)
BRLCAD_ADDEXEC(pixbackgnd pixbackgnd.c libbu STRICT)
BRLCAD_ADDEXEC(pixbgstrip pixbgstrip.c "libfb libbu" STRICT)
BRLCAD_ADDEXEC(pixblend pixblend.c libbu STRICT)
BRLCAD_ADDEXEC(pixborder pixborder.c "libfb libbn libbu" STRICT)
BRLCAD_ADDEXEC(pixbustup pixbustup.c libbu STRICT)
BRLCAD_ADDEXEC(pixclump pixclump.c "libbn libbu" STRICT)
BRLCAD_ADDEXEC(pixcolors pixcolors.c libbu STRICT)
BRLCAD_ADDEXEC(pixcount pixcount.c libbu STRICT)
BRLCAD_ADDEXEC(pixcut pixcut.c "libfb libbu" STRICT)
BRLCAD_ADDEXEC(pixdiff pixdiff.c libbu STRICT)
BRLCAD_ADDEXEC(pixdsplit pixdsplit.c libbu STRICT)
BRLCAD_ADDEXEC(pixelswap pixelswap.c libbu STRICT)
BRLCAD_ADDEXEC(pixembed pixembed.c libbu STRICT)
BRLCAD_ADDEXEC(pixfade pixfade.c "libbn libbu" STRICT)
BRLCAD_ADDEXEC(pixfields pixfields.c libbu STRICT)
BRLCAD_ADDEXEC(pixfieldsep pixfieldsep.c libbu STRICT)
BRLCAD_ADDEXEC(pixfilter pixfilter.c libbu STRICT)
BRLCAD_ADDEXEC(pixhalve pixhalve.c "libfb libbu" STRICT)
BRLCAD_ADDEXEC(pixhist pixhist.c "libfb libbu" STRICT)
BRLCAD_ADDEXEC(pixhist3d pixhist3d.c "libfb libbu" STRICT)
BRLCAD_ADDEXEC(pixhist3d-pl pixhist3d-pl.c "libbn libbu" STRICT)
BRLCAD_ADDEXEC(pixinterp2x pixinterp2x.c libbu STRICT)
BRLCAD_ADDEXEC(pixmatte pixmatte.c libbu STRICT)
BRLCAD_ADDEXEC(pixmerge pixmerge.c libbu STRICT)
BRLCAD_ADDEXEC(pixmorph pixmorph.c "libfb libbu" STRICT)
BRLCAD_ADDEXEC(pixpaste pixpaste.c "libfb libbu" STRICT)
BRLCAD_ADDEXEC(pixrect pixrect.c libbu STRICT)
BRLCAD_ADDEXEC(pixrot pixrot.c libbu STRICT) 
BRLCAD_ADDEXEC(pixsaturate pixsaturate.c libbu STRICT)
BRLCAD_ADDEXEC(pixscale pixscale.c libbu STRICT)
BRLCAD_ADDEXEC(pixshrink pixshrink.c libbu STRICT)
BRLCAD_ADDEXEC(pixstat pixstat.c libbu STRICT)
BRLCAD_ADDEXEC(pixsubst pixsubst.c libbu STRICT)
BRLCAD_ADDEXEC(pixtile pixtile.c libbu STRICT)
BRLCAD_ADDEXEC(pixuntile pixuntile.c libbu STRICT)
BRLCAD_ADDEXEC(pl-asc pl-asc.c libbu STRICT)
BRLCAD_ADDEXEC(pl-hpgl pl-hpgl.c libbu STRICT)
BRLCAD_ADDEXEC(pl-pl pl-pl.c libbu STRICT)
BRLCAD_ADDEXEC(pl-ps pl-ps.c libbu STRICT)
BRLCAD_ADDEXEC(pl-tek pl-tek.c libbu STRICT)
BRLCAD_ADDEXEC(plcolor plcolor.c libbn STRICT)
BRLCAD_ADDEXEC(pldebug pldebug libbu STRICT)
BRLCAD_ADDEXEC(plgetframe plgetframe.c libbu STRICT)
BRLCAD_ADDEXEC(plline2 plline2.c "libbn libbu" STRICT)
BRLCAD_ADDEXEC(plrot plrot.c "libbn libbu" STRICT)
BRLCAD_ADDEXEC(plstat plstat.c libbu STRICT)
BRLCAD_ADDEXEC(png-bw png-bw.c "libbn libbu" STRICT)
BRLCAD_ADDEXEC(png-pix png-pix.c "libbn libbu" STRICT)
BRLCAD_ADDEXEC(png_info png_info.c "libbn libbu" STRICT)
BRLCAD_ADDEXEC(rle-pix rle-pix.c "libbu libsysv ${UTAHRLE_LIBRARY}" STRICT)
BRLCAD_ADDEXEC(sun-pix sun-pix.c libbu STRICT)
BRLCAD_ADDEXEC(terrain terrain.c "libbn libbu" STRICT)
BRLCAD_ADDEXEC(texturescale texturescale.c "libfb libbn libbu" STRICT)
#FIXME - Need to look at pcattcp port to Windows - ttcp should be
#maintained and ported
IF(NOT WIN32)
   BRLCAD_ADDEXEC(ttcp ttcp.c libbu STRICT)
ENDIF(NOT WIN32)
BRLCAD_ADDEXEC(wavelet wavelet.c "libfb libbn libbu" STRICT)
BRLCAD_ADDEXEC(xyz-pl xyz-pl.c "libbn libbu" STRICT)
BRLCAD_ADDEXEC(yuv-pix yuv-pix.c "libfb libbn libbu" STRICT)

INSTALL(PROGRAMS morphedit.tcl DESTINATION bin)

add_executable(roots_example roots_example.c)
target_link_libraries(roots_example libbu librt)
BRLCAD_ADDFILE(roots_example.c sample_applications)

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
  pl-tek.1 
  plcolor.1 
  pldebug.1 
  plgetframe.1 
  plline2.1 
  plrot.1 
  png-bw.1 
  png-pix.1 
  png_info.1 
  random.1 
  rle-pix.1 
  sun-pix.1 
  terrain.1 
  wavelet.1
)
install(FILES ${util_MANS} DESTINATION ${MAN_DIR}/man1)
