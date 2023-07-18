set(LDIR "${BRLCAD_SOURCE_DIR}/src/other/ext/linenoise")

set(GTOOLS_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  ${TCLCAD_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  ${LDIR}
  )
list(REMOVE_DUPLICATES GTOOLS_INCLUDE_DIRS)
BRLCAD_INCLUDE_DIRS(GTOOLS_INCLUDE_DIRS)

BRLCAD_ADDDATA(gtransfer.c sample_applications)
BRLCAD_ADDEXEC(gex "gex.cpp" "libbu;librt")
BRLCAD_ADDEXEC(glint glint.cpp "librt;${M_LIBRARY}")
BRLCAD_ADDEXEC(gqa gqa.c "libged;librt")
BRLCAD_ADDEXEC(gchecker gchecker.cpp "libged;librt;libbu")

set(GSH_SRCS
  gsh.cpp
  ${LDIR}/utf8.c
  ${LDIR}/linenoise.c
  ${LDIR}/stringbuf.c
  )
BRLCAD_ADDEXEC(gsh "${GSH_SRCS}"  "libged;libdm;libbu")
add_dependencies(gsh dm_plugins)

BRLCAD_ADDEXEC(gtransfer gtransfer.c "librt;libpkg" NO_INSTALL)

set(GANALYZE_SRCS
  ganalyze.cpp
  ${LDIR}/utf8.c
  ${LDIR}/linenoise.c
  ${LDIR}/stringbuf.c
  )
BRLCAD_ADDEXEC(ganalyze "${GANALYZE_SRCS}" "libanalyze;librt;libbu;libpkg" NO_INSTALL)

add_subdirectory(beset)
add_subdirectory(gdiff)

add_subdirectory(tests)

# We use the std::filesystem from C++17 in this utility, so only try to build
# it if we know we have a compiler that supports it - BRL-CAD as a whole does
# not yet require C++17, so until we do this target may need to be disabled
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++17" COMPILER_SUPPORTS_CXX17)
if (COMPILER_SUPPORTS_CXX17)
  BRLCAD_ADDEXEC(gsurvey gsurvey.cpp "librt;libbu" NO_INSTALL)
  target_compile_features(gsurvey PRIVATE cxx_std_17)
  check_cxx_compiler_flag(-O3 O3_COMPILER_FLAG)
  if (O3_COMPILER_FLAG)
    # If we have the O3 flag, use it
    target_compile_options(gsurvey PRIVATE "-O3")
  endif (O3_COMPILER_FLAG)
endif (COMPILER_SUPPORTS_CXX17)

CMAKEFILES(
  CMakeLists.txt
  gsurvey.cpp
  md5.hpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
