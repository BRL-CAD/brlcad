# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# set CMake project name
project(TKTABLE)

find_package(TCL)

if (WIN32 OR "${TK_SYSTEM_GRAPHICS}" MATCHES "aqua")
  include_directories("${TK_SOURCE_DIR}/xlib/X11")
else ()
  find_package(X11)
endif ()

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
  ${X11_INCLUDE_DIR}
  )

set(TKTABLE_PKGNAME Tktable)
set(TKTABLE_PKGVERSION "2.10")

# Windows specific flags
IF(WIN32)
  set(TKTABLE_CFLAGS "${TKTABLE_CFLAGS} -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEP -Ot -Oi -fp:strict -Gs -GS -GL")
  set(TKTABLE_CFLAGS "${TKTABLE_CFLAGS} -Dinline=__inline -DBUILD_Tktable")
  set(TKTABLE_CFLAGS "${TKTABLE_CFLAGS} -DTBL_COMMAND=\"table\" -DTBL_RUNTIME=\"tkTable.tcl\" -DTBL_RUNTIME_DIR=\"\"")
ENDIF(WIN32)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TKTABLE_CFLAGS}")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tkTable.tcl.h
  COMMAND ${TCL_TCLSH} ${CMAKE_CURRENT_SOURCE_DIR}/misc/tkTable_header.tcl ${CMAKE_CURRENT_SOURCE_DIR}/library/tkTable.tcl ${CMAKE_CURRENT_BINARY_DIR}/tkTable.tcl.h.new
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/tkTable.tcl.h.new ${CMAKE_CURRENT_BINARY_DIR}/tkTable.tcl.h
  COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/tkTable.tcl.h.new
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/library/tkTable.tcl ${CMAKE_CURRENT_SOURCE_DIR}/misc/tkTable_header.tcl ${TCL_TCLSH}
  )
add_custom_target(tktable_header_gen ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tkTable.tcl.h)

set(TKTABLE_SRCS
  generic/tkTable.c
  generic/tkTableCell.c
  generic/tkTableCellSort.c
  generic/tkTableCmds.c
  generic/tkTableEdit.c
  #generic/tkTablePs.c
  generic/tkTableTag.c
  generic/tkTableUtil.c
  generic/tkTableWin.c
  )

set(TKTABLE_CFG_H_FILE ${CMAKE_CURRENT_BINARY_DIR}/tktable_cfg.h.tmp)
file(WRITE ${TKTABLE_CFG_H_FILE} "/**** Variables for Tktable build - replaces -D options ****/\n")
file(APPEND ${TKTABLE_CFG_H_FILE} "#define PACKAGE_NAME \"${TKTABLE_PKGNAME}\"\n")
file(APPEND ${TKTABLE_CFG_H_FILE} "#define PACKAGE_VERSION \"${TKTABLE_PKGVERSION}\"\n")
file(APPEND ${TKTABLE_CFG_H_FILE} "#define TBL_COMMAND \"table\"\n")
file(APPEND ${TKTABLE_CFG_H_FILE} "#define TBL_RUNTIME \"tkTable.tcl\"\n")
file(APPEND ${TKTABLE_CFG_H_FILE} "#define TBL_RUNTIME_DIR \"${CMAKE_INSTALL_PREFIX}/lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION}\"\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/tktable_cfg.h.tmp ${CMAKE_CURRENT_BINARY_DIR}/tktable_cfg.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/tktable_cfg.h.tmp)


add_definitions(
  -DSTDC_HEADERS=1
  )

add_library(Tktable SHARED ${TKTABLE_SRCS})
add_dependencies(Tktable tktable_header_gen)
target_link_libraries(Tktable ${TCL_LIBRARY} ${TK_LIBRARY} ${X11_LIBRARIES})
set_target_properties(Tktable PROPERTIES VERSION ${TKTABLE_PKGVERSION})
install(TARGETS Tktable
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

# Create the pkgIndex.tcl file.
include(TCL_PKGINDEX)
TCL_PKGINDEX(Tktable ${TKTABLE_PKGNAME} ${TKTABLE_PKGVERSION})

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.txt DESTINATION lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/license.txt DESTINATION lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION})

set(TKTABLE_MISC
  library/tkTable.tcl
  library/tktable.py
  )
if(CMAKE_CONFIGURATION_TYPES)
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    file(COPY ${TKTABLE_MISC} DESTINATION ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION})
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
else(CMAKE_CONFIGURATION_TYPES)
  file(COPY ${TKTABLE_MISC} DESTINATION ${CMAKE_BINARY_DIR}/lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION})
endif(CMAKE_CONFIGURATION_TYPES)

install(FILES ${TKTABLE_MISC} DESTINATION lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
