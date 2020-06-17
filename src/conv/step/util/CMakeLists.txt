include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

set(STEP_SCHEMA_FILE ${STEPCODE_DIR}/data/ap203/ap203.exp)
add_definitions(-DAP203)

set(lelem_LIBS
  libbrep
  libbn
  libbu
  steputils
  stepeditor
  stepdai
  stepcore
  ${OPENNURBS_LIBRARIES}
  )

GENERATE_SCHEMA_INPUTS(${STEP_SCHEMA_FILE} list_elements)

set(list_elements_srcs ${express_srcs} list_elements.cpp)
BRLCAD_ADDEXEC(list_elements "${list_elements_srcs}" "${lelem_LIBS}" NO_STRICT NO_INSTALL)
add_dependencies(list_elements step-express-${SCHEMA_NAME}-list_elements)
set_property(TARGET list_elements APPEND PROPERTY INCLUDE_DIRECTORIES "${list_elements_SCHEMA_OUT_DIR}")

if (HIDE_INTERNAL_SYMBOLS)
  set(list_elements_IMPORTS
    BU_DLL_IMPORTS
    BN_DLL_IMPORTS
    RT_DLL_IMPORTS
    WDB_DLL_IMPORTS
    SC_CORE_DLL_IMPORTS
    SC_DAI_DLL_IMPORTS
    SC_EDITOR_DLL_IMPORTS
    SC_UTILS_DLL_IMPORTS
    )
  list(APPEND list_elements_IMPORTS ${list_elements_IMPORTS})
  set_property(TARGET list_elements APPEND PROPERTY COMPILE_DEFINITIONS "${list_elements_IMPORTS}")
endif (HIDE_INTERNAL_SYMBOLS)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
