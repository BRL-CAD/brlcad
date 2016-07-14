# This file should only be included when WIN32

SET(LIB_NAME OpenThreads)

ADD_DEFINITIONS(-DWIN32_LEAN_AND_MEAN)
ADD_DEFINITIONS(-D_WIN32_WINNT=0x0400)

ADD_LIBRARY(${LIB_NAME}
    HandleHolder.h
    Win32BarrierPrivateData.h
    WIN32Condition.cpp
    Win32ConditionPrivateData.h
    Win32Mutex.cpp
    Win32MutexPrivateData.h
    Win32Thread.cpp
    Win32ThreadBarrier.cpp
    Win32ThreadPrivateData.h
    ../common/Version.cpp
    ../common/Atomic.cpp
    ${OPENTHREADS_VERSIONINFO_RC}
)

SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES FOLDER "OSG Core")
IF(OPENTHREADS_SONAMES)
  SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES VERSION ${OPENTHREADS_VERSION} SOVERSION ${OPENTHREADS_SOVERSION})
ENDIF()

# Since we're building different platforms binaries in 
# their respective directories, we need to set the 
# link directory so it can find this location.
LINK_DIRECTORIES(
    ${CMAKE_CURRENT_BINARY_DIR}
)

IF(MSVC AND OSG_MSVC_VERSIONED_DLL)
    HANDLE_MSVC_DLL(ot ${OPENTHREADS_SOVERSION})
ENDIF()

INSTALL(
    TARGETS OpenThreads
    ARCHIVE DESTINATION lib COMPONENT libopenthreads-dev
    LIBRARY DESTINATION lib COMPONENT libopenthreads
    RUNTIME DESTINATION bin COMPONENT libopenthreads
)

#commented out# INCLUDE(ModuleInstall OPTIONAL)
