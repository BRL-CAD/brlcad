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


# TODO - clear compiler and linker flags

# User must locate CREO and BRL-CAD
set(BRLCAD_ROOT_DIR "C:/Program Files/BRLCAD 7.XX.X")
set(CREO_ROOT_DIR "C:/Program Files/PTC/Creo XXX")

# There will be a sub-directory that is specific to the installed
# version of CREO - find it
file(GLOB MDIR RELATIVE "${CREO_ROOT_DIR}" "${CREO_ROOT_DIR}/M*" )

# Set the relevant include directories
include_directories(
  "${BRLCAD_ROOT_DIR}/include/brlcad"
  "${CREO_ROOT_DIR}/${MDIR}/Common Files/protoolkit/includes"
  "${CREO_ROOT_DIR}/${MDIR}/Common Files/protoolkit/protk_appls/includes"
  )


# Win32 libraries
set(WIN_LIBS
  libcmt.lib
  kernel32.lib
  user32.lib
  wsock32.lib
  advapi32.lib
  mpr.lib
  winspool.lib
  netapi32.lib
  psapi.lib
  gdi32.lib
  shell32.lib
  comdlg32.lib
  ole32.lib
  ws2_32.lib
  )

# for libcmt
add_definitions(/NODEFAULTLIB:msvcrt.lib)

# CREO libraries
set(CREO_EXE_LIBS "${CREO_ROOT_DIR}/${MDIR}/Common Files/protoolkit/x86e_win64/obj/protoolkit.lib")
set(CREO_DLL_LIBS "${CREO_ROOT_DIR}/${MDIR}/Common Files/protoolkit/x86e_win64/obj/protk_dll.lib")

# We will need copies of the BRL-CAD dlls along with the creo-brl.dll to make
# a stand-alone package.
set(BRLCAD_LIBS libbu libbn y2038 regex)
set(BRLCAD_STATIC_LIBS
foreach(blib ${BRLCAD_LIBS})
  configure_file(${BRLCAD_ROOT_DIR}/bin/${blib}.dll ${CMAKE_CURRENT_BINARY_DIR}/${blib}.dll COPYONLY)
  set(BRLCAD_STATIC_LIBS ${BRLCAD_STATIC_LIBS} "${BRLCAD_ROOT_DIR}/lib/${blib}.lib")
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${blib}.dll DESTINATION "${MDIR}/Common Files/x86e_win64/obj")
endforeach(blib ${BRLCAD_LIBS})


# CREO Definitions (see example nmake file in your CREO install to
# double check these and modify if needed)
add_definitions(-DPRO_MACHINE=36 -DPRO_OS=4 -DPRO_USE_VAR_ARGS -D_USING_V100_SDK71_)

# BRL-CAD definitions
add_definitions(-DHAVE_CONFIG_H -DBRLCAD_DLL -DBRLCADBUILD
  -DBU_DLL_IMPORTS
  -DBN_DLL_IMPORTS
  )

# Primary (DLL) version of plugin
# Note that for successful running of the DLL version of this plugin,
# the BRL-CAD dlls need to be present in Common Files/x86e_win64/obj.  Also,
# the resource files need to be present in Common Files/text/resource and
# creo-brl-msg.txt present in Common Files/text/usascii before this will be
# usable.  The simplest thing to do is build a package and install it in the
# CREO root directory.  *** Any attempt run this plugin without the correct
# DLLs in place will result in silent failure of the loading step. ***
add_library(creo-brl SHARED creo-brl.cpp)
target_link_libraries(creo-brl ${WIN_LIBS} ${CREO_DLL_LIBS} ${BRLCAD_STATIC_LIBS})
install(creo-brl DESTINATION "${MDIR}/Common Files/x86e_win64/obj")
configure_file(creo-brl.dat.in ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dat @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dat DESTINATION "${MDIR}/Common Files/protoolkit")

# Debugging executable version of plugin
add_executable(creo-g creo-brl.cpp)
target_link_libraries(creo-brl ${WIN_LIBS} ${CREO_EXE_LIBS} ${BRLCAD_STATIC_LIBS})
configure_file(creo-brl-debug.dat.in ${CMAKE_CURRENT_BINARY_DIR}/creo-brl-debug.dat @ONLY)

# Install the resource files
file(GLOB RESOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.res)
foreach(rfile ${RESOURCE_FILES})
  install(FILES ${rfile} DESTINATION "${MDIR}/Common Files/text/resource")
endforeach(rfile ${RESOURCE_FILES})

# Install the messages file
install(FILES resources/creo-brl-msg.txt DESTINATION "${MDIR}/Common Files/text/usascii")


add_custom_target(UNLOCK
  COMMAND "${CREO_ROOT_DIR}/${MDIR}/Parametric/bin/protk_unlock.bat" ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dll
  DEPENDS CREO-G
  COMMENT "Unlocking CREO-G dll..."
  )

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
set(CPACK_GENERATOR NSIS ZIP)
include(CPack)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
