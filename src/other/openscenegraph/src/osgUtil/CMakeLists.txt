# FIXME: For OS X, need flag for Framework or dylib
if(DYNAMIC_OPENSCENEGRAPH)
  add_definitions(-DOSGUTIL_LIBRARY)
else()
  add_definitions(-DOSG_LIBRARY_STATIC)
endif()

set(OSGUTIL_SRCS
  CubeMapGenerator.cpp
  CullVisitor.cpp
  DelaunayTriangulator.cpp
  DisplayRequirementsVisitor.cpp
  DrawElementTypeSimplifier.cpp
  EdgeCollector.cpp
  GLObjectsVisitor.cpp
  HalfWayMapGenerator.cpp
  HighlightMapGenerator.cpp
  IntersectionVisitor.cpp
  IntersectVisitor.cpp
  IncrementalCompileOperation.cpp
  LineSegmentIntersector.cpp
  MeshOptimizers.cpp
  Optimizer.cpp
  PerlinNoise.cpp
  PlaneIntersector.cpp
  PolytopeIntersector.cpp
  PositionalStateContainer.cpp
  PrintVisitor.cpp
  RenderBin.cpp
  RenderLeaf.cpp
  RenderStage.cpp
  ReversePrimitiveFunctor.cpp
  SceneView.cpp
  ShaderGen.cpp
  Simplifier.cpp
  SmoothingVisitor.cpp
  SceneGraphBuilder.cpp
  StateGraph.cpp
  Statistics.cpp
  TangentSpaceGenerator.cpp
  Tessellator.cpp
  TransformAttributeFunctor.cpp
  TransformCallback.cpp

  tristripper/src/tri_stripper.cpp
  tristripper/src/policy.cpp
  tristripper/src/connectivity_graph.cpp

  TriStripVisitor.cpp
  UpdateVisitor.cpp
  Version.cpp
  ${OPENSCENEGRAPH_VERSIONINFO_RC}
  )

include_directories("tristripper/include")

add_library(osgUtil ${OPENSCENEGRAPH_USER_DEFINED_DYNAMIC_OR_STATIC} ${OSGUTIL_SRCS})
target_link_libraries(osgUtil OpenThreads osg ${CMAKE_THREAD_LIBS_INIT}
  ${MATH_LIBRARY} ${RT_LIBRARY} ${DL_LIBRARY})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
