brlcad_find_package(OSMESA REQUIRED)

if(BRLCAD_ENABLE_TK)
  set(TCL_ENABLE_TK ON)
  find_package_tcl(TCL)

  if(TCL_FOUND AND TK_FOUND)
    set(TKSWRAST_INCLUDE_DIRS
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/..
      ${BRLCAD_BINARY_DIR}/include
      ${BRLCAD_SOURCE_DIR}/include
      ${BRLCAD_SOURCE_DIR}/src/libbu
      ${BU_INCLUDE_DIRS}
      ${OSMESA_INCLUDE_DIR}
      ${TCL_INCLUDE_PATH}
      ${TK_INCLUDE_PATH}
    )

    set(TKSWRAST_LIBS
      libdm
      libbu
      ${OSMESA_LIBRARY}
      ${TCL_LIBRARY}
      ${TK_LIBRARY}
    )
    set(TKSWRAST_COMPILE_DEFINITIONS)
    list(APPEND TKSWRAST_COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H HAVE_TK OSMESA)

    # If Tk is using X11, we need the X11 headers/libs as well.  Aqua Tk
    # builds do not need or want these.
    if("${TK_WINDOWING_SYSTEM}" STREQUAL "x11")
      list(APPEND TKSWRAST_INCLUDE_DIRS ${BRLCAD_X11_INCLUDE_DIRS})
      list(APPEND TKSWRAST_LIBS ${BRLCAD_X11_LIBRARIES})
      list(APPEND TKSWRAST_COMPILE_DEFINITIONS TKSWRAST_X11)
    endif()

    include_directories(${TKSWRAST_INCLUDE_DIRS})

    add_definitions(-DDM_PLUGIN)
    add_definitions(-DUSE_MGL_NAMESPACE)

    set(TKSWRAST_SRCS dm-tkswrast.cpp)

    dm_plugin_library(dm-tkswrast SHARED ${TKSWRAST_SRCS})
    target_link_libraries(dm-tkswrast ${TKSWRAST_LIBS})
    set_property(TARGET dm-tkswrast APPEND PROPERTY COMPILE_DEFINITIONS ${TKSWRAST_COMPILE_DEFINITIONS})
    validate_style(dm-tkswrast "${TKSWRAST_SRCS}")

    plugin_setup(dm-tkswrast dm)
  endif(TCL_FOUND AND TK_FOUND)
endif(BRLCAD_ENABLE_TK)

cmakefiles(
  CMakeLists.txt
  dm-tkswrast.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 cino=N-s
