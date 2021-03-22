# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set CMake project name
project(CHRPATH)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

include(CheckIncludeFiles)
check_include_files(elf.h HAVE_ELF_H)
check_include_files(fcntl.h HAVE_FCNTL_H)
check_include_files(getopt.h HAVE_GETOPT_H)
check_include_files(inttypes.h HAVE_INTTYPES_H)
check_include_files(link.h HAVE_LINK_H)
check_include_files(memory.h HAVE_MEMORY_H)
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(stdlib.h HAVE_STDLIB_H)
check_include_files(strings.h HAVE_STRINGS_H)
check_include_files(string.h HAVE_STRING_H)
check_include_files(sys/link.h HAVE_SYS_LINK_H)
check_include_files(sys/stat.h HAVE_SYS_STAT_H)
check_include_files(sys/types.h HAVE_SYS_TYPES_H)
check_include_files(unistd.h HAVE_UNISTD_H)

include(CheckFunctionExists)
check_function_exists(getopt_long HAVE_GETOPT_LONG)

set(SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P})

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(CHRPATH_SRCS
  chrpath.c
  elf.c
  killrpath.c
  main.c
  )

add_executable(chrpath ${CHRPATH_SRCS})
set_target_properties(chrpath PROPERTIES FOLDER "Compilation Utilities")
set_property(TARGET chrpath APPEND PROPERTY COMPILE_DEFINITIONS HAVE_CONFIG_H)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
