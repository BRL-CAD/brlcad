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

# clear and reset compiler and linker flags
set(flaglists
  CMAKE_C_FLAGS
  CMAKE_CXX_FLAGS
  CMAKE_SHARED_LINKER_FLAGS
  CMAKE_STATIC_LINKER_FLAGS
  CMAKE_MODULE_LINKER_FLAGS
  CMAKE_EXE_LINKER_FLAGS
  )
foreach(bflag ${flaglists})
  set(${bflag} "" CACHE STRING "" FORCE)
  foreach(BTYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${BTYPE}" BTYPE_UPPER)
    set(${bflag}_${BTYPE_UPPER} "" CACHE STRING "" FORCE)
  endforeach(BTYPE ${CMAKE_CONFIGURATION_TYPES})
endforeach(bflag ${flaglists})

foreach(flist ${flaglists})
  string(REPLACE "/MD" "/MT" ${flist} "${${flist}}")
endforeach(flist ${flaglists})

foreach(CT ${CMAKE_CONFIGURATION_TYPES})
  set(CR ${CREO2G_BINARY_DIR})
  string(TOUPPER "${CT}" CT_UPPER)
  set("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CT_UPPER}" "${CR}" CACHE INTERNAL "")
  set("CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CT_UPPER}" "${CR}" CACHE INTERNAL "")
  set("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CT_UPPER}" "${CR}" CACHE INTERNAL "")
  set("CMAKE_BINARY_DIR_${CT_UPPER}" "${CR}" CACHE INTERNAL "")
  set("CREO2G_BINARY_DIR_${CT_UPPER}" ${CR} CACHE INTERNAL "")
endforeach(CT ${CMAKE_CONFIGURATION_TYPES})

# We're not set up to support multiple VS configs - limit our options
set(CMAKE_CONFIGURATION_TYPES Release CACHE INTERNAL "Constrained configuration set")

# Install dir is a function of the CREO dir.
mark_as_advanced(CMAKE_INSTALL_PREFIX)

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
set(CREO_PTK_APPLS_INCDIR "${CREO_ROOT_DIR}/${MDIR}/Common Files/protoolkit/protk_appls/includes")


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

# When doing development, to avoid the need to unlock dlls and exes every
# time for testing, you need to configure your parametric.psf file - see
# https://www.ptcusercommunity.com/message/399253#399253
#
# This is necessary only for iterative development: for compilation the
# program can just be built and unlocked (the latter step being necessary
# prior to distribution in any case.)  Since it is not always needed,
# we will check and print a warning to alert developers but will not
# hault the configure process.
if(EXISTS "${CREO_ROOT_DIR}/${MDIR}/Parametric/bin/parametric.psf")
  file(STRINGS "${CREO_ROOT_DIR}/${MDIR}/Parametric/bin/parametric.psf" psf_strs)
  foreach(pstr ${psf_strs})
    if("${pstr}" MATCHES ".*CREOPMA_FEATURE_NAME.*")
      set(CFN_STR "${pstr}")
      string(REGEX REPLACE ".*CREOPMA_FEATURE_NAME.*[(]" "" CFN_STR "${CFN_STR}")
      string(REGEX REPLACE "[)].*" "" CFN_STR "${CFN_STR}")
      string(STRIP "${CFN_STR}" CFN_STR)
      if("${CFN_STR}" STREQUAL "")
	message(WARNING "The CREOPMA_FEATURE_NAME variable in ${CREO_ROOT_DIR}/${MDIR}/Parametric/bin/parametric.psf has no arguments.  This means it is not set up to load a Pro/Toolkit license when started, and the plugin built by this project must be unlocked before it can be tested.  See https://www.ptcusercommunity.com/message/399253#399253 for more info.  This is a concern only for developers wanting to do iterative development without repeating the unlock step every time.  If you are only looking to compile and use the plugin, this warning may be safely ignored as long as the UNLOCK step is performed.")
      endif("${CFN_STR}" STREQUAL "")
    endif("${pstr}" MATCHES ".*CREOPMA_FEATURE_NAME.*")
  endforeach(pstr ${psf_strs})
endif(EXISTS "${CREO_ROOT_DIR}/${MDIR}/Parametric/bin/parametric.psf")


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



# We will need copies of the BRL-CAD dlls along with the creo-brl.dll to make
# a stand-alone package.
set(BRLCAD_LIBS libbu libbn y2038 regex)
set(BRLCAD_STATIC_LIBS)
foreach(blib ${BRLCAD_LIBS})
  configure_file(${BRLCAD_ROOT_DIR}/bin/${blib}.dll ${CMAKE_CURRENT_BINARY_DIR}/${blib}.dll COPYONLY)
  set(BRLCAD_STATIC_LIBS ${BRLCAD_STATIC_LIBS} "${BRLCAD_ROOT_DIR}/lib/${blib}.lib")
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${blib}.dll DESTINATION "${MDIR}/${CREO_OBJ_DIR}")
  if(NOT EXISTS ${CREO_ROOT_DIR}/${MDIR}/${CREO_OBJ_DIR}/${blib}.dll)
    message("Note: ${blib}.dll is not present in ${CREO_ROOT_DIR}/${MDIR}/${CREO_OBJ_DIR}\nBe aware that attempting to load the DLL version of the plugin without this file in place will\ncause the loading process to silently fail: *NO* messages indicating why the loading failed will\nbe generated.\n")
  endif(NOT EXISTS ${CREO_ROOT_DIR}/${MDIR}/${CREO_OBJ_DIR}/${blib}.dll)
endforeach(blib ${BRLCAD_LIBS})


# CREO Definitions (see example nmake file in your CREO install to
# double check these and modify if needed)
set(CREO_DEFS "-DPRO_MACHINE=36 -DPRO_OS=4 -DPRO_USE_VAR_ARGS -D_USING_V100_SDK71_")

# for libcmt
#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT")
add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)

# BRL-CAD definitions
set(BRLCAD_DEFS "-DHAVE_CONFIG_H -DBRLCAD_DLL -DBRLCADBUILD -DBU_DLL_IMPORTS -DBN_DLL_IMPORTS")

set(CMAKE_C_FLAGS "-c -GS -fp:precise /MT ${CREO_DEFS} ${BRLCAD_DEFS}")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS "/subsystem:console /debug:none /machine:amd64")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/subsystem:console /debug /machine:amd64")

# Set the relevant include directories
include_directories(
  "${BRLCAD_ROOT_DIR}/include/brlcad"
  "${CREO_PTK_INCDIR}"
  "${CREO_PTK_APPLS_INCDIR}"
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
target_link_libraries(creo-brl ${WIN_LIBS} ${CREO_PTK_DLL_LIBS} ${BRLCAD_STATIC_LIBS})
install(TARGETS creo-brl DESTINATION "${MDIR}/${CREO_OBJ_DIR}")
configure_file(creo-brl.dat.in ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dat @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dat DESTINATION "${MDIR}/${CREO_DAT_DIR}")

# Debugging executable version of plugin
add_executable(creo-g creo-brl.cpp)
target_link_libraries(creo-g ${WIN_LIBS} ${CREO_PTK_EXE_LIBS} ${BRLCAD_STATIC_LIBS})
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
