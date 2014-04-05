# FIXME: For OS X, need flag for Framework or dylib
if(DYNAMIC_OPENSCENEGRAPH)
  add_definitions(-DOSGVIEWER_LIBRARY)
else()
  add_definitions(-DOSG_LIBRARY_STATIC)
endif()

set(LIB_COMMON_FILES
  config/AcrossAllScreens.cpp
  config/PanoramicSphericalDisplay.cpp
  config/SingleScreen.cpp
  config/SingleWindow.cpp
  config/SphericalDisplay.cpp
  config/WoWVxDisplay.cpp
  CompositeViewer.cpp
  GraphicsWindow.cpp
  HelpHandler.cpp
  Keystone.cpp
  Renderer.cpp
  Scene.cpp
  ScreenCaptureHandler.cpp
  StatsHandler.cpp
  Version.cpp
  View.cpp
  Viewer.cpp
  ViewerBase.cpp
  ViewerEventHandlers.cpp
  ${OPENSCENEGRAPH_VERSIONINFO_RC}
  )

# Collect all the configuration files
set(LIB_EXTRA_LIBS)

if(WIN32)
  # Enable workaround for OpenGL driver issues when used in multithreaded/multiscreen with NVidia drivers on Windows XP
  # For example: osgviewer dumptruck.osg was showing total garbage (screen looked like shattered, splashed hedgehog)
  # There were also serious issues with render to texture cameras.
  # Workaround repeats makeCurrentContext call as it was found that this causes the problems to dissapear.
  option(OSG_MULTIMONITOR_MULTITHREAD_WIN32_NVIDIA_WORKAROUND "Set to ON if you have NVidia board and drivers earlier than 177.92 ver" OFF)
  mark_as_advanced(OSG_MULTIMONITOR_MULTITHREAD_WIN32_NVIDIA_WORKAROUND)
  if(OSG_MULTIMONITOR_MULTITHREAD_WIN32_NVIDIA_WORKAROUND)
    add_definitions(-DOSG_MULTIMONITOR_MULTITHREAD_WIN32_NVIDIA_WORKAROUND)
  endif(OSG_MULTIMONITOR_MULTITHREAD_WIN32_NVIDIA_WORKAROUND)

  set(LIB_COMMON_FILES ${LIB_COMMON_FILES}
    GraphicsWindowWin32.cpp
    PixelBufferWin32.cpp
    )
elseif(APPLE AND NOT OSG_APPLE_X11)
  if(${OSG_OSX_SDK_NAME} STREQUAL "macosx10.4" OR
      ${OSG_OSX_SDK_NAME} STREQUAL "macosx10.3" OR
      ${OSG_OSX_SDK_NAME} STREQUAL "macosx10.2" OR
      ${OSG_OSX_SDK_NAME} STREQUAL "macosx10.1")
    set(OSG_WINDOWING_SYSTEM "Carbon" CACHE STRING "Windowing system type for graphics window creation, options Carbon, Cocoa or X11.")
  else()
    set(OSG_WINDOWING_SYSTEM "Cocoa" CACHE STRING "Windowing system type for graphics window creation, options Carbon, Cocoa or X11.")
  endif()

  if(${OSG_WINDOWING_SYSTEM} STREQUAL "Cocoa")
    add_definitions(-DUSE_DARWIN_COCOA_IMPLEMENTATION)

    if(OSG_COMPILE_FRAMEWORKS)
      set(LIB_COMMON_FILES ${LIB_COMMON_FILES}
	${HEADER_PATH}/api/Cocoa/GraphicsHandleCocoa
	${HEADER_PATH}/api/Cocoa/GraphicsWindowCocoa
	${HEADER_PATH}/api/Cocoa/PixelBufferCocoa
	)
      SET_PROPERTY(SOURCE ${HEADER_PATH}/api/Cocoa/GraphicsHandleCocoa PROPERTY MACOSX_PACKAGE_LOCATION Headers/api/Cocoa)
      SET_PROPERTY(SOURCE ${HEADER_PATH}/api/Cocoa/GraphicsWindowCocoa PROPERTY MACOSX_PACKAGE_LOCATION Headers/api/Cocoa)
      SET_PROPERTY(SOURCE ${HEADER_PATH}/api/Cocoa/PixelBufferCocoa PROPERTY MACOSX_PACKAGE_LOCATION Headers/api/Cocoa)
    endif(OSG_COMPILE_FRAMEWORKS)

    set(LIB_COMMON_FILES ${LIB_COMMON_FILES}
      GraphicsWindowCocoa.mm
      DarwinUtils.h
      DarwinUtils.mm
      PixelBufferCocoa.mm
      )
    set(LIB_EXTRA_LIBS ${COCOA_LIBRARY} ${LIB_EXTRA_LIBS})

  elseif(${OSG_WINDOWING_SYSTEM} STREQUAL "Carbon")

    add_definitions(-DUSE_DARWIN_CARBON_IMPLEMENTATION)

    if(OSG_COMPILE_FRAMEWORKS)
      set(LIB_COMMON_FILES ${LIB_COMMON_FILES}
	${HEADER_PATH}/api/Carbon/GraphicsHandleCarbon
	${HEADER_PATH}/api/Carbon/GraphicsWindowCarbon
	${HEADER_PATH}/api/Carbon/PixelBufferCarbon
	)
      SET_PROPERTY(SOURCE ${HEADER_PATH}/api/Carbon/GraphicsHandleCarbon PROPERTY MACOSX_PACKAGE_LOCATION Headers/api/Carbon)
      SET_PROPERTY(SOURCE ${HEADER_PATH}/api/Carbon/GraphicsWindowCarbon PROPERTY MACOSX_PACKAGE_LOCATION Headers/api/Carbon)
      SET_PROPERTY(SOURCE ${HEADER_PATH}/api/Carbon/PixelBufferCarbon PROPERTY MACOSX_PACKAGE_LOCATION Headers/api/Carbon)
    endif(OSG_COMPILE_FRAMEWORKS)

    set(LIB_COMMON_FILES ${LIB_COMMON_FILES}
      GraphicsWindowCarbon.cpp
      DarwinUtils.h
      DarwinUtils.mm
      PixelBufferCarbon.cpp
      )

    set(LIB_EXTRA_LIBS ${COCOA_LIBRARY} ${AGL_LIBRARY} ${LIB_EXTRA_LIBS})

  endif(${OSG_WINDOWING_SYSTEM} STREQUAL "Cocoa")

