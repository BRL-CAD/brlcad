MACRO(FIND_TCL_PACKAGE wishcmd packagename)
   MESSAGE("wishcmd: ${wishcmd} packagename: ${packagename}")
   STRING(TOUPPER ${packagename} PKGNAME_UPPER)
   SET(packagefind_script "
set packageversion NOTFOUND
set packageversion [package versions ${packagename}]
set filename \"${CMAKE_BINARY_DIR}/CMakeTmp/${PKGNAME_UPPER}_PKG_VERSION\"
set fileId [open $filename \"w\"]
puts $fileId $packageversion
close $fileId
exit
")
   SET(packagefind_scriptfile "${CMAKE_BINARY_DIR}/CMakeTmp/${packagename}_packageversion.tcl")
   FILE(WRITE ${packagefind_scriptfile} ${packagefind_script})
   EXEC_PROGRAM(${wishcmd} ARGS ${packagefind_scriptfile} OUTPUT_VARIABLE EXECOUTPUT)
   FILE(READ ${CMAKE_BINARY_DIR}/CMakeTmp/${PKGNAME_UPPER}_PKG_VERSION pkgversion)
   #Need to handle multiple returned versions - this regex is wrong, fix
   #STRING(REGEX REPLACE "([0-9]+\.?[0-9]*)" "\\1" ${pkgversion} ${pkgversion})
   STRING(REGEX REPLACE "\n" "" ${PKGNAME_UPPER}_PACKAGE_VERSION ${pkgversion})
   FIND_PACKAGE_HANDLE_STANDARD_ARGS(${PKGNAME_UPPER} DEFAULT_MSG ${PKGNAME_UPPER}_PACKAGE_VERSION)
   MARK_AS_ADVANCED(
      ${PKGNAME_UPPER}_PACKAGE_VERSION
   )
ENDMACRO(FIND_TCL_PACKAGE)

