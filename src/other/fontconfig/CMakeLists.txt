cmake_minimum_required(VERSION 3.1.3)

# set CMake project name
project(FONTCONFIG)


if (NOT BIN_DIR)
  set(BIN_DIR bin)
endif (NOT BIN_DIR)

if (NOT INCLUDE_DIR)
  set(INCLUDE_DIR include)
endif (NOT INCLUDE_DIR)

if (NOT LIB_DIR)
  set(LIB_DIR lib)
endif (NOT LIB_DIR)

if (NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif (NOT DEFINED BUILD_SHARED_LIBS)

if (NOT DEFINED BUILD_STATIC_LIBS)
  set(BUILD_STATIC_LIBS OFF)
endif (NOT DEFINED BUILD_STATIC_LIBS)

if (MSVC)
  set(FLEXIBLE_ARRAY_MEMBERS 1)
else (MSVC)
  set(FLEXIBLE_ARRAY_MEMBERS)
endif (MSVC)

include(CheckLibraryExists)
check_library_exists(m cos "" HAVE_M_LIBRARY)
if (HAVE_M_LIBRARY)
  set(SYS_LIBS ${SYS_LIBS} m)
endif (HAVE_M_LIBRARY)

include(CheckIncludeFiles)
check_include_files(dirent.h HAVE_DIRENT_H)
check_include_files(dlfcn.h HAVE_DLFCN_H)
check_include_files(fcntl.h HAVE_FCNTL_H)
check_include_files(inttypes.h HAVE_INTTYPES_H)
check_include_files(memory.h HAVE_MEMORY_H)
check_include_files(ndir.h HAVE_NDIR_H)
check_include_files(sched.h HAVE_SCHED_H)
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(stdlib.h HAVE_STDLIB_H)
check_include_files(string.h HAVE_STRING_H)
check_include_files(strings.h HAVE_STRINGS_H)
check_include_files(sys/dir.h HAVE_SYS_DIR_H)
check_include_files(sys/mount.h HAVE_SYS_MOUNT_H)
check_include_files(sys/ndir.h HAVE_SYS_NDIR_H)
check_include_files(sys/param.h HAVE_SYS_PARAM_H)
check_include_files(sys/stat.h HAVE_SYS_STAT_H)
check_include_files(sys/statfs.h HAVE_SYS_STATVFS_H)
check_include_files(sys/statvfs.h HAVE_SYS_STATVFS_H)
check_include_files(sys/types.h HAVE_SYS_TYPES_H)
check_include_files(sys/vfs.h HAVE_SYS_VFS_H)
check_include_files(unistd.h HAVE_UNISTD_H)

include (CheckFunctionExists)
check_function_exists(__mkstemp_s HAVE__MKSTEMP_S)
check_function_exists(fstatfs HAVE_FSTATFS)
check_function_exists(fstatvfs HAVE_FSTATVFS)
check_function_exists(getexecname HAVE_GETEXECNAME)
check_function_exists(getopt HAVE_GETOPT)
check_function_exists(getopt_long HAVE_GETOPT_LONG)
check_function_exists(getpagesize HAVE_GETPAGESIZE)
check_function_exists(getprogname HAVE_GETPROGNAME)
check_function_exists(gettext HAVE_GETTEXT)
check_function_exists(iconv HAVE_ICONV)
check_function_exists(link HAVE_LINK)
check_function_exists(lrand48 HAVE_LRAND48)
check_function_exists(lstat HAVE_LSTAT)
check_function_exists(localeconv HAVE_LOCALECONV)
check_function_exists(mkdtemp HAVE_MKDTEMP)
check_function_exists(mkostemp HAVE_MKOSTEMP)
check_function_exists(mkstemp HAVE_MKSTEMP)
check_function_exists(mmap HAVE_MMAP)
check_function_exists(posix_fadvise HAVE_POSIX_FADVISE)
check_function_exists(rand HAVE_RAND)
check_function_exists(rand_r HAVE_RAND_R)
check_function_exists(random HAVE_RANDOM)
check_function_exists(random_r HAVE_RANDOM_R)
check_function_exists(readlink HAVE_READLINK)
check_function_exists(strerror HAVE_STRERROR)
check_function_exists(strerror_r HAVE_STRERROR_R)
check_function_exists(vprintf HAVE_VPRINTF)

include(CheckTypeSize)
check_type_size(char SIZEOF_CHAR)
check_type_size(int SIZEOF_INT)
check_type_size(long SIZEOF_LONG)
check_type_size(short SIZEOF_SHORT)
check_type_size("void *" SIZEOF_VOID_P)

find_package(Threads)
if (CMAKE_HAVE_PTHREAD_CREATE)
  set(HAVE_PTHREAD 1)
endif (CMAKE_HAVE_PTHREAD_CREATE)

find_package(PNG)
find_package(Freetype)

include(CheckCSourceRuns)
set(INTEL_ATOMIC_SRC "
int main()
{
  void memory_barrier (void) { __sync_synchronize (); }
  int atomic_add (int *i) { return __sync_fetch_and_add (i, 1); }
  int mutex_trylock (int *m) { return __sync_lock_test_and_set (m, 1); }
  void mutex_unlock (int *m) { __sync_lock_release (m);}
}
")
check_c_source_runs("${INTEL_ATOMIC_SRC}" HAVE_INTEL_ATOMIC_PRIMITIVES)

include("${CMAKE_CURRENT_SOURCE_DIR}/expat/ConfigureChecks.cmake")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/expat/expat_config.h.cmake" ${CMAKE_CURRENT_BINARY_DIR}/include/expat_config.h)

set(FC_DEFAULT_FONTS "${CMAKE_INSTALL_PREFIX}/share/fonts")

configure_file(include/config.h.in ${CMAKE_BINARY_DIR}/include/config.h)

add_definitions(-DFONTCONFIG_PATH="${CMAKE_INSTALL_PREFIX}/share/fonts")
add_definitions(-DFC_CACHEDIR="${CMAKE_INSTALL_PREFIX}/cache/fontconfig")
add_definitions(-DFC_TEMPLATEDIR="${CMAKE_INSTALL_PREFIX}/fontconfig/conf.avail")

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

add_subdirectory(src)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
