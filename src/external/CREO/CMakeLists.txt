# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
if(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
  if ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
    CMAKE_POLICY(SET CMP0026 OLD)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
endif(COMMAND CMAKE_POLICY)

# Set CMake project name
project(CREO2G)


set(BRLCAD_ROOT_DIR "C:/Program Files/BRLCAD 7.XX.X")
set(CREO_ROOT_DIR "C:/Program Files/PTC/Creo XXX/MXXX/Common Files")

# Will need copies of the BRL-CAD dlls along with the creo-brl.dll to make
# a stand-alone package.
set(BRLCAD_LIBS libbu libbn)
foreach(blib ${BRLCAD_LIBS})
  configure_file(${BRLCAD_ROOT_DIR}/bin/${blib}.dll ${CMAKE_CURRENT_BINARY_DIR}/${blib}.dll COPYONLY)
endforeach(blib ${BRLCAD_LIBS})

# We will need to know where nmake.exe and link.exe are
if (NOT DEFINED NMAKE_EXEC)
  get_filename_component(VS_BIN_PATH ${CMAKE_CXX_COMPILER} DIRECTORY)
  set(LINK_EXEC "${VS_BIN_PATH}/link.exe")
  get_filename_component(VS_BIN_PATH ${VS_BIN_PATH} DIRECTORY)
  set(NMAKE_EXEC "${VS_BIN_PATH}/nmake.exe")
endif (NOT DEFINED NMAKE_EXEC)
if (NOT EXISTS "${NMAKE_EXEC}")
  message(FATAL_ERROR "nmake.exe not present: ${NMAKE_EXEC}\nPlease set NMAKE_EXEC to the location of the nmake executable associated with this version of Visual Studio.")
endif (NOT EXISTS "${NMAKE_EXEC}")
if (NOT EXISTS "${LINK_EXEC}")
  message(FATAL_ERROR "link.exe not present: ${LINK_EXEC}\nPlease set LINK_EXEC to the location of the link executable associated with this version of Visual Studio.")
endif (NOT EXISTS "${LINK_EXEC}")

configure_file(creo-brl-nmake.in ${CMAKE_CURRENT_BINARY_DIR}/creo-brl-nmake @ONLY)
configure_file(creo-brl.dat.in ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dat @ONLY)

# List the resource files so we rebuild if one of them changes
file(GLOB RESOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/*)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dll
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/creo-brl.cpp ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.cpp
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/resources
  COMMAND ${NMAKE_EXEC} -f creo-brl-nmake clean
  COMMAND ${NMAKE_EXEC} -f creo-brl-nmake dll
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/creo-brl.cpp ${RESOURCE_FILES}
  )
add_custom_target(CREO-G ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dll)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dll DESTINATION .)

add_custom_target(UNLOCK
  COMMAND "${CREO_ROOT_DIR}/../Parametric/bin/protk_unlock.bat" ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dll
  DEPENDS CREO-G
  COMMENT "Unlocking CREO-G dll..."
  )

set(CPACK_GENERATOR NSIS ZIP)
include(CPack)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
