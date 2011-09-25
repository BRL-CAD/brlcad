SET(UTIL_INCLUDE_DIRS
	${BU_INCLUDE_DIRS}
	${BN_INCLUDE_DIRS}
	${DM_INCLUDE_DIRS}
	${FB_INCLUDE_DIRS}
	${RT_INCLUDE_DIRS}
	${SYSV_INCLUDE_DIRS}
	${ORLE_INCLUDE_DIRS}
	${TCLCAD_INCLUDE_DIRS}
	${WDB_INCLUDE_DIRS}
	)
LIST(REMOVE_DUPLICATES UTIL_INCLUDE_DIRS)
include_directories(${UTIL_INCLUDE_DIRS})

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${UTAHRLE_INCLUDE_DIR}
)

IF(MSVC)
	add_definitions(
	-D_CONSOLE
	-DIF_WGL
	)
ENDIF(MSVC)


IF(BRLCAD-ENABLE_X11)
	BRLCAD_ADDEXEC(pl-X pl-X.c "libdm libbu")
	IF(BRLCAD-ENABLE_TK)
		BRLCAD_ADDEXEC(pl-dm pl-dm.c "libdm libbu ${M_LIBRARY}")
		BRLCAD_ADDEXEC(bombardier bombardier.c "libtclcad libbu")
	ENDIF(BRLCAD-ENABLE_TK)
ENDIF(BRLCAD-ENABLE_X11)

BRLCAD_ADDFILE(pl-dm.c sample_applications)

IF(LIBPC)
	add_executable(pc_test pc_test.c)
	target_link_libraries(pc_test libwdb libpc)
ENDIF(LIBPC)

BRLCAD_ADDEXEC(alias-pix alias-pix.c libbu)
BRLCAD_ADDEXEC(ap-pix ap-pix.c libbu)
BRLCAD_ADDEXEC(asc-pl asc-pl.c "libbn libbu")
BRLCAD_ADDEXEC(azel azel.c "libbu ${M_LIBRARY}")
BRLCAD_ADDEXEC(bary bary.c libbu)
BRLCAD_ADDEXEC(buffer buffer.c libbu)
BRLCAD_ADDEXEC(bw-a bw-a.c "libfb libbu")
BRLCAD_ADDEXEC(bw-imp bw-imp.c libbu)
BRLCAD_ADDEXEC(bw-pix bw-pix.c libbu)
BRLCAD_ADDEXEC(bw-png bw-png.c "libfb libbu ${PNG_LIBRARY}")
BRLCAD_ADDEXEC(bw-ps bw-ps.c libbu)
BRLCAD_ADDEXEC(bw-rle bw-rle.c "libbu libsysv ${UTAHRLE_LIBRARY}")
BRLCAD_ADDEXEC(bw3-pix bw3-pix.c libbu)
BRLCAD_ADDEXEC(bwcrop bwcrop.c libbu)
BRLCAD_ADDEXEC(bwdiff bwdiff.c libbu)
BRLCAD_ADDEXEC(bwfilter bwfilter.c libbu)
BRLCAD_ADDEXEC(bwhist bwhist.c libfb)
BRLCAD_ADDEXEC(bwhisteq bwhisteq.c libbu)
BRLCAD_ADDEXEC(bwmod bwmod.c "libbu ${M_LIBRARY}")
BRLCAD_ADDEXEC(bwrect bwrect.c libbu)
BRLCAD_ADDEXEC(bwrot bwrot.c "libbu ${M_LIBRARY}")
BRLCAD_ADDEXEC(bwscale bwscale.c libbu)
BRLCAD_ADDEXEC(bwshrink bwshrink.c libbu)
BRLCAD_ADDEXEC(bwstat bwstat.c "libbu ${M_LIBRARY}")
BRLCAD_ADDEXEC(bwthresh bwthresh.c libbu)
BRLCAD_ADDEXEC(cv cv.c libbu)
# FIXME dbcp is a "copy program for UNIX" - will need some porting
# work for Windows
IF(NOT WIN32)
	BRLCAD_ADDEXEC(dbcp dbcp.c libbu)
