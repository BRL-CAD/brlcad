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

find_package(TCL)

###############################################################################
#                                 Tcl/Tk
###############################################################################

if (${CMAKE_PROJECT_NAME}_TCL_BUILD)

  if(BRLCAD_ENABLE_TK)

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

