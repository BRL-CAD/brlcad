# Primitives have their own individual editing requirements
add_subdirectory(ell)

# Things to check include:
#
# preparing selectable "schematic" 2D graphics with
# https://doc.qt.io/qt-5/qgraphicsitem.html
#
# .g object aware QLineEdit:
# https://doc.qt.io/qt-5/qlineedit.html
# https://doc.qt.io/qt-5/qvalidator.html
# https://doc.qt.io/qt-5/stylesheet-examples.html
#
# Sketch editor - will need some variation on Archer's abilities, which will
# involve a custom 2D widget.  Possibly useful resources:
# https://github.com/peizhan/psketcher
# https://github.com/cmerrill/sketchsolve

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
