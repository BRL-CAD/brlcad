if(BRLCAD_ENABLE_QT)
  include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}
    ${BRLCAD_SOURCE_DIR}/include
    ${GED_INCLUDE_DIRS}
    ${DM_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    ${BU_INCLUDE_DIRS}
    )
endif(BRLCAD_ENABLE_QT)

# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(dm_srcs
  main.cpp
  main_window.cpp
  dmapp.cpp
  dmgl.cpp
  )

set(dm_moc_headers
  main_window.h
  dmapp.h
  dmgl.h
  )

if(BRLCAD_ENABLE_QT)
  QT5_WRAP_CPP(dm_moc_srcs ${dm_moc_headers})
  BRLCAD_ADDEXEC(qdm "${dm_srcs};${dm_moc_srcs}" "libged;libdm;librt;libbu;${OPENGL_LIBRARIES};Qt5::Core;Qt5::Widgets;Qt5::OpenGL")
endif(BRLCAD_ENABLE_QT)

set(dm_ignore_srcs
  CMakeLists.txt
  ${dm_srcs}
  ${dm_moc_headers}
  )

CMAKEFILES(${dm_ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

