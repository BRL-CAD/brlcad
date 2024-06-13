link_directories(${CMAKE_BINARY_DIR}/${LIB_DIR})

set(STEP_SCHEMA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../step-g/ap203.exp)
add_definitions(-DAP203)

GENERATE_SCHEMA_INPUTS(${STEP_SCHEMA_FILE} list_elements)

set(list_elements_srcs ${express_srcs} list_elements.cpp)

if (BRLCAD_ENABLE_STEP)
  set(lelem_LIBS
    libbu
    ${STEPCODE_UTILS_LIBRARY}
    ${STEPCODE_EDITOR_LIBRARY}
    ${STEPCODE_DAI_LIBRARY}
    ${STEPCODE_CORE_LIBRARY}
    )

  set(lelem_INCLUDE_DIRS
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${STEPCODE_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src/libbrep # for PullbackCurve.h
    )

  BRLCAD_ADDEXEC(list_elements "${list_elements_srcs}" "${lelem_LIBS}" NO_STRICT NO_INSTALL)
  target_include_directories(list_elements BEFORE PRIVATE ${lelem_INCLUDE_DIRS})
  add_dependencies(list_elements step-express-${SCHEMA_NAME}-list_elements)
  set_property(TARGET list_elements APPEND PROPERTY INCLUDE_DIRECTORIES "${list_elements_SCHEMA_OUT_DIR}")
  target_compile_definitions(list_elements PRIVATE SC_SCHEMA_DLL_EXPORTS)

  if (HIDE_INTERNAL_SYMBOLS)
    set(list_elements_IMPORTS
      BU_DLL_IMPORTS
      SC_CORE_DLL_IMPORTS
      SC_DAI_DLL_IMPORTS
      SC_EDITOR_DLL_IMPORTS
      SC_UTILS_DLL_IMPORTS
      )
    set_property(TARGET list_elements APPEND PROPERTY COMPILE_DEFINITIONS "${list_elements_IMPORTS}")
  endif (HIDE_INTERNAL_SYMBOLS)
endif (BRLCAD_ENABLE_STEP)

CMAKEFILES(
  CMakeLists.txt
  ${list_elements_srcs}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
