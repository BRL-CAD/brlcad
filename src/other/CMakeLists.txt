#-----------------------------------------------------------------------
# Local Copies of External Libraries
#
# BRL-CAD depends on a variety of external libraries and tools -
# rather than fail if those requirements are not satisfied, we build
# local copies at need.
#
# There are three overall approaches to the handling of these
# dependencies:
#
# 1.  Auto - detect system libraries and use them if suitable,
#     otherwise build and use the local copy.  This is the default
#     approach.
#
# 2.  Bundled - regardless of system conditions, build and use all
#     bundled libraries.
#
# 3.  System - fail to build if the system libraries do not satisfy
#     requirements.  This is primarily useful for distributions that
#     want to ensure packages are using external libraries.
#
# In addition to the broad toplevel control, individual libraries can
# also be overridden - for example, if the toplevel setting is for
# Bundled libs, it is still possible to request a system library in
# individual cases.
#
#-----------------------------------------------------------------------

# Most third party items have a list calling out files for distcheck -
# these are stored in files in the dlists directory.  Ignore that
# directory for distcheck
file(GLOB dlists "*.dist")
foreach(ITEM ${dlists})
  get_filename_component(dlist ${ITEM} NAME)
  CMAKEFILES(${dlist})
endforeach(ITEM ${dlists})

# By default, changes in src/other files won't trigger a rebuild.  Setting this
# to 1 at configure time changes that, but at the expense of running the build
# step every time.  This may be fast if there is nothing to update in the
# project, but it will be slower than skipping the step completely.
if (NOT DEFINED EXTERNAL_BUILD_UPDATE)
  set(EXTERNAL_BUILD_UPDATE 0)
endif (NOT DEFINED EXTERNAL_BUILD_UPDATE)

# Use this variable to key both CMake messages and tools like rpath_replace
if(NOT DEFINED EXTPROJ_VERBOSE)
  set(EXTPROJ_VERBOSE 0)
endif(NOT DEFINED EXTPROJ_VERBOSE)

# ExternalProject_Add uses configure_file internally - we don't want warnings
# about those files, so suppress our warning system
set(SUPPRESS_GENERATED_TAG TRUE)

# Note- make sure CMAKE_BINARY_DIR and CMAKE_INSTALL_PREFIX are finalized BEFORE including this file!
include(ExternalProject_Target)

# External build outputs can be verbose - capture them to files instead of
# outputting to console
#set(LOG_OPTS
#  LOG_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/ExternalProject_logs"
#  LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON
#  LOG_MERGED_STDOUTERR ON LOG_OUTPUT_ON_FAILURE ON
#  )

set(LOG_OPTS)

find_package(LEMON)
find_package(RE2C)
find_package(PERPLEX)
find_package(REGEX)
find_package(ZLIB)
find_package(PNG)
find_package(NETPBM)
find_package(UTAHRLE)
find_package(OPENNURBS)
find_package(SPSR)
find_package(STEPCODE)
find_package(VDS)
find_package(GDIAM)
find_package(POLY2TRI)

# For testing set(BRLCAD_ENABLE_BINARY_ATTRIBUTES ON)
if (BRLCAD_ENABLE_BINARY_ATTRIBUTES)
  find_package(BSON)
endif (BRLCAD_ENABLE_BINARY_ATTRIBUTES)

if (BRLCAD_ENABLE_GDAL)
  find_package(PROJ)
  find_package(GDAL)
endif (BRLCAD_ENABLE_GDAL)

###############################################################################
#                                 Tcl/Tk
###############################################################################

