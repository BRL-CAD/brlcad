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

# install(CODE) uses generator expressions - requires 3.14 or newer.
# We want it to be on, but until we can bump our minimum requirement
# set it to OLD to make sure we don't break anything.
if (POLICY CMP0087)
  cmake_policy(SET CMP0087 OLD)
endif (POLICY CMP0087)

if(NOT DEFINED LIB_DIR)
  set(LIB_DIR lib)
endif(NOT DEFINED LIB_DIR)

set(IWIDGETS_PKGNAME Iwidgets)
set(IWIDGETS_VERSION "4.1.1")

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
install(FILES ${IWIDGETS_SCRIPTS} DESTINATION ${LIB_DIR}/${IWIDGETS_PKGNAME}${IWIDGETS_VERSION}/scripts)

# Create the pkgIndex.tcl file and configure iwidgets.tcl.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded ${IWIDGETS_PKGNAME} ${IWIDGETS_VERSION} [list source [file join $dir ../../${LIB_DIR}/Iwidgets${IWIDGETS_VERSION}/iwidgets.tcl]]")
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl PROPERTIES GENERATED TRUE)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/iwidgets.tcl.in ${CMAKE_CURRENT_BINARY_DIR}/iwidgets.tcl)

# Add install rules for generated files
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION ${LIB_DIR}/${IWIDGETS_PKGNAME}${IWIDGETS_VERSION})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/iwidgets.tcl DESTINATION ${LIB_DIR}/${IWIDGETS_PKGNAME}${IWIDGETS_VERSION})
install(FILES license.terms DESTINATION ${LIB_DIR}/${IWIDGETS_PKGNAME}${IWIDGETS_VERSION})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

