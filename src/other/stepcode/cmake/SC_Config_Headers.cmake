# create sc_cf.h and sc_version_string.h

# Take the sc config file template as the starting point for
# sc_cf.h.in - scripts may need to append to the template, so
# it is read into memory initially.
set(CONFIG_H_FILE ${SC_BINARY_DIR}/include/sc_cf.h.in)
set_source_files_properties(${CONFIG_H_FILE} PROPERTIES GENERATED TRUE)
set(CMAKE_CURRENT_PROJECT SC)
define_property(GLOBAL PROPERTY SC_CONFIG_H_CONTENTS BRIEF_DOCS "config.h.in contents" FULL_DOCS "config.h.in contents for SC project")
if(NOT COMMAND CONFIG_H_APPEND)
  macro(CONFIG_H_APPEND PROJECT_NAME NEW_CONTENTS)
    if(PROJECT_NAME)
      get_property(${PROJECT_NAME}_CONFIG_H_CONTENTS GLOBAL PROPERTY ${PROJECT_NAME}_CONFIG_H_CONTENTS)
      set(${PROJECT_NAME}_CONFIG_H_FILE_CONTENTS "${${PROJECT_NAME}_CONFIG_H_CONTENTS}${NEW_CONTENTS}")
      set_property(GLOBAL PROPERTY ${PROJECT_NAME}_CONFIG_H_CONTENTS "${${PROJECT_NAME}_CONFIG_H_FILE_CONTENTS}")
    endif(PROJECT_NAME)
  endmacro(CONFIG_H_APPEND NEW_CONTENTS)
endif(NOT COMMAND CONFIG_H_APPEND)
file(READ ${SC_SOURCE_DIR}/include/sc_cf_cmake.h.in CONFIG_H_FILE_CONTENTS)
CONFIG_H_APPEND(SC "${CONFIG_H_FILE_CONTENTS}")

include(CheckLibraryExists)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CMakePushCheckState)
include(CheckCSourceCompiles)
include(CheckCXXSourceRuns)

CHECK_INCLUDE_FILE(ndir.h HAVE_NDIR_H)
CHECK_INCLUDE_FILE(stdarg.h HAVE_STDARG_H)
CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE(sys/param.h HAVE_SYS_PARAM_H)
CHECK_INCLUDE_FILE(sysent.h HAVE_SYSENT_H)
CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILE(dirent.h HAVE_DIRENT_H)
CHECK_INCLUDE_FILE(stdbool.h HAVE_STDBOOL_H)
CHECK_INCLUDE_FILE(process.h HAVE_PROCESS_H)
CHECK_INCLUDE_FILE(io.h HAVE_IO_H)

# ensure macro functions are captured
CHECK_SYMBOL_EXISTS(abs "stdlib.h" HAVE_ABS)
CHECK_SYMBOL_EXISTS(memcpy "string.h" HAVE_MEMCPY)
CHECK_SYMBOL_EXISTS(memmove "string.h" HAVE_MEMMOVE)
CHECK_SYMBOL_EXISTS(getopt "getopt.h" HAVE_GETOPT)
CHECK_SYMBOL_EXISTS(snprintf "stdio.h" HAVE_SNPRINTF)
CHECK_SYMBOL_EXISTS(vsnprintf "stdio.h" HAVE_VSNPRINTF)

CHECK_TYPE_SIZE("ssize_t" SSIZE_T)

if(SC_ENABLE_CXX11)
  set( TEST_STD_THREAD "
#include <iostream>
#include <thread>
void do_work() {std::cout << \"thread\" << std::endl;}
int main() {std::thread t(do_work);t.join();}
  " )
  cmake_push_check_state()
  if( UNIX )
    set( CMAKE_REQUIRED_FLAGS "-pthread -std=c++11" )
  else( UNIX )
    # vars probably need set for embarcadero, etc
  endif( UNIX )
  CHECK_CXX_SOURCE_RUNS( "${TEST_STD_THREAD}" HAVE_STD_THREAD )   #quotes are *required*!
  cmake_pop_check_state()

  set( TEST_STD_CHRONO "
#include <iostream>
#include <chrono>
int main() {
std::chrono::seconds sec(1);
std::cout << \"1s is \"<< std::chrono::duration_cast<std::chrono::milliseconds>(sec).count() << \" ms\" << std::endl;
}
  " )
  cmake_push_check_state()
  if( UNIX )
    set( CMAKE_REQUIRED_FLAGS "-std=c++11" )
  else( UNIX )
    # vars probably need set for embarcadero, etc
  endif( UNIX )
  CHECK_CXX_SOURCE_RUNS( "${TEST_STD_CHRONO}" HAVE_STD_CHRONO )   #quotes are *required*!
  cmake_pop_check_state()

  set( TEST_NULLPTR "
#include <cstddef>
std::nullptr_t f() {return nullptr;}
int main() {return !(f() == f());}
  " )
  cmake_push_check_state()
  if( UNIX )
    set( CMAKE_REQUIRED_FLAGS "-std=c++11" )
  else( UNIX )
    # vars probably need set for embarcadero, etc
  endif( UNIX )
  CHECK_CXX_SOURCE_RUNS( "${TEST_NULLPTR}" HAVE_NULLPTR )   #quotes are *required*!
  cmake_pop_check_state()
endif(SC_ENABLE_CXX11)

# Now that all the tests are done, configure the sc_cf.h file:
get_property(CONFIG_H_FILE_CONTENTS GLOBAL PROPERTY SC_CONFIG_H_CONTENTS)
file(WRITE ${CONFIG_H_FILE} "${CONFIG_H_FILE_CONTENTS}")
configure_file(${CONFIG_H_FILE} ${SC_BINARY_DIR}/${INCLUDE_INSTALL_DIR}/sc_cf.h)

set(VER_HDR "
#ifndef SC_VERSION_STRING
#define SC_VERSION_STRING
static char sc_version[512] = {\"${SC_VERSION}\"};
#endif"
)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${INCLUDE_INSTALL_DIR}/sc_version_string.h "${VER_HDR}")
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/${INCLUDE_INSTALL_DIR}/sc_version_string.h
  PROPERTIES GENERATED TRUE
  HEADER_FILE_ONLY TRUE
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

