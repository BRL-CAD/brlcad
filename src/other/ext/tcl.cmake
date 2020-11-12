# TODO - We appear to have a problem with spaces in pathnames and the 8.6 autotools
# build. (https://core.tcl-lang.org/tcl/tktview/888c1a8c9d84c1f5da4a46352bbf531424fe7126)
# May have to switch back to the CMake system until that's fixed, if we want to
# keep the odd pathnames test running with bundled libs...

if (BRLCAD_ENABLE_TCL)
  set(TCL_VERSION "8.6" CACHE STRING "BRL-CAD uses Tcl 8.6" FORCE)
endif (BRLCAD_ENABLE_TCL)

set(tcl_DESCRIPTION "
Option for enabling and disabling compilation of the Tcl library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")

THIRD_PARTY(tcl TCL tcl
  tcl_DESCRIPTION
  ALIASES ENABLE_TCL
  REQUIRED_VARS "BRLCAD_LEVEL2;BRLCAD_ENABLE_TCL"
  RESET_VARS TCL_LIBRARY TCL_LIBRARIES TCL_STUB_LIBRARY TCL_TCLSH TCL_INCLUDE_PATH TCL_INCLUDE_DIRS
  )

if (BRLCAD_TCL_BUILD)

  set(TCL_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/TCL_BLD-prefix/src/TCL_BLD")
  set(TCL_MAJOR_VERSION 8)
  set(TCL_MINOR_VERSION 6)
  set(TCL_PATCH_VERSION 10)

  # In addition to the usual target dependencies, we need to adjust for the
  # non-standard BRL-CAD zlib name, if we are using our bundled version.  Set a
  # variable here so the tcl_replace utility will know the right value.
  if (TARGET zlib_stage)
    set(ZLIB_TARGET zlib_stage)
    set(ZLIB_NAME z_brl)
    set(DEFLATE_NAME brl_deflateSetHeader)
  else (TARGET zlib_stage)
    set(ZLIB_NAME z)
    set(DEFLATE_NAME deflateSetHeader)
  endif (TARGET zlib_stage)

  # We need to set internal Tcl variables to the final install paths, not the intermediate install paths that
  # Tcl's own build will think are the final paths.  Rather than attempt build system trickery we simply
  # hard set the values in the source files by rewriting them.
  set(TCL_REWORK_FILES ${TCL_PATCH_FILES} "${TCL_SRC_DIR}/unix/tclUnixInit.c" "${TCL_SRC_DIR}/generic/tclPkgConfig.c")
  if (NOT TARGET tcl_replace)
    configure_file(${BDEPS_CMAKE_DIR}/tcl_replace.cxx.in ${CMAKE_CURRENT_BINARY_DIR}/tcl_replace.cxx)
    add_executable(tcl_replace ${CMAKE_CURRENT_BINARY_DIR}/tcl_replace.cxx)
  endif (NOT TARGET tcl_replace)


  set(TCL_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/tcl)

  if (MSVC)
    set(TCL_BASENAME tcl)
    set(TCL_STUBNAME tclstub)
    set(TCL_EXECNAME tclsh${TCL_MAJOR_VERSION}${TCL_MINOR_VERSION})
    set(TCL_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(TCL_SYMLINK_1 ${TCL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(TCL_SYMLINK_2 ${TCL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})
    set(TCL_APPINIT)
  elseif (APPLE)
    set(TCL_BASENAME libtcl)
    set(TCL_STUBNAME libtclstub)
    set(TCL_EXECNAME tclsh-${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})
    set(TCL_SUFFIX .${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}.${TCL_PATCH_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(TCL_SYMLINK_1 ${TCL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(TCL_SYMLINK_2 ${TCL_BASENAME}.${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(TCL_APPINIT tclAppInit.c)
  else (MSVC)
    set(TCL_BASENAME libtcl)
    set(TCL_STUBNAME libtclstub)
    set(TCL_EXECNAME tclsh-${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})
    set(TCL_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}.${TCL_PATCH_VERSION})
    set(TCL_SYMLINK_1 ${TCL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(TCL_SYMLINK_2 ${TCL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})
    set(TCL_APPINIT tclAppInit.c)
  endif (MSVC)

  ExternalProject_Add(TCL_BLD
    URL "${CMAKE_CURRENT_SOURCE_DIR}/tcl"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    PATCH_COMMAND tcl_replace ${TCL_REWORK_FILES}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${TCL_INSTDIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    $<$<NOT:$<BOOL:${CMAKE_CONFIGURATION_TYPES}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DZLIB_ROOT=$<$<BOOL:${ZLIB_TARGET}>:${CMAKE_BINARY_ROOT}>
    -DZLIB_LIBRARY=$<$<BOOL:${ZLIB_TARGET}>:${ZLIB_LIBRARY}>
    $<$<BOOL:${ZLIB_TARGET}>:-DZ_PREFIX=ON>
    $<$<BOOL:${ZLIB_TARGET}>:-DZ_PREFIX_STR=${Z_PREFIX_STR}>
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/TCL_BLD-prefix")

  # Tell the parent build about files and libraries
  ExternalProject_Target(SHARED tcl TCL_BLD ${TCL_INSTDIR}
    ${TCL_BASENAME}${TCL_SUFFIX}
    SYMLINKS ${TCL_SYMLINK_1};${TCL_SYMLINK_2}
    LINK_TARGET ${TCL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_Target(STATIC tclstub TCL_BLD ${TCL_INSTDIR}
    ${TCL_STUBNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    )

  ExternalProject_Target(EXEC tclsh_exe TCL_BLD ${TCL_INSTDIR}
    ${TCL_EXECNAME}${CMAKE_EXECUTABLE_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(tcl TCL_BLD ${TCL_INSTDIR} ${LIB_DIR}/tcl8.${TCL_MINOR_VERSION}
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
    ${TCL_APPINIT}
    tclIndex
    tm.tcl
    word.tcl
    )

  ExternalProject_ByProducts(tcl TCL_BLD ${TCL_INSTDIR} ${LIB_DIR}/tcl8/8.5
    msgcat-1.6.1.tm
    )

  ExternalProject_ByProducts(tcl TCL_BLD ${TCL_INSTDIR} ${INCLUDE_DIR}
    tclDecls.h
    tcl.h
    tclOODecls.h
    tclOO.h
    tclPlatDecls.h
    tclTomMathDecls.h
    tclTomMath.h
    )

  # Anything building against the stub will want the headers, etc. in place
  add_dependencies(tclstub tcl_stage)

  set(TCL_LIBRARY tcl CACHE STRING "Building bundled tcl" FORCE)
  set(TCL_LIBRARIES tcl CACHE STRING "Building bundled tcl" FORCE)
  set(TCL_STUB_LIBRARY tclstub CACHE STRING "Building bundled tcl" FORCE)
  set(TCL_TCLSH tclsh_exe CACHE STRING "Building bundled tcl" FORCE)
  set(TCL_INCLUDE_PATH "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}" CACHE STRING "Directory containing tcl headers." FORCE)
  set(TCL_INCLUDE_DIRS "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}" CACHE STRING "Directory containing tcl headers." FORCE)


  SetTargetFolder(TCL_BLD "Third Party Libraries")
  SetTargetFolder(tcl "Third Party Libraries")

endif (BRLCAD_TCL_BUILD)

include("${CMAKE_CURRENT_SOURCE_DIR}/tcl.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