else()
  # X11 for everybody else
  include(FindPkgConfig OPTIONAL)
  if(PKG_CONFIG_FOUND)
    PKG_CHECK_MODULES(XRANDR xrandr)
    if(XRANDR_FOUND)
      OPTION(OSGVIEWER_USE_XRANDR "Set to ON to enable Xrandr support for GraphicsWindowX11." ON)
    else()
      set(OSGVIEWER_USE_XRANDR OFF)
    endif()
  else()
    set(OSGVIEWER_USE_XRANDR OFF)
  endif()

  set(LIB_COMMON_FILES ${LIB_COMMON_FILES}
    GraphicsWindowX11.cpp
    PixelBufferX11.cpp
    )

  IF(OSGVIEWER_USE_XRANDR)
    ADD_DEFINITIONS(-DOSGVIEWER_USE_XRANDR)
    SET(LIB_PRIVATE_HEADERS ${LIB_PRIVATE_HEADERS} ${XRANDR_INCLUDE_DIRS} )

    IF(X11_Xrandr_LIB)
      SET(LIB_EXTRA_LIBS ${X11_Xrandr_LIB} ${LIB_EXTRA_LIBS})
    ELSE()
      SET(LIB_EXTRA_LIBS ${XRANDR_LIBRARIES} ${LIB_EXTRA_LIBS})
    ENDIF()
  ENDIF()

  # X11 on Apple requires X11 library plus OpenGL linking hack on Leopard
  if(APPLE)
    # Find GL/glx.h
    if(EXISTS ${CMAKE_OSX_SYSROOT}/usr/X11/include/GL/glx.h)
      set(OPENGL_INCLUDE_DIR /usr/X11/include)
      set(OPENGL_LIBRARIES /usr/X11/lib/libGL.dylib)
    elseif(EXISTS ${CMAKE_OSX_SYSROOT}/usr/X11R6/include/GL/glx.h)
      set(OPENGL_INCLUDE_DIR /usr/X11R6/include)
      set(OPENGL_LIBRARIES /usr/X11R6/lib/libGL.dylib)
    endif()
    include_directories(BEFORE SYSTEM ${OPENGL_INCLUDE_DIR})

    SET(LIB_EXTRA_LIBS ${X11_X11_LIB} ${OPENGL_LIBRARIES} ${LIB_EXTRA_LIBS})
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-dylib_file,/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib:${CMAKE_OSX_SYSROOT}/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib")

  else(APPLE)

    set(LIB_EXTRA_LIBS ${X11_X11_LIB} ${LIB_EXTRA_LIBS})

  endif(APPLE)
endif()

set(TARGET_EXTERNAL_LIBRARIES ${LIB_EXTRA_LIBS})

if(MINGW OR CYGWIN)
    set(TARGET_EXTERNAL_LIBRARIES ${TARGET_EXTERNAL_LIBRARIES} gdi32 )
endif()

add_library(osgViewer ${OPENSCENEGRAPH_USER_DEFINED_DYNAMIC_OR_STATIC} ${LIB_COMMON_FILES})
target_link_libraries(osgViewer OpenThreads ${CMAKE_THREAD_LIBS_INIT}
  ${MATH_LIBRARY} ${RT_LIBRARY} ${DL_LIBRARY} ${TARGET_EXTERNAL_LIBRARIES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
