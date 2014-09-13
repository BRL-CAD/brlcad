if(DYNAMIC_OPENSCENEGRAPH)
  add_definitions(-DOSGGA_LIBRARY)
else(DYNAMIC_OPENSCENEGRAPH)
  add_definitions(-DOSG_LIBRARY_STATIC)
endif(DYNAMIC_OPENSCENEGRAPH)

set(OSGGA_SRCS
  AnimationPathManipulator.cpp
  DriveManipulator.cpp
  Device.cpp
  EventQueue.cpp
  EventVisitor.cpp
  FirstPersonManipulator.cpp
  FlightManipulator.cpp
  GUIEventAdapter.cpp
  GUIEventHandler.cpp
  KeySwitchMatrixManipulator.cpp
  CameraManipulator.cpp
  MultiTouchTrackballManipulator.cpp
  NodeTrackerManipulator.cpp
  OrbitManipulator.cpp
  StandardManipulator.cpp
  SphericalManipulator.cpp
  StateSetManipulator.cpp
  TerrainManipulator.cpp
  TrackballManipulator.cpp
  UFOManipulator.cpp
  Version.cpp
  CameraViewSwitchManipulator.cpp
  ${OPENSCENEGRAPH_VERSIONINFO_RC}
  )

add_library(osgGA ${OPENSCENEGRAPH_USER_DEFINED_DYNAMIC_OR_STATIC} ${OSGGA_SRCS})
target_link_libraries(osgGA osgDB osgUtil osg OpenThreads ${CMAKE_THREAD_LIBS_INIT}
  ${MATH_LIBRARY} ${RT_LIBRARY} ${DL_LIBRARY})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
