
if(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)
  # Minimum required version of CMake
  cmake_minimum_required(VERSION 3.12)

  # Set CMake project name
  project(CREO2BRL)

  # Version (TODO - display this in the plugin somehow...)
  set(CREO2BRL_VERSION_MAJOR 0)
  set(CREO2BRL_VERSION_MINOR 2)
  set(CREO2BRL_VERSION_PATCH 4)

  # Tweak compiler flags - we *must* use /MT for this to work
  set(flaglists
    CMAKE_C_FLAGS
    CMAKE_CXX_FLAGS
    )
  foreach(bflag ${flaglists})
    set(${bflag} "" CACHE STRING "" FORCE)
    foreach(BTYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${BTYPE}" BTYPE_UPPER)
      string(REPLACE "/MD" "/MT" ${bflag}_${BTYPE_UPPER} "${${bflag}_${BTYPE_UPPER}}")
      string(REPLACE "/MTd" "/MT" ${bflag}_${BTYPE_UPPER} "${${bflag}_${BTYPE_UPPER}}")
      # _DEBUG pulls in the wrong runtime for linking with CREO libs
      string(REPLACE "/D_DEBUG" "" ${bflag}_${BTYPE_UPPER} "${${bflag}_${BTYPE_UPPER}}")
      string(REPLACE "/D _DEBUG" "" ${bflag}_${BTYPE_UPPER} "${${bflag}_${BTYPE_UPPER}}")
      set(${bflag}_${BTYPE_UPPER} "${${bflag}_${BTYPE_UPPER}}" CACHE STRING "" FORCE)
    endforeach(BTYPE ${CMAKE_CONFIGURATION_TYPES})
  endforeach(bflag ${flaglists})
  mark_as_advanced(CMAKE_CONFIGURATION_TYPES)

  # Install dir is a function of the CREO dir.
  mark_as_advanced(CMAKE_INSTALL_PREFIX)

  # CREO Definitions (see example nmake file in your CREO install to
  # double check these and modify if needed)
  set(CREO_DEFS "-DPRO_MACHINE=36 -DPRO_OS=4 -DPRO_USE_VAR_ARGS -D_USING_V100_SDK71_")

  # BRL-CAD definitions
  set(BRLCAD_DEFS "-DHAVE_CONFIG_H -DBRLCAD_DLL -DBRLCADBUILD -DBU_DLL_IMPORTS -DBN_DLL_IMPORTS -DBV_DLL_IMPORTS -DNMG_DLL_IMPORTS -DBG_DLL_IMPORTS -DBREP_DLL_IMPORTS -DRT_DLL_IMPORTS -DWDB_DLL_IMPORTS -DTIE_DLL_IMPORTS -DDB5_DLL_IMPORTS -DOPENNURBS_IMPORTS")

  # These settings are global to all the configs.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CREO_DEFS} ${BRLCAD_DEFS}" CACHE STRING "" FORCE)
  set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "" FORCE)
  set(CMAKE_SHARED_LINKER_FLAGS "/subsystem:console /machine:amd64" CACHE STRING "" FORCE)
  set(CMAKE_EXE_LINKER_FLAGS "/subsystem:console /machine:amd64" CACHE STRING "" FORCE)

  # User must locate CREO and BRL-CAD
  if(NOT DEFINED BRLCAD_ROOT_DIR)
    set(BRLCAD_ROOT_DIR "C:/Program Files/BRLCAD X.XX.X" CACHE PATH "BRL-CAD root directory")
  endif(NOT DEFINED BRLCAD_ROOT_DIR)
  if(NOT DEFINED CREO_ROOT_DIR)
    set(CREO_ROOT_DIR "C:/Program Files/PTC/Creo XXX" CACHE PATH "CREO root directory")
  endif(NOT DEFINED CREO_ROOT_DIR)

  # If paths haven't been set, hault configure immediately so the user
  # can set them.  We need this information for subsequent steps.
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

  # Install dir is a function of the CREO dir.
  set(CMAKE_INSTALL_PREFIX "${CREO_ROOT_DIR}" CACHE PATH "Installation path" FORCE)

  # CREO supports a specific toolset.  Based on the version, check
  # if we are using the supported toolset and warn if we aren't.
  string(REGEX REPLACE "[A-Za-z:/ ]*([0-9.]+).*" "\\1" CREO_VERSION "${CREO_ROOT_DIR}")
  if("${CREO_VERSION}" VERSION_GREATER "2.9999" AND "${CREO_VERSION}" VERSION_LESS "4.0")
    if(NOT "${CMAKE_GENERATOR_TOOLSET}" STREQUAL "v110_xp")
      message(WARNING "CREO version is ${CREO_VERSION}, but supplied generator toolset is not set to v110_xp.  To use v110_xp, specify it as the -T option in the inital CMake GUI dialog thrown up by \"Configure\" below the dropdown menu where the Visual Studio version is chosen")
    endif(NOT "${CMAKE_GENERATOR_TOOLSET}" STREQUAL "v110_xp")
  endif("${CREO_VERSION}" VERSION_GREATER "2.9999" AND "${CREO_VERSION}" VERSION_LESS "4.0")

  # There may be a sub-directory that is specific to the installed
  # version of CREO - find it
  file(GLOB MDIR RELATIVE "${CREO_ROOT_DIR}" "${CREO_ROOT_DIR}/M*" )

  # Define a variable for the current CREO platform
  set(CREO_OS_VERSION x86e_win64)

  # Unlocker script
  set(CREO_PTK_UNLOCKER "${CREO_ROOT_DIR}/${MDIR}/Parametric/bin/protk_unlock.bat")

  # CREO libraries
  set(CREO_LIB_PATH "${CREO_ROOT_DIR}/${MDIR}/Common Files/protoolkit/${CREO_OS_VERSION}/obj/")
  find_file(CREO_PTK_DLL_LIBS NAMES "protk_dll_NU.lib" "protk_dll.lib" PATHS "${CREO_LIB_PATH}")
  find_file(CREO_PTK_EXE_LIBS NAMES "protoolkit_NU.lib" "protoolkit.lib" PATHS "${CREO_LIB_PATH}")

  foreach(libname "ucore.lib" "udata.lib")
    set(unicode_lib "${CREO_LIB_PATH}/${libname}")

    if(EXISTS "${unicode_lib}")
      set(CREO_PTK_DLL_LIBS "${CREO_PTK_DLL_LIBS}" "${unicode_lib}")
    endif(EXISTS "${unicode_lib}")
  endforeach(libname "ucore.lib" "udata.lib")

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
  foreach(reqpath_set ${CREO_REQUIRED_PATHS_ABS})
    foreach(reqpath ${${reqpath_set}})
      if(NOT EXISTS "${reqpath}")
	set(have_creo_paths 0)
	message(WARNING "Required CREO path variable ${reqpath_set} has non-existant path ${reqpath} - update ${reqpath} to reflect the correct path for this installation of CREO.")
      endif(NOT EXISTS "${reqpath}")
    endforeach(reqpath ${${reqpath_set}})
  endforeach(reqpath_set ${CREO_REQUIRED_PATHS_ABS})

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
  # a stand-alone package, and we want the dlls in the same directories as our
  # build target dirs for the .exe version to find.  We'll link against the
  #.lib files so build that list at the same time.
  set(BRLCAD_LIBS
    libbg
    libbn
    libbrep
    libbu
    libbv
    libnmg
    librt
    libwdb
    openNURBS
    poly2tri
    regex_brl
    z_brl1
    )
  set(BRLCAD_STATIC_LIBS)
  set(brlcadlibs_notinstalled)
  foreach(blib ${BRLCAD_LIBS})
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      configure_file(${BRLCAD_ROOT_DIR}/bin/${blib}.dll ${CMAKE_CURRENT_BINARY_DIR}/${CFG_TYPE}/${blib}.dll COPYONLY)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    configure_file(${BRLCAD_ROOT_DIR}/bin/${blib}.dll ${CMAKE_CURRENT_BINARY_DIR}/${blib}.dll COPYONLY)
    # Sigh - zlib doesn't follow the general pattern
    if("${blib}" STREQUAL "z_brl1")
      set(BRLCAD_STATIC_LIBS ${BRLCAD_STATIC_LIBS} "${BRLCAD_ROOT_DIR}/lib/z_brl.lib")
    else("${blib}" STREQUAL "z_brl1")
      set(BRLCAD_STATIC_LIBS ${BRLCAD_STATIC_LIBS} "${BRLCAD_ROOT_DIR}/lib/${blib}.lib")
    endif("${blib}" STREQUAL "z_brl1")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${blib}.dll DESTINATION "${MDIR}/${CREO_OBJ_DIR}")
    if(NOT EXISTS ${CREO_ROOT_DIR}/${MDIR}/${CREO_OBJ_DIR}/${blib}.dll)
      set(brlcadlibs_notinstalled "${brlcadlibs_notinstalled}\n     ${blib}.dll")
    endif(NOT EXISTS ${CREO_ROOT_DIR}/${MDIR}/${CREO_OBJ_DIR}/${blib}.dll)
  endforeach(blib ${BRLCAD_LIBS})
  if(brlcadlibs_notinstalled)
    message("Note: The following BRL-CAD dll(s) is/are not present in ${CREO_ROOT_DIR}/${MDIR}/${CREO_OBJ_DIR}:\n${brlcadlibs_notinstalled}\n\nBe aware that attempting to load the DLL version of the plugin without these files in place will\ncause the loading process to silently fail: *NO* messages indicating why the loading failed will\nbe generated.  Be sure to run the INSTALL command after building but before testing.\n")
  endif(brlcadlibs_notinstalled)


  # Set the relevant include directories
  include_directories(
    "${BRLCAD_ROOT_DIR}/include/brlcad"
    "${BRLCAD_ROOT_DIR}/include"
    "${BRLCAD_ROOT_DIR}/include/openNURBS"
    "${CREO_PTK_INCDIR}"
    "${CREO_PTK_APPLS_INCDIR}"
    )

