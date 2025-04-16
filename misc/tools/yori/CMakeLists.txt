cmake_minimum_required(VERSION 3.20)

project(YEDIT)

# This editor is specific to Microsoft Windows
if (MSVC)
  set(YORI_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/YEDIT_BLD-prefix/src/YEDIT_BLD)

  include(ExternalProject)
  ExternalProject_Add(YEDIT_BLD
    URL ${CMAKE_CURRENT_SOURCE_DIR}/yori
    BUILD_ALWAYS OFF
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${CMAKE_COMMAND} -DYORI_SRC_DIR=${YORI_SRC_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/yedit_build.cmake
    INSTALL_COMMAND ""
    )

  # Add a custom copy rule to stage yedit.exe so we can depend on YEDIT_BLD
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/${BIN_DIR}/yedit.exe
    COMMAND ${CMAKE_COMMAND} -E copy ${YORI_SRC_DIR}/edit/yedit.exe ${CMAKE_BINARY_DIR}/${BIN_DIR}/yedit.exe
    DEPENDS YEDIT_BLD
    )
  add_custom_target(yedit ALL DEPENDS ${CMAKE_BINARY_DIR}/${BIN_DIR}/yedit.exe)
  set_target_properties(yedit PROPERTIES FOLDER "BRL-CAD Executables/Util")

  # Install the output of the custom command
  install(PROGRAMS ${CMAKE_BINARY_DIR}/${BIN_DIR}/yedit.exe DESTINATION ${BIN_DIR})

endif()

set(YORI_FILES
  yori/config/common.mk
  yori/config/ver.mk
  yori/crt/Makefile
  yori/crt/bigdiv.asm
  yori/crt/bigdvrm.asm
  yori/crt/bigmul.asm
  yori/crt/bigrem.asm
  yori/crt/bigshl.asm
  yori/crt/bigshr.asm
  yori/crt/cmpxchg.asm
  yori/crt/ep_cons.c
  yori/crt/ep_consw.c
  yori/crt/ep_dll.c
  yori/crt/mem.c
  yori/crt/rand.c
  yori/crt/string.c
  yori/crt/stringw.c
  yori/crt/yoricrt.h
  yori/edit/Makefile
  yori/edit/edit.c
  yori/edit/edit.h
  yori/edit/options.c
  yori/lib/Makefile
  yori/lib/airplane.c
  yori/lib/bargraph.c
  yori/lib/builtin.c
  yori/lib/bytebuf.c
  yori/lib/cabinet.c
  yori/lib/call.c
  yori/lib/cancel.c
  yori/lib/clip.c
  yori/lib/cmdline.c
  yori/lib/color.c
  yori/lib/condrv.c
  yori/lib/cpuinfo.c
  yori/lib/cshot.c
  yori/lib/curdir.c
  yori/lib/cvtcons.c
  yori/lib/cvthtml.c
  yori/lib/cvtrtf.c
  yori/lib/dblclk.c
  yori/lib/debug.c
  yori/lib/dyld.c
  yori/lib/dyld_adv.c
  yori/lib/dyld_cab.c
  yori/lib/dyld_net.c
  yori/lib/dyld_usr.c
  yori/lib/env.c
  yori/lib/ep_yori.c
  yori/lib/filecomp.c
  yori/lib/fileenum.c
  yori/lib/filefilt.c
  yori/lib/fileinfo.c
  yori/lib/fullpath.c
  yori/lib/group.c
  yori/lib/hash.c
  yori/lib/hexdump.c
  yori/lib/http.c
  yori/lib/iconv.c
  yori/lib/jobobj.c
  yori/lib/license.c
  yori/lib/lineread.c
  yori/lib/list.c
  yori/lib/malloc.c
  yori/lib/movefile.c
  yori/lib/numkey.c
  yori/lib/obenum.c
  yori/lib/osver.c
  yori/lib/path.c
  yori/lib/printf.c
  yori/lib/printf.inc
  yori/lib/printfa.c
  yori/lib/priv.c
  yori/lib/process.c
  yori/lib/progman.c
  yori/lib/recycle.c
  yori/lib/rsrc.c
  yori/lib/scheme.c
  yori/lib/scut.c
  yori/lib/select.c
  yori/lib/strarray.c
  yori/lib/string.c
  yori/lib/strmenum.c
  yori/lib/temp.c
  yori/lib/update.c
  yori/lib/util.c
  yori/lib/vt.c
  yori/lib/ylhomedr.c
  yori/lib/ylstralc.c
  yori/lib/ylstrcat.c
  yori/lib/ylstrcmp.c
  yori/lib/ylstrcnt.c
  yori/lib/ylstrcnv.c
  yori/lib/ylstrfnd.c
  yori/lib/ylstrhex.c
  yori/lib/ylstrnum.c
  yori/lib/ylstrsrt.c
  yori/lib/ylstrtrm.c
  yori/lib/ylvolpth.c
  yori/lib/ylvtdbg.c
  yori/lib/yori.man
  yori/lib/yoricall.h
  yori/lib/yoricmpt.h
  yori/lib/yorilib.h
  yori/lib/yoripch.h
  yori/lib/yoriver.inc
  yori/libdlg/Makefile
  yori/libdlg/about.c
  yori/libdlg/device.c
  yori/libdlg/dir.c
  yori/libdlg/file.c
  yori/libdlg/find.c
  yori/libdlg/findhex.c
  yori/libdlg/input.c
  yori/libdlg/msgbox.c
  yori/libdlg/replace.c
  yori/libdlg/replhex.c
  yori/libdlg/yoridlg.h
  yori/libwin/Makefile
  yori/libwin/border.c
  yori/libwin/button.c
  yori/libwin/checkbox.c
  yori/libwin/color.c
  yori/libwin/combo.c
  yori/libwin/ctrl.c
  yori/libwin/edit.c
  yori/libwin/hexedit.c
  yori/libwin/itemaray.c
  yori/libwin/label.c
  yori/libwin/list.c
  yori/libwin/menubar.c
  yori/libwin/mledit.c
  yori/libwin/radio.c
  yori/libwin/scrolbar.c
  yori/libwin/text.c
  yori/libwin/window.c
  yori/libwin/winmgr.c
  yori/libwin/winpriv.h
  yori/libwin/yoriwin.h
  yedit_build.cmake
  CMakeLists.txt
  README.txt
  LICENSE
  )

cmakefiles(${YORI_FILES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 cino=N-s
