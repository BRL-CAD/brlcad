include_directories(
	../..
	)

find_package(Threads)

if(CMAKE_THREAD_LIBS_INIT)

SET(thread_win32_SRCS
	src/win32/thread.cpp
	src/win32/tss_dll.cpp
	src/win32/tss_pe.cpp
	)

set(pthread_SRCS
	src/pthread/thread.cpp
	src/pthread/once.cpp
	)

IF(WIN32)
	SET(thread_SRCS ${thread_win32_SRCS})
ELSE(WIN32)
	SET(thread_SRCS ${pthread_SRCS})
ENDIF(WIN32)

add_library(boost_threads SHARED ${thread_SRCS})
target_link_libraries(boost_threads ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(boost_threads PROPERTIES LINK_FLAGS "-DBOOST_THREAD_USE_DLL=1")

add_library(boost_threads_static STATIC ${thread_SRCS})
target_link_libraries(boost_threads_static ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(boost_threads_static PROPERTIES LINK_FLAGS "-DBOOST_THREAD_USE_LIB=1")

endif(CMAKE_THREAD_LIBS_INIT)
