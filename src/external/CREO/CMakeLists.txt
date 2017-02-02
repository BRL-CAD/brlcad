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

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CONFIGURATION_TYPES Release CACHE INTERNAL "Constrained configuration set")
mark_as_advanced(CMAKE_INSTALL_PREFIX)

# TODO - clear compiler and linker flags

# User must locate CREO and BRL-CAD
if(NOT DEFINED BRLCAD_ROOT_DIR)
  set(BRLCAD_ROOT_DIR "C:/Program Files/BRLCAD X.XX.X" CACHE PATH "BRL-CAD root directory")
endif(NOT DEFINED BRLCAD_ROOT_DIR)
if(NOT DEFINED CREO_ROOT_DIR)
  set(CREO_ROOT_DIR "C:/Program Files/PTC/Creo XXX" CACHE PATH "CREO root directory")
endif(NOT DEFINED CREO_ROOT_DIR)

set(have_paths 1)
if(NOT EXISTS "${BRLCAD_ROOT_DIR}")
  set(have_paths 0)
  message(WARNING "${BRLCAD_ROOT_DIR} doesn't exist - set BRLCAD_ROOT_DIR to the location of your BRL-CAD installation.")
endif(NOT EXISTS "${BRLCAD_ROOT_DIR}")
if(NOT EXISTS "${CREO_ROOT_DIR}")
  set(have_paths 0)
  message(WARNING "${CREO_ROOT_DIR} doesn't exist - set CREO_ROOT_DIR to the location of your CREO installation.")
endif(NOT EXISTS "${CREO_ROOT_DIR}")
if(NOT have_paths)
  message(FATAL_ERROR "Correct path settings to continue with configure")
endif(NOT have_paths)

set(CMAKE_INSTALL_PREFIX "${CREO_ROOT_DIR}" CACHE PATH "Installation path" FORCE)

# There will be a sub-directory that is specific to the installed
# version of CREO - find it
file(GLOB MDIR RELATIVE "${CREO_ROOT_DIR}" "${CREO_ROOT_DIR}/M*" )
if(NOT MDIR)
  message(FATAL_ERROR "No directory matching the pattern M* found in ${CREO_ROOT_DIR}.  This may indicate the CREO installation directory conventions have changed - if so, the CMakeLists.txt file will need to be updated to reflect the new path structures.")
endif(NOT MDIR)

# Define a variable for the current CREO platform
set(CREO_OS_VERSION x86e_win64)

# Unlocker script
set(CREO_PTK_UNLOCKER "${CREO_ROOT_DIR}/${MDIR}/Parametric/bin/protk_unlock.bat")

# CREO libraries
set(CREO_PTK_EXE_LIBS "${CREO_ROOT_DIR}/${MDIR}/Common Files/protoolkit/${CREO_OS_VERSION}/obj/protoolkit.lib")
set(CREO_PTK_DLL_LIBS "${CREO_ROOT_DIR}/${MDIR}/Common Files/protoolkit/${CREO_OS_VERSION}/obj/protk_dll.lib")

# CREO include directories
set(CREO_PTK_INCDIR "${CREO_ROOT_DIR}/${MDIR}/Common Files/protoolkit/includes")
set(CREO_PTK_APPLS_INCDIR "${CREO_ROOT_DIR}/${MDIR}/Common Files/protoolkit/includes")


# Check that the expected directories and files in the CREO installation
# are present. If not, these paths need to be updated for the current CREO
# layout.
set(have_creo_paths 1)
set(CREO_REQUIRED_PATHS_ABS
  CREO_PTK_UNLOCKER
  CREO_PTK_EXE_LIBS
  CREO_PTK_DLL_LIBS
  CREO_PTK_INCDIR
  CREO_PTK_APPLS_INCDIR
  )
foreach(reqpath ${CREO_REQUIRED_PATHS_ABS})
  if(NOT EXISTS "${${reqpath}}")
    set(have_creo_paths 0)
    message(WARNING "Required CREO path variable ${reqpath} has non-existant path ${${reqpath}} - update ${reqpath} to reflect the correct path for this installation of CREO.")
  endif(NOT EXISTS "${${reqpath}}")
endforeach(reqpath ${CREO_REQUIRED_PATHS_ABS})

# CREO resource paths
set(CREO_RESOURCE_DIR "Common Files/text/resource")
set(CREO_MSG_DIR "Common Files/text/usascii")
set(CREO_OBJ_DIR "Common Files/${CREO_OS_VERSION}/obj")
set(CREO_DAT_DIR "Common Files/protoolkit")

