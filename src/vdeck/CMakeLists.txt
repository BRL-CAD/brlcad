include_directories(
	${RT_INCLUDE_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}
	)

BRLCAD_ADDEXEC(vdeck "cgarbs.c match.c parsarg.c vdeck.c" "librt ${M_LIBRARY}")
ADD_MAN_PAGE(1 vdeck.1)
CMAKEFILES(std.h vextern.h)
CMAKEFILES(Makefile.am)
