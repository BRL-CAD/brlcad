include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_STD_INCLUDES}
  ${TCL_INCLUDE_PATH}
)

add_definitions(
  ${BRLCAD_STD_DEFS}
)


BRLCAD_ADDEXEC(bolt bolt.c wdb)
BRLCAD_ADDEXEC(coil coil.c wdb)
BRLCAD_ADDEXEC(fence fence.c wdb)
BRLCAD_ADDEXEC(gastank gastank.c wdb)
BRLCAD_ADDEXEC(handle handle.c wdb)
BRLCAD_ADDEXEC(human human.c "wdb ged")
BRLCAD_ADDEXEC(picket_fence picket_fence.c wdb)
BRLCAD_ADDEXEC(tire tire.c "wdb ged")
BRLCAD_ADDEXEC(window window.c wdb)
BRLCAD_ADDEXEC(window_frame window_frame.c wdb)
BRLCAD_ADDEXEC(wire wire.c wdb)
