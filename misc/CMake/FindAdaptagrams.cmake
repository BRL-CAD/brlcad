# FindADAPTAGRAMS.cmake (modernized)
#
# Dual-mode finder for Adaptagrams:
#   1. Config mode (preferred): locate installed AdaptagramsConfig.cmake
#      and use exported targets adaptagrams::<component>.
#   2. Legacy mode: search headers/libs from a non-CMake upstream install
#      and synthesize imported INTERFACE targets.
#
# Exposed modern variables:
#   Adaptagrams_FOUND
#   Adaptagrams_INCLUDE_DIRS
#   Adaptagrams_LIBRARIES          (chooser targets adaptagrams::<comp>)
#   Adaptagrams_COMPONENTS         (components successfully detected)
#
# User hints:
#   Adaptagrams_ROOT               : prefix hint (added to CMAKE_PREFIX_PATH)
#   ADAPTAGRAMS_ROOT               : legacy hint also honored
#   Adaptagrams_USE_STATIC=ON      : prefer static libraries in legacy mode
#
# Invocation examples:
#   find_package(Adaptagrams CONFIG REQUIRED COMPONENTS avoid cola)
#   find_package(Adaptagrams MODULE)  # falls back to legacy if config unavailable
#
# Supported component names (fixed list for legacy mode):
#   avoid vpsc cola dialect topology project
#
# ------------------------------------------------------------------------------

if(DEFINED Adaptagrams_FOUND)
  return()
endif()

set(_Adaptagrams_ALL_COMPONENTS avoid vpsc cola dialect topology project)

# Capture user-specified components (if any) from outer find_package call
if(DEFINED Adaptagrams_FIND_COMPONENTS)
  set(_Adaptagrams_REQUESTED_COMPONENTS "${Adaptagrams_FIND_COMPONENTS}")
else()
  set(_Adaptagrams_REQUESTED_COMPONENTS "${_Adaptagrams_ALL_COMPONENTS}")
endif()

# Inject root hints into CMAKE_PREFIX_PATH so CONFIG search sees them
set(_Adaptagrams_ROOTS "")
if(Adaptagrams_ROOT)
  list(APPEND _Adaptagrams_ROOTS "${Adaptagrams_ROOT}")
endif()
if(ADAPTAGRAMS_ROOT) # legacy naming
  list(APPEND _Adaptagrams_ROOTS "${ADAPTAGRAMS_ROOT}")
endif()
foreach(_rt IN LISTS _Adaptagrams_ROOTS)
  if(_rt AND IS_DIRECTORY "${_rt}")
    list(PREPEND CMAKE_PREFIX_PATH "${_rt}")
  endif()
endforeach()

# --- Try CONFIG mode (NO_MODULE to skip this module) ---
find_package(Adaptagrams CONFIG QUIET NO_MODULE COMPONENTS ${_Adaptagrams_REQUESTED_COMPONENTS})

if(Adaptagrams_FOUND)
  # Collect targets & includes
  set(_Adaptagrams_TARGETS "")
  set(_Adaptagrams_INCS "")
  foreach(_comp IN LISTS _Adaptagrams_REQUESTED_COMPONENTS)
    if(TARGET adaptagrams::${_comp})
      list(APPEND _Adaptagrams_TARGETS adaptagrams::${_comp})
      get_target_property(_chooser_incs adaptagrams::${_comp} INTERFACE_INCLUDE_DIRECTORIES)
      if(_chooser_incs)
        list(APPEND _Adaptagrams_INCS ${_chooser_incs})
      else()
        # Fall back to variants for include dirs
        foreach(_variant static shared)
          if(TARGET adaptagrams::${_comp}_${_variant})
            get_target_property(_vincs adaptagrams::${_comp}_${_variant} INTERFACE_INCLUDE_DIRECTORIES)
            if(_vincs)
              list(APPEND _Adaptagrams_INCS ${_vincs})
            endif()
          endif()
        endforeach()
      endif()
    endif()
  endforeach()
  list(REMOVE_DUPLICATES _Adaptagrams_INCS)

  set(Adaptagrams_INCLUDE_DIRS "${_Adaptagrams_INCS}" CACHE STRING "Adaptagrams include directories (config)")
  set(Adaptagrams_LIBRARIES    "${_Adaptagrams_TARGETS}" CACHE STRING "Adaptagrams chooser targets (config)")
  # Limit components list to those actually found (some may be optional)
  set(Adaptagrams_COMPONENTS   "${_Adaptagrams_REQUESTED_COMPONENTS}" CACHE STRING "Adaptagrams requested components (config)")

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Adaptagrams DEFAULT_MSG Adaptagrams_LIBRARIES Adaptagrams_INCLUDE_DIRS)
  mark_as_advanced(Adaptagrams_INCLUDE_DIRS Adaptagrams_LIBRARIES Adaptagrams_COMPONENTS)
  return()
endif()

# ---------- Legacy fallback ----------
set(Adaptagrams_FOUND FALSE)
set(Adaptagrams_COMPONENTS "")

# Static preference
set(Adaptagrams_USE_STATIC "${Adaptagrams_USE_STATIC}")

function(_adaptagrams_pick_lib OUT NAME)
  if(Adaptagrams_USE_STATIC)
    find_library(_lib_static "${NAME}" PATH_SUFFIXES lib lib64)
    find_library(_lib_shared "${NAME}" PATH_SUFFIXES lib lib64)
    if(_lib_static)
      set(${OUT} "${_lib_static}" PARENT_SCOPE)
    else()
      set(${OUT} "${_lib_shared}" PARENT_SCOPE)
    endif()
  else()
    find_library(_lib_shared "${NAME}" PATH_SUFFIXES lib lib64)
    find_library(_lib_static "${NAME}" PATH_SUFFIXES lib lib64)
    if(_lib_shared)
      set(${OUT} "${_lib_shared}" PARENT_SCOPE)
    else()
      set(${OUT} "${_lib_static}" PARENT_SCOPE)
    endif()
  endif()