ENDIF(NOT WIN32)
BRLCAD_ADDEXEC(decimate decimate.c libbu)
BRLCAD_ADDEXEC(double-asc double-asc.c "libfb libbu")
BRLCAD_ADDEXEC(dpix-pix dpix-pix.c libbu)
BRLCAD_ADDEXEC(dsp_add dsp_add.c libbu)
# Not fixing these for Windows - they're on their way out (deprecated)
IF(NOT WIN32)
	BRLCAD_ADDEXEC(dunncolor "dunncolor.c dunncomm.c" libbu)
	BRLCAD_ADDEXEC(dunnsnap "dunnsnap.c dunncomm.c" "libfb libbu")
ENDIF(NOT WIN32)
BRLCAD_ADDEXEC(fix_polysolids fix_polysolids.c "librt libbu")
BRLCAD_ADDEXEC(gencolor gencolor.c libbu)
BRLCAD_ADDEXEC(hex hex.c libbu)
BRLCAD_ADDEXEC(imgdims imgdims.c "libfb libbu")
BRLCAD_ADDEXEC(loop loop.c libbu)
BRLCAD_ADDEXEC(lowp lowp.c libbu)
BRLCAD_ADDEXEC(mac-pix mac-pix.c libbu)
BRLCAD_ADDEXEC(random random.c "libbn libbu")
BRLCAD_ADDEXEC(orle-pix orle-pix.c "libfb liborle libbu")
BRLCAD_ADDEXEC(pix-alias pix-alias.c libbu)
BRLCAD_ADDEXEC(pix-bw pix-bw.c libbu)
BRLCAD_ADDEXEC(pix-bw3 pix-bw3.c libbu)
BRLCAD_ADDEXEC(pix-orle pix-orle.c "libfb liborle libbu")
BRLCAD_ADDEXEC(pix-png pix-png.c "libfb libbu")
BRLCAD_ADDEXEC(pix-ppm pix-ppm.c "libfb libbu")
BRLCAD_ADDEXEC(pix-ps pix-ps.c libbu)
BRLCAD_ADDEXEC(pix-rle pix-rle.c "libbu libsysv ${UTAHRLE_LIBRARY}")
BRLCAD_ADDEXEC(pix-spm pix-spm.c libbn)
BRLCAD_ADDEXEC(pix-sun pix-sun.c libbu)
BRLCAD_ADDEXEC(pix-yuv pix-yuv.c "libfb libbn libbu")
BRLCAD_ADDEXEC(pix3filter pix3filter.c libbu)
BRLCAD_ADDEXEC(pixbackgnd pixbackgnd.c libbu)
BRLCAD_ADDEXEC(pixbgstrip pixbgstrip.c "libfb libbu")
BRLCAD_ADDEXEC(pixblend pixblend.c libbu)
BRLCAD_ADDEXEC(pixborder pixborder.c "libfb libbn libbu ${M_LIBRARY}")
BRLCAD_ADDEXEC(pixbustup pixbustup.c libbu)
BRLCAD_ADDEXEC(pixclump pixclump.c "libbn libbu")
BRLCAD_ADDEXEC(pixcolors pixcolors.c libbu)
BRLCAD_ADDEXEC(pixcount pixcount.c libbu)
BRLCAD_ADDEXEC(pixcut pixcut.c "libfb libbu")
BRLCAD_ADDEXEC(pixdiff pixdiff.c libbu)
BRLCAD_ADDEXEC(pixdsplit pixdsplit.c libbu)
BRLCAD_ADDEXEC(pixelswap pixelswap.c libbu)
BRLCAD_ADDEXEC(pixembed pixembed.c libbu)
BRLCAD_ADDEXEC(pixfade pixfade.c "libbn libbu")
BRLCAD_ADDEXEC(pixfields pixfields.c libbu)
BRLCAD_ADDEXEC(pixfieldsep pixfieldsep.c libbu)
BRLCAD_ADDEXEC(pixfilter pixfilter.c libbu)
BRLCAD_ADDEXEC(pixhalve pixhalve.c "libfb libbu")
BRLCAD_ADDEXEC(pixhist pixhist.c "libfb libbu")
BRLCAD_ADDEXEC(pixhist3d pixhist3d.c "libfb libbu")
BRLCAD_ADDEXEC(pixhist3d-pl pixhist3d-pl.c "libbn libbu")
BRLCAD_ADDEXEC(pixinterp2x pixinterp2x.c libbu)
BRLCAD_ADDEXEC(pixmatte pixmatte.c libbu)
BRLCAD_ADDEXEC(pixmerge pixmerge.c libbu)
BRLCAD_ADDEXEC(pixmorph pixmorph.c "libfb libbu ${M_LIBRARY}")
BRLCAD_ADDEXEC(pixpaste pixpaste.c "libfb libbu")
BRLCAD_ADDEXEC(pixrect pixrect.c libbu)
BRLCAD_ADDEXEC(pixrot pixrot.c libbu)
BRLCAD_ADDEXEC(pixsaturate pixsaturate.c libbu)
BRLCAD_ADDEXEC(pixscale pixscale.c libbu)
BRLCAD_ADDEXEC(pixshrink pixshrink.c libbu)
BRLCAD_ADDEXEC(pixstat pixstat.c "libbu ${M_LIBRARY}")
BRLCAD_ADDEXEC(pixsubst pixsubst.c libbu)
BRLCAD_ADDEXEC(pixtile pixtile.c libbu)
BRLCAD_ADDEXEC(pixuntile pixuntile.c libbu)
BRLCAD_ADDEXEC(pl-asc pl-asc.c libbu)
BRLCAD_ADDEXEC(pl-hpgl pl-hpgl.c libbu)
BRLCAD_ADDEXEC(pl-pl pl-pl.c libbu)
BRLCAD_ADDEXEC(pl-ps pl-ps.c libbu)
BRLCAD_ADDEXEC(pl-tek pl-tek.c libbu)
BRLCAD_ADDEXEC(plcolor plcolor.c libbn)
BRLCAD_ADDEXEC(pldebug pldebug.c libbu)
SET_TARGET_PROPERTIES(pldebug PROPERTIES LINKER_LANGUAGE C)
BRLCAD_ADDEXEC(plgetframe plgetframe.c libbu)
BRLCAD_ADDEXEC(plline2 plline2.c "libbn libbu")
BRLCAD_ADDEXEC(plrot plrot.c "libbn libbu ${M_LIBRARY}")
BRLCAD_ADDEXEC(plstat plstat.c libbu)
BRLCAD_ADDEXEC(png-bw png-bw.c "libbn libbu")
BRLCAD_ADDEXEC(png-pix png-pix.c "libbn libbu")
BRLCAD_ADDEXEC(png_info png_info.c "libbn libbu")
BRLCAD_ADDEXEC(rle-pix rle-pix.c "libbu libsysv ${UTAHRLE_LIBRARY}")
BRLCAD_ADDEXEC(sun-pix sun-pix.c libbu)
BRLCAD_ADDEXEC(terrain terrain.c "libbn libbu ${M_LIBRARY}")
BRLCAD_ADDEXEC(texturescale texturescale.c "libfb libbn libbu ${M_LIBRARY}")
#FIXME - Need to look at pcattcp port to Windows - ttcp should be
#maintained and ported
IF(NOT WIN32)
   BRLCAD_ADDEXEC(ttcp ttcp.c libbu)
ENDIF(NOT WIN32)
BRLCAD_ADDEXEC(wavelet wavelet.c "libfb libbn libbu")
BRLCAD_ADDEXEC(xyz-pl xyz-pl.c "libbn libbu")
BRLCAD_ADDEXEC(yuv-pix yuv-pix.c "libfb libbn libbu")

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
ADD_MAN_PAGES(1 util_MANS)
CMAKEFILES(bombardier.h morphedit.tcl pldebug.c)
CMAKEFILES(Makefile.am)
