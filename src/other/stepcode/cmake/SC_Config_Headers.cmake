# create sc_cf.h

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
configure_file(${SC_SOURCE_DIR}/include/sc_cf.h.in ${SC_BINARY_DIR}/${INCLUDE_DIR}/sc_cf.h.gen)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SC_BINARY_DIR}/${INCLUDE_DIR}/sc_cf.h.gen ${SC_BINARY_DIR}/${INCLUDE_DIR}/sc_cf.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

