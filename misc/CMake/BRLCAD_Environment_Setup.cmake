# Setup and checks related to system environment settings

#---------------------------------------------------------------------
# Save the current LC_ALL, LC_MESSAGES, and LANG environment variables
# and set them to "C" so things like date output are as expected.
set(_orig_lc_all      $ENV{LC_ALL})
set(_orig_lc_messages $ENV{LC_MESSAGES})
set(_orig_lang        $ENV{LANG})
if(_orig_lc_all)
  set(ENV{LC_ALL}      C)
endif(_orig_lc_all)
if(_orig_lc_messages)
  set(ENV{LC_MESSAGES} C)
endif(_orig_lc_messages)
if(_orig_lang)
  set(ENV{LANG}        C)
endif(_orig_lang)

#---------------------------------------------------------------------
# Package creation with CMake depends on the value of umask - if permissions
# are such that temporary files are created without permissions needed for
# generated packages, the resulting packages may behave badly when installed.
# In particular, RPM packages may improperly reset permissions on core
# directories such as /usr.
function(check_umask umask_val status_var)
  string(REGEX REPLACE "[^x]" "" umask_x "${umask_val}")
  string(REGEX REPLACE "[^r]" "" umask_r "${umask_val}")
  string(LENGTH "${umask_r}" UMASK_HAVE_R)
  set(${status_var} 0 PARENT_SCOPE)
  if(UMASK_HAVE_R AND "${umask_x}" STREQUAL "xxx")
    set(${status_var} 1 PARENT_SCOPE)
  endif(UMASK_HAVE_R AND "${umask_x}" STREQUAL "xxx")
endfunction(check_umask)

# Note - umask is not always an executable, so find_program wont' necessarily
# determine whether the umask check is appropriate.  If we don't find an
# executable, follow up to see if we can use sh to get the info.
find_program(UMASK_EXEC umask)
mark_as_advanced(UMASK_EXEC)
if(NOT UMASK_EXEC)
  # If we don't have a umask cmd, see if sh -c "umask -S" works
  execute_process(COMMAND sh -c "umask -S" OUTPUT_VARIABLE umask_out)
  # Check if we've got something that looks like a umask output
  if("${umask_out}" MATCHES "^u=.*g=.*o=.*")
    set(UMASK_EXEC sh)
    set(UMASK_EXEC_ARGS -c "umask -S")
  endif("${umask_out}" MATCHES "^u=.*g=.*o=.*")
else(NOT UMASK_EXEC)
  set(UMASK_EXEC_ARGS -S)
endif(NOT UMASK_EXEC)

if(UMASK_EXEC)
  execute_process(COMMAND ${UMASK_EXEC} ${UMASK_EXEC_ARGS} OUTPUT_VARIABLE umask_curr)
  string(STRIP "${umask_curr}" umask_curr)
  check_umask("${umask_curr}" UMASK_OK)
  if(NOT UMASK_OK)
    message(" ")
    message(WARNING "umask is set to ${umask_curr} - this setting is not recommended if one of the goals of this build is to generate packages. Use 'umask 022' for improved package behavior.")
    if(SLEEP_EXEC)
      execute_process(COMMAND ${SLEEP_EXEC} 1)
    endif(SLEEP_EXEC)
  endif(NOT UMASK_OK)
endif(UMASK_EXEC)

#---------------------------------------------------------------------
# Allow the BRLCAD_ROOT environment variable to set during build, but be noisy
# about it unless we're specifically told this is intentional.  Having this
# happen by accident is generally not a good idea.
find_program(SLEEP_EXEC sleep)
mark_as_advanced(SLEEP_EXEC)
if(NOT "$ENV{BRLCAD_ROOT}" STREQUAL "" AND NOT BRLCAD_ROOT_OVERRIDE)
  message(WARNING "\nBRLCAD_ROOT is presently set to \"$ENV{BRLCAD_ROOT}\"\nBRLCAD_ROOT should typically be used only when needed as a runtime override, not during compilation.  Building with BRLCAD_ROOT set may produce unexpected behavior during both compilation and subsequent program execution.  It is *highly* recommended that BRLCAD_ROOT be unset and not used.\n")
  if(SLEEP_EXEC)
    execute_process(COMMAND ${SLEEP_EXEC} 2)
  endif(SLEEP_EXEC)
endif(NOT "$ENV{BRLCAD_ROOT}" STREQUAL "" AND NOT BRLCAD_ROOT_OVERRIDE)

