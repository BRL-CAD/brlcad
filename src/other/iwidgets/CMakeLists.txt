# Copyright (c) 2010-2016 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# *******************************************************************
# ***                libutahrle CMakeLists.txt                     ***
# *******************************************************************

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

project(IWIDGETS)

if(NOT DEFINED LIB_DIR)
  set(LIB_DIR lib)
endif(NOT DEFINED LIB_DIR)

set(IWIDGETS_PKGNAME Iwidgets)
set(IWIDGETS_PKGVERSION "4.1.1")
set(VERSION ${IWIDGETS_PKGVERSION})
set(IWIDGETS_VERSION ${IWIDGETS_PKGVERSION})

set(IWIDGETS_SCRIPTS
  generic/buttonbox.itk
  generic/calendar.itk
  generic/canvasprintbox.itk
  generic/canvasprintdialog.itk
  generic/checkbox.itk
  generic/colors.itcl
  generic/combobox.itk
  generic/dateentry.itk
  generic/datefield.itk
  generic/dialog.itk
  generic/dialogshell.itk
  generic/disjointlistbox.itk
  generic/entryfield.itk
  generic/extbutton.itk
  generic/extfileselectionbox.itk
  generic/extfileselectiondialog.itk
  generic/feedback.itk
  generic/fileselectionbox.itk
  generic/fileselectiondialog.itk
  generic/finddialog.itk
  generic/hierarchy.itk
  generic/hyperhelp.itk
  generic/labeledframe.itk
  generic/labeledwidget.itk
  generic/mainwindow.itk
  generic/menubar.itk
  generic/messagebox.itk
  generic/messagedialog.itk
  generic/notebook.itk
  generic/optionmenu.itk
  generic/panedwindow.itk
  generic/pane.itk
  generic/promptdialog.itk
  generic/pushbutton.itk
  generic/radiobox.itk
  generic/regexpfield.itk
  generic/roman.itcl
  generic/scopedobject.itcl
  generic/scrolledcanvas.itk
  generic/scrolledframe.itk
  generic/scrolledhtml.itk
  generic/scrolledlistbox.itk
  generic/scrolledtext.itk
  generic/scrolledwidget.itk
  generic/selectionbox.itk
  generic/selectiondialog.itk
  generic/shell.itk
  generic/spindate.itk
  generic/spinint.itk
  generic/spinner.itk
  generic/spintime.itk
  generic/tabnotebook.itk
  generic/tabset.itk
  generic/tclIndex
  generic/timeentry.itk
  generic/timefield.itk
  generic/toolbar.itk
  generic/unknownimage.gif
  generic/watch.itk
  )

# Set up install rules
foreach(script ${IWIDGETS_SCRIPTS})
  install(FILES ${script} DESTINATION ${LIB_DIR}/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION}/scripts)
endforeach(script ${IWIDGETS_SCRIPTS})

# Create the pkgIndex.tcl file and configure iwidgets.tcl.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded ${IWIDGETS_PKGNAME} ${IWIDGETS_PKGVERSION} [list source [file join $dir iwidgets.tcl]]")
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl PROPERTIES GENERATED TRUE)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/iwidgets.tcl.in ${CMAKE_CURRENT_BINARY_DIR}/iwidgets.tcl)

# Place files in build directory
foreach(file ${IWIDGETS_SCRIPTS})
  get_filename_component(filename ${file} NAME)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    configure_file(${file} ${CMAKE_BINARY_DIR}/${LIB_DIR}/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION}/scripts/${filename} COPYONLY)
  else(NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      configure_file(${file} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${LIB_DIR}/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION}/scripts/${filename} COPYONLY)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif(NOT CMAKE_CONFIGURATION_TYPES)
endforeach(file ${IWIDGETS_SCRIPTS})

# Place files in build directory
if(NOT CMAKE_CONFIGURATION_TYPES)
  configure_file(${CMAKE_CURRENT_BINARY_DIR}/iwidgets.tcl ${CMAKE_BINARY_DIR}/${LIB_DIR}/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION}/iwidgets.tcl COPYONLY)
  configure_file(${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl ${CMAKE_BINARY_DIR}/${LIB_DIR}/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION}/pkgIndex.tcl COPYONLY)
else(NOT CMAKE_CONFIGURATION_TYPES)
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    configure_file(${CMAKE_CURRENT_BINARY_DIR}/iwidgets.tcl ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${LIB_DIR}/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION}/iwidgets.tcl COPYONLY)
    configure_file(${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${LIB_DIR}/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION}/pkgIndex.tcl COPYONLY)
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
endif(NOT CMAKE_CONFIGURATION_TYPES)

# Add install rules for generated files
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION ${LIB_DIR}/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/iwidgets.tcl DESTINATION ${LIB_DIR}/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION})
install(FILES license.terms DESTINATION ${LIB_DIR}/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION})

CMAKEFILES(${IWIDGETS_SCRIPTS})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

