set(UTIL_INCLUDE_DIRS
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

set(UTIL_LOCAL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${UTAHRLE_INCLUDE_DIR}
  ${PNG_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/../other/tclap/include  #TODO - need to get this into libbu
  ${CMAKE_CURRENT_SOURCE_DIR}/../other/tclap/include  #TODO - need to get this into libbu
  )

set(UTIL_ALL_INCLUDE_DIRS ${UTIL_INCLUDE_DIRS} ${UTIL_LOCAL_INCLUDE_DIRS})
BRLCAD_SORT_INCLUDE_DIRS(UTIL_ALL_INCLUDE_DIRS)
include_directories(${UTIL_ALL_INCLUDE_DIRS})

if(MSVC)
  add_definitions(
    -D_CONSOLE
    -DIF_WGL
    )
endif(MSVC)


if(BRLCAD_ENABLE_X11)
  BRLCAD_ADDEXEC(plot3-X plot3-X.c "libdm;libbu")
  if(BRLCAD_ENABLE_TK)
    BRLCAD_ADDEXEC(plot3-dm plot3-dm.c "libdm;libbu;${M_LIBRARY}")
    BRLCAD_ADDEXEC(bombardier bombardier.c "libtclcad;libbu")
  endif(BRLCAD_ENABLE_TK)
endif(BRLCAD_ENABLE_X11)

BRLCAD_ADDDATA(plot3-dm.c sample_applications)

if(LIBPC_DIR)
  BRLCAD_ADDEXEC(pc_test pc_test.c "libwdb;libpc" NO_INSTALL)
else(LIBPC_DIR)
  CMAKEFILES(pc_test.c)
endif(LIBPC_DIR)

BRLCAD_ADDEXEC(alias-pix alias-pix.c libbu)
BRLCAD_ADDEXEC(ap-pix ap-pix.c libbu)
BRLCAD_ADDEXEC(asc-plot3 asc-plot3.c "libbn;libbu")
BRLCAD_ADDEXEC(azel azel.c "libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(bary bary.c libbu)
BRLCAD_ADDEXEC(buffer buffer.c libbu)
BRLCAD_ADDEXEC(bw-a bw-a.c "libfb;libbu")
BRLCAD_ADDEXEC(bw-imp bw-imp.c libbu)
BRLCAD_ADDEXEC(bw-pix bw-pix.c "libbu;libicv")
BRLCAD_ADDEXEC(bw-png bw-png.c "libfb;libbu;${PNG_LIBRARY}")
BRLCAD_ADDEXEC(bw-ps bw-ps.c libbu)
BRLCAD_ADDEXEC(bw-rle "bw-rle.c;rle_args.c" "libbu;libsysv;${UTAHRLE_LIBRARY}")
BRLCAD_ADDEXEC(bw3-pix bw3-pix.c libbu)
BRLCAD_ADDEXEC(bwcrop bwcrop.c libbu)
BRLCAD_ADDEXEC(bwdiff bwdiff.c libbu)
BRLCAD_ADDEXEC(bwfilter bwfilter.c "libicv;libbu")
BRLCAD_ADDEXEC(bwhist bwhist.c libfb)
BRLCAD_ADDEXEC(bwhisteq bwhisteq.c libbu)
BRLCAD_ADDEXEC(bwmod bwmod.c "libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(bwrect bwrect.c "libbu;libicv")
BRLCAD_ADDEXEC(bwrot bwrot.c "libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(bwscale bwscale.c libbu)
BRLCAD_ADDEXEC(bwshrink bwshrink.c libbu)
BRLCAD_ADDEXEC(bwstat bwstat.c "libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(bwthresh bwthresh.c libbu)
BRLCAD_ADDEXEC(cv cv.c libbu)
# FIXME dbcp is a "copy program for UNIX" - will need some porting
# work for Windows
if(NOT WIN32)
  BRLCAD_ADDEXEC(dbcp dbcp.c libbu)
endif(NOT WIN32)
BRLCAD_ADDEXEC(decimate decimate.c libbu)
BRLCAD_ADDEXEC(double-asc double-asc.c "libfb;libbu")
BRLCAD_ADDEXEC(dpix-pix dpix-pix.c libbu)
BRLCAD_ADDEXEC(dsp_add dsp_add.c libbu)

#=====================================================
# TCLAP testing
#=====================================================
# a test copy for tclap/auto-man-page proof of concept:
BRLCAD_ADDEXEC(dsp_add_t dsp_add_t.cpp libbu NO_INSTALL)

# tmp def for testing C wrapper for TCLAP
BRLCAD_ADDEXEC(dsp_add4 "dsp_add4.c;bu_arg_parse.cpp" libbu NO_INSTALL)

# a test file for the entire TCLAP wrapper interface
#BRLCAD_ADDEXEC(test_bu_arg_parse "test_bu_arg_parse.c;bu_arg_parse.cpp" libbu NO_INSTALL)
# ignore it for now
CMAKEFILES(test_bu_arg_parse.c)

# a new admin file
BRLCAD_ADDEXEC(admin-db "admin-db.cpp;bu_arg_parse.cpp" "libbu;librt" NO_INSTALL)

# tmp header to stand-in for bu.h (ignore it)
CMAKEFILES(bu_arg_parse.h)
# permanent private header for TCLAP customizations (ignore it)
CMAKEFILES(bu_arg_parse_private.h)
#=====================================================
# end TCLAP testing
#=====================================================

# Not fixing these for Windows - they're on their way out (deprecated)
if(NOT WIN32)
  BRLCAD_ADDEXEC(dunncolor "dunncolor.c;dunncomm.c" libbu)
  BRLCAD_ADDEXEC(dunnsnap "dunnsnap.c;dunncomm.c" "libfb;libbu")
endif(NOT WIN32)
BRLCAD_ADDEXEC(fix_polysolids fix_polysolids.c "librt;libbu")
BRLCAD_ADDEXEC(gencolor gencolor.c libbu)
BRLCAD_ADDEXEC(hex hex.c libbu)
BRLCAD_ADDEXEC(imgdims imgdims.c "libfb;libbu")
BRLCAD_ADDEXEC(loop loop.c libbu)
BRLCAD_ADDEXEC(lowp lowp.c libbu)
BRLCAD_ADDEXEC(mac-pix mac-pix.c libbu)
BRLCAD_ADDEXEC(random random.c "libbn;libbu")
BRLCAD_ADDEXEC(orle-pix orle-pix.c "libfb;liborle;libbu")
BRLCAD_ADDEXEC(pix-alias pix-alias.c libbu)
BRLCAD_ADDEXEC(pix-bw pix-bw.c "libbu;libicv")
BRLCAD_ADDEXEC(pix-bw3 pix-bw3.c libbu)
BRLCAD_ADDEXEC(pix-orle pix-orle.c "libfb;liborle;libbu")
BRLCAD_ADDEXEC(pix-png pix-png.c "libfb;libbu;${PNG_LIBRARY}")
BRLCAD_ADDEXEC(pix-ppm pix-ppm.c "libfb;libbu")
BRLCAD_ADDEXEC(pix-ps pix-ps.c libbu)
BRLCAD_ADDEXEC(pix-rle "pix-rle.c;rle_args.c" "libbu;libsysv;${UTAHRLE_LIBRARY}")
BRLCAD_ADDEXEC(pix-spm pix-spm.c libbn)
BRLCAD_ADDEXEC(pix-sun pix-sun.c libbu)
BRLCAD_ADDEXEC(pix-yuv pix-yuv.c "libfb;libbn;libbu")
BRLCAD_ADDEXEC(pix3filter pix3filter.c libbu)
BRLCAD_ADDEXEC(pixbackgnd pixbackgnd.c libbu)
BRLCAD_ADDEXEC(pixbgstrip pixbgstrip.c "libfb;libbu")
BRLCAD_ADDEXEC(pixblend pixblend.c libbu)
BRLCAD_ADDEXEC(pixborder pixborder.c "libfb;libbn;libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(pixbustup pixbustup.c libbu)
BRLCAD_ADDEXEC(pixclump pixclump.c "libbn;libbu")
BRLCAD_ADDEXEC(pixcolors pixcolors.c libbu)
BRLCAD_ADDEXEC(pixcount pixcount.c libbu)
BRLCAD_ADDEXEC(pixcut pixcut.c "libfb;libbu")
BRLCAD_ADDEXEC(pixdiff pixdiff.c libbu)
BRLCAD_ADDEXEC(pixdsplit pixdsplit.c libbu)
BRLCAD_ADDEXEC(pixelswap pixelswap.c libbu)
BRLCAD_ADDEXEC(pixembed pixembed.c libbu)
BRLCAD_ADDEXEC(pixfade pixfade.c "libicv;libbu")
BRLCAD_ADDEXEC(pixfields pixfields.c libbu)
BRLCAD_ADDEXEC(pixfieldsep pixfieldsep.c libbu)
BRLCAD_ADDEXEC(pixfilter pixfilter.c libbu)
BRLCAD_ADDEXEC(pixhalve pixhalve.c "libfb;libbu")
BRLCAD_ADDEXEC(pixhist pixhist.c "libfb;libbu")
BRLCAD_ADDEXEC(pixhist3d pixhist3d.c "libfb;libbu")
BRLCAD_ADDEXEC(pixhist3d-plot3 pixhist3d-plot3.c "libbn;libbu")
BRLCAD_ADDEXEC(pixinterp2x pixinterp2x.c libbu)
BRLCAD_ADDEXEC(pixmatte pixmatte.c libbu)
BRLCAD_ADDEXEC(pixmerge pixmerge.c libbu)
BRLCAD_ADDEXEC(pixmorph pixmorph.c "libfb;libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(pixpaste pixpaste.c "libfb;libbu")
BRLCAD_ADDEXEC(pixrect pixrect.c "libbu;libicv")
BRLCAD_ADDEXEC(pixrot pixrot.c libbu)
BRLCAD_ADDEXEC(pixsaturate pixsaturate.c libbu)
BRLCAD_ADDEXEC(pixscale pixscale.c libbu)
BRLCAD_ADDEXEC(pixshrink pixshrink.c libbu)
BRLCAD_ADDEXEC(pixstat pixstat.c "libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(pixsubst pixsubst.c libbu)
BRLCAD_ADDEXEC(pixtile pixtile.c libbu)
BRLCAD_ADDEXEC(pixuntile pixuntile.c libbu)
BRLCAD_ADDEXEC(plot3-asc plot3-asc.c libbu)
BRLCAD_ADDEXEC(plot3-hpgl plot3-hpgl.c libbu)
BRLCAD_ADDEXEC(plot3-plot3 plot3-plot3.c libbu)
BRLCAD_ADDEXEC(plot3-ps plot3-ps.c libbu)
BRLCAD_ADDEXEC(plot3-tek plot3-tek.c libbu)
BRLCAD_ADDEXEC(plot3color plot3color.c libbn)
BRLCAD_ADDEXEC(plot3debug plot3debug.c libbu)
SET_TARGET_PROPERTIES(plot3debug PROPERTIES LINKER_LANGUAGE C)
BRLCAD_ADDEXEC(plot3getframe plot3getframe.c libbu)
BRLCAD_ADDEXEC(plot3line2 plot3line2.c "libbn;libbu")
BRLCAD_ADDEXEC(plot3rot plot3rot.c "libbn;libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(plot3stat plot3stat.c libbu)
BRLCAD_ADDEXEC(png-bw png-bw.c "libbn;libbu;${PNG_LIBRARY}")
BRLCAD_ADDEXEC(png-pix png-pix.c "libbn;libbu;${PNG_LIBRARY}")
BRLCAD_ADDEXEC(png_info png_info.c "libbn;libbu;${PNG_LIBRARY}")
BRLCAD_ADDEXEC(rle-pix rle-pix.c "libbu;libsysv;${UTAHRLE_LIBRARY}")
if(CPP_DLL_DEFINES)
  SET_PROPERTY(TARGET rle-pix APPEND PROPERTY COMPILE_DEFINITIONS "RLE_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)
BRLCAD_ADDEXEC(sun-pix sun-pix.c libbu)
BRLCAD_ADDEXEC(terrain terrain.c "libbn;libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(texturescale texturescale.c "libfb;libbn;libbu;${M_LIBRARY}")
#FIXME - Need to look at pcattcp port to Windows - ttcp should be
#maintained and ported
if(NOT WIN32)
  BRLCAD_ADDEXEC(ttcp ttcp.c "${SOCKET_LIBRARY};${NSL_LIBRARY};${NETWORK_LIBRARY}")
endif(NOT WIN32)
BRLCAD_ADDEXEC(wavelet wavelet.c "libfb;libbn;libbu")
BRLCAD_ADDEXEC(xyz-plot3 xyz-plot3.c "libbn;libbu")
BRLCAD_ADDEXEC(yuv-pix yuv-pix.c "libfb;libbn;libbu")

install(PROGRAMS morphedit.tcl DESTINATION bin)

BRLCAD_ADDEXEC(roots_example roots_example.c "libbu;librt" NO_INSTALL)
BRLCAD_ADDDATA(roots_example.c sample_applications)

CMAKEFILES(bombardier.h rle_args.h morphedit.tcl plot3debug.c)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
