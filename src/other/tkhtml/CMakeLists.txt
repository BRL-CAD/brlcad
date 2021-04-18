if (TK_SYSTEM_GRAPHICS MATCHES "x11")
  find_package(X11)
endif (TK_SYSTEM_GRAPHICS MATCHES "x11")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
  ${X11_INCLUDE_DIR}
  )

if (CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BINARY_ROOT "${CMAKE_BINARY_DIR}/$<CONFIG>")
else (CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BINARY_ROOT "${CMAKE_BINARY_DIR}")
endif (CMAKE_CONFIGURATION_TYPES)

if(NOT SHARED_DIR)
  if(NOT WIN32)
    set(SHARED_DIR lib)
  else(NOT WIN32)
    set(SHARED_DIR bin)
  endif(NOT WIN32)
endif(NOT SHARED_DIR)


set(TKHTML_PKGNAME Tkhtml)
set(TKHTML_PKGVERSION "3.0")

# Windows specific flags
if (WIN32)
  set(TKHTML_CFLAGS "${TKHTML_CFLAGS} -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEP -Ot -Oi -fp:strict -Gs -GS -GL -DUSE_TCL_STUBS -DUSE_TK_STUBS -D__WIN32__")
  set(TKHTML_CFLAGS "${TKHTML_CFLAGS} -Dinline=__inline -DBUILD_tkhtml")
endif (WIN32)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TKHTML_CFLAGS}")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/htmldefaultstyle.c
  COMMAND ${TCL_TCLSH} ${CMAKE_CURRENT_SOURCE_DIR}/src/mkdefaultstyle.tcl > ${CMAKE_CURRENT_BINARY_DIR}/htmldefaultstyle.c
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/mkdefaultstyle.tcl ${TCL_TCLSH}
  )

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/htmltokens.c ${CMAKE_CURRENT_BINARY_DIR}/htmltokens.h
  COMMAND ${TCL_TCLSH} ${CMAKE_CURRENT_SOURCE_DIR}/src/tokenlist.txt
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/tokenlist.txt ${TCL_TCLSH}
  )

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cssprop.c ${CMAKE_CURRENT_BINARY_DIR}/cssprop.h
  COMMAND ${TCL_TCLSH} ${CMAKE_CURRENT_SOURCE_DIR}/src/cssprop.tcl
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/htmltokens.c ${CMAKE_CURRENT_SOURCE_DIR}/src/cssprop.tcl ${TCL_TCLSH}
  )

set(TKHTML_SRCS
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
if (TK_SYSTEM_GRAPHICS MATCHES "aqua")
  # Need the X emulation functions from Tk when using aqua - the stubs
  # library doesn't seem to supply them
  target_link_libraries(Tkhtml ${TCL_STUB_LIBRARY} ${TK_STUB_LIBRARY} ${TK_LIBRARY} ${X11_LIBRARIES})
else (TK_SYSTEM_GRAPHICS MATCHES "aqua")
  target_link_libraries(Tkhtml ${TCL_STUB_LIBRARY} ${TK_STUB_LIBRARY} ${X11_LIBRARIES})
endif (TK_SYSTEM_GRAPHICS MATCHES "aqua")

set_target_properties(Tkhtml PROPERTIES VERSION ${TKHTML_PKGVERSION})
set_property(TARGET Tkhtml APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)
set_property(TARGET Tkhtml APPEND PROPERTY COMPILE_DEFINITIONS USE_TK_STUBS)
install(TARGETS Tkhtml
	RUNTIME DESTINATION ${BIN_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${LIB_DIR})

# Create pkgIndex.tcl files
file(GENERATE
	OUTPUT "${CMAKE_BINARY_ROOT}/${LIB_DIR}/${TKHTML_PKGNAME}${TKHTML_PKGVERSION}/pkgIndex.tcl"
  CONTENT "
package ifneeded Tkhtml ${TKHTML_PKGVERSION} [list load [file join $dir ../../${SHARED_DIR} $<TARGET_FILE_NAME:Tkhtml>] Tkhtml]
")
install(FILES "${CMAKE_BINARY_ROOT}/${LIB_DIR}/${TKHTML_PKGNAME}${TKHTML_PKGVERSION}/pkgIndex.tcl" DESTINATION ${LIB_DIR}/${TKHTML_PKGNAME}${TKHTML_PKGVERSION})

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tkhtml.n
  COMMAND ${TCL_TCLSH} ${CMAKE_CURRENT_SOURCE_DIR}/doc/macros.tcl -nroff ${CMAKE_CURRENT_SOURCE_DIR}/doc/html.man > ${CMAKE_CURRENT_BINARY_DIR}/tkhtml.n
  )
add_custom_target(tkhtml_n_gen ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tkhtml.n)

if(NOT MAN_DIR)
  set(MAN_DIR man)
endif(NOT MAN_DIR)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tkhtml.n DESTINATION ${MAN_DIR}/mann)
