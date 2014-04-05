# FIXME: For OS X, need flag for Framework or dylib
if(DYNAMIC_OPENSCENEGRAPH)
    add_definitions(-DOSG_LIBRARY)
else(DYNAMIC_OPENSCENEGRAPH)
    add_definitions(-DOSG_LIBRARY_STATIC)
endif(DYNAMIC_OPENSCENEGRAPH)

#
# Enable workaround for OpenGL driver crash with occlusion query
#
option(OSG_FORCE_QUERY_RESULT_AVAILABLE_BEFORE_RETRIEVAL "Set to ON to build OcclussionQueryNode with a workaround for multi-threaded OpenGL driver occlussion query crash. " OFF)
if(OSG_FORCE_QUERY_RESULT_AVAILABLE_BEFORE_RETRIEVAL)
    add_definitions(-DFORCE_QUERY_RESULT_AVAILABLE_BEFORE_RETRIEVAL)
endif()
mark_as_advanced(OSG_FORCE_QUERY_RESULT_AVAILABLE_BEFORE_RETRIEVAL)

set(OSG_SRCS
    AlphaFunc.cpp
    AnimationPath.cpp
    ApplicationUsage.cpp
    ArgumentParser.cpp
    Array.cpp
    ArrayDispatchers.cpp
    AudioStream.cpp
    AutoTransform.cpp
    Billboard.cpp
    BlendColor.cpp
    BlendEquation.cpp
    BlendFunc.cpp
    BufferIndexBinding.cpp
    BufferObject.cpp
    Camera.cpp
    CameraView.cpp
    ClampColor.cpp
    ClearNode.cpp
    ClipNode.cpp
    ClipPlane.cpp
    ClusterCullingCallback.cpp
    CollectOccludersVisitor.cpp
    ColorMask.cpp
    ColorMatrix.cpp
    ComputeBoundsVisitor.cpp
    ConvexPlanarOccluder.cpp
    ConvexPlanarPolygon.cpp
    CoordinateSystemNode.cpp
    CopyOp.cpp
    CullFace.cpp
    CullingSet.cpp
    CullSettings.cpp
    CullStack.cpp
    DeleteHandler.cpp
    Depth.cpp
    DisplaySettings.cpp
    Drawable.cpp
    DrawPixels.cpp
    dxtctool.cpp
    dxtctool.h
    Fog.cpp
    FragmentProgram.cpp
    FrameBufferObject.cpp
    FrameStamp.cpp
    FrontFace.cpp
    Geode.cpp
    Geometry.cpp
    GL2Extensions.cpp
    GLExtensions.cpp
    GLBeginEndAdapter.cpp
    GLObjects.cpp
    GLStaticLibrary.h
    GLStaticLibrary.cpp
    GraphicsCostEstimator.cpp
    GraphicsContext.cpp
    GraphicsThread.cpp
    Group.cpp
    Hint.cpp
    Image.cpp
    ImageSequence.cpp
    ImageStream.cpp
    ImageUtils.cpp
    KdTree.cpp
    Light.cpp
    LightModel.cpp
    LightSource.cpp
    LineSegment.cpp
    LineStipple.cpp
    LineWidth.cpp
    LOD.cpp
    LogicOp.cpp
    Material.cpp
    Math.cpp
    Matrixd.cpp
    MatrixDecomposition.cpp
    Matrixf.cpp
    # We don't build this one
    #    Matrix_implementation.cpp
    MatrixTransform.cpp
    Multisample.cpp
    NodeCallback.cpp
    Node.cpp
    NodeTrackerCallback.cpp
    NodeVisitor.cpp
    Notify.cpp
    Object.cpp
    Observer.cpp
    ObserverNodePath.cpp
    OccluderNode.cpp
    OcclusionQueryNode.cpp
    OperationThread.cpp
    PatchParameter.cpp
    PagedLOD.cpp
    Point.cpp
    PointSprite.cpp
    PolygonMode.cpp
    PolygonOffset.cpp
    PolygonStipple.cpp
    PositionAttitudeTransform.cpp
    PrimitiveSet.cpp
    PrimitiveRestartIndex.cpp
    Program.cpp
    Projection.cpp
    ProxyNode.cpp
    Quat.cpp
    Referenced.cpp
    SampleMaski.cpp
    Scissor.cpp
    Sequence.cpp
    ShadeModel.cpp
    Shader.cpp
    ShaderAttribute.cpp
    ShaderComposer.cpp
    ShadowVolumeOccluder.cpp
    Shape.cpp
    ShapeDrawable.cpp
    StateAttribute.cpp
    State.cpp
    StateSet.cpp
    Stats.cpp
    Stencil.cpp
    StencilTwoSided.cpp
    Switch.cpp
    TexEnvCombine.cpp
    TexEnv.cpp
    TexEnvFilter.cpp
    TexGen.cpp
    TexGenNode.cpp
    TexMat.cpp
    Texture1D.cpp
    Texture2DArray.cpp
    Texture2D.cpp
    Texture2DMultisample.cpp
    Texture3D.cpp
    Texture.cpp
    TextureBuffer.cpp
    TextureCubeMap.cpp
    TextureRectangle.cpp
    Timer.cpp
    TransferFunction.cpp
    Transform.cpp
    Uniform.cpp
    UserDataContainer.cpp
    Version.cpp
    VertexProgram.cpp
    View.cpp
    Viewport.cpp

    glu/libutil/error.cpp
    glu/libutil/mipmap.cpp

    glu/libtess/normal.cpp
    glu/libtess/memalloc.cpp
    glu/libtess/dict-list.h
    glu/libtess/alg-outline
    glu/libtess/priorityq.cpp
    glu/libtess/normal.h
    glu/libtess/dict.h
    glu/libtess/render.cpp
    glu/libtess/tessmono.h
    glu/libtess/mesh.cpp
    glu/libtess/render.h
    glu/libtess/tessmono.cpp
    glu/libtess/priorityq.h
    glu/libtess/sweep.h
    glu/libtess/priorityq-sort.h
    glu/libtess/sweep.cpp
    glu/libtess/tess.h
    glu/libtess/geom.h
    glu/libtess/memalloc.h
    glu/libtess/dict.cpp
    glu/libtess/priorityq-heap.h
    glu/libtess/geom.cpp
    glu/libtess/tess.cpp
    glu/libtess/mesh.h
)

add_library(osg ${OPENSCENEGRAPH_USER_DEFINED_DYNAMIC_OR_STATIC} ${OSG_SRCS})
target_link_libraries(osg osgViewer OpenThreads ${CMAKE_THREAD_LIBS_INIT}
  ${OPENGL_gl_LIBRARY} ${MATH_LIBRARY} ${RT_LIBRARY} ${DL_LIBRARY})


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