set(tcl_DESCRIPTION "
Option for enabling and disabling compilation of the Tcl library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")

THIRD_PARTY(tcl TCL tcl tcl_DESCRIPTION ALIASES ENABLE_TCL REQUIRED_VARS BRLCAD_LEVEL2)

if (NOT ${CMAKE_PROJECT_NAME}_TCL_BUILD)
  # Trying to manage all the component pieces of a Tcl install (mixing and
  # matching system and local) is way too complex for too little benefit.
  # For the most part this is an all or nothing arrangement - either the
  # system version is adequate, or we're on our own.  If we think we have a
  # system version, check to see if it has everything we need.  If not, we're
  # rolling our own.
  #
  # (TODO - Might make an exception for tkhtml since that's not very standard
  # in the broader ecosystem...
endif (NOT ${CMAKE_PROJECT_NAME}_TCL_BUILD)

# If we're building, build it all
if (${CMAKE_PROJECT_NAME}_TCL_BUILD)

  ############
  #    Tcl
  ############

  set(TCL_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/TCL_BLD-prefix/src/TCL_BLD")
  set(TCL_MINOR_VERSION 6)
  set(ITCL_VERSION 4.2.0)
  CONFIG_H_APPEND(BRLCAD "#define ITCL_VERSION \"${ITCL_VERSION}\"\n")

  # We need to set internal Tcl variables to the final install paths, not the intermediate install paths that
  # Tcl's own build will think are the final paths.  Rather than attempt build system trickery we simply
  # hard set the values in the source files by rewriting them.
  configure_file(${BRLCAD_CMAKE_DIR}/tcl_replace.cxx.in ${CMAKE_CURRENT_BINARY_DIR}/tcl_replace.cxx)
  add_executable(tcl_replace ${CMAKE_CURRENT_BINARY_DIR}/tcl_replace.cxx)

  if (NOT MSVC)

    set(TCL_PATCH_FILES "${TCL_SRC_DIR}/unix/configure" "${TCL_SRC_DIR}/macosx/configure" "${TCL_SRC_DIR}/unix/tcl.m4")
    set(TCL_REWORK_FILES "${TCL_SRC_DIR}/unix/tclUnixInit.c" "${TCL_SRC_DIR}/generic/tclPkgConfig.c")

    ExternalProject_Add(TCL_BLD
      URL "${CMAKE_CURRENT_SOURCE_DIR}/tcl"
      BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
      PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${TCL_PATCH_FILES}
      COMMAND tcl_replace ${TCL_REWORK_FILES}
      CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_BINARY_DIR}/${LIB_DIR} ${TCL_SRC_DIR}/unix/configure --prefix=${CMAKE_BINARY_DIR}
      BUILD_COMMAND make -j${pcnt}
      INSTALL_COMMAND make install
      DEPENDS ${ZLIB_LIBRARIES} tcl_replace
      )
  else (NOT MSVC)

    ExternalProject_Add(TCL_BLD
      SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tcl"
      BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
      CONFIGURE_COMMAND ""
      BINARY_DIR ${TCL_SRC_DIR}/win
      BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR}
      INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR}
      )

  endif (NOT MSVC)

  ExternalProject_Target(tcl TCL_BLD
    OUTPUT_FILE libtcl8.${TCL_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE libtclstub8.${TCL_MINOR_VERSION}${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )
  ExternalProject_Target(itcl TCL_BLD
    SUBDIR itcl${ITCL_VERSION}
    OUTPUT_FILE libitcl${ITCL_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE libitclstub${ITCL_VERSION}${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_Target(tclsh TCL_BLD
    OUTPUT_FILE tclsh8.${TCL_MINOR_VERSION}${CMAKE_EXECUTABLE_SUFFIX}
    RPATH EXEC
    )
  ExternalProject_ByProducts(TCL_BLD ${LIB_DIR} FIXPATH
    tclConfig.sh
    tclooConfig.sh
    )
  ExternalProject_ByProducts(TCL_BLD ${LIB_DIR}/tcl8.${TCL_MINOR_VERSION}
    auto.tcl
    clock.tcl
    encoding/ascii.enc
    encoding/big5.enc
    encoding/cp1250.enc
    encoding/cp1251.enc
    encoding/cp1252.enc
    encoding/cp1253.enc
    encoding/cp1254.enc
    encoding/cp1255.enc
    encoding/cp1256.enc
    encoding/cp1257.enc
    encoding/cp1258.enc
    encoding/cp437.enc
    encoding/cp737.enc
    encoding/cp775.enc
    encoding/cp850.enc
    encoding/cp852.enc
    encoding/cp855.enc
    encoding/cp857.enc
    encoding/cp860.enc
    encoding/cp861.enc
    encoding/cp862.enc
    encoding/cp863.enc
    encoding/cp864.enc
    encoding/cp865.enc
    encoding/cp866.enc
    encoding/cp869.enc
    encoding/cp874.enc
    encoding/cp932.enc
    encoding/cp936.enc
    encoding/cp949.enc
    encoding/cp950.enc
    encoding/dingbats.enc
    encoding/ebcdic.enc
    encoding/euc-cn.enc
    encoding/euc-jp.enc
    encoding/euc-kr.enc
    encoding/gb12345.enc
    encoding/gb1988.enc
    encoding/gb2312-raw.enc
    encoding/gb2312.enc
    encoding/iso2022-jp.enc
    encoding/iso2022-kr.enc
    encoding/iso2022.enc
    encoding/iso8859-1.enc
    encoding/iso8859-10.enc
    encoding/iso8859-13.enc
    encoding/iso8859-14.enc
    encoding/iso8859-15.enc
    encoding/iso8859-16.enc
    encoding/iso8859-2.enc
    encoding/iso8859-3.enc
    encoding/iso8859-4.enc
    encoding/iso8859-5.enc
    encoding/iso8859-6.enc
    encoding/iso8859-7.enc
    encoding/iso8859-8.enc
    encoding/iso8859-9.enc
    encoding/jis0201.enc
    encoding/jis0208.enc
    encoding/jis0212.enc
    encoding/koi8-r.enc
    encoding/koi8-u.enc
    encoding/ksc5601.enc
    encoding/macCentEuro.enc
    encoding/macCroatian.enc
    encoding/macCyrillic.enc
    encoding/macDingbats.enc
    encoding/macGreek.enc
    encoding/macIceland.enc
    encoding/macJapan.enc
    encoding/macRoman.enc
    encoding/macRomania.enc
    encoding/macThai.enc
    encoding/macTurkish.enc
    encoding/macUkraine.enc
    encoding/shiftjis.enc
    encoding/symbol.enc
    encoding/tis-620.enc
    history.tcl
    http1.0/http.tcl
    http1.0/pkgIndex.tcl
    init.tcl
    msgs/af.msg
    msgs/af_za.msg
    msgs/ar.msg
    msgs/ar_in.msg
    msgs/ar_jo.msg
    msgs/ar_lb.msg
    msgs/ar_sy.msg
    msgs/be.msg
    msgs/bg.msg
    msgs/bn.msg
    msgs/bn_in.msg
    msgs/ca.msg
    msgs/cs.msg
    msgs/da.msg
    msgs/de.msg
    msgs/de_at.msg
    msgs/de_be.msg
    msgs/el.msg
    msgs/en_au.msg
    msgs/en_be.msg
    msgs/en_bw.msg
    msgs/en_ca.msg
    msgs/en_gb.msg
    msgs/en_hk.msg
    msgs/en_ie.msg
    msgs/en_in.msg
    msgs/en_nz.msg
    msgs/en_ph.msg
    msgs/en_sg.msg
    msgs/en_za.msg
    msgs/en_zw.msg
    msgs/eo.msg
    msgs/es.msg
    msgs/es_ar.msg
    msgs/es_bo.msg
    msgs/es_cl.msg
    msgs/es_co.msg
    msgs/es_cr.msg
    msgs/es_do.msg
    msgs/es_ec.msg
    msgs/es_gt.msg
    msgs/es_hn.msg
    msgs/es_mx.msg
    msgs/es_ni.msg
    msgs/es_pa.msg
    msgs/es_pe.msg
    msgs/es_pr.msg
    msgs/es_py.msg
    msgs/es_sv.msg
    msgs/es_uy.msg
    msgs/es_ve.msg
    msgs/et.msg
    msgs/eu.msg
    msgs/eu_es.msg
    msgs/fa.msg
    msgs/fa_in.msg
    msgs/fa_ir.msg
    msgs/fi.msg
    msgs/fo.msg
    msgs/fo_fo.msg
    msgs/fr.msg
    msgs/fr_be.msg
    msgs/fr_ca.msg
    msgs/fr_ch.msg
    msgs/ga.msg
    msgs/ga_ie.msg
    msgs/gl.msg
    msgs/gl_es.msg
    msgs/gv.msg
    msgs/gv_gb.msg
    msgs/he.msg
    msgs/hi.msg
    msgs/hi_in.msg
    msgs/hr.msg
    msgs/hu.msg
    msgs/id.msg
    msgs/id_id.msg
    msgs/is.msg
    msgs/it.msg
    msgs/it_ch.msg
    msgs/ja.msg
    msgs/kl.msg
    msgs/kl_gl.msg
    msgs/ko.msg
    msgs/ko_kr.msg
    msgs/kok.msg
    msgs/kok_in.msg
    msgs/kw.msg
    msgs/kw_gb.msg
    msgs/lt.msg
    msgs/lv.msg
    msgs/mk.msg
    msgs/mr.msg
    msgs/mr_in.msg
    msgs/ms.msg
    msgs/ms_my.msg
    msgs/mt.msg
    msgs/nb.msg
    msgs/nl.msg
    msgs/nl_be.msg
    msgs/nn.msg
    msgs/pl.msg
    msgs/pt.msg
    msgs/pt_br.msg
    msgs/ro.msg
    msgs/ru.msg
    msgs/ru_ua.msg
    msgs/sh.msg
    msgs/sk.msg
    msgs/sl.msg
    msgs/sq.msg
    msgs/sr.msg
    msgs/sv.msg
    msgs/sw.msg
    msgs/ta.msg
    msgs/ta_in.msg
    msgs/te.msg
    msgs/te_in.msg
    msgs/th.msg
    msgs/tr.msg
    msgs/uk.msg
    msgs/vi.msg
    msgs/zh.msg
    msgs/zh_cn.msg
    msgs/zh_hk.msg
    msgs/zh_sg.msg
    msgs/zh_tw.msg
    opt0.4/optparse.tcl
    opt0.4/pkgIndex.tcl
    package.tcl
    parray.tcl
    safe.tcl
    tclAppInit.c
    tclIndex
    tm.tcl
    word.tcl
    )
  ExternalProject_ByProducts(TCL_BLD ${INCLUDE_DIR}
    tclDecls.h
    tcl.h
    tclOODecls.h
    tclOO.h
    tclPlatDecls.h
    tclTomMathDecls.h
    tclTomMath.h
    )

  set(TCL_LIBRARIES tcl CACHE STRING "Building bundled tcl" FORCE)
  set(TCL_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing tcl headers." FORCE)

  set(ITCL_LIBRARIES itcl CACHE STRING "Building bundled itcl" FORCE)
  set(ITCL_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing itcl headers." FORCE)



  SetTargetFolder(TCL_BLD "Third Party Libraries")
  SetTargetFolder(tcl "Third Party Libraries")

  if(BRLCAD_ENABLE_TK)

    set(HAVE_TK 1 CACHE STRING "C level Tk flag" FORCE)
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_TK\n")

    ############
    #    Tk
    ############


    set(TK_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/TK_BLD-prefix/src/TK_BLD")
    set(TK_MINOR_VERSION 6)

    if (NOT MSVC)

      set(TK_PATCH_FILES "${TK_SRC_DIR}/unix/configure" "${TK_SRC_DIR}/macosx/configure" "${TK_SRC_DIR}/unix/tcl.m4")

      ExternalProject_Add(TK_BLD
	URL "${CMAKE_CURRENT_SOURCE_DIR}/tk"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${TK_PATCH_FILES}
	CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_BINARY_DIR}/${LIB_DIR} ${TK_SRC_DIR}/unix/configure --prefix=${CMAKE_BINARY_DIR} --with-tcl=${CMAKE_BINARY_DIR}/${LIB_DIR} --disable-xft --enable-64bit --enable-rpath
	BUILD_COMMAND make -j${pcnt}
	INSTALL_COMMAND make install
	DEPENDS TCL_BLD ${PNG_LIBRARIES}
	)
    else (NOT MSVC)

      ExternalProject_Add(TK_BLD
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tk"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	CONFIGURE_COMMAND ""
	BINARY_DIR ${TK_SRC_DIR}/win
	BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR}
	INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR}
	DEPENDS TCL_BLD ${PNG_LIBRARIES}
	)

    endif (NOT MSVC)

    ExternalProject_Target(tk TK_BLD
      OUTPUT_FILE libtk8.${TK_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
      STATIC_OUTPUT_FILE libtkstub8.${TK_MINOR_VERSION}${CMAKE_STATIC_LIBRARY_SUFFIX}
      RPATH
      )
    ExternalProject_Target(wish TK_BLD
      OUTPUT_FILE wish8.${TK_MINOR_VERSION}${CMAKE_EXECUTABLE_SUFFIX}
      RPATH EXEC
      )

    ExternalProject_ByProducts(TK_BLD ${LIB_DIR} FIXPATH
      tkConfig.sh
      )

    ExternalProject_ByProducts(TK_BLD ${LIB_DIR}/tk8.${TK_MINOR_VERSION}
      bgerror.tcl
      button.tcl
      choosedir.tcl
      clrpick.tcl
      comdlg.tcl
      console.tcl
      demos/README
      demos/anilabel.tcl
      demos/aniwave.tcl
      demos/arrow.tcl
      demos/bind.tcl
      demos/bitmap.tcl
      demos/browse
      demos/button.tcl
      demos/check.tcl
      demos/clrpick.tcl
      demos/colors.tcl
      demos/combo.tcl
      demos/cscroll.tcl
      demos/ctext.tcl
      demos/dialog1.tcl
      demos/dialog2.tcl
      demos/en.msg
      demos/entry1.tcl
      demos/entry2.tcl
      demos/entry3.tcl
      demos/filebox.tcl
      demos/floor.tcl
      demos/fontchoose.tcl
      demos/form.tcl
      demos/goldberg.tcl
      demos/hello
      demos/hscale.tcl
      demos/icon.tcl
      demos/image1.tcl
      demos/image2.tcl
      demos/images/earth.gif
      demos/images/earthmenu.png
      demos/images/earthris.gif
      demos/images/flagdown.xbm
      demos/images/flagup.xbm
      demos/images/gray25.xbm
      demos/images/letters.xbm
      demos/images/noletter.xbm
      demos/images/ouster.png
      demos/images/pattern.xbm
      demos/images/tcllogo.gif
      demos/images/teapot.ppm
      demos/items.tcl
      demos/ixset
      demos/knightstour.tcl
      demos/label.tcl
      demos/labelframe.tcl
      demos/license.terms
      demos/mclist.tcl
      demos/menu.tcl
      demos/menubu.tcl
      demos/msgbox.tcl
      demos/nl.msg
      demos/paned1.tcl
      demos/paned2.tcl
      demos/pendulum.tcl
      demos/plot.tcl
      demos/puzzle.tcl
      demos/radio.tcl
      demos/rmt
      demos/rolodex
      demos/ruler.tcl
      demos/sayings.tcl
      demos/search.tcl
      demos/spin.tcl
      demos/states.tcl
      demos/style.tcl
      demos/tclIndex
      demos/tcolor
      demos/text.tcl
      demos/textpeer.tcl
      demos/timer
      demos/toolbar.tcl
      demos/tree.tcl
      demos/ttkbut.tcl
      demos/ttkmenu.tcl
      demos/ttknote.tcl
      demos/ttkpane.tcl
      demos/ttkprogress.tcl
      demos/ttkscale.tcl
      demos/twind.tcl
      demos/unicodeout.tcl
      demos/vscale.tcl
      demos/widget
      dialog.tcl
      entry.tcl
      focus.tcl
      fontchooser.tcl
      iconlist.tcl
      icons.tcl
      images/README
      images/logo.eps
      images/logo100.gif
      images/logo64.gif
      images/logoLarge.gif
      images/logoMed.gif
      images/pwrdLogo.eps
      images/pwrdLogo100.gif
      images/pwrdLogo150.gif
      images/pwrdLogo175.gif
      images/pwrdLogo200.gif
      images/pwrdLogo75.gif
      images/tai-ku.gif
      listbox.tcl
      megawidget.tcl
      menu.tcl
      mkpsenc.tcl
      msgbox.tcl
      msgs/cs.msg
      msgs/da.msg
      msgs/de.msg
      msgs/el.msg
      msgs/en.msg
      msgs/en_gb.msg
      msgs/eo.msg
      msgs/es.msg
      msgs/fr.msg
      msgs/hu.msg
      msgs/it.msg
      msgs/nl.msg
      msgs/pl.msg
      msgs/pt.msg
      msgs/ru.msg
      msgs/sv.msg
      obsolete.tcl
      optMenu.tcl
      palette.tcl
      panedwindow.tcl
      pkgIndex.tcl
      safetk.tcl
      scale.tcl
      scrlbar.tcl
      spinbox.tcl
      tclIndex
      tearoff.tcl
      text.tcl
      tk.tcl
      tkAppInit.c
      tkfbox.tcl
      ttk/altTheme.tcl
      ttk/aquaTheme.tcl
      ttk/button.tcl
      ttk/clamTheme.tcl
      ttk/classicTheme.tcl
      ttk/combobox.tcl
      ttk/cursors.tcl
      ttk/defaults.tcl
      ttk/entry.tcl
      ttk/fonts.tcl
      ttk/menubutton.tcl
      ttk/notebook.tcl
      ttk/panedwindow.tcl
      ttk/progress.tcl
      ttk/scale.tcl
      ttk/scrollbar.tcl
      ttk/sizegrip.tcl
      ttk/spinbox.tcl
      ttk/treeview.tcl
      ttk/ttk.tcl
      ttk/utils.tcl
      ttk/vistaTheme.tcl
      ttk/winTheme.tcl
      ttk/xpTheme.tcl
      unsupported.tcl
      xmfbox.tcl
      )
    ExternalProject_ByProducts(TK_BLD ${INCLUDE_DIR}
      tkDecls.h
      tk.h
      tkPlatDecls.h
      )

    set(TK_LIBRARIES tk CACHE STRING "Building bundled tk" FORCE)
    set(TK_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing tcl headers." FORCE)

    SetTargetFolder(TK_BLD "Third Party Libraries")
    SetTargetFolder(tk "Third Party Libraries")

    ############
    #    Itk
    ############

    if (NOT MSVC)
      # TODO - looks like this will require a CMake build after all - the Windows build included
      # with the vanilla tarball doesn't appear to work...

      set(ITK_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/ITK_BLD-prefix/src/ITK_BLD")
      set(ITK_VERSION 4.1.0)

      set(ITK_PATCH_FILES "${ITK_SRC_DIR}/configure" "${ITK_SRC_DIR}/tclconfig/tcl.m4")

      ExternalProject_Add(ITK_BLD
	URL "${CMAKE_CURRENT_SOURCE_DIR}/itk"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${ITK_PATCH_FILES}
	CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_BINARY_DIR}/${LIB_DIR} ${ITK_SRC_DIR}/configure --prefix=${CMAKE_BINARY_DIR} --with-tcl=${CMAKE_BINARY_DIR}/${LIB_DIR} --with-tk=${CMAKE_BINARY_DIR}/${LIB_DIR} --with-itcl=${CMAKE_BINARY_DIR}/lib/itcl${ITCL_VERSION}
	BUILD_COMMAND make -j${pcnt}
	INSTALL_COMMAND make install
	DEPENDS TK_BLD TCL_BLD
	)
    else (NOT MSVC)

      ExternalProject_Add(ITK_BLD
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tk"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	CONFIGURE_COMMAND ""
	BINARY_DIR ${ITK_SRC_DIR}/win
	BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR} TKDIR=${TK_SRC_DIR}
	INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR} TKDIR=${TK_SRC_DIR}
	DEPENDS TK_BLD TCL_BLD
	)

    endif (NOT MSVC)


    ExternalProject_Target(itk ITK_BLD
      SUBDIR itk${ITK_VERSION}
      OUTPUT_FILE libitk${ITK_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
      STATIC_OUTPUT_FILE libitkstub${ITCL_VERSION}${CMAKE_STATIC_LIBRARY_SUFFIX}
      )

    ExternalProject_ByProducts(ITK_BLD ${INCLUDE_DIR}
      itk.h
      itkInt.h
      )

    ExternalProject_ByProducts(ITK_BLD ${LIB_DIR}
      itk${ITK_VERSION}/itk.tcl
      itk${ITK_VERSION}/Widget.itk
      itk${ITK_VERSION}/Archetype.itk
      itk${ITK_VERSION}/Toplevel.itk
      itk${ITK_VERSION}/tclIndex
      )
    ExternalProject_ByProducts(ITK_BLD ${LIB_DIR}
      itk${ITK_VERSION}/pkgIndex.tcl
      FIXPATH
      )

    set(ITK_LIBRARIES itk CACHE STRING "Building bundled itk" FORCE)
    set(ITK_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing itk headers." FORCE)

    SetTargetFolder(ITK_BLD "Third Party Libraries")
    SetTargetFolder(itk "Third Party Libraries")

    ################
    #    IWidgets
    ################

    if (NOT MSVC)

      set(IWIDGETS_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/IWIDGETS_BLD-prefix/src/IWIDGETS_BLD")
      set(IWIDGETS_VERSION 4.1.1)
      CONFIG_H_APPEND(BRLCAD "#define IWIDGETS_VERSION \"${IWIDGETS_VERSION}\"\n")

      set(IWIDGETS_PATCH_FILES "${IWIDGETS_SRC_DIR}/configure" "${IWIDGETS_SRC_DIR}/tclconfig/tcl.m4")

      ExternalProject_Add(IWIDGETS_BLD
	URL "${CMAKE_CURRENT_SOURCE_DIR}/iwidgets"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${IWIDGETS_PATCH_FILES}
	CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_BINARY_DIR}/${LIB_DIR} ${IWIDGETS_SRC_DIR}/configure --prefix=${CMAKE_BINARY_DIR} --with-tcl=${CMAKE_BINARY_DIR}/${LIB_DIR} --with-tk=${CMAKE_BINARY_DIR}/${LIB_DIR} --with-itcl=${CMAKE_BINARY_DIR}/lib/itcl${ITCL_VERSION} --with-itk=${ITK_SRC_DIR}
	BUILD_COMMAND make -j${pcnt}
	INSTALL_COMMAND make install
	DEPENDS TK_BLD TCL_BLD
	)
    else (NOT MSVC)

      ExternalProject_Add(IWIDGETS_BLD
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tk"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	CONFIGURE_COMMAND ""
	BINARY_DIR ${IWIDGETS_SRC_DIR}/win
	BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR} TKDIR=${TK_SRC_DIR}
	INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR} TKDIR=${TK_SRC_DIR}
	DEPENDS TK_BLD TCL_BLD
	)

    endif (NOT MSVC)

    ExternalProject_ByProducts(IWIDGETS_BLD ${LIB_DIR}/iwidgets${IWIDGETS_VERSION}
      demos/buttonbox
      demos/calendar
      demos/canvasprintbox
      demos/canvasprintdialog
      demos/catalog
      demos/checkbox
      demos/combobox
      demos/dateentry
      demos/datefield
      demos/demo.html
      demos/dialog
      demos/dialogshell
      demos/disjointlistbox
      demos/entryfield
      demos/extbutton
      demos/extfileselectionbox
      demos/extfileselectiondialog
      demos/feedback
      demos/fileselectionbox
      demos/fileselectiondialog
      demos/finddialog
      demos/hierarchy
      demos/html/buttonbox.n.html
      demos/html/calendar.n.html
      demos/html/canvasprintbox.n.html
      demos/html/canvasprintdialog.n.html
      demos/html/checkbox.n.html
      demos/html/combobox.n.html
      demos/html/dateentry.n.html
      demos/html/datefield.n.html
      demos/html/dialog.n.html
      demos/html/dialogshell.n.html
      demos/html/disjointlistbox.n.html
      demos/html/entryfield.n.html
      demos/html/extbutton.n.html
      demos/html/extfileselectionbox.n.html
      demos/html/extfileselectiondialog.n.html
      demos/html/feedback.n.html
      demos/html/fileselectionbox.n.html
      demos/html/fileselectiondialog.n.html
      demos/html/finddialog.n.html
      demos/html/hierarchy.n.html
      demos/html/hyperhelp.n.html
      demos/html/iwidgets4.0.0UserCmds.html
      demos/html/labeledframe.n.html
      demos/html/labeledwidget.n.html
      demos/html/mainwindow.n.html
      demos/html/menubar.n.html
      demos/html/messagebox.n.html
      demos/html/messagedialog.n.html
      demos/html/notebook.n.html
      demos/html/optionmenu.n.html
      demos/html/panedwindow.n.html
      demos/html/promptdialog.n.html
      demos/html/pushbutton.n.html
      demos/html/radiobox.n.html
      demos/html/scopedobject.n.html
      demos/html/scrolledcanvas.n.html
      demos/html/scrolledframe.n.html
      demos/html/scrolledhtml.n.html
      demos/html/scrolledlistbox.n.html
      demos/html/scrolledtext.n.html
      demos/html/selectionbox.n.html
      demos/html/selectiondialog.n.html
      demos/html/shell.n.html
      demos/html/spindate.n.html
      demos/html/spinint.n.html
      demos/html/spinner.n.html
      demos/html/spintime.n.html
      demos/html/tabnotebook.n.html
      demos/html/tabset.n.html
      demos/html/timeentry.n.html
      demos/html/timefield.n.html
      demos/html/toolbar.n.html
      demos/html/watch.n.html
      demos/hyperhelp
      demos/images/box.xbm
      demos/images/clear.gif
      demos/images/close.gif
      demos/images/copy.gif
      demos/images/cut.gif
      demos/images/exit.gif
      demos/images/find.gif
      demos/images/help.gif
      demos/images/line.xbm
      demos/images/mag.gif
      demos/images/new.gif
      demos/images/open.gif
      demos/images/oval.xbm
      demos/images/paste.gif
      demos/images/points.xbm
      demos/images/poly.gif
      demos/images/print.gif
      demos/images/ruler.gif
      demos/images/save.gif
      demos/images/select.gif
      demos/images/text.xbm
      demos/iwidgets.gif
      demos/labeledframe
      demos/labeledwidget
      demos/mainwindow
      demos/menubar
      demos/messagebox
      demos/messagedialog
      demos/notebook
      demos/optionmenu
      demos/panedwindow
      demos/promptdialog
      demos/pushbutton
      demos/radiobox
      demos/scopedobject
      demos/scrolledcanvas
      demos/scrolledframe
      demos/scrolledhtml
      demos/scrolledlistbox
      demos/scrolledtext
      demos/selectionbox
      demos/selectiondialog
      demos/shell
      demos/spindate
      demos/spinint
      demos/spinner
      demos/spintime
      demos/tabnotebook
      demos/tabset
      demos/timeentry
      demos/timefield
      demos/toolbar
      demos/watch
      iwidgets.tcl
      license.terms
      pkgIndex.tcl
      scripts/buttonbox.itk
      scripts/calendar.itk
      scripts/canvasprintbox.itk
      scripts/canvasprintdialog.itk
      scripts/checkbox.itk
      scripts/colors.itcl
      scripts/combobox.itk
      scripts/dateentry.itk
      scripts/datefield.itk
      scripts/dialog.itk
      scripts/dialogshell.itk
      scripts/disjointlistbox.itk
      scripts/entryfield.itk
      scripts/extbutton.itk
      scripts/extfileselectionbox.itk
      scripts/extfileselectiondialog.itk
      scripts/feedback.itk
      scripts/fileselectionbox.itk
      scripts/fileselectiondialog.itk
      scripts/finddialog.itk
      scripts/hierarchy.itk
      scripts/hyperhelp.itk
      scripts/labeledframe.itk
      scripts/labeledwidget.itk
      scripts/mainwindow.itk
      scripts/menubar.itk
      scripts/messagebox.itk
      scripts/messagedialog.itk
      scripts/notebook.itk
      scripts/optionmenu.itk
      scripts/pane.itk
      scripts/panedwindow.itk
      scripts/promptdialog.itk
      scripts/pushbutton.itk
      scripts/radiobox.itk
      scripts/regexpfield.itk
      scripts/roman.itcl
      scripts/scopedobject.itcl
      scripts/scrolledcanvas.itk
      scripts/scrolledframe.itk
      scripts/scrolledhtml.itk
      scripts/scrolledlistbox.itk
      scripts/scrolledtext.itk
      scripts/scrolledwidget.itk
      scripts/selectionbox.itk
      scripts/selectiondialog.itk
      scripts/shell.itk
      scripts/spindate.itk
      scripts/spinint.itk
      scripts/spinner.itk
      scripts/spintime.itk
      scripts/tabnotebook.itk
      scripts/tabset.itk
      scripts/tclIndex
      scripts/timeentry.itk
      scripts/timefield.itk
      scripts/toolbar.itk
      scripts/unknownimage.gif
      scripts/watch.itk
      )

    SetTargetFolder(IWIDGETS_BLD "Third Party Libraries")
    SetTargetFolder(iwidgets "Third Party Libraries")

    #############
    #   TkTable
    #############

    if (NOT MSVC)
      # TODO - looks like this will require a CMake build after all - the Windows build included
      # with the vanilla tarball is pretty hopeless...

      set(TKTABLE_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/TKTABLE_BLD-prefix/src/TKTABLE_BLD")
      set(TKTABLE_VERSION 2.10)

      # We need to set internal Tcl variables to the final install paths, not the intermediate install paths that
      # Tcl's own build will think are the final paths.  Rather than attempt build system trickery we simply
      # hard set the values in the source files by rewriting them.
      configure_file(${BRLCAD_CMAKE_DIR}/tcl_replace.cxx.in ${CMAKE_CURRENT_BINARY_DIR}/tktable_replace.cxx)
      add_executable(tktable_replace ${CMAKE_CURRENT_BINARY_DIR}/tktable_replace.cxx)

      set(TKTABLE_PATCH_FILES "${TKTABLE_SRC_DIR}/configure" "${TKTABLE_SRC_DIR}/tclconfig/tcl.m4")
      set(TKTABLE_REWORK_FILES "${TKTABLE_SRC_DIR}/generic/tkTableInitScript.h")

      ExternalProject_Add(TKTABLE_BLD
	URL "${CMAKE_CURRENT_SOURCE_DIR}/tktable"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${TKTABLE_PATCH_FILES}
	COMMAND tktable_replace ${TKTABLE_REWORK_FILES}
	CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_BINARY_DIR}/${LIB_DIR} ${TKTABLE_SRC_DIR}/configure --prefix=${CMAKE_BINARY_DIR} --with-tcl=${CMAKE_BINARY_DIR}/${LIB_DIR} --with-tk=${CMAKE_BINARY_DIR}/${LIB_DIR}
	BUILD_COMMAND make -j${pcnt}
	INSTALL_COMMAND make install
	DEPENDS TK_BLD TCL_BLD
	)
    else (NOT MSVC)

      ExternalProject_Add(TKTABLE_BLD
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tk"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	CONFIGURE_COMMAND ""
	BINARY_DIR ${TKTABLE_SRC_DIR}/win
	BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR} TKDIR=${TK_SRC_DIR}
	INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR} TKDIR=${TK_SRC_DIR}
	DEPENDS TK_BLD TCL_BLD
	)

    endif (NOT MSVC)


    ExternalProject_Target(tktable TKTABLE_BLD
      SUBDIR Tktable${TKTABLE_VERSION}
      OUTPUT_FILE libTktable${TKTABLE_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
      )
    ExternalProject_ByProducts(TKTABLE_BLD ${LIB_DIR}
      Tktable${TKTABLE_VERSION}/README.txt
      Tktable${TKTABLE_VERSION}/html/tkTable.html
      Tktable${TKTABLE_VERSION}/tktable.py
      Tktable${TKTABLE_VERSION}/license.txt
      )
    ExternalProject_ByProducts(TKTABLE_BLD ${LIB_DIR}
      Tktable${TKTABLE_VERSION}/tkTable.tcl
      FIXPATH
      )

    SetTargetFolder(TKTABLE_BLD "Third Party Libraries")
    SetTargetFolder(tktable "Third Party Libraries")

    #############
    #   Tkhtml
    #############

    set(TKHTML_VERSION 3.0)

    cmake_set_rpath(SUFFIX "Tkhtml${TKHTML_VERSION}")
    message("CMAKE_BUILD_RPATH: ${CMAKE_BUILD_RPATH}")
    ExternalProject_Add(TKHTML_BLD
      SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tkhtml
      CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR}
      -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
      -DTCL_INCLUDE_DIRS=${CMAKE_BINARY_DIR}/${INCLUDE_DIR}
      -DTCL_INCLUDE_PATH=${CMAKE_BINARY_DIR}/${INCLUDE_DIR}
      -DTCL_LIBRARY=${CMAKE_BINARY_DIR}/${LIB_DIR}/libtcl8.${TCL_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
      -DTCL_STUB_LIBRARY=${CMAKE_BINARY_DIR}/${LIB_DIR}/libtclstub8.${TCL_MINOR_VERSION}${CMAKE_STATIC_LIBRARY_SUFFIX}
      -DTCL_TCLSH=${CMAKE_BINARY_DIR}/${BIN_DIR}/tclsh8.${TCL_MINOR_VERSION}${CMAKE_EXECUTABLE_SUFFIX}
      -DTCL_TCLSH_EXECUTABLE=${CMAKE_BINARY_DIR}/${BIN_DIR}/tclsh8.${TCL_MINOR_VERSION}${CMAKE_EXECUTABLE_SUFFIX}
      -DTCL_TK_CONF_PREFIX=${CMAKE_BINARY_DIR}/${LIB_DIR}
      -DTCL_TK_LIBRARY=${CMAKE_BINARY_DIR}/${LIB_DIR}/libtk8.${TCL_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
      -DTCL_TK_STUB_LIBRARY=${CMAKE_BINARY_DIR}/${LIB_DIR}/libtkstub8.${TCL_MINOR_VERSION}${CMAKE_STATIC_LIBRARY_SUFFIX}
      -DTCL_WISH_EXECUTABLE=${CMAKE_BINARY_DIR}/${BIN_DIR}/wish8.${TCL_MINOR_VERSION}${CMAKE_EXECUTABLE_SUFFIX}
      -DTK_INCLUDE_PATH=${CMAKE_BINARY_DIR}/${INCLUDE_DIR}
      -DTK_LIBRARY=${CMAKE_BINARY_DIR}/${LIB_DIR}/libtk8.${TCL_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
      DEPENDS TCL_BLD TK_BLD
      )
    ExternalProject_Target(tkhtml TKHTML_BLD
      SUBDIR Tkhtml${TKHTML_VERSION}
      OUTPUT_FILE libTkhtml${CMAKE_SHARED_LIBRARY_SUFFIX}.${TKHTML_VERSION}
      SYMLINKS "libTkhtml${CMAKE_SHARED_LIBRARY_SUFFIX}"
      LINK_TARGET "libTkhtml${CMAKE_SHARED_LIBRARY_SUFFIX}"
      RPATH
      )
    cmake_set_rpath("")

    ExternalProject_ByProducts(TKHTML_BLD ${LIB_DIR}/Tkhtml${TKHTML_VERSION}
      pkgIndex.tcl
      FIXPATH
      )

    SetTargetFolder(TKHTML_BLD "Third Party Libraries")
    SetTargetFolder(tkhtml "Third Party Libraries")

  else(BRLCAD_ENABLE_TK)

    set(HAVE_TK 0 CACHE STRING "C level Tk flag" FORCE)

  endif(BRLCAD_ENABLE_TK)

  mark_as_advanced(HAVE_TK)

endif (${CMAKE_PROJECT_NAME}_TCL_BUILD)

if(0)
# OpenSceneGraph Libraries
set(openscenegraph_DESCRIPTION "
Option for enabling and disabling compilation of the OpenSceneGraph
libraries provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(openscenegraph OSG osg openscenegraph_DESCRIPTION
  ALIASES ENABLE_OPENSCENEGRAPH REQUIRED_VARS "BRLCAD_ENABLE_OSG;BRLCAD_LEVEL3" FIND_NAME OpenSceneGraph FIND_COMPONENTS
  osgText osgViewer FLAGS NOSYS)
if(BRLCAD_OSG_BUILD)
  set(OSG_LIBRARY osg CACHE STRING "libosg" FORCE)
  set(OSGUTIL_LIBRARY osgUtil CACHE STRING "osgutil" FORCE)
  set(OSGDB_LIBRARY osgDB CACHE STRING "osgdb" FORCE)
  set(OSGGA_LIBRARY osgGA CACHE STRING "osgGA" FORCE)
  set(OSGTEXT_LIBRARY osgText CACHE STRING "osg Text library" FORCE)
  set(OSGVIEWER_LIBRARY osgViewer CACHE STRING "osg Viewer library" FORCE)
  set(OPENTHREADS_LIBRARY OpenThreads CACHE STRING "OpenThreads library" FORCE)
  set(OPENTHREADS_INCLUDE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/openscenegraph/src/OpenThreads/include"
    "${BRLCAD_SOURCE_DIR}/src/other/openscenegraph/src/OpenThreads/include"
    CACHE STRING "OpenThreads include dirs" FORCE
    )
  set(OSG_INCLUDE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/openscenegraph/include"
    "${BRLCAD_SOURCE_DIR}/src/other/openscenegraph/include"
    CACHE STRING "OpenSceneGraph include dirs" FORCE
    )
  set(OSG_INCLUDE_DIR "${OSG_INCLUDE_DIR}" CACHE STRING "Directory containing OpenSceneGraph headers." FORCE)
  set(OSG_LIBRARIES "${OSG_LIBRARY};${OSGUTIL_LIBRARY};${OSGDB_LIBRARY};${OSGGA_LIBRARY};${OSGTEXT_LIBRARY};${OSGVIEWER_LIBRARY};${OPENTHREADS_LIBRARY}" CACHE STRING "OpenSceneGraph Libraries")
  SetTargetFolder(osg "Third Party Libraries")
  SetTargetFolder(osgDB "Third Party Libraries")
  SetTargetFolder(osgGA "Third Party Libraries")
  SetTargetFolder(osgText "Third Party Libraries")
  SetTargetFolder(osgViewer "Third Party Libraries")
  SetTargetFolder(osgUtil "Third Party Libraries")
  SetTargetFolder(osgWidget "Third Party Libraries")
  SetTargetFolder(osgdb_freetype "Third Party Libraries")
  SetTargetFolder(osgdb_osg "Third Party Libraries")
  SetTargetFolder(osgdb_png "Third Party Libraries")
else(BRLCAD_OSG_BUILD)
  if (BRLCAD_ENABLE_OSG)
    find_package(OpenThreads)
  endif (BRLCAD_ENABLE_OSG)
endif(BRLCAD_OSG_BUILD)

endif (0)

# GCT is a collection of algorithms for geometry processing and conversion
#add_subdirectory(gct)
include("${CMAKE_CURRENT_SOURCE_DIR}/gct.dist")
CMAKEFILES_IN_DIR(gct_ignore_files gct)

# linenoise is used by applications directly
include(${CMAKE_CURRENT_SOURCE_DIR}/linenoise.dist)
CMAKEFILES_IN_DIR(linenoise_ignore_files linenoise)

# Eigen is header only
include("${CMAKE_CURRENT_SOURCE_DIR}/Eigen.dist")
CMAKEFILES_IN_DIR(Eigen_ignore_files Eigen)

CMAKEFILES(README)
CMAKEFILES(CMakeLists.txt)

# Done with ExternalProject_Add
set(SUPPRESS_GENERATED_TAG FALSE)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

