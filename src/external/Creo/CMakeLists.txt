#----------------------------------------------------------------------#
# Title:   CMakeLists.txt                                              #
# Purpose: Configure and generate build files for CREO2BRL project     #
# Note:    Requires an active Creo Pro/Toolkit license                 #
# Revised: 3/22/2023                                                   #
#----------------------------------------------------------------------#

# Establish the list of Source files - this is needed
# both for the plugin build and the main BRL-CAD build
set(CREO2BRL_SOURCES
  assembly.cpp
  csg.cpp
  main.cpp
  part.cpp
  util.cpp
  )

if(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)

  # Minimum required version of CMake
  cmake_minimum_required(VERSION 3.18)
  message(STATUS "CMake version = ${CMAKE_VERSION}")

  # Produce console log of CMake variables
  set(verbose 0)

  #----------------------------------------------------------------------#
  # Start building the CREO2BRL project                                  #
  #----------------------------------------------------------------------#

  # Set CMake project name
  set(project_name "CREO2BRL")
  project(${project_name})

  # Set Project Version
  string(TIMESTAMP YYYY "%Y")
  string(TIMESTAMP MM   "%m")
  string(TIMESTAMP DD   "%d")
  set(project_version "${YYYY}.${MM}.${DD}")
  message(STATUS "${project_name} version = ${project_version}")

  # Display current date and time
  string(TIMESTAMP date %Y-%m-%d)
  string(TIMESTAMP time %H:%M:%S)
  message(STATUS "Current date and time: ${date} ${time}")

  # Allowed BRL-CAD configuration types
  set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

  #----------------------------------------------------------------------#
  # CAUTION: Verify installation settings before running CMake           #
  #----------------------------------------------------------------------#
  #        |        |        |        |        |        |        |       #
  #        v        v        v        v        v        v        v       #

  set(BRLCAD_ROOT_DIR "C:/Workspace/BRL-CAD/Source"          CACHE PATH "BRL-CAD root directory")
  set(CREO_ROOT_DIR   "C:/Program Files/PTC/Creo 8.0.8.0"    CACHE PATH "Creo root directory")

  #        ^        ^        ^        ^        ^        ^        ^       #
  #        |        |        |        |        |        |        |       #
  #----------------------------------------------------------------------#

  # BRL-CAD directories
  set(BRLCAD_BLD_DIR       "${BRLCAD_ROOT_DIR}/Build")
  set(BRLCAD_BIN_DIR       "${BRLCAD_ROOT_DIR}/Build/Release/bin")
  set(BRLCAD_INC_BRL_DIR   "${BRLCAD_ROOT_DIR}/Build/Release/include/brlcad")
  set(BRLCAD_INC_DIR       "${BRLCAD_ROOT_DIR}/Build/Release/include")
  set(BRLCAD_INC_NRB_DIR   "${BRLCAD_ROOT_DIR}/Build/Release/include/openNURBS")
  set(BRLCAD_LIB_DIR       "${BRLCAD_ROOT_DIR}/Build/Release/lib")
  set(BRLCAD_REL_DIR       "${BRLCAD_ROOT_DIR}/Build/Release")

  # Creo platform version
  set(CREO_OS_VERSION x86e_win64)

  # Creo relative paths
  set(CREO_DAT_REL         "Common Files/protoolkit")
  set(CREO_MSG_REL         "Common Files/text/usascii")
  set(CREO_OBJ_REL         "Common Files/${CREO_OS_VERSION}/obj")
  set(CREO_PTK_REL         "Common Files/protoolkit/${CREO_OS_VERSION}/obj")
  set(CREO_RES_REL         "Common Files/text/resource")

  # Creo directories
  set(CREO_DAT_DIR         "${CREO_ROOT_DIR}/${CREO_DAT_REL}")
  set(CREO_MSG_DIR         "${CREO_ROOT_DIR}/${CREO_MSG_REL}")
  set(CREO_OBJ_DIR         "${CREO_ROOT_DIR}/${CREO_OBJ_REL}")
  set(CREO_PTK_DIR         "${CREO_ROOT_DIR}/${CREO_PTK_REL}")
  set(CREO_RES_DIR         "${CREO_ROOT_DIR}/${CREO_RES_REL}")

  # Creo ProToolkit include directories
  set(CREO_PTK_INC_DIR     "${CREO_ROOT_DIR}/Common Files/protoolkit/includes")
  set(CREO_PTK_APP_INC_DIR "${CREO_ROOT_DIR}/Common Files/protoolkit/protk_appls/includes")

  # Creo startup file
  set(CREO_PSF_TXT         "${CREO_ROOT_DIR}/Parametric/bin/parametric.psf")

  # Creo ProToolkit Unlocker script
  set(CREO_PTK_UNLOCKER    "${CREO_ROOT_DIR}/Parametric/bin/protk_unlock.bat")

  # Locate Creo libraries
  find_file(CREO_PTK_DLL_LIBS NAMES "protk_dll_NU.lib" "protk_dll.lib" PATHS "${CREO_PTK_DIR}")
  find_file(CREO_PTK_EXE_LIBS NAMES "protoolkit_NU.lib" "protoolkit.lib" PATHS "${CREO_PTK_DIR}")
  foreach(libname "ucore.lib" "udata.lib")
    set(unicode_lib "${CREO_PTK_DIR}/${libname}")
    if(EXISTS ${unicode_lib})
      set(CREO_PTK_DLL_LIBS ${CREO_PTK_DLL_LIBS} ${unicode_lib})
    endif()
  endforeach()

  # Verify PATH dependent items
  set(path_dependent_items
    BRLCAD_BLD_DIR
    BRLCAD_BIN_DIR
    BRLCAD_INC_BRL_DIR
    BRLCAD_INC_DIR
    BRLCAD_INC_NRB_DIR
    BRLCAD_LIB_DIR
    BRLCAD_REL_DIR
    BRLCAD_ROOT_DIR
    CREO_DAT_DIR
    CREO_MSG_DIR
    CREO_OBJ_DIR
    CREO_PSF_TXT
    CREO_PTK_APP_INC_DIR
    CREO_PTK_DIR
    CREO_PTK_DLL_LIBS
    CREO_PTK_EXE_LIBS
    CREO_PTK_INC_DIR
    CREO_PTK_UNLOCKER
    CREO_RES_DIR
    )

  # Identify missing PATH dependent items
  set(path_fails 0)
  foreach(items ${path_dependent_items})
    foreach(item ${${items}})
      if(NOT EXISTS ${item})
        if(NOT path_fails)
          message("--")
          message("-- #----------------------------------------------------------------------#")
          message("-- # ERROR: Unable to locate the following PATH dependent item(s):        #")
          message("-- #----------------------------------------------------------------------#")
        endif()
        set(path_fails 1)
        message("-- # Missing: ${item}")
      endif()
    endforeach()
  endforeach()
  if(path_fails)
    message("-- #----------------------------------------------------------------------#")
    message("-- # ACTION: All PATH dependent items must be correct before continuing   #")
    message("-- #----------------------------------------------------------------------#")
    message("--")
    message(FATAL_ERROR "This is a FATAL error, unable to continue.")
  endif()

  # Report the status of all PATH dependent items
  message("--")
  message("-- #----------------------------------------------------------------------#")
  message("-- # STATUS: Summary of PATH dependent items:                             #")
  message("-- #----------------------------------------------------------------------#")
  foreach(items ${path_dependent_items})
    foreach(item ${${items}})
      if(EXISTS ${item})
        message("-- # Found: ${item}")
      else()
        message("-- # Missing: ${item}")
      endif()
    endforeach()
  endforeach()
  message("-- #----------------------------------------------------------------------#")
  message("--")

  # Set install dir as a function of the Creo directory
  set(CMAKE_INSTALL_PREFIX ${CREO_ROOT_DIR} CACHE PATH "Installation path" FORCE)
  mark_as_advanced(CMAKE_INSTALL_PREFIX)

  #----------------------------------------------------------------------#
  # Establish Creo Specific C++ Compiler Settings                        #
  #----------------------------------------------------------------------#
  # Notes:                                                               #
  #   1) Set these to match those found in Creo file "make_install_cxx"  #
  #   2) Located here:  ..\Common Files\protoolkit\x86e_win64\obj\       #
  #   3) CMake handles include files separately (exclude the "-I" flag)  #
  #   4) Compiler options based upon MSVC runtime libraries:             #
  #      a) /MT  --> MultiThreaded                                       #
  #      b) /MD  --> MultiThreadedDLL                                    #
  #      c) /MTd --> MultiThreadedDebug                                  #
  #      d) /MDd --> MultiThreadedDebugDLL                               #
  #----------------------------------------------------------------------#
  
