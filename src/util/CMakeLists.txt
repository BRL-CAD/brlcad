set(UTIL_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${DM_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${SYSV_INCLUDE_DIRS}
  ${TCLCAD_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  )

set(UTIL_LOCAL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${UTAHRLE_INCLUDE_DIRS}
  ${PNG_INCLUDE_DIRS}
  )

set(UTIL_ALL_INCLUDE_DIRS ${UTIL_INCLUDE_DIRS} ${UTIL_LOCAL_INCLUDE_DIRS})
BRLCAD_INCLUDE_DIRS(UTIL_ALL_INCLUDE_DIRS)

if(MSVC)
  add_definitions(
    -D_CONSOLE
    -DIF_WGL
    )
endif(MSVC)

if(BRLCAD_ENABLE_TK)
  BRLCAD_ADDEXEC(bombardier bombardier.c "libtclcad;libbu;${TK_LIBRARY};${TCL_LIBRARY}")
  BRLCAD_ADDEXEC(plot3-dm plot3-dm.c "libtclcad;libdm;libbv;libbu;${TK_LIBRARY};${TCL_LIBRARY};${M_LIBRARY}")
  ADD_TARGET_DEPS(plot3-dm dm_plugins)
else(BRLCAD_ENABLE_TK)
  CMAKEFILES(
    bombardier.c
    plot3-dm.c
    )
endif(BRLCAD_ENABLE_TK)

BRLCAD_ADDDATA(plot3-dm.c sample_applications)

if(LIBPC_DIR)
  BRLCAD_ADDEXEC(pc_test pc_test.c "libwdb;libpc" NO_INSTALL)
else(LIBPC_DIR)
  CMAKEFILES(pc_test.c)
endif(LIBPC_DIR)

BRLCAD_ADDEXEC(alias-pix alias-pix.c libbu FOLDER Util)
BRLCAD_ADDEXEC(ap-pix ap-pix.c libbu FOLDER Util)
BRLCAD_ADDEXEC(asc-plot3 asc-plot3.c "libbv;libbn;libbu" FOLDER Util)
BRLCAD_ADDEXEC(bw-a bw-a.c "libdm;libbu" FOLDER Util)
ADD_TARGET_DEPS(bw-a dm_plugins)
BRLCAD_ADDEXEC(bw-pix bw-pix.c "libbu;libicv" FOLDER Util)
BRLCAD_ADDEXEC(bw-png bw-png.c "libdm;libbu;${PNG_LIBRARIES}" FOLDER Util)
ADD_TARGET_DEPS(bw-png dm_plugins)
BRLCAD_ADDEXEC(bw-ps bw-ps.c libbu FOLDER Util)
BRLCAD_ADDEXEC(bwcrop bwcrop.c libbu FOLDER Util)
BRLCAD_ADDEXEC(bwdiff bwdiff.c libbu FOLDER Util)
BRLCAD_ADDEXEC(bwfilter bwfilter.c "libicv;libbu" FOLDER Util)
BRLCAD_ADDEXEC(bwhist bwhist.c libdm FOLDER Util)
ADD_TARGET_DEPS(bwhist dm_plugins)
BRLCAD_ADDEXEC(bwhisteq bwhisteq.c libbu FOLDER Util)
BRLCAD_ADDEXEC(bwmod bwmod.c "libbu;${M_LIBRARY}" FOLDER Util)
BRLCAD_ADDEXEC(bwrect bwrect.c "libbu;libicv" FOLDER Util)
BRLCAD_ADDEXEC(bwrot bwrot.c "libbu;${M_LIBRARY}" FOLDER Util)
BRLCAD_ADDEXEC(bwscale bwscale.c libbu FOLDER Util)
BRLCAD_ADDEXEC(bwshrink bwshrink.c libbu FOLDER Util)
BRLCAD_ADDEXEC(bwstat bwstat.c "libbu;${M_LIBRARY}" FOLDER Util)
BRLCAD_ADDEXEC(bwthresh bwthresh.c libbu FOLDER Util)
BRLCAD_ADDEXEC(cv cv.c libbu FOLDER Util)
BRLCAD_ADDEXEC(decimate decimate.c libbu FOLDER Util)
BRLCAD_ADDEXEC(double-asc double-asc.c "libdm;libbu" FOLDER Util)
ADD_TARGET_DEPS(double-asc dm_plugins)
BRLCAD_ADDEXEC(dpix-pix dpix-pix.c libbu FOLDER Util)
BRLCAD_ADDEXEC(gencolor gencolor.c libbu FOLDER Util)
BRLCAD_ADDEXEC(halftone halftone.c "librt;libdm;libbu" FOLDER Util)
ADD_TARGET_DEPS(halftone dm_plugins)

# This is intended to replace all of the img-img utils eventually
BRLCAD_ADDEXEC(icv icv.cpp "libicv;libbu" FOLDER Util)

BRLCAD_ADDEXEC(imgdims imgdims.c "libdm;libbu" FOLDER Util)
ADD_TARGET_DEPS(imgdims dm_plugins)
BRLCAD_ADDEXEC(loop loop.c libbu FOLDER Util)
BRLCAD_ADDEXEC(lowp lowp.c libbu FOLDER Util)
BRLCAD_ADDEXEC(mac-pix mac-pix.c libbu FOLDER Util)
BRLCAD_ADDEXEC(random random.c "libbn;libbu" FOLDER Util)
BRLCAD_ADDEXEC(pix-alias pix-alias.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pix-bw pix-bw.c "libbu;libicv" FOLDER Util)
BRLCAD_ADDEXEC(pix-png pix-png.c "libdm;libbu;${PNG_LIBRARIES}" FOLDER Util)
ADD_TARGET_DEPS(pix-png dm_plugins)
BRLCAD_ADDEXEC(pix-ppm pix-ppm.c "libdm;libbu" FOLDER Util)
ADD_TARGET_DEPS(pix-ppm dm_plugins)
BRLCAD_ADDEXEC(pix-ps pix-ps.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pix-spm pix-spm.c libbn FOLDER Util)
BRLCAD_ADDEXEC(pix-sun pix-sun.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pix3filter pix3filter.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixbackgnd pixbackgnd.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixbgstrip pixbgstrip.c "libdm;libbu" FOLDER Util)
ADD_TARGET_DEPS(pixbgstrip dm_plugins)
BRLCAD_ADDEXEC(pixblend pixblend.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixborder pixborder.c "libdm;libbn;libbu;${M_LIBRARY}" FOLDER Util)
ADD_TARGET_DEPS(pixborder dm_plugins)
BRLCAD_ADDEXEC(pixbustup pixbustup.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixclump pixclump.c "libbn;libbu" FOLDER Util)
BRLCAD_ADDEXEC(pixcolors pixcolors.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixcount pixcount.cpp libbu FOLDER Util)
BRLCAD_ADDEXEC(pixcrop pixcrop.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixdiff pixdiff.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixelswap pixelswap.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixembed pixembed.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixfade pixfade.c "libicv;libbu" FOLDER Util)
BRLCAD_ADDEXEC(pixfilter pixfilter.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixhalve pixhalve.c "libdm;libbu" FOLDER Util)
ADD_TARGET_DEPS(pixhalve dm_plugins)
BRLCAD_ADDEXEC(pixhist pixhist.c "libdm;libbu" FOLDER Util)
ADD_TARGET_DEPS(pixhist dm_plugins)
BRLCAD_ADDEXEC(pixhist3d pixhist3d.c "libdm;libbu" FOLDER Util)
ADD_TARGET_DEPS(pixhist3d dm_plugins)
BRLCAD_ADDEXEC(pixhist3d-plot3 pixhist3d-plot3.c "libbv;libbn;libbu" FOLDER Util)
BRLCAD_ADDEXEC(pixinterp2x pixinterp2x.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixmatte pixmatte.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixmerge pixmerge.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixmorph pixmorph.c "libdm;libbu;${M_LIBRARY}" FOLDER Util)
ADD_TARGET_DEPS(pixmorph dm_plugins)
BRLCAD_ADDEXEC(pixrect pixrect.c "libbu;libicv" FOLDER Util)
BRLCAD_ADDEXEC(pixrot pixrot.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixsaturate pixsaturate.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixscale pixscale.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixshrink pixshrink.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixstat pixstat.c "libbu;${M_LIBRARY}" FOLDER Util)
BRLCAD_ADDEXEC(pixsubst pixsubst.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixtile pixtile.c libbu FOLDER Util)
BRLCAD_ADDEXEC(pixuntile pixuntile.c libbu FOLDER Util)
BRLCAD_ADDEXEC(plot3-asc plot3-asc.c "libbv;libbu" FOLDER Util)
BRLCAD_ADDEXEC(plot3-plot3 plot3-plot3.c "libbv;libbu" FOLDER Util)
BRLCAD_ADDEXEC(plot3-ps plot3-ps.c "libbv;libbu" FOLDER Util)
BRLCAD_ADDEXEC(plot3color plot3color.c "libbv;libbu" FOLDER Util)
BRLCAD_ADDEXEC(plot3getframe plot3getframe.c "libbv;libbu" FOLDER Util)
BRLCAD_ADDEXEC(plot3line2 plot3line2.c "libbv;libbu" FOLDER Util)
BRLCAD_ADDEXEC(plot3rot plot3rot.c "libbg;libbv;libbu;${M_LIBRARY}" FOLDER Util)
BRLCAD_ADDEXEC(plot3stat plot3stat.c "libbv;libbu" FOLDER Util)
BRLCAD_ADDEXEC(png-bw png-bw.c "libbn;libbu;${PNG_LIBRARIES}" FOLDER Util)
BRLCAD_ADDEXEC(png-pix png-pix.c "libbn;libbu;${PNG_LIBRARIES}" FOLDER Util)
BRLCAD_ADDEXEC(sun-pix sun-pix.c libbu FOLDER Util)
BRLCAD_ADDEXEC(terrain terrain.c "libbn;libbu;${M_LIBRARY}" FOLDER Util)
#FIXME - Need to look at pcattcp port to Windows - ttcp should be
#maintained and ported
if(NOT WIN32)
  BRLCAD_ADDEXEC(ttcp ttcp.c "${SOCKET_LIBRARY};${NSL_LIBRARY};${NETWORK_LIBRARY}" FOLDER Util)
else(NOT WIN32)
  CMAKEFILES(ttcp.c)
endif(NOT WIN32)
BRLCAD_ADDEXEC(wavelet wavelet.c "libdm;libbn;libbu" FOLDER Util)
ADD_TARGET_DEPS(wavelet dm_plugins)

install(PROGRAMS morphedit.tcl DESTINATION bin)

BRLCAD_ADDEXEC(roots_example roots_example.c "libbu;librt" NO_INSTALL)
BRLCAD_ADDDATA(roots_example.c sample_applications)

CMAKEFILES(bombardier.h morphedit.tcl)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
