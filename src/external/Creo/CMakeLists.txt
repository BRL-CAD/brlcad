#----------------------------------------------------------------------#
# Title:   CMakeLists.txt                                              #
# Purpose: Configure and generate build files for CREO2BRL project     #
# Note:    Requires an active Creo Pro/Toolkit license                 #
# Revised: 2/6/2024                                                    #
#----------------------------------------------------------------------#

# Establish the list of Source files - this is needed
# both for the plugin build and the main BRL-CAD build
set(CREO2BRL_SHARED_SOURCES
  part_writer.cpp
  )

set(CREO2BRL_LEGACY_SOURCES
  assembly.cpp
  csg.cpp
  main.cpp
  part.cpp
  util.cpp
  ${CREO2BRL_SHARED_SOURCES}
  )

set(CREO2BRL_CORE_SOURCES
  core.cpp
  conversion.cpp
  profile.cpp
  ${CREO2BRL_SHARED_SOURCES}
  )

if(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)

  # Minimum required version of CMake
  cmake_minimum_required(VERSION 3.22)
  message(STATUS "CMake version = ${CMAKE_VERSION}")

  # Produce console log of CMake variables
  set(verbose 0)

  #----------------------------------------------------------------------#
  # Start building the CREO2BRL project                                  #
  #----------------------------------------------------------------------#

  # Set CMake project name
  set(project_name CREO2BRL)
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

  option(CREO_EXEC_PLUGIN "Build the spawned executable diagnostic fallback" OFF)

  find_package(BRLCAD CONFIG REQUIRED COMPONENTS bu bn bv bg nmg brep rt wdb)
  set(CREO_BRLCAD_LIBS
    BRLCAD::rt
    BRLCAD::wdb
    BRLCAD::brep
    BRLCAD::bg
    BRLCAD::nmg
    BRLCAD::bv
    BRLCAD::bn
    BRLCAD::bu
    )

  # TARGET_RUNTIME_DLLS does not capture the full BRL-CAD runtime set for this
  # plugin build, so stage the required DLLs from the BRL-CAD package bin/.
  set(CREO_BRLCAD_RUNTIME_DLL_NAMES
    Clipper2
    libbg
    libbn
    libbrep
    libbu
    libbv
    libnmg
    librt
    libwdb
    lmdb
    manifold
    mmesh
    OpenNURBS
    regex_brl
    z_brl
    )
  set(CREO_BRLCAD_RUNTIME_DLLS)
  foreach(dll_name ${CREO_BRLCAD_RUNTIME_DLL_NAMES})
    set(dll_path "${BRLCAD_DIR}/bin/${dll_name}.dll")
    if(NOT EXISTS "${dll_path}")
      message(FATAL_ERROR "Missing BRL-CAD runtime DLL: ${dll_path}")
    endif()
    list(APPEND CREO_BRLCAD_RUNTIME_DLLS "${dll_path}")
  endforeach()

  # Must supply CREO_ROOT_DIR (typically /c/Program Files/PTC/Creo <version>/)
  set(CREO_ROOT_DIR "" CACHE PATH "Creo installation root")
  if(NOT IS_DIRECTORY "${CREO_ROOT_DIR}")
    message(FATAL_ERROR "CREO_ROOT_DIR must identify an existing Creo installation")
  endif()

  find_path(CREO_COMMON_DIR NAMES protoolkit PATHS "${CREO_ROOT_DIR}" PATH_SUFFIXES "Common Files" NO_DEFAULT_PATH REQUIRED)
  set(CREO_DAT_DIR "${CREO_COMMON_DIR}/protoolkit")
  file(GLOB CREO_PTK_OBJ_DIRS LIST_DIRECTORIES true "${CREO_DAT_DIR}/*/obj")
  find_library(CREO_PTK_DLL_LIB NAMES protk_dll_NU protk_dll PATHS ${CREO_PTK_OBJ_DIRS} NO_DEFAULT_PATH REQUIRED)

  get_filename_component(CREO_PTK_DIR "${CREO_PTK_DLL_LIB}" DIRECTORY)
  get_filename_component(CREO_OS_DIR "${CREO_PTK_DIR}" DIRECTORY)
  get_filename_component(CREO_OS_VERSION "${CREO_OS_DIR}" NAME)
  set(CREO_PTK_INC_DIR     "${CREO_DAT_DIR}/includes")
  set(CREO_PTK_APP_INC_DIR "${CREO_DAT_DIR}/protk_appls/includes")

  find_library(CREO_PTK_EXE_LIBS NAMES protoolkit_NU protoolkit PATHS "${CREO_PTK_DIR}" NO_DEFAULT_PATH REQUIRED)
  find_library(CREO_UCORE_LIB NAMES ucore PATHS "${CREO_PTK_DIR}" NO_DEFAULT_PATH)
  find_library(CREO_UDATA_LIB NAMES udata PATHS "${CREO_PTK_DIR}" NO_DEFAULT_PATH)
  set(CREO_PTK_DLL_LIBS "${CREO_PTK_DLL_LIB}")
  foreach(lib "${CREO_UCORE_LIB}" "${CREO_UDATA_LIB}")
    if(lib)
      list(APPEND CREO_PTK_DLL_LIBS "${lib}")
    endif()
  endforeach()

  find_file(CREO_PSF_TXT NAMES parametric.psf PATHS "${CREO_ROOT_DIR}" PATH_SUFFIXES Parametric/bin NO_DEFAULT_PATH REQUIRED)
  get_filename_component(CREO_PARAMETRIC_BIN_DIR "${CREO_PSF_TXT}" DIRECTORY)
  find_program(CREO_PTK_UNLOCKER NAMES protk_unlock.bat PATHS "${CREO_PARAMETRIC_BIN_DIR}" NO_DEFAULT_PATH REQUIRED)

  # Verify the Creo directories derived from the discovered installation
  foreach(dir CREO_DAT_DIR CREO_PTK_INC_DIR CREO_PTK_APP_INC_DIR)
    if(NOT IS_DIRECTORY "${${dir}}")
      message(FATAL_ERROR "Unable to locate required Creo directory: ${${dir}}")
    endif()
  endforeach()

  # Build a relocatable bundle in the build tree. CMAKE_INSTALL_PREFIX is
  # the expected deployment location used by creo-brl.dat, not a Creo SDK path.
  set(CREO_BUNDLE_ROOT "${CMAKE_BINARY_DIR}/Creo")
  set(CREO_BUNDLE_DIR "${CREO_BUNDLE_ROOT}/$<CONFIG>")
  set(CREO_BUNDLE_BIN_DIR "${CREO_BUNDLE_DIR}/bin")
  if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CREO_BUNDLE_DIR}" CACHE PATH "Creo-BRL deployment directory" FORCE)
  endif()

  # Bundle-specific usage and relocation notes.
  set(CREO_BUNDLE_README "${CMAKE_CURRENT_BINARY_DIR}/README.bundle.txt")
  file(WRITE "${CREO_BUNDLE_README}"
    "Creo-BRL plugin bundle\n\n"
    "Default deployment location:\n  ${CMAKE_INSTALL_PREFIX}\n\n"
    "To use this bundle, copy it to the deployment location and point Creo at\n"
    "creo-brl.dat using protkdat, creotkdat, toolkit_registry_file, or the\n"
    "Auxiliary Applications interface.\n\n"
    "If the bundle is moved, update exec_file and text_dir in creo-brl.dat,\n"
    "then update the Creo setting that points at creo-brl.dat.  No files need\n"
    "to be copied into the Creo installation directory.\n"
    )

  #----------------------------------------------------------------------#
  # Establish Creo Specific C++ Compiler Settings                        #
  #----------------------------------------------------------------------#
  # Notes:                                                               #
  #   1) Set these to match those found in Creo file "make_install_cxx"  #
  #   2) Located here:  ..\Common Files\protoolkit\<platform>\obj\       #
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
  set(c_flag_list   ${c_flag_list}   /MT /EHsc)
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
  set(CMAKE_C_FLAGS             "${c_flag_list}   ${brlcad_defs}"   CACHE STRING "" FORCE)
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
    "${BRLCAD_INCLUDE_DIRS}"
    "${CREO_PTK_INC_DIR}"
    "${CREO_PTK_APP_INC_DIR}"
    "${OPENNURBS_INCLUDE_DIRS}"
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

  file(STRINGS "${CREO_PSF_TXT}" psf_strs)
  foreach(pstr ${psf_strs})
    if("${pstr}" MATCHES ".*CREOPMA_FEATURE_NAME.*")
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
        endif()
    endif()
  endforeach()

  #----------------------------------------------------------------------#
  # Primary (DLL) version of plugin                                      #
  #----------------------------------------------------------------------#
  # The build produces a self-contained bundle under:                    #
  #                                                                      #
  #   ${CMAKE_BINARY_DIR}/Creo/<config>/                                 #
  #     bin/        plugin and BRL-CAD runtime DLLs                      #
  #     text/       Creo messages and resources                          #
  #     config/     default user profile                                 #
  #     creo-brl.dat                                                     #
  #     README.txt                                                       #
  #                                                                      #
  # CMAKE_INSTALL_PREFIX defines the expected deployment location        #
  # embedded in creo-brl.dat.                                            #
  #----------------------------------------------------------------------#

  # Win32 libraries (excludes libcmt.lib)
  set(WIN_LIBS
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    kernel32.lib
    mpr.lib
    netapi32.lib
    ole32.lib
    psapi.lib
    shell32.lib
    user32.lib
    version.lib
    winmm.lib
    winspool.lib
    ws2_32.lib
    wsock32.lib
    )

  add_library(creo-brl-core SHARED ${CREO2BRL_CORE_SOURCES})
  set_property(TARGET creo-brl-core APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  target_link_libraries(creo-brl-core REGEX::REGEX ${WIN_LIBS} ${CREO_BRLCAD_LIBS})
  set_target_properties(creo-brl-core PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CREO_BUNDLE_BIN_DIR}")

  add_library(creo-brl SHARED
    loader.c
    frontend.cpp)
  target_link_libraries(creo-brl ${WIN_LIBS} ${CREO_PTK_DLL_LIBS})
  set_target_properties(creo-brl PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CREO_BUNDLE_BIN_DIR}")
  add_dependencies(creo-brl creo-brl-core)

  set(DAT_STARTUP "dll")
  set(DAT_EXEC_FILE "${CMAKE_INSTALL_PREFIX}/bin/creo-brl.dll")
  set(DAT_TEXT_DIR "${CMAKE_INSTALL_PREFIX}/text")
  configure_file(creo-brl.dat.in "${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dat" @ONLY)

  file(GLOB RESOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/*.res")
  add_custom_command(TARGET creo-brl POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
      "${CREO_BUNDLE_BIN_DIR}"
      "${CREO_BUNDLE_DIR}/text/usascii"
      "${CREO_BUNDLE_DIR}/text/resource"
      "${CREO_BUNDLE_DIR}/config"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dat" "${CREO_BUNDLE_DIR}/creo-brl.dat"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${CREO_BUNDLE_README}" "${CREO_BUNDLE_DIR}/README.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/creo-brl-msg.txt" "${CREO_BUNDLE_DIR}/text/usascii/creo-brl-msg.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/ptc_materials.mtl" "${CREO_BUNDLE_DIR}/text/usascii/ptc_materials.mtl"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      ${RESOURCE_FILES} "${CREO_BUNDLE_DIR}/text/resource"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/Creo_to_BRL.g" "${CREO_BUNDLE_DIR}/config/Creo_to_BRL.g"
    COMMAND_EXPAND_LISTS
    COMMENT "Assembling Creo-BRL bundle in ${CREO_BUNDLE_ROOT}/$<CONFIG>"
    )

  # Stage the BRL-CAD runtime DLL set alongside the facade and core.
  add_custom_command(TARGET creo-brl-core POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      ${CREO_BRLCAD_RUNTIME_DLLS} "${CREO_BUNDLE_BIN_DIR}"
    COMMAND_EXPAND_LISTS
    )

  # Keep install/CPack output identical to the build-tree bundle layout.
  install(TARGETS creo-brl creo-brl-core RUNTIME DESTINATION bin)
  install(FILES ${CREO_BRLCAD_RUNTIME_DLLS} DESTINATION bin)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/creo-brl.dat" DESTINATION .)
  install(FILES "${CREO_BUNDLE_README}" DESTINATION . RENAME README.txt)
  install(FILES resources/creo-brl-msg.txt resources/ptc_materials.mtl DESTINATION text/usascii)
  install(FILES ${RESOURCE_FILES} DESTINATION text/resource)
  install(FILES resources/Creo_to_BRL.g DESTINATION config)

  #----------------------------------------------------------------------#
  # Debugging executable version of plugin - only use this if necessary. #
  # For normal debugging, it is preferable to use the DLL in order to    #
  # avoid getting into situations where the .exe converter plugin works  #
  # but the DLL does not.  Note that this is also identified as a        #
  # possible issue in the Creo dev documentation.                        #
  #----------------------------------------------------------------------#
  if(CREO_EXEC_PLUGIN)
    add_executable(creo-brl-exe ${CREO2BRL_LEGACY_SOURCES} frontend.cpp)
    target_link_libraries(creo-brl-exe REGEX::REGEX ${WIN_LIBS} ${CREO_PTK_EXE_LIBS} ${CREO_UCORE_LIB} ${CREO_UDATA_LIB} ${CREO_BRLCAD_LIBS})
    set_target_properties(creo-brl-exe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CREO_BUNDLE_BIN_DIR}")
    set_property(TARGET creo-brl-exe APPEND PROPERTY COMPILE_DEFINITIONS TINYCTHREAD_DLL_IMPORTS CREO_EXEC_PLUGIN)

    set(DAT_STARTUP "spawn")
    set(DAT_EXEC_FILE "${CMAKE_INSTALL_PREFIX}/bin/creo-brl-exe.exe")
    set(DAT_TEXT_DIR "${CMAKE_INSTALL_PREFIX}/text")
    configure_file(creo-brl.dat.in "${CMAKE_CURRENT_BINARY_DIR}/creo-brl-debug.dat" @ONLY)
    add_custom_command(TARGET creo-brl-exe POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/creo-brl-debug.dat" "${CREO_BUNDLE_DIR}/creo-brl-debug.dat"
      )
    install(TARGETS creo-brl-exe RUNTIME DESTINATION bin)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/creo-brl-debug.dat" DESTINATION .)
  endif()

  message("--")
  message("-- #----------------------------------------------------------------------#")
  message("-- # NOTICE: Creo-BRL bundle                                              #")
  message("-- #----------------------------------------------------------------------#")
  message("-- # Build output : ${CREO_BUNDLE_ROOT}/<config>")
  message("-- # Deploy target: ${CMAKE_INSTALL_PREFIX}")
  message("-- # Run UNLOCK after building and before distributing the bundle.       #")
  message("-- #----------------------------------------------------------------------#")
  message("--")

  #----------------------------------------------------------------------#
  # Create the UNLOCK target                                             #
  #----------------------------------------------------------------------#
  if(CREO_EXEC_PLUGIN)
    set(CREO_PTK_UNLOCK_EXE_COMMAND
      COMMAND "${CMAKE_COMMAND}" -E env "PROCESSOR_ARCHITECTURE=AMD64"
        "${CREO_PTK_UNLOCKER}" "$<TARGET_FILE:creo-brl-exe>")
    set(CREO_PTK_UNLOCK_EXE_DEPENDENCY creo-brl-exe)
  endif()

  add_custom_target(UNLOCK
    COMMAND "${CMAKE_COMMAND}" -E env "PROCESSOR_ARCHITECTURE=AMD64"
      "${CREO_PTK_UNLOCKER}" "$<TARGET_FILE:creo-brl>"
    ${CREO_PTK_UNLOCK_EXE_COMMAND}
    DEPENDS creo-brl creo-brl-core ${CREO_PTK_UNLOCK_EXE_DEPENDENCY}
    COMMENT "Unlocking Creo-BRL dll"
  )

  #----------------------------------------------------------------------#
  # Provide an UNINSTALL target for users who deploy the bundle with the #
  # CMake install target rather than copying the staged directory.       #
  #----------------------------------------------------------------------#
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)
  add_custom_target(UNINSTALL
    COMMAND "${CMAKE_COMMAND}" -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    COMMENT "Removing installed CREO2BRL components..."
    )

  set(CPACK_GENERATOR ZIP)
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
  brlcad_find_package(REGEX REQUIRED)

  set(CREO_DIRS
    "${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}"
    "${BRLCAD_SOURCE_DIR}/${INCLUDE_DIR}"
    "${OPENNURBS_INCLUDE_DIRS}"
     )
  BRLCAD_INCLUDE_DIRS(creo-brl "${CREO_DIRS}" PUBLIC)

  #----------------------------------------------------------------------#
  # The build shims aren't so great at mocking the initialization        #
  # behavior of the real code.                                           #
  #----------------------------------------------------------------------#
  CHECK_CXX_FLAG("Wno-maybe-uninitialized")

  set(CREO2BRL_SOURCES ${CREO2BRL_LEGACY_SOURCES} shim.cpp)
  add_library(creo-brl ${CREO2BRL_SOURCES})
  set_property(TARGET creo-brl APPEND PROPERTY COMPILE_DEFINITIONS TEST_BUILD BRLCADBUILD HAVE_CONFIG_H)
  target_link_libraries(creo-brl librt libwdb libbrep libbg libnmg libbv libbn libbu REGEX::REGEX)
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
    ${CREO2BRL_CORE_SOURCES}
    loader.c
    frontend.cpp
    README.txt
    cmake_uninstall.cmake.in
    creo-brl.dat.in
    creo-brl.h
    conversion_snapshot.h
    part_writer.h
    profile_controls.h
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
