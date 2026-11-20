function(SetTargetFolder targetname folder)
  if(TARGET ${targetname})
    set_target_properties(${targetname} PROPERTIES FOLDER "${folder}")
  endif(TARGET ${targetname})
endfunction(SetTargetFolder)

# The following are build or utility tools written by BRL-CAD developers
# specifically for use with our build and maintenance processes

# asciiquack - AsciiDoc processor used to build BRL-CAD documentation
add_subdirectory(asciiquack)

# brlinst is a packaging-time host tool. Keep its project settings, static
# runtime, and install rules isolated from the main build. The CPack external
# script builds it on demand without adding it to ordinary builds.
function(brlinst_add_host)
  set(BRLINST_EXTRA_NOTICES "" CACHE FILEPATH "Additional notices for installer runtime libraries")
  set(_brlinst_host_ready TRUE)
  if(MINGW AND NOT BRLINST_EXTRA_NOTICES)
    set(_brlinst_host_ready FALSE)
    message(WARNING
      "Built-in EXE/MSI packaging is disabled for MinGW until BRLINST_EXTRA_NOTICES "
      "names a file containing the toolchain runtime notices"
    )
  endif()
  set(_brlinst_target_processor "${CMAKE_SYSTEM_PROCESSOR}")
  if(CMAKE_VS_PLATFORM_NAME)
    set(_brlinst_target_processor "${CMAKE_VS_PLATFORM_NAME}")
  elseif(CMAKE_GENERATOR_PLATFORM)
    set(_brlinst_target_processor "${CMAKE_GENERATOR_PLATFORM}")
  endif()
  if(_brlinst_target_processor MATCHES "^(AMD64|amd64|x86_64|x64)$")
    set(_brlinst_target_arch x64)
  elseif(_brlinst_target_processor MATCHES "^(ARM64|arm64|aarch64)$")
    set(_brlinst_target_arch arm64)
  endif()
  if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|x64)$")
    set(_brlinst_host_arch x64)
  elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
    set(_brlinst_host_arch arm64)
  endif()
  if(WIN32 AND CMAKE_HOST_WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND
      _brlinst_target_arch AND "${_brlinst_target_arch}" STREQUAL "${_brlinst_host_arch}" AND
      _brlinst_host_ready)
    include(ExternalProject)
    set(_brlinst_host_binary_dir "${CMAKE_BINARY_DIR}/misc/tools/brlinst-host")
    set(_brlinst_cmake_args
      "-DCMAKE_BUILD_TYPE=Release"
      "-DBUILD_TESTING=OFF"
      "-DBRLINST_JSON_INCLUDE=${CMAKE_SOURCE_DIR}/src/libbu"
      "-DBRLINST_WITH_MSI=ON"
    )
    if(BRLINST_EXTRA_NOTICES)
      list(APPEND _brlinst_cmake_args "-DBRLINST_EXTRA_NOTICES:FILEPATH=${BRLINST_EXTRA_NOTICES}")
    endif()
    ExternalProject_Add(brlinst_host
      SOURCE_DIR "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/brlinst"
      BINARY_DIR "${_brlinst_host_binary_dir}"
      PREFIX "${CMAKE_BINARY_DIR}/misc/tools/brlinst_host-prefix"
      CMAKE_ARGS ${_brlinst_cmake_args}
      BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config Release
      INSTALL_COMMAND ""
      TEST_COMMAND ""
      BUILD_ALWAYS TRUE
      EXCLUDE_FROM_ALL TRUE
    )
    set_target_properties(brlinst_host PROPERTIES FOLDER "Compilation Utilities")
    set(BRLINST_HOST_BINARY_DIR "${_brlinst_host_binary_dir}" PARENT_SCOPE)
    set(BRLINST_HOST_CONFIG Release PARENT_SCOPE)
    distclean("${CMAKE_BINARY_DIR}/misc/tools/brlinst_host-prefix")
    distclean("${_brlinst_host_binary_dir}")
  endif()
endfunction()

# On some platforms distclean has additional cleanup to do
distclean("${CMAKE_BINARY_DIR}/${BIN_DIR}/asciiquack.pdb")

# inactvhdrs - utility that tries to find local headers unused by other
# source code in a file hierarchy
if(BUILD_TESTING)
  add_subdirectory(inactvhdrs)
  settargetfolder(debug2c "Maintenance Utilities")
  distclean(${CMAKE_CURRENT_BINARY_DIR}/inactvhdrs/Makefile)
endif(BUILD_TESTING)

cmakefiles(
  CMakeLists.txt
  README
  asciiquack/CMakeLists.txt
  asciiquack/LICENSE
  asciiquack/README.md
  asciiquack/asciiquack.cpp
  asciiquack/attr_list.h
  asciiquack/block_scanner.h
  asciiquack/block_scanner_hand.c
  asciiquack/docbook5.hpp
  asciiquack/document.hpp
  asciiquack/html5.hpp
  asciiquack/inline_scanner.hpp
  asciiquack/manpage.hpp
  asciiquack/outbuf.hpp
  asciiquack/parser.cpp
  asciiquack/parser.hpp
  asciiquack/pdf.hpp
  asciiquack/reader.hpp
  asciiquack/substitutors.hpp
  asciiquack/vendor/cxxopts.hpp
  brlinst/CMakeLists.txt
  brlinst/COPYING
  brlinst/README.adoc
  brlinst/THIRD_PARTY.txt
  brlinst/cmake/BRLCAD.cmake
  brlinst/cmake/Brlinst.cmake
  brlinst/cmake/Package.cmake.in
  brlinst/examples/brlcad.json.in
  brlinst/include/installer/core.hpp
  brlinst/include/installer/msi.hpp
  brlinst/include/installer/windows.hpp
  brlinst/resources/notices.hpp.in
  brlinst/resources/runtime.manifest
  brlinst/resources/runtime.rc
  brlinst/resources/uninstall.ico
  brlinst/src/core.cpp
  brlinst/src/document.cpp
  brlinst/src/input.cpp
  brlinst/src/msi.cpp
  brlinst/src/msi_actions.cpp
  brlinst/src/msi_model.cpp
  brlinst/src/pack.cpp
  brlinst/src/package.cpp
  brlinst/src/resources.cpp
  brlinst/src/runtime.cpp
  brlinst/src/transaction.cpp
  brlinst/src/windows.cpp
  brlinst/src/wizard.cpp
  brlinst/third_party/miniz/LICENSE
  brlinst/third_party/miniz/miniz.c
  brlinst/third_party/miniz/miniz.h
  brlinst/third_party/miniz/miniz_common.h
  brlinst/third_party/miniz/miniz_export.h
  brlinst/third_party/miniz/miniz_tdef.c
  brlinst/third_party/miniz/miniz_tdef.h
  brlinst/third_party/miniz/miniz_tinfl.c
  brlinst/third_party/miniz/miniz_tinfl.h
  brlinst/third_party/miniz/miniz_zip.h
  brlinst/tests/core_tests.cpp
  brlinst/tests/cpack_test.cmake
  brlinst/tests/msi_model_tests.cpp
  brlinst/tests/windows-msi-smoke.ps1
  brlinst/tests/windows-smoke.ps1
  db2adoc/db2adoc.py
  db2adoc/db2adoc.xsl
  inactvhdrs/CMakeLists.txt
  inactvhdrs/inactvhdrs.cpp
)

# Locac Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
