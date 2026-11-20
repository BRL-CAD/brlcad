set(
  cmake_ignore_files
  AsciiDoc.cmake
  BRLCAD_API_Flag.cmake
  BRLCAD_Build_Types.cmake
  BRLCAD_CPack.cmake
  BRLCAD_CPackOptions.cmake.in
  BRLCAD_CheckFunctions.cmake
  BRLCAD_Copy_Files.cmake.in
  BRLCAD_Environment_Setup.cmake
  BRLCAD_EXT_Setup.cmake
  BRLCAD_ExternalDeps.cmake
  BRLCAD_Find_Package.cmake
  BRLCAD_Install_Prefix.cmake
  BRLCAD_Path_Privacy.cmake
  BRLCAD_Package_Name.cmake
  BRLCAD_Package_Bundle.cmake
  InstallPathPrivacy.cmake.in
  BRLCAD_Options.cmake
  BRLCAD_Probe_Validation.cmake
  BRLCAD_Regress_Util.cmake
  BRLCAD_Repository.cmake
  BRLCAD_Summary.cmake
  BRLCAD_Targets.cmake
  BRLCAD_Test_Wrappers.cmake
  BRLCAD_User_Options.cmake
  BRLCAD_Util.cmake
  BRLCADConfig.cmake.in
  CMakeLists.txt
  CheckCInline.cmake
  CheckCSourceRuns.cmake
  CheckStructHasMember.cmake
  CompilerFlags.cmake
  Distcheck.cmake
  Doxygen.cmake
  FindASTYLE.cmake
  FindAdaptagrams.cmake
  FindAppleseed.cmake
  FindBlosc.cmake
  FindBSON.cmake
  FindGECODE.cmake
  FindGTE.cmake
  FindIlmBase.cmake
  FindLEMON.cmake
  FindLMDB.cmake
  FindLibUV.cmake
  FindLITEHTML.cmake
  FindMANIFOLD.cmake
  FindMMESH.cmake
  FindNETPBM.cmake
  FindOPENNURBS.cmake
  FindOSL.cmake
  FindOSMESA.cmake
  FindOpenCL.cmake
  FindOpenCV.cmake
  FindOpenVDB.cmake
  FindPERPLEX.cmake
  FindPkgMacros.cmake
  FindPNG.cmake
  FindPROJ4.cmake
  FindRE2C.cmake
  FindREGEX.cmake
  FindSPSR.cmake
  FindSTL.cmake
  FindShellDeps.cmake
  FindSTEPCODE.cmake
  FindTBB.cmake
  FindTCL.cmake
  FindUUID.cmake
  FindXMLLINT.cmake
  FindZLIB.cmake
  OpenVDBUtils.cmake
  Path_Setup.cmake
  RPath_Setup.cmake
  ResetCache.cmake
  TrackFiles.cmake
  static_link_test_main.cpp
  compat/README
  compat/README.compat
  compat/README.compat.c99
  compat/fdopen_compat.h
  compat/fileno_compat.h
  compat/gethostname_compat.h
  compat/hypot_compat.h
  compat/posix_memalign_compat.h
  compat/realpath_compat.h
  compat/sigaction_compat.h
  compat/snprintf_compat.h
  compat/strcasecmp_compat.h
  compat/strncasecmp_compat.h
  compat/strtok_r_compat.h
  compat/test_fdopen.c
  compat/test_fileno.c
  compat/test_gethostname.c
  compat/test_posix_memalign.c
  compat/test_realpath.c
  compat/test_sigaction.c
  compat/test_strcasecmp.c
  compat/test_strncasecmp.c
  compat/test_strtok_r.c
  date_delta.cpp
  distcheck_hdr.yml
  distcheck_in_src_dir.cmake.in
  distcheck_no_tcl.cmake.in
  distcheck_repo_verify.cmake.in
  distcheck_target.cmake.in
  distclean.cmake.in
  source_archive_setup.cmake.in
  tcltest.tcl.in
  probe_validation/README.md
  probe_validation/CMakeLists.txt
  probe_validation/probe_validation.cpp
  scripts/pgo_benchmark.cmake
  scripts/printtime.cmake
  scripts/tcl_mkindex.cmake
  scripts/timestamp.cmake
  package_name_test.cmake
  package_bundle_test.cmake
)
cmakefiles(${cmake_ignore_files})

cmakefiles(NOTES)

if(BUILD_TESTING)
  add_test(
    NAME cmake-package-bundle
    COMMAND "${CMAKE_COMMAND}"
      "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/package-bundle-test"
      "-DMODULE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/BRLCAD_Package_Bundle.cmake"
      "-DTEST_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
      "-DTEST_STRCLEAR=${STRCLEAR_EXECUTABLE}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/package_bundle_test.cmake"
  )
  find_package(Git QUIET)
  if(GIT_EXECUTABLE)
    add_test(
      NAME cmake-package-name
      COMMAND "${CMAKE_COMMAND}"
        "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/package-name-test"
        "-DMODULE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/BRLCAD_Package_Name.cmake"
        "-DOPTIONS_FILE=${CMAKE_BINARY_DIR}/BRLCAD_CPackOptions.cmake"
        "-DPROJECT_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/package_name_test.cmake"
    )
  endif()
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
