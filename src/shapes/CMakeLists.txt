include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${TCL_INCLUDE_DIRS}
)

BRLCAD_ADDEXEC(bolt bolt.c "libwdb ${M_LIBRARY}")
BRLCAD_ADDEXEC(coil coil.c "libwdb ${M_LIBRARY}")
BRLCAD_ADDEXEC(fence fence.c "libwdb ${M_LIBRARY}")
BRLCAD_ADDEXEC(gastank gastank.c libwdb)
BRLCAD_ADDEXEC(handle handle.c libwdb)
BRLCAD_ADDEXEC(human human.c "libged libwdb")
BRLCAD_ADDEXEC(picket_fence picket_fence.c "libwdb ${M_LIBRARY}")
BRLCAD_ADDEXEC(tire tire.c "libged libwdb")
BRLCAD_ADDEXEC(window window.c libwdb)
BRLCAD_ADDEXEC(window_frame window_frame.c libwdb)
BRLCAD_ADDEXEC(wire wire.c "libwdb ${M_LIBRARY}")

CMAKEFILES(fence.h)
CMAKEFILES(Makefile.am)
