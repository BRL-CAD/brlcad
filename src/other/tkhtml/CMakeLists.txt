# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(TKHTML)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)

find_package(TCL)
find_package(X11)

include_directories(
		  ${CMAKE_CURRENT_SOURCE_DIR}/src
		  ${CMAKE_CURRENT_BINARY_DIR}
		  ${TCL_INCLUDE_DIRS}
		  ${TK_INCLUDE_PATH}
		  ${X11_INCLUDE_DIR}
		  )

SET(TKHTML_PKGNAME Tkhtml)
SET(TKHTML_PKGVERSION "3.0")

# Windows specific flags
IF(WIN32)
	SET(TKHTML_CFLAGS "${TKHTML_CFLAGS} -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEP -Ot -Oi -fp:strict -Gs -GS -GL -DUSE_TCL_STUBS -DUSE_TK_STUBS -D__WIN32__")
	SET(TKHTML_CFLAGS "${TKHTML_CFLAGS} -Dinline=__inline -DBUILD_tkhtml")
ENDIF(WIN32)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TKHTML_CFLAGS}")

ADD_CUSTOM_COMMAND(
		  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/htmldefaultstyle.c
		  COMMAND ${TCL_TCLSH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/src/mkdefaultstyle.tcl > ${CMAKE_CURRENT_BINARY_DIR}/htmldefaultstyle.c
		  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/mkdefaultstyle.tcl ${TCL_TCLSH_EXECUTABLE}
)

ADD_CUSTOM_COMMAND(
		  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/htmltokens.c ${CMAKE_CURRENT_BINARY_DIR}/htmltokens.h
		  COMMAND ${TCL_TCLSH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/src/tokenlist.txt
		  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/tokenlist.txt ${TCL_TCLSH_EXECUTABLE}
)

ADD_CUSTOM_COMMAND(
		  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cssprop.c ${CMAKE_CURRENT_BINARY_DIR}/cssprop.h
		  COMMAND ${TCL_TCLSH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/src/cssprop.tcl
		  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/htmltokens.c ${TCL_TCLSH_EXECUTABLE}
)

SET(TKHTML_SRCS
		  ${CMAKE_CURRENT_BINARY_DIR}/htmldefaultstyle.c
		  ${CMAKE_CURRENT_BINARY_DIR}/cssprop.c
		  src/css.c
		  src/cssdynamic.c
		  src/cssparser.c
		  src/csssearch.c
		  src/htmldecode.c
		  src/htmldraw.c
		  src/htmlfloat.c
		  src/htmlhash.c
		  src/htmlimage.c
		  src/htmlinline.c
		  src/htmllayout.c
		  src/htmlparse.c
		  src/htmlprop.c
		  src/htmlstyle.c
		  src/htmltable.c
		  src/htmltagdb.c
		  src/htmltcl.c
		  src/htmltext.c
		  src/htmltree.c
		  src/htmlutil.c
		  src/restrack.c
		  src/swproc.c
)

add_library(Tkhtml SHARED ${TKHTML_SRCS})
IF(WIN32)
   target_link_libraries(Tkhtml ${TCL_STUB_LIBRARIES} ${X11_LIBRARIES})
ELSE(WIN32)
   target_link_libraries(Tkhtml ${TCL_LIBRARY} ${TCL_TK_LIBRARY} ${TCL_STUB_LIBRARIES} ${X11_LIBRARIES})
ENDIF(WIN32)
SET_TARGET_PROPERTIES(Tkhtml PROPERTIES VERSION ${TKHTML_PKGVERSION})
install(TARGETS Tkhtml DESTINATION ${LIB_DIR})

# Create the pkgIndex.tcl file.
GET_TARGET_PROPERTY(TKHTML_LIBLOCATION Tkhtml LOCATION_${CMAKE_BUILD_TYPE})
GET_FILENAME_COMPONENT(TKHTML_LIBNAME ${TKHTML_LIBLOCATION} NAME)
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded ${TKHTML_PKGNAME} ${TKHTML_PKGVERSION} [list load [file join $dir .. .. ${LIB_DIR} ${TKHTML_LIBNAME}]]")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION lib/${TKHTML_PKGNAME}${TKHTML_PKGVERSION})

FILE(WRITE ${CMAKE_BINARY_DIR}/lib/${TKHTML_PKGNAME}${TKHTML_PKGVERSION}/pkgIndex.tcl "package ifneeded ${TKHTML_PKGNAME} ${TKHTML_PKGVERSION} [list load [file join $dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} ${TKHTML_LIBNAME}] ${TKHTML_PKGNAME}]")


ADD_CUSTOM_COMMAND(
		  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tkhtml.n
		  COMMAND ${TCL_TCLSH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/doc/macros.tcl -nroff ${CMAKE_CURRENT_SOURCE_DIR}/doc/html.man > ${CMAKE_CURRENT_BINARY_DIR}/tkhtml.n
)
ADD_CUSTOM_TARGET(tkhtml_n_gen ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tkhtml.n)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tkhtml.n DESTINATION ${MAN_DIR})
