function(SetTargetFolder targetname folder)
  if(TARGET ${targetname})
    set_target_properties(${targetname} PROPERTIES FOLDER "${folder}")
  endif(TARGET ${targetname})
endfunction(SetTargetFolder)

# The following are build or utility tools written by BRL-CAD developers
# specifically for use with our build and maintenance processes

add_executable(brlcad-sbom brlcad-sbom.cpp)
target_include_directories(brlcad-sbom PRIVATE "${CMAKE_SOURCE_DIR}/src/libbu")
target_sources(brlcad-sbom PRIVATE "${CMAKE_SOURCE_DIR}/src/libbu/sha1.c")
target_compile_features(brlcad-sbom PRIVATE cxx_std_17)
settargetfolder(brlcad-sbom "Compilation Utilities")
add_custom_target(brlcad-sbom-regress
  COMMAND "${CMAKE_COMMAND}"
    -DSBOM_TOOL=$<TARGET_FILE:brlcad-sbom>
    -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/sbom-test
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/sbom.cmake"
  DEPENDS brlcad-sbom
  VERBATIM
)
settargetfolder(brlcad-sbom-regress "BRL-CAD Regression Tests")

# asciiquack - AsciiDoc processor used to build BRL-CAD documentation
add_subdirectory(asciiquack)

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
  brlcad-sbom.cpp
  tests/sbom.cmake
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
