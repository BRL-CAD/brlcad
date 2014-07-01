# Third party tools



# Clear all pre-defined CMake flags
CLEAR_BUILD_FLAGS()

# Restore CMake defaults, in case they are needed
RESTORE_CACHED_BUILD_FLAGS(_CMAKE_DEFAULT)

# Quiet all warnings in this directory
if(BRLCAD_DISABLE_SRC_OTHER_WARN)
  add_definitions(-w)
endif(BRLCAD_DISABLE_SRC_OTHER_WARN)

# Most third party items have a list calling out files for distcheck -
# these are stored in files in the dlists directory.  Ignore that
# directory for distcheck
file(GLOB dlists "*.dist")
foreach(ITEM ${dlists})
  get_filename_component(dlist ${ITEM} NAME)
  CMAKEFILES(${dlist})
endforeach(ITEM ${dlists})

# To enforce style guidelines rigorously, BRL-CAD bundles the
# Artistic Style code formatter.  This tool is LGPLv3 licensed
# and its code can *not* be used in BRL-CAD itself - it
# is present striclty and only to be used as a tool in the
# compilation process.
set(astyle_ALIASES ENABLE_ASTYLE)
set(astyle_DESCRIPTION "
Option for enabling and disabling compilation of the Artistic Style
(astyle) utility provided with BRL-CAD's source distribution.  Default is
AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY_EXECUTABLE(astyle astyle "BRLCAD_LEVEL2" astyle_ALIASES astyle_DESCRIPTION)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/astyle/Makefile)

CMAKEFILES(astyle.dist)

# dom2dox - Doc-O-Matic to Doxygen C++ source converter
include(${CMAKE_CURRENT_SOURCE_DIR}/dom2dox.dist)
CMAKEFILES_IN_DIR(dom2dox_ignore_files dom2dox)

# libxml and libxslt tools for DocBook processing.  We build just
# enough library support to get these two tools working.
set(xsltproc_ALIASES ENABLE_XSLTPROC)
set(xsltproc_DESCRIPTION "
Option for enabling and disabling compilation of the xsltproc XML
transformation utility provided with BRL-CAD's source distribution.
Used for DocBook documentation processing.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
Depends on BRLCAD_EXTRADOCS.
")
THIRD_PARTY_EXECUTABLE(xsltproc xmltools "BRLCAD_EXTRADOCS;BRLCAD_LEVEL3" xsltproc_ALIASES xsltproc_DESCRIPTION)
SetTargetFolder(xsltproc "Compilation Utilities")

# Used only when validating XML for schema compliance
# (BRLCAD_EXTRADOCS_VALIDATE must be on to use this, although it is compiled
# regardless if the xmltools subdirectory build is added.)
set(xmllint_ALIASES ENABLE_XMLLINT)
set(xmllint_DESCRIPTION "
Option for enabling and disabling compilation of the xmllint XML
validation utility provided with BRL-CAD's source distribution.  Used
for DocBook documentation validation.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY_EXECUTABLE(xmllint xmltools "BRLCAD_EXTRADOCS_VALIDATE;BRLCAD_LEVEL3" xmllint_ALIASES xmllint_DESCRIPTION)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/xmltools/include)
SetTargetFolder(xmllint "Compilation Utilities")

SetTargetFolder(xml "Third Party Libraries")
SetTargetFolder(xslt "Third Party Libraries")
SetTargetFolder(exslt "Third Party Libraries")

# Locac Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
