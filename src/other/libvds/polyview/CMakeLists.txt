find_package(FLEX)
find_package(X11)
find_package(FLTK)
find_package(OpenGL)
find_package(GLUT)

INCLUDE_DIRECTORIES(
	${X11_INCLUDE_DIR}
	${FLEX_INCLUDE_DIR}
	${OPENGL_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
	${CMAKE_CURRENT_SOURCE_DIR}
	/home/cyapp/opengllibs-install/include
	/home/cyapp/fltk-install/include
)

FLEX_TARGET(POLYLEX polylex.l ${CMAKE_CURRENT_BINARY_DIR}/polylex.cxx)

SET(polyview_srcs
	polyview.cxx
	polyviewctrl.cxx
	trackball.cxx
	${FLEX_POLYLEX_OUTPUTS}
	poly.cxx
	geom.cxx
	invmatrix.cxx
	cluster.cxx
	)

add_executable(polyview ${polyview_srcs})
target_link_libraries(polyview ${X11_LIBRARIES} ${X11_Xft_LIB}
	${X11_Xinerama_LIB} ${OPENGL_LIBRARIES} ${FLTK_LIBRARIES} ${GLUT_LIBRARIES}
	vds stdvds)
