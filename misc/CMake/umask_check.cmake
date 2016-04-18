# Package creation with CMake depends on the value of umask - if permissions
# are such that temporary files are created without permissions needed for
# generated packages, the resulting packages may behave badly when installed.
# In particular, RPM packages may improperly reset permissions on core
# directories such as /usr.
macro(CHECK_UMASK umask_val status_var)
  string(REGEX REPLACE "[^x]" "" umask_x "${umask_val}")
  string(REGEX REPLACE "[^r]" "" umask_r "${umask_val}")
  string(LENGTH "${umask_r}" UMASK_HAVE_R)
  set(${status_var} 0)
  if(UMASK_HAVE_R AND "${umask_x}" STREQUAL "xxx")
    set(${status_var} 1)
  endif(UMASK_HAVE_R AND "${umask_x}" STREQUAL "xxx")
endmacro(CHECK_UMASK)
# Note - umask is not always an executable, so we can't use find_program
# to determine whether the umask check is appropriate.
IF(NOT WIN32)
  exec_program(umask ARGS -S OUTPUT_VARIABLE umask_curr)
  string(STRIP "${umask_curr}" umask_curr)
  CHECK_UMASK("${umask_curr}" UMASK_OK)
  if (NOT UMASK_OK)
    message(" ")
    message(WARNING "umask is set to ${umask_curr} - this setting is not recommended if one of the goals of this build is to generate packages. Use 'umask 022' for improved package behavior.")
    if(SLEEP_EXEC)
      execute_process(COMMAND ${SLEEP_EXEC} 1)
    endif(SLEEP_EXEC)
  endif (NOT UMASK_OK)
ENDIF(NOT WIN32)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

