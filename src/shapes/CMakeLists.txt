set(SHAPES_INCLUDE_DIRS ${GED_INCLUDE_DIRS} ${WDB_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
list(REMOVE_DUPLICATES SHAPES_INCLUDE_DIRS)
include_directories(${SHAPES_INCLUDE_DIRS})

brlcad_addexec(bolt bolt.c "libwdb;${M_LIBRARY}" FOLDER Shapes)
brlcad_addexec(coil coil.c "libged;libwdb;${M_LIBRARY}" FOLDER Shapes)
brlcad_addexec(fence fence.c "libwdb;${M_LIBRARY}" FOLDER Shapes)
brlcad_addexec(gastank gastank.c libwdb FOLDER Shapes)
brlcad_addexec(handle handle.c libwdb FOLDER Shapes)
brlcad_addexec(human human.c "libged;libwdb" FOLDER Shapes)
brlcad_addexec(picket_fence picket_fence.c "libwdb;${M_LIBRARY}" FOLDER Shapes)
brlcad_addexec(tire tire.c "libged;libwdb" FOLDER Shapes)
brlcad_addexec(window window.c libwdb FOLDER Shapes)
brlcad_addexec(window_frame window_frame.c libwdb FOLDER Shapes)
brlcad_addexec(wire wire.c "libwdb;${M_LIBRARY}" FOLDER Shapes)

cmakefiles(fence.h)
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