# Set the default runtime library
  set(CMAKE_MSVC_RUNTIME_LIBRARY_DEFAULT "MultiThreaded")

  message("-- #----------------------------------------------------------------------#")
  message("-- # Default CMake compiler flags...                                      #")
  message("-- #----------------------------------------------------------------------#")
  message("-- # CMAKE_C_FLAGS   = ${CMAKE_C_FLAGS}")
  message("-- # CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")
  message("-- #----------------------------------------------------------------------#")
  message("--")

  # Convert default flags to lists
  string(REPLACE " " ";" c_flag_list   "${CMAKE_C_FLAGS}")
  string(REPLACE " " ";" cxx_flag_list "${CMAKE_CXX_FLAGS}")

  # Remove unwanted default compiler flags
  set(remove_flags /MD /MTd /D_DEBUG /D _DEBUG)
  foreach(flag ${remove_flags})
    list(REMOVE_ITEM c_flag_list   ${flag})
    list(REMOVE_ITEM cxx_flag_list ${flag})
  endforeach()

  # Enforce use of the /MT and /EHsc flags
  set(c_flag_list   ${c_flag_list} /MT /EHsc)
  set(cxx_flag_list ${cxx_flag_list} /MT /EHsc)

  # Append the Creo specific compiler options
  set(cc_mach       -DPRO_MACHINE=36 -DPRO_OS=4)
  set(cc_flags      -DPRO_USE_VAR_ARGS)
  set(c_flag_list   ${c_flag_list}   ${cc_mach} ${cc_flags})
  set(cxx_flag_list ${cxx_flag_list} ${cc_mach} ${cc_flags})

  # Remove duplicates
  list(REMOVE_DUPLICATES c_flag_list)
  list(REMOVE_DUPLICATES cxx_flag_list)

  # BRL-CAD definitions
  set(brlcad_defs "-DHAVE_CONFIG_H"    "-DBRLCAD_DLL"     "-DBRLCADBUILD"     "-DBU_DLL_IMPORTS"
                  "-DBN_DLL_IMPORTS"   "-DBV_DLL_IMPORTS" "-DNMG_DLL_IMPORTS" "-DBG_DLL_IMPORTS"
                  "-DBREP_DLL_IMPORTS" "-DRT_DLL_IMPORTS" "-DWDB_DLL_IMPORTS" "-DTIE_DLL_IMPORTS"
                  "-DDB5_DLL_IMPORTS"  "-DOPENNURBS_IMPORTS")

  # Convert these to plain strings
  string(REPLACE ";" " " brlcad_defs   "${brlcad_defs}")
  string(REPLACE ";" " " c_flag_list   "${c_flag_list}")
  string(REPLACE ";" " " cxx_flag_list "${cxx_flag_list}")

  # Settings global to all configurations
  set(CMAKE_C_FLAGS             "${c_flag_list} ${brlcad_defs}"     CACHE STRING "" FORCE)
  set(CMAKE_CXX_FLAGS           "${cxx_flag_list} ${brlcad_defs}"   CACHE STRING "" FORCE)
  set(CMAKE_SHARED_LINKER_FLAGS "/subsystem:console /machine:amd64" CACHE STRING "" FORCE)
  set(CMAKE_EXE_LINKER_FLAGS    "/subsystem:console /machine:amd64" CACHE STRING "" FORCE)

  message("-- #----------------------------------------------------------------------#")
  message("-- # Current CMake compiler flags...                                      #")
  message("-- #----------------------------------------------------------------------#")
  message("-- # CMAKE_C_FLAGS   = ${CMAKE_C_FLAGS}")
  message("-- # CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")
  message("-- #----------------------------------------------------------------------#")
  message("--")

  # Assign CMake include directories
  include_directories(
    ${BRLCAD_INC_BRL_DIR}
    ${BRLCAD_INC_DIR}
    ${BRLCAD_INC_NRB_DIR}
    ${CREO_PTK_INC_DIR}
    ${CREO_PTK_APP_INC_DIR}
    )

  #----------------------------------------------------------------------#
  # When doing development, to avoid the need to unlock DLLs and EXEs    #
  # every time for testing, you need to configure your parametric.psf    #
  # file                                                                 #
  #                                                                      #
  #      https://www.ptcusercommunity.com/message/399253#399253          #
  #                                                                      #
  # This is necessary only for iterative development: for compilation    #
  # the program can just be built and unlocked (the latter step being    #
  # necessary prior to distribution in any case.)  Since it is not       #
  # always needed, we will check and print a warning to alert developers #
  # but will not halt the configure process.                             #
  #----------------------------------------------------------------------#

  file(STRINGS ${CREO_PSF_TXT} psf_strs)
  foreach(pstr ${psf_strs})
    if(${pstr} MATCHES ".*CREOPMA_FEATURE_NAME.*")
      set(CFN_STR ${pstr})
      string(REGEX REPLACE ".*CREOPMA_FEATURE_NAME.*[(]" "" CFN_STR "${CFN_STR}")
      string(REGEX REPLACE "[)].*" "" CFN_STR "${CFN_STR}")
      string(STRIP "${CFN_STR}" CFN_STR)
      if("${CFN_STR}" STREQUAL "")
        message("-- #----------------------------------------------------------------------#")
        message("-- # WARNING: Pro/Toolkit license setup remains incomplete...             #")
        message("-- #----------------------------------------------------------------------#")
        message("-- # The CREOPMA_FEATURE_NAME variable in file parametric.psf has no      #")
        message("-- # arguments.  It is therefore possible to load a Pro/Toolkit license   #")
        message("-- # when started, and the plugin built by this project must be unlocked  #")
        message("-- # before it can be tested.  For additional information see:            #")
        message("-- #                                                                      #")
        message("-- #       https://www.ptcusercommunity.com/message/399253#399253         #")
        message("-- #                                                                      #")
        message("-- # This is a concern only for developers wanting to do iterative        #")
        message("-- # development without repeating the unlock step every time.  If you    #")
        message("-- # are only looking to compile and use the plugin, this warning may be  #")
        message("-- # safely ignored as long as the UNLOCK step is performed.              #")
        message("-- #----------------------------------------------------------------------#")
        message("--")
        message("-- #----------------------------------------------------------------------#")
        message("-- # NOTICE: For installation follow this BUILD sequence                  #")
        message("-- #----------------------------------------------------------------------#")
        message("-- # BUILD:                                                               #")
        message("-- #   1) creo-brl                                                        #")
        message("-- #   2) unlock                                                          #")
        message("-- #   3) install                                                         #")
        message("-- #----------------------------------------------------------------------#")
        message("--")
        endif()
    endif()
  endforeach()

  #----------------------------------------------------------------------#
  # We will need copies of the BRL-CAD DLLs along with the creo-brl.dll  #
  # to make a stand-alone package, and we want the dlls in the same      #
  # directories as our build target dirs for the .exe version to find.   #
  # We will link against the .lib files so we will build that list at    #
  # the same time.                                                       #
  #----------------------------------------------------------------------#
  set(BRLCAD_LIBS
      libbg
      libbn
      libbrep
      libbu
      libbv
      libnmg
      librt
      libwdb
      lmdb
      openNURBS
      poly2tri
      regex_brl
      z_brl
      )

  set(BRLCAD_STATIC_LIBS)
  set(brlcadlibs_notinstalled)
  foreach(blib ${BRLCAD_LIBS})
    foreach(ctype ${CMAKE_CONFIGURATION_TYPES})
      configure_file(${BRLCAD_BIN_DIR}/${blib}.dll ${CMAKE_CURRENT_BINARY_DIR}/${ctype}/${blib}.dll COPYONLY)
    endforeach()
    configure_file(${BRLCAD_BIN_DIR}/${blib}.dll ${CMAKE_CURRENT_BINARY_DIR}/${blib}.dll COPYONLY)
    set(BRLCAD_STATIC_LIBS ${BRLCAD_STATIC_LIBS} "${BRLCAD_LIB_DIR}/${blib}.lib")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${blib}.dll" DESTINATION ${CREO_OBJ_REL})
    if(NOT EXISTS "${CREO_OBJ_DIR}/${blib}.dll")
      set(brlcadlibs_notinstalled ${brlcadlibs_notinstalled} ${blib}.dll)
    endif()
  endforeach()

  if(brlcadlibs_notinstalled)
    message("-- #----------------------------------------------------------------------#")
    message("-- # WARNING: The following OBJECT librarie(s) are not installed:         #")
    message("-- #----------------------------------------------------------------------#")
    message("-- #"                                                                       )
    message("-- # Location  = ${CREO_OBJ_DIR}"                                           )
    message("-- #"                                                                       )
    foreach(item ${brlcadlibs_notinstalled})
      message("-- # ${item}")
    endforeach()
    message("-- #"                                                                       )
    message("-- #----------------------------------------------------------------------#")
    message("-- # Attempting to load the DLL version of the plugin without these files #")
    message("-- # in place will cause the loading process to silently fail.  Therefore #")
    message("-- # *NO* messages indicating why the loading failed will be generated.   #")
    message("-- # Be sure to run the INSTALL command after building but before testing #")
    message("-- # this CREO2BRL API plugin with Creo.                                  #")
    message("-- #                                                                      #")
    message("-- # For additional information see:                                      #")
    message("-- #                                                                      #")
    message("-- #           https://apps.dtic.mil/sti/pdfs/AD1036571.pdf               #")
    message("-- #                                                                      #")
    message("-- #----------------------------------------------------------------------#")
    message("--")
  endif()

  #----------------------------------------------------------------------#
  # Primary (DLL) version of plugin                                      #
  #----------------------------------------------------------------------#
  #                                                                      #
  # To run the DLL version of this plugin successfully, the files shown  #
  # below must be pre-installed within the Creo installation:            #
  #                                                                      #
  #     -->  C:/Program Files/PTC/Creo ${CREO_VERSION_NUMBER}  <--       #
  #                                                                      #
  #     Files                  Creo Installation Sub-Directory           #
  #  ------------------------------------------------------------------  #
  #  BRL-CAD dlls     --> Common Files/protoolkit/${WIN_OS_VERSION}/obj  #
  #  creo_brl.dat     --> Common Files/protoolkit                        #
  #  creo_brl.res     --> Common Files/text/resource                     #
  #  creo-brl-msg.txt --> Common Files/text/usascii                      #
  #  ------------------------------------------------------------------  #
  #                                                                      #
  # The simplest thing to do is build a package and install it in the    #
  # Creo root directory.                                                 #
  #                                                                      #
  # WARNING: Any attempt run this plugin without the correct DLL files   #
  #          in place will result in silent failure at the loading step. #
  #                                                                      #
  #          For additional information see:                             #
  #                                                                      #
  #              https://apps.dtic.mil/sti/pdfs/AD1036571.pdf            #
  #                                                                      #
  #----------------------------------------------------------------------#

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

  add_library(creo-brl SHARED ${CREO2BRL_SOURCES})
  set_property(TARGET creo-brl APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  target_link_libraries(creo-brl ${WIN_LIBS} ${CREO_PTK_DLL_LIBS} ${BRLCAD_STATIC_LIBS})
  install(TARGETS creo-brl RUNTIME DESTINATION ${CREO_OBJ_REL})
  set(DAT_STARTUP "dll")
  set(DAT_EXEC_FILE "${CREO_OBJ_DIR}/creo-brl.dll")
  set(DAT_TEXT_DIR  ${CREO_MSG_DIR})
  configure_file(creo-brl.dat.in ${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dat @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dat" DESTINATION ${CREO_PTK_REL})

  #----------------------------------------------------------------------#
  # Debugging executable version of plugin - only use this if necessary. #
  # For normal debugging, it is preferable to use the DLL in order to    #
  # avoid getting into situations where the .exe converter plugin works  #
  # but the DLL does not.  Note that this is also identified as a        #
  # possible issue in the Creo dev documentation.                        #
  #----------------------------------------------------------------------#
  if(DEFINED CREO_EXEC_PLUGIN)
    add_executable(creo-brl-exe ${CREO2BRL_SOURCES})
    target_link_libraries(creo-brl-exe ${WIN_LIBS} ${CREO_PTK_EXE_LIBS} ${BRLCAD_STATIC_LIBS})
    foreach(ctype ${CMAKE_CONFIGURATION_TYPES})
      set(DAT_STARTUP "spawn")
      set(DAT_EXEC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${ctype}/creo-brl-exe.exe")
      set(DAT_TEXT_DIR ${CREO_MSG_DIR})
      configure_file(creo-brl.dat.in "${CMAKE_CURRENT_BINARY_DIR}/${ctype}/creo-brl-debug.dat" @ONLY)
    endforeach()
    set_property(TARGET creo-brl-exe APPEND PROPERTY COMPILE_DEFINITIONS "TINYCTHREAD_DLL_IMPORTS")
  endif()

  # Install the resource files
  file(GLOB RESOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.res)
  set(rfiles_installed 1)
  foreach(rfile ${RESOURCE_FILES})
    install(FILES ${rfile} DESTINATION ${CREO_RES_REL})
    get_filename_component(rfilename ${rfile} NAME)
    if(NOT EXISTS "${CREO_RES_DIR}/${rfilename}")
      set(rfiles_installed 0)
    endif()
  endforeach()

  # Install the material translation file
  install(FILES resources/ptc_materials.mtl DESTINATION ${CREO_MSG_REL})

  # Install the messages file
  install(FILES resources/creo-brl-msg.txt DESTINATION ${CREO_MSG_REL})

  if(NOT EXISTS "${CREO_MSG_DIR}/creo-brl-msg.txt" OR NOT rfiles_installed)
    message("-- #----------------------------------------------------------------------#")
    message("-- # WARNING: CREO2BRL resource files are not installed...                #")
    message("-- #----------------------------------------------------------------------#")
    message("-- # Note that even with the EXE version of the plugin, the resource      #")
    message("-- # files must be present in the correct locations within the CREO       #")
    message("-- # installation in order to successfully run the plugin.  Be sure to    #")
    message("-- # run the INSTALL target before starting a testing session.            #")
    message("-- #----------------------------------------------------------------------#")
    message("--")
  endif()

  message("--")

  #----------------------------------------------------------------------#
  # Create the UNLOCK target                                             #
  #----------------------------------------------------------------------#
  add_custom_target(UNLOCK
    COMMAND "${CREO_PTK_UNLOCKER}" ${CMAKE_CURRENT_BINARY_DIR}/$(configuration)/creo-brl.dll
    DEPENDS creo-brl
    COMMENT "Unlocking Creo-BRL dll"
  )

  #----------------------------------------------------------------------#
  # Since we need to run "INSTALL" as part of normal development, it is  #
  # worthwhile to provide an "UNINSTALL" target to automate the cleanup. #
  #----------------------------------------------------------------------#
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)
  add_custom_target(UNINSTALL
    COMMAND "${CMAKE_COMMAND}" -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    COMMENT "Removing installed CREO2BRL components..."
    )

  set(CPACK_GENERATOR NSIS ZIP)
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Creo to BRL-CAD exporter for PTC's Creo Parametric")
  set(CPACK_PACKAGE_VENDOR "BRL-CAD Development Team")
  #TODO CPACK_RESOURCE_FILE_README and CPACK_RESOURCE_FILE_LICENSE)
  set(CPACK_PACKAGE_VERSION_MAJOR ${YYYY})
  set(CPACK_PACKAGE_VERSION_MINOR ${MM})
  set(CPACK_PACKAGE_VERSION_PATCH ${DD})
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
  include(CPack)

else(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)

  #----------------------------------------------------------------------#
  # Start building the EXTERNALS in Creo, Cubit, Unigraphics             #
  #----------------------------------------------------------------------#

  set(CREO_DIRS
    "${CMAKE_CURRENT_BINARY_DIR}/../../../include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../include"
    "${OPENNURBS_INCLUDE_DIRS}"
     )
  BRLCAD_INCLUDE_DIRS(CREO_DIRS)

  #----------------------------------------------------------------------#
  # The build shims aren't so great at mocking the initialization        #
  # behavior of the real code.                                           #
  #----------------------------------------------------------------------#
  CHECK_CXX_FLAG("Wno-maybe-uninitialized")

  set(CREO2BRL_SOURCES ${CREO2BRL_SOURCES} shim.cpp)
  add_library(creo-brl ${CREO2BRL_SOURCES})
  set_property(TARGET creo-brl APPEND PROPERTY COMPILE_DEFINITIONS TEST_BUILD BRLCADBUILD HAVE_CONFIG_H)
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
  endif()

  # Distcheck file list
  set(creo_dist_files
    ${CREO2BRL_SOURCES}
    README.txt
    cmake_uninstall.cmake.in
    creo-brl.dat.in
    creo-brl.h
    resources/creo-brl-msg.txt
    resources/creo_brl.res
    resources/ptc_materials.mtl
    shim.h
    )
  CMAKEFILES(${creo_dist_files})
  CMAKEFILES(CMakeLists.txt)

endif(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)

# Produce console log of CMake variables
if(verbose)
  message("-- #----------------------------------------------------------------------#")
  message("-- # List of CMake variables with their assigned values                   #")
  message("-- #----------------------------------------------------------------------#")
  get_cmake_property(varnames VARIABLES)
  list(SORT varnames)
  foreach(item ${varnames})
    list(LENGTH ${item} count)
    if(${count} GREATER 0)
      message("-- # ${item} = ${${item}}")
    else()
      message("-- # ${item} = EMPTY_STRING")
    endif()
  endforeach()
  message("-- #----------------------------------------------------------------------#")
  message("-- # End of CMake variables                                               #")
  message("-- #----------------------------------------------------------------------#")
  message("-- ")
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