set(CREO_REQUIRED_PATHS_REL
  CREO_RESOURCE_DIR
  CREO_MSG_DIR
  CREO_OBJ_DIR
  CREO_DAT_DIR
)
foreach(reqpath ${CREO_REQUIRED_PATHS_REL})
  if(NOT EXISTS "${CREO_ROOT_DIR}/${MDIR}/${${reqpath}}")
    set(have_creo_paths 0)
    message(WARNING "Required CREO path variable ${reqpath} contains ${${reqpath}}, which is not found at ${CREO_ROOT_DIR}/${MDIR}/${${reqpath}} - update ${reqpath} to reflect the correct path for this installation of CREO.")
  endif(NOT EXISTS "${CREO_ROOT_DIR}/${MDIR}/${${reqpath}}")
endforeach(reqpath ${CREO_REQUIRED_PATHS_REL})

if(NOT have_creo_paths)
  message(FATAL_ERROR "Update the above variables to reflect correct CREO paths to continue with configure.  If paths containing \"protoolkit\" are consistently not found, you may need to re-run your CREO installer and add the development frameworks - they are not installed as part of a \"standard\" CREO installation.")
endif(NOT have_creo_paths)

# Set the relevant include directories
include_directories(
  "${BRLCAD_ROOT_DIR}/include/brlcad"
  "${CREO_PTK_INCDIR}"
  "${CREO_PTK_APPLS_INCDIR}"
  )

# Win32 libraries
set(WIN_LIBS
  #libcmt.lib
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

set(flaglists
  CMAKE_CXX_FLAGS
  CMAKE_CXX_FLAGS_DEBUG
  CMAKE_CXX_FLAGS_RELEASE
  CMAKE_C_FLAGS
  CMAKE_C_FLAGS_DEBUG
  CMAKE_C_FLAGS_RELEASE
  )
foreach(flist ${flaglists})
  string(REPLACE "/MD" "/MT" ${flist} "${${flist}}")
endforeach(flist ${flaglists})

# We will need copies of the BRL-CAD dlls along with the creo-brl.dll to make
# a stand-alone package.
set(BRLCAD_LIBS libbu libbn y2038 regex)
set(BRLCAD_STATIC_LIBS)
foreach(blib ${BRLCAD_LIBS})
  configure_file(${BRLCAD_ROOT_DIR}/bin/${blib}.dll ${CMAKE_CURRENT_BINARY_DIR}/${blib}.dll COPYONLY)
  set(BRLCAD_STATIC_LIBS ${BRLCAD_STATIC_LIBS} "${BRLCAD_ROOT_DIR}/lib/${blib}.lib")
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${blib}.dll DESTINATION "${MDIR}/${CREO_OBJ_DIR}")
endforeach(blib ${BRLCAD_LIBS})


# CREO Definitions (see example nmake file in your CREO install to
# double check these and modify if needed)
add_definitions(-DPRO_MACHINE=36 -DPRO_OS=4 -DPRO_USE_VAR_ARGS -D_USING_V100_SDK71_)

# for libcmt
#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT")
add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)

# BRL-CAD definitions
add_definitions(-DHAVE_CONFIG_H -DBRLCAD_DLL -DBRLCADBUILD
  -DBU_DLL_IMPORTS
  -DBN_DLL_IMPORTS
  )

# Primary (DLL) version of plugin
# Note that for successful running of the DLL version of this plugin,
# the BRL-CAD dlls need to be present in Common Files/${CREO_OS_VERSION}/obj.  Also,
# the resource files need to be present in Common Files/text/resource and
# creo-brl-msg.txt present in Common Files/text/usascii before this will be
# usable.  The simplest thing to do is build a package and install it in the
# CREO root directory.  *** Any attempt run this plugin without the correct
# DLLs in place will result in silent failure of the loading step. ***
add_library(creo-brl SHARED creo-brl.cpp)
target_link_libraries(creo-brl ${WIN_LIBS} ${CREO_DLL_LIBS} ${BRLCAD_STATIC_LIBS})
install(TARGETS creo-brl DESTINATION "${MDIR}/${CREO_OBJ_DIR}")
configure_file(creo-brl.dat.in ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dat @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dat DESTINATION "${MDIR}/${CREO_DAT_DIR}")

# Debugging executable version of plugin
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /debug")
add_executable(creo-g creo-brl.cpp)
target_link_libraries(creo-g ${WIN_LIBS} ${CREO_EXE_LIBS} ${BRLCAD_STATIC_LIBS})
configure_file(creo-brl-debug.dat.in ${CMAKE_CURRENT_BINARY_DIR}/creo-brl-debug.dat @ONLY)

# Install the resource files
file(GLOB RESOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.res)
foreach(rfile ${RESOURCE_FILES})
  install(FILES ${rfile} DESTINATION "${MDIR}/${CREO_RESOURCE_DIR}")
endforeach(rfile ${RESOURCE_FILES})

# Install the messages file
install(FILES resources/creo-brl-msg.txt DESTINATION "${MDIR}/${CREO_MSG_DIR}")

add_custom_target(UNLOCK
  COMMAND "${CREO_PTK_UNLOCKER}" ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dll
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
