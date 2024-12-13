cmake_minimum_required(VERSION 3.18)

project(YEDIT)

# This editor is specific to Microsoft Windows
if (MSVC)
  set(YORI_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/YEDIT_BLD-prefix/src/YEDIT_BLD)

  include(ExternalProject)
  ExternalProject_Add(YEDIT_BLD
    URL ${CMAKE_CURRENT_SOURCE_DIR}/yori
    BUILD_ALWAYS ON
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

  # Install the output of the custom command
  install(PROGRAMS ${CMAKE_BINARY_DIR}/${BIN_DIR}/yedit.exe DESTINATION ${BIN_DIR})

endif()

set(YORI_FILES
  yori/crt/bigshl.asm
  yori/crt/ep_consw.c
  yori/crt/cmpxchg.asm
  yori/crt/ep_cons.c
  yori/crt/rand.c
  yori/crt/yoricrt.h
  yori/crt/bigdiv.asm
  yori/crt/bigrem.asm
  yori/crt/mem.c
  yori/crt/bigmul.asm
  yori/crt/bigshr.asm
  yori/crt/Makefile
  yori/crt/bigdvrm.asm
  yori/crt/ep_dll.c
  yori/crt/stringw.c
  yori/crt/string.c
  yori/config/ver.mk
  yori/config/common.mk
  yori/libdlg/msgbox.c
  yori/libdlg/findhex.c
  yori/libdlg/replace.c
  yori/libdlg/dir.c
  yori/libdlg/device.c
  yori/libdlg/about.c
  yori/libdlg/yoridlg.h
  yori/libdlg/file.c
  yori/libdlg/input.c
  yori/libdlg/replhex.c
  yori/libdlg/Makefile
  yori/libdlg/find.c
  yori/libwin/color.c
  yori/libwin/winmgr.c
  yori/libwin/ctrl.c
  yori/libwin/winpriv.h
  yori/libwin/label.c
  yori/libwin/button.c
  yori/libwin/hexedit.c
  yori/libwin/checkbox.c
  yori/libwin/radio.c
  yori/libwin/yoriwin.h
  yori/libwin/combo.c
  yori/libwin/edit.c
  yori/libwin/itemaray.c
  yori/libwin/border.c
  yori/libwin/window.c
  yori/libwin/scrolbar.c
  yori/libwin/Makefile
  yori/libwin/menubar.c
  yori/libwin/list.c
  yori/libwin/mledit.c
  yori/edit/options.c
  yori/edit/edit.h
  yori/edit/edit.c
  yori/edit/Makefile
  yori/lib/env.c
  yori/lib/cmdline.c
  yori/lib/color.c
  yori/lib/printf.inc
  yori/lib/call.c
  yori/lib/numkey.c
  yori/lib/group.c
  yori/lib/yori.man
  yori/lib/strmenum.c
  yori/lib/clip.c
  yori/lib/dyld_net.c
  yori/lib/condrv.c
  yori/lib/bytebuf.c
  yori/lib/printfa.c
  yori/lib/yoriver.inc
  yori/lib/path.c
  yori/lib/filecomp.c
  yori/lib/dyld.c
  yori/lib/hash.c
  yori/lib/ep_yori.c
  yori/lib/cvtcons.c
  yori/lib/malloc.c
  yori/lib/license.c
  yori/lib/fileinfo.c
  yori/lib/dyld_usr.c
  yori/lib/debug.c
  yori/lib/select.c
  yori/lib/process.c
  yori/lib/hexdump.c
  yori/lib/cvtrtf.c
  yori/lib/osver.c
  yori/lib/vt.c
  yori/lib/curdir.c
  yori/lib/cshot.c
  yori/lib/temp.c
  yori/lib/util.c
  yori/lib/fullpath.c
  yori/lib/scut.c
  yori/lib/lineread.c
  yori/lib/filefilt.c
  yori/lib/dyld_adv.c
  yori/lib/printf.c
  yori/lib/builtin.c
  yori/lib/iconv.c
  yori/lib/yoricmpt.h
  yori/lib/cvthtml.c
  yori/lib/Makefile
  yori/lib/yorilib.h
  yori/lib/yoricall.h
  yori/lib/dblclk.c
  yori/lib/cancel.c
  yori/lib/dyld_cab.c
  yori/lib/list.c
  yori/lib/fileenum.c
  yori/lib/strarray.c
  yori/lib/string.c
  yori/lib/yoripch.h
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
