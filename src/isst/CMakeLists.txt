if(BRLCAD_ENABLE_QT)
  include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../adrt
    ${CMAKE_CURRENT_SOURCE_DIR}/../adrt/librender
    ${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}
    ${BRLCAD_SOURCE_DIR}/include
    ${GED_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    ${BU_INCLUDE_DIRS}
    )
endif(BRLCAD_ENABLE_QT)

# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(isst_srcs
  main.cpp
  main_window.cpp
  isstapp.cpp
  isstgl.cpp
  )

set(moc_headers
  main_window.h
  isstapp.h
  isstgl.h
  )

if(BRLCAD_ENABLE_QT AND NOT BRLCAD_ENABLE_TCL)
  QT5_WRAP_CPP(moc_srcs ${moc_headers})
  BRLCAD_ADDEXEC(isst "${isst_srcs};${moc_srcs}" "libbu;librt;libgcv;librender;${OPENGL_LIBRARIES};Qt5::Core;Qt5::Widgets;Qt5::OpenGL")
endif(BRLCAD_ENABLE_QT AND NOT BRLCAD_ENABLE_TCL)

set(isst_ignore_srcs
  CMakeLists.txt
  ${isst_srcs}
  ${moc_headers}
  )

CMAKEFILES(${isst_ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

