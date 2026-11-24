set(SHAPES_INCLUDE_DIRS ${GED_INCLUDE_DIRS} ${WDB_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
list(REMOVE_DUPLICATES SHAPES_INCLUDE_DIRS)
include_directories(${SHAPES_INCLUDE_DIRS})

add_subdirectory(naca456)
add_subdirectory(building)
add_subdirectory(propeller)
add_subdirectory(tree)

brlcad_addexec(bolt bolt.c "libwdb;${M_LIBRARY}" FOLDER Shapes ARG_SCHEMA)
brlcad_addexec(coil coil.c "libged;libwdb;${M_LIBRARY}" FOLDER Shapes ARG_SCHEMA)
brlcad_addexec(fence fence.c "libwdb;${M_LIBRARY}" FOLDER Shapes ARG_SCHEMA)
brlcad_addexec(gastank gastank.c libwdb FOLDER Shapes ARG_SCHEMA)
brlcad_addexec(handle handle.c libwdb FOLDER Shapes ARG_SCHEMA)
brlcad_addexec(human human.c "libged;libwdb" FOLDER Shapes ARG_SCHEMA)
brlcad_addexec(picket_fence picket_fence.c "libwdb;${M_LIBRARY}" FOLDER Shapes ARG_SCHEMA)
brlcad_addexec(tire tire.c "libged;libwdb" FOLDER Shapes ARG_SCHEMA)
brlcad_addexec(window window.c libwdb FOLDER Shapes ARG_SCHEMA)
brlcad_addexec(window_frame window_frame.c libwdb FOLDER Shapes ARG_SCHEMA)
brlcad_addexec(wire wire.c "libwdb;${M_LIBRARY}" FOLDER Shapes NO_OPTIONS)

cmakefiles(fence.h)
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
