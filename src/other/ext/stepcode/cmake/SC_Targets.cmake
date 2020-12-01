macro(SC_ADDEXEC execname)
  set(_addexec_opts NO_INSTALL TESTABLE)
  set(_addexec_multikw SOURCES LINK_LIBRARIES)
  string(TOUPPER "SC_ADDEXEC_${execname}_ARG" _arg_prefix)
  cmake_parse_arguments(${_arg_prefix} "${_addexec_opts}" "" "${_addexec_multikw}" ${ARGN})

  if(NOT DEFINED "${_arg_prefix}_SOURCES")
    message(SEND_ERROR "SC_ADDEXEC usage error!")
  endif()

  add_executable(${execname} ${${_arg_prefix}_SOURCES})

  if(DEFINED "${_arg_prefix}_LINK_LIBRARIES")
    foreach(_lib ${${_arg_prefix}_LINK_LIBRARIES})
        if($CACHE{SC_STATIC_UTILS})
            if(NOT $<TARGET_PROPERTY:${_lib},TYPE> STREQUAL "STATIC_LIBRARY")
                message(SEND_ERROR "SC_ADDEXEC usage error - expected STATIC LINK_LIBRARIES targets (${_lib})")
            endif()
        endif()
        target_link_libraries(${execname} ${_lib})
    endforeach()
  endif()

  if(NOT ${_arg_prefix}_NO_INSTALL AND NOT ${_arg_prefix}_TESTABLE)
    install(TARGETS ${execname}
      RUNTIME DESTINATION ${BIN_DIR}
      LIBRARY DESTINATION ${LIB_DIR}
      ARCHIVE DESTINATION ${LIB_DIR}	
     )
  endif()

  if(NOT SC_ENABLE_TESTING AND ${_arg_prefix}_TESTABLE)
    set_target_properties(${execname} PROPERTIES EXCLUDE_FROM_ALL ON)
  endif()
endmacro()

macro(SC_ADDLIB _addlib_target)
  set(_addlib_opts SHARED STATIC NO_INSTALL TESTABLE)
  set(_addlib_multikw SOURCES LINK_LIBRARIES)
  string(TOUPPER "SC_ADDLIB_${libname}_ARG" _arg_prefix)
  cmake_parse_arguments(${_arg_prefix} "${_addlib_opts}" "" "${_addlib_multikw}" ${ARGN})

  if(NOT DEFINED ${_arg_prefix}_SOURCES)
    message(SEND_ERROR "SC_ADDLIB usage error!")
  endif()

  if(${_arg_prefix}_SHARED)
    add_library(${_addlib_target} SHARED ${${_arg_prefix}_SOURCES})
    set_target_properties(${_addlib_target} PROPERTIES VERSION ${SC_ABI_VERSION} SOVERSION ${SC_ABI_SOVERSION})
    if(APPLE)
      set_target_properties(${_addlib_target} PROPERTIES LINK_FLAGS "-flat_namespace -undefined suppress")
    endif(APPLE)
  elseif(${_arg_prefix}_STATIC)
    add_library(${_addlib_target} STATIC ${${_arg_prefix}_SOURCES})
    target_compile_definitions(${_addlib_target} PRIVATE SC_STATIC)
  else()
    message(SEND_ERROR "SC_ADDLIB usage error!")
  endif()

  if(DEFINED ${_arg_prefix}_LINK_LIBRARIES)
    foreach(_lib ${${_arg_prefix}_LINK_LIBRARIES})
        if(${_arg_prefix}_STATIC AND TARGET ${_lib})
	  get_property(_libtype TARGET ${_lib} PROPERTY TYPE)
	  if(NOT ${_libtype} STREQUAL "STATIC_LIBRARY")
	      message(SEND_ERROR "SC_ADDLIB usage error - expected (static) LINK_LIBRARIES targets (${_lib})")
            endif()
        endif()
        target_link_libraries(${_addlib_target} ${_lib})
    endforeach()
  endif()

  if(NOT ${_arg_prefix}_NO_INSTALL AND NOT ${_arg_prefix}_TESTABLE)
    install(TARGETS ${_addlib_target}
      RUNTIME DESTINATION ${BIN_DIR}
      LIBRARY DESTINATION ${LIB_DIR}
      ARCHIVE DESTINATION ${LIB_DIR}	
    )
  endif()
endmacro()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