endif(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)

set(creo_brl_srcs
  assembly.cpp
  csg.cpp
  main.cpp
  part.cpp
  util.cpp
  )

if(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)
  # Primary (DLL) version of plugin
  # Note that for successful running of the DLL version of this plugin,
  # the BRL-CAD dlls need to be present in Common Files/${CREO_OS_VERSION}/obj.  Also,
  # the resource files need to be present in Common Files/text/resource and
  # creo-brl-msg.txt present in Common Files/text/usascii before this will be
  # usable.  The simplest thing to do is build a package and install it in the
  # CREO root directory.  *** Any attempt run this plugin without the correct
  # DLLs in place will result in silent failure of the loading step. ***
  add_library(creo-brl SHARED ${creo_brl_srcs})
  target_link_libraries(creo-brl ${WIN_LIBS} ${CREO_PTK_DLL_LIBS} ${BRLCAD_STATIC_LIBS})
  set_property(TARGET creo-brl APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  install(TARGETS creo-brl
    RUNTIME DESTINATION "${MDIR}/${CREO_OBJ_DIR}"
    )
  set(DAT_STARTUP "dll")
  set(DAT_EXEC_FILE "${CREO_ROOT_DIR}/${MDIR}/${CREO_OBJ_DIR}/creo-brl.dll")
  set(DAT_TEXT_DIR "${CREO_ROOT_DIR}/${MDIR}/${CREO_MSG_DIR}")
  configure_file(creo-brl.dat.in ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dat @ONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dat DESTINATION "${MDIR}/${CREO_DAT_DIR}")

  if(DEFINED CREO_EXEC_PLUGIN)
    # Debugging executable version of plugin - only use this if necessary.  For
    # normal debugging, it is preferable to use the DLL in order to avoid
    # getting into situations where the .exe converter plugin works but the DLL
    # does not.  Note that this is also identified as a possible issue in the
    # CREO dev documentation.
    add_executable(creo-brl-exe ${creo_brl_srcs})
    target_link_libraries(creo-brl-exe ${WIN_LIBS} ${CREO_PTK_EXE_LIBS} ${BRLCAD_STATIC_LIBS})
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      set(DAT_STARTUP "spawn")
      set(DAT_EXEC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${CFG_TYPE}/creo-brl-exe.exe")
      set(DAT_TEXT_DIR "${CREO_ROOT_DIR}/${MDIR}/${CREO_MSG_DIR}")
      configure_file(creo-brl.dat.in ${CMAKE_CURRENT_BINARY_DIR}/${CFG_TYPE}/creo-brl-debug.dat @ONLY)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	set_property(TARGET creo-brl-exe APPEND PROPERTY COMPILE_DEFINITIONS "TINYCTHREAD_DLL_IMPORTS")
  endif(DEFINED CREO_EXEC_PLUGIN)

  # Install the resource files
  file(GLOB RESOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.res)
  set(rfiles_installed 1)
  foreach(rfile ${RESOURCE_FILES})
    install(FILES ${rfile} DESTINATION "${MDIR}/${CREO_RESOURCE_DIR}")
    get_filename_component(rfilename "${rfile}" NAME)
    if(NOT EXISTS "${CREO_ROOT_DIR}/${MDIR}/${CREO_RESOURCE_DIR}/${rfilename}")
      set(rfiles_installed 0)
    endif(NOT EXISTS "${CREO_ROOT_DIR}/${MDIR}/${CREO_RESOURCE_DIR}/${rfilename}")
  endforeach(rfile ${RESOURCE_FILES})

  # Install the messages file
  install(FILES resources/creo-brl-msg.txt DESTINATION "${MDIR}/${CREO_MSG_DIR}")

  if(NOT EXISTS "${CREO_ROOT_DIR}/${MDIR}/${CREO_MSG_DIR}/creo-brl-msg.txt" OR NOT rfiles_installed)
    message("\nNote that even with the exe version of the plugin, the resource files\nmust be present in the correct locations within the CREO tree in order\nto successfully run the plugin.  Be sure to run the INSTALL target\nbefore starting a testing session.\n")
  endif(NOT EXISTS "${CREO_ROOT_DIR}/${MDIR}/${CREO_MSG_DIR}/creo-brl-msg.txt" OR NOT rfiles_installed)

  add_custom_target(UNLOCK
    COMMAND "${CREO_PTK_UNLOCKER}" ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration)/creo-brl.dll
    DEPENDS creo-brl
    COMMENT "Unlocking CREO-G dll..."
    )

  # Since we need to run "INSTALL" as part of normal development, it is
  # worthwhile to provide an "UNINSTALL" target to automate the cleanup.
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)
  add_custom_target(UNINSTALL
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    COMMENT "Removing installed CREO-BRL components..."
    )

  set(CPACK_GENERATOR NSIS ZIP)
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Creo to BRL-CAD exporter for PTC's Creo Parametric")
  set(CPACK_PACKAGE_VENDOR "BRL-CAD Development Team")
  #TODO CPACK_RESOURCE_FILE_README and CPACK_RESOURCE_FILE_LICENSE)
  set(CPACK_PACKAGE_VERSION_MAJOR ${CREO2BRL_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${CREO2BRL_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${CREO2BRL_VERSION_PATCH})
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
  include(CPack)

else(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)

  set(CREO_DIRS
    "${CMAKE_CURRENT_BINARY_DIR}/../../../include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/other/openNURBS"
    ${ZLIB_INCLUDE_DIRS}
    ${REGEX_INCLUDE_DIRS}
    )
  BRLCAD_INCLUDE_DIRS(CREO_DIRS)

  # The build shims aren't so great at mocking the initialization
  # behavior of the real code.
  CHECK_CXX_FLAG("Wno-maybe-uninitialized")

  set(creo_brl_srcs ${creo_brl_srcs} shim.cpp)
  add_definitions(-DTEST_BUILD)
  add_library(creo-brl ${creo_brl_srcs})
  target_link_libraries(creo-brl librt libwdb libbrep libbg libnmg libbv libbn libbu)
  set_target_properties(creo-brl PROPERTIES FOLDER "BRL-CAD Executables/Build Only")
  if(HIDE_INTERNAL_SYMBOLS)
    set_property(TARGET creo-brl APPEND PROPERTY COMPILE_DEFINITIONS "BU_DLL_IMPORTS")
    set_property(TARGET creo-brl APPEND PROPERTY COMPILE_DEFINITIONS "BN_DLL_IMPORTS")
    set_property(TARGET creo-brl APPEND PROPERTY COMPILE_DEFINITIONS "BV_DLL_IMPORTS")
    set_property(TARGET creo-brl APPEND PROPERTY COMPILE_DEFINITIONS "NMG_DLL_IMPORTS")
    set_property(TARGET creo-brl APPEND PROPERTY COMPILE_DEFINITIONS "BG_DLL_IMPORTS")
    set_property(TARGET creo-brl APPEND PROPERTY COMPILE_DEFINITIONS "BREP_DLL_IMPORTS")
    set_property(TARGET creo-brl APPEND PROPERTY COMPILE_DEFINITIONS "RT_DLL_IMPORTS")
    set_property(TARGET creo-brl APPEND PROPERTY COMPILE_DEFINITIONS "DB5_DLL_IMPORTS")
    set_property(TARGET creo-brl APPEND PROPERTY COMPILE_DEFINITIONS "WDB_DLL_IMPORTS")
    set_property(TARGET creo-brl APPEND PROPERTY COMPILE_DEFINITIONS "TIE_DLL_IMPORTS")
    set_property(TARGET creo-brl APPEND PROPERTY COMPILE_DEFINITIONS "OPENNURBS_IMPORTS")
  endif(HIDE_INTERNAL_SYMBOLS)

  # Distcheck file list
  set(creo_dist_files
    ${creo_brl_srcs}
    README.txt
    cmake_uninstall.cmake.in
    creo-brl.dat.in
    creo-brl.h
    resources/creo-brl-msg.txt
    resources/creo_brl.res
    shim.h
    )
CMAKEFILES(${creo_dist_files})
CMAKEFILES(CMakeLists.txt)

endif(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
