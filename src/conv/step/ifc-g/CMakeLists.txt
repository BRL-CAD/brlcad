set(IFC_INCLUDE_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${STEPCODE_INCLUDE_DIRS}
  ${OPENNURBS_INCLUDE_DIRS}
  )

link_directories(${CMAKE_BINARY_DIR}/${LIB_DIR})

BRLCAD_INCLUDE_DIRS(IFC_INCLUDE_DIRS)

set(IFC_SCHEMA_FILE ${STEPCODE_DIR}/data/ifc4/IFC4.exp)

GENERATE_SCHEMA_INPUTS(${IFC_SCHEMA_FILE} ifcg)

set(ifcg_SOURCES
  ${express_srcs}
  IFCWrapper.cpp
  ifc-g.cpp
  )

set(ifcg_LIBS
  libwdb
  librt
  libbu
  libbrep
  steputils
  stepeditor
  stepdai
  stepcore
  ${OPENNURBS_LIBRARIES}
  )
list(APPEND ifcg_LIBS ${ifcg_LIBS})

BRLCAD_ADDEXEC(ifc-g "${ifcg_SOURCES}" "${ifcg_LIBS}" NO_STRICT)
add_dependencies(ifc-g ifc-express-${SCHEMA_NAME}-ifcg)

if (HIDE_INTERNAL_SYMBOLS)
  set(ifcg_IMPORTS
    BU_DLL_IMPORTS
    RT_DLL_IMPORTS
    WDB_DLL_IMPORTS
    SC_CORE_DLL_IMPORTS
    SC_DAI_DLL_IMPORTS
    SC_EDITOR_DLL_IMPORTS
    SC_UTILS_DLL_IMPORTS
    )
  list(APPEND ifcg_IMPORTS ${ifcg_IMPORTS})
  set_property(TARGET ifc-g APPEND PROPERTY COMPILE_DEFINITIONS "${ifcg_IMPORTS}")
endif (HIDE_INTERNAL_SYMBOLS)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
