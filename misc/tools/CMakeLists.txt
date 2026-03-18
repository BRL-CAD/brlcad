function(SetTargetFolder targetname folder)
  if(TARGET ${targetname})
    set_target_properties(${targetname} PROPERTIES FOLDER "${folder}")
  endif(TARGET ${targetname})
endfunction(SetTargetFolder)

# The following are build or utility tools written by BRL-CAD developers
# specifically for use with our build and maintenance processes

# asciiquack - AsciiDoc processor used to build BRL-CAD documentation
add_subdirectory(asciiquack)

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
