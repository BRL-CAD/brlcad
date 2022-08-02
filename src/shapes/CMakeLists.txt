set(SHAPES_INCLUDE_DIRS
  ${GED_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )
LIST(REMOVE_DUPLICATES SHAPES_INCLUDE_DIRS)
include_directories(${SHAPES_INCLUDE_DIRS})

BRLCAD_ADDEXEC(bolt bolt.c "libwdb;${M_LIBRARY}" FOLDER Shapes)
BRLCAD_ADDEXEC(coil coil.c "libged;libwdb;${M_LIBRARY}" FOLDER Shapes)
BRLCAD_ADDEXEC(fence fence.c "libwdb;${M_LIBRARY}" FOLDER Shapes)
BRLCAD_ADDEXEC(gastank gastank.c libwdb FOLDER Shapes)
BRLCAD_ADDEXEC(handle handle.c libwdb FOLDER Shapes)
BRLCAD_ADDEXEC(human human.c "libged;libwdb" FOLDER Shapes)
BRLCAD_ADDEXEC(picket_fence picket_fence.c "libwdb;${M_LIBRARY}" FOLDER Shapes)
BRLCAD_ADDEXEC(tire tire.c "libged;libwdb" FOLDER Shapes)
BRLCAD_ADDEXEC(window window.c libwdb FOLDER Shapes)
BRLCAD_ADDEXEC(window_frame window_frame.c libwdb FOLDER Shapes)
BRLCAD_ADDEXEC(wire wire.c "libwdb;${M_LIBRARY}" FOLDER Shapes)

CMAKEFILES(fence.h)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