endfunction()

# Includes
find_path(_AVOID_INC    NAMES libavoid/libavoid.h)
find_path(_VPSC_INC     NAMES libvpsc/variable.h)
find_path(_COLA_INC     NAMES libcola/cola.h)
find_path(_DIALECT_INC  NAMES libdialect/libdialect.h)
find_path(_TOPOLOGY_INC NAMES libtopology/topology_graph.h)
find_path(_PROJECT_INC  NAMES libproject/project.h)

# Libraries
_adaptagrams_pick_lib(_AVOID_LIB    avoid)
_adaptagrams_pick_lib(_VPSC_LIB     vpsc)
_adaptagrams_pick_lib(_COLA_LIB     cola)
_adaptagrams_pick_lib(_DIALECT_LIB  dialect)
_adaptagrams_pick_lib(_TOPOLOGY_LIB topology)
_adaptagrams_pick_lib(_PROJECT_LIB  project)

# Determine found components (only those requested matter)
set(_Adaptagrams_pairs
  _AVOID_LIB avoid
  _VPSC_LIB vpsc
  _COLA_LIB cola
  _DIALECT_LIB dialect
  _TOPOLOGY_LIB topology
  _PROJECT_LIB project
)

list(LENGTH _Adaptagrams_pairs _n)
math(EXPR _count "${_n} / 2")
math(EXPR _upper "${_count} - 1")

foreach(_i RANGE 0 ${_upper})
  math(EXPR _var_idx "${_i} * 2")
  math(EXPR _name_idx "${_var_idx} + 1")
  list(GET _Adaptagrams_pairs ${_var_idx} _var)
  list(GET _Adaptagrams_pairs ${_name_idx} _name)
  if(${_var} AND _name IN_LIST _Adaptagrams_REQUESTED_COMPONENTS)
    list(APPEND Adaptagrams_COMPONENTS "${_name}")
  endif()
endforeach()

# Success criterion: at minimum avoid lib + header
if(_AVOID_LIB AND _AVOID_INC)
  set(Adaptagrams_FOUND TRUE)
endif()

# Synthesize targets
function(_adaptagrams_make_target NAME LIB INC)
  if(LIB AND NOT TARGET adaptagrams::${NAME})
    add_library(adaptagrams::${NAME} INTERFACE IMPORTED)
    if(INC)
      set_property(TARGET adaptagrams::${NAME} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${INC}")
    elseif(_AVOID_INC)
      set_property(TARGET adaptagrams::${NAME} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${_AVOID_INC}")
    endif()
    set_property(TARGET adaptagrams::${NAME} PROPERTY INTERFACE_LINK_LIBRARIES "${LIB}")
  endif()
endfunction()

_adaptagrams_make_target(avoid    "${_AVOID_LIB}"    "${_AVOID_INC}")
_adaptagrams_make_target(vpsc     "${_VPSC_LIB}"     "${_VPSC_INC}")
_adaptagrams_make_target(cola     "${_COLA_LIB}"     "${_COLA_INC}")
_adaptagrams_make_target(dialect  "${_DIALECT_LIB}"  "${_DIALECT_INC}")
_adaptagrams_make_target(topology "${_TOPOLOGY_LIB}" "${_TOPOLOGY_INC}")
_adaptagrams_make_target(project  "${_PROJECT_LIB}"  "${_PROJECT_INC}")

# Aggregate includes
set(Adaptagrams_INCLUDE_DIRS "")
foreach(_inc IN LISTS _AVOID_INC _VPSC_INC _COLA_INC _DIALECT_INC _TOPOLOGY_INC _PROJECT_INC)
  if(_inc)
    list(APPEND Adaptagrams_INCLUDE_DIRS "${_inc}")
  endif()
endforeach()
list(REMOVE_DUPLICATES Adaptagrams_INCLUDE_DIRS)

# Aggregate libraries (targets) only for components found
set(Adaptagrams_LIBRARIES "")
foreach(_c IN LISTS Adaptagrams_COMPONENTS)
  list(APPEND Adaptagrams_LIBRARIES adaptagrams::${_c})
endforeach()

# Cache the results
set(Adaptagrams_INCLUDE_DIRS "${Adaptagrams_INCLUDE_DIRS}" CACHE STRING "Adaptagrams include directories (legacy)")
set(Adaptagrams_LIBRARIES    "${Adaptagrams_LIBRARIES}"    CACHE STRING "Adaptagrams chooser targets (legacy)")
set(Adaptagrams_COMPONENTS   "${Adaptagrams_COMPONENTS}"   CACHE STRING "Adaptagrams components (legacy)")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Adaptagrams
  REQUIRED_VARS _AVOID_LIB _AVOID_INC
  HANDLE_COMPONENTS
)

mark_as_advanced(
  Adaptagrams_INCLUDE_DIRS Adaptagrams_LIBRARIES Adaptagrams_COMPONENTS
  _AVOID_LIB _VPSC_LIB _COLA_LIB _DIALECT_LIB _TOPOLOGY_LIB _PROJECT_LIB
  _AVOID_INC _VPSC_INC _COLA_INC _DIALECT_INC _TOPOLOGY_INC _PROJECT_INC
)
