include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/main
  ${CMAKE_CURRENT_SOURCE_DIR}/glapi
  ${CMAKE_CURRENT_SOURCE_DIR}/shader
  ${CMAKE_CURRENT_SOURCE_DIR}/drivers/common
  ${CMAKE_CURRENT_SOURCE_DIR}/tnl
  ${CMAKE_CURRENT_SOURCE_DIR}/swrast
  ${CMAKE_CURRENT_SOURCE_DIR}/swrast_setup
  ${CMAKE_CURRENT_SOURCE_DIR}/shader/grammar
  ${CMAKE_CURRENT_SOURCE_DIR}/shader/slang
  ${CMAKE_CURRENT_SOURCE_DIR}/x86-64
  ${CMAKE_CURRENT_SOURCE_DIR}/x86
  ${CMAKE_CURRENT_SOURCE_DIR}/glu/include
  ${CMAKE_CURRENT_SOURCE_DIR}/glu/libnurbs/internals
  ${CMAKE_CURRENT_SOURCE_DIR}/glu/libnurbs/interface
  ${CMAKE_CURRENT_SOURCE_DIR}/glu/libnurbs/nurbtess
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

set(osmesa_srcs
  drivers/common/driverfuncs.c
  drivers/osmesa/osmesa.c
  glapi/glapi.c
  glapi/glthread.c
  main/accum.c
  main/api_arrayelt.c
  main/api_loopback.c
  main/api_noop.c
  main/api_validate.c
  main/arrayobj.c
  main/attrib.c
  main/blend.c
  main/bufferobj.c
  main/buffers.c
  main/clip.c
  main/colortab.c
  main/context.c
  main/convolve.c
  main/debug.c
  main/depth.c
  main/depthstencil.c
  main/dispatch.c
  main/dlist.c
  main/drawpix.c
  main/enable.c
  main/enums.c
  main/eval.c
  main/execmem.c
  main/extensions.c
  main/fbobject.c
  main/feedback.c
  main/fog.c
  main/framebuffer.c
  main/get.c
  main/getstring.c
  main/hash.c
  main/hint.c
  main/histogram.c
  main/image.c
  main/imports.c
  main/light.c
  main/lines.c
  main/matrix.c
  main/mipmap.c
  main/mm.c
  main/pixel.c
  main/points.c
  main/polygon.c
  main/queryobj.c
  main/rastpos.c
  main/rbadaptors.c
  main/renderbuffer.c
  main/shaders.c
  main/state.c
  main/stencil.c
  main/texcompress.c
  main/texcompress_fxt1.c
  main/texcompress_s3tc.c
  main/texenvprogram.c
  main/texformat.c
  main/teximage.c
  main/texobj.c
  main/texrender.c
  main/texstate.c
  main/texstore.c
  main/varray.c
  main/vtxfmt.c
  math/m_debug_clip.c
  math/m_debug_norm.c
  math/m_debug_xform.c
  math/m_eval.c
  math/m_matrix.c
  math/m_translate.c
  math/m_vector.c
  math/m_xform.c
  shader/arbprogparse.c
  shader/arbprogram.c
  shader/atifragshader.c
  shader/grammar/grammar_mesa.c
  shader/nvfragparse.c
  shader/nvprogram.c
  shader/nvvertparse.c
  shader/prog_debug.c
  shader/prog_execute.c
  shader/prog_instruction.c
  shader/prog_parameter.c
  shader/prog_print.c
  shader/prog_statevars.c
  shader/program.c
  shader/programopt.c
  shader/shader_api.c
  shader/slang/slang_builtin.c
  shader/slang/slang_codegen.c
  shader/slang/slang_compile.c
  shader/slang/slang_compile_function.c
  shader/slang/slang_compile_operation.c
  shader/slang/slang_compile_struct.c
  shader/slang/slang_compile_variable.c
  shader/slang/slang_emit.c
  shader/slang/slang_ir.c
  shader/slang/slang_label.c
  shader/slang/slang_library_noise.c
  shader/slang/slang_link.c
  shader/slang/slang_log.c
  shader/slang/slang_mem.c
  shader/slang/slang_preprocess.c
  shader/slang/slang_print.c
  shader/slang/slang_simplify.c
  shader/slang/slang_storage.c
  shader/slang/slang_typeinfo.c
  shader/slang/slang_utility.c
  shader/slang/slang_vartable.c
  swrast/s_aaline.c
  swrast/s_aatriangle.c
  swrast/s_accum.c
  swrast/s_alpha.c
  swrast/s_atifragshader.c
  swrast/s_bitmap.c
  swrast/s_blend.c
  swrast/s_blit.c
  swrast/s_buffers.c
  swrast/s_context.c
  swrast/s_copypix.c
  swrast/s_depth.c
  swrast/s_drawpix.c
  swrast/s_feedback.c
  swrast/s_fog.c
  swrast/s_fragprog.c
  swrast/s_imaging.c
  swrast/s_lines.c
  swrast/s_logic.c
  swrast/s_masking.c
  swrast/s_points.c
  swrast/s_readpix.c
  swrast/s_span.c
  swrast/s_stencil.c
  swrast/s_texcombine.c
  swrast/s_texfilter.c
  swrast/s_texstore.c
  swrast/s_triangle.c
  swrast/s_zoom.c
  swrast_setup/ss_context.c
  swrast_setup/ss_triangle.c
  tnl/t_context.c
  tnl/t_draw.c
  tnl/t_pipeline.c
  tnl/t_vb_cull.c
  tnl/t_vb_fog.c
  tnl/t_vb_light.c
  tnl/t_vb_normals.c
  tnl/t_vb_points.c
  tnl/t_vb_program.c
  tnl/t_vb_render.c
  tnl/t_vb_texgen.c
  tnl/t_vb_texmat.c
  tnl/t_vb_vertex.c
  tnl/t_vertex.c
  tnl/t_vertex_generic.c
  tnl/t_vertex_sse.c
  tnl/t_vp_build.c
  vbo/vbo_context.c
  vbo/vbo_exec.c
  vbo/vbo_exec_api.c
  vbo/vbo_exec_array.c
  vbo/vbo_exec_draw.c
  vbo/vbo_exec_eval.c
  vbo/vbo_rebase.c
  vbo/vbo_save.c
  vbo/vbo_save_api.c
  vbo/vbo_save_draw.c
  vbo/vbo_save_loopback.c
  vbo/vbo_split.c
  vbo/vbo_split_copy.c
  vbo/vbo_split_inplace.c
  x86-64/x86-64.c
  x86/3dnow.c
  x86/common_x86.c
  x86/rtasm/x86sse.c
  x86/sse.c
  x86/x86.c
  )

# Bundle the glu functionality as well
set(glu_srcs
  glu/libutil/error.c
  glu/libutil/glue.c
  glu/libutil/mipmap.c
  glu/libutil/project.c
  glu/libutil/quad.c
  glu/libutil/registry.c
  glu/libtess/dict.c
  glu/libtess/geom.c
  glu/libtess/memalloc.c
  glu/libtess/mesh.c
  glu/libtess/normal.c
  glu/libtess/priorityq.c
  glu/libtess/render.c
  glu/libtess/sweep.c
  glu/libtess/tess.c
  glu/libtess/tessmono.c
  glu/libnurbs/interface/bezierEval.cc
  glu/libnurbs/interface/bezierPatch.cc
  glu/libnurbs/interface/bezierPatchMesh.cc
  glu/libnurbs/interface/glcurveval.cc
  glu/libnurbs/interface/glinterface.cc
  glu/libnurbs/interface/glrenderer.cc
  glu/libnurbs/interface/glsurfeval.cc
  glu/libnurbs/interface/incurveeval.cc
  glu/libnurbs/interface/insurfeval.cc
  glu/libnurbs/internals/arc.cc
  glu/libnurbs/internals/arcsorter.cc
  glu/libnurbs/internals/arctess.cc
  glu/libnurbs/internals/backend.cc
  glu/libnurbs/internals/basiccrveval.cc
  glu/libnurbs/internals/basicsurfeval.cc
  glu/libnurbs/internals/bin.cc
  glu/libnurbs/internals/bufpool.cc
  glu/libnurbs/internals/cachingeval.cc
  glu/libnurbs/internals/ccw.cc
  glu/libnurbs/internals/coveandtiler.cc
  glu/libnurbs/internals/curve.cc
  glu/libnurbs/internals/curvelist.cc
  glu/libnurbs/internals/curvesub.cc
  glu/libnurbs/internals/dataTransform.cc
  glu/libnurbs/internals/displaylist.cc
  glu/libnurbs/internals/flist.cc
  glu/libnurbs/internals/flistsorter.cc
  glu/libnurbs/internals/hull.cc
  glu/libnurbs/internals/intersect.cc
  glu/libnurbs/internals/knotvector.cc
  glu/libnurbs/internals/mapdesc.cc
  glu/libnurbs/internals/mapdescv.cc
  glu/libnurbs/internals/maplist.cc
  glu/libnurbs/internals/mesher.cc
  glu/libnurbs/internals/monoTriangulationBackend.cc
  glu/libnurbs/internals/monotonizer.cc
  glu/libnurbs/internals/mycode.cc
  glu/libnurbs/internals/nurbsinterfac.cc
  glu/libnurbs/internals/nurbstess.cc
  glu/libnurbs/internals/patch.cc
  glu/libnurbs/internals/patchlist.cc
  glu/libnurbs/internals/quilt.cc
  glu/libnurbs/internals/reader.cc
  glu/libnurbs/internals/renderhints.cc
  glu/libnurbs/internals/slicer.cc
  glu/libnurbs/internals/sorter.cc
  glu/libnurbs/internals/splitarcs.cc
  glu/libnurbs/internals/subdivider.cc
  glu/libnurbs/internals/tobezier.cc
  glu/libnurbs/internals/trimline.cc
  glu/libnurbs/internals/trimregion.cc
  glu/libnurbs/internals/trimvertpool.cc
  glu/libnurbs/internals/uarray.cc
  glu/libnurbs/internals/varray.cc
  glu/libnurbs/nurbtess/directedLine.cc
  glu/libnurbs/nurbtess/gridWrap.cc
  glu/libnurbs/nurbtess/monoChain.cc
  glu/libnurbs/nurbtess/monoPolyPart.cc
  glu/libnurbs/nurbtess/monoTriangulation.cc
  glu/libnurbs/nurbtess/partitionX.cc
  glu/libnurbs/nurbtess/partitionY.cc
  glu/libnurbs/nurbtess/polyDBG.cc
  glu/libnurbs/nurbtess/polyUtil.cc
  glu/libnurbs/nurbtess/primitiveStream.cc
  glu/libnurbs/nurbtess/quicksort.cc
  glu/libnurbs/nurbtess/rectBlock.cc
  glu/libnurbs/nurbtess/sampleComp.cc
  glu/libnurbs/nurbtess/sampleCompBot.cc
  glu/libnurbs/nurbtess/sampleCompRight.cc
  glu/libnurbs/nurbtess/sampleCompTop.cc
  glu/libnurbs/nurbtess/sampleMonoPoly.cc
  glu/libnurbs/nurbtess/sampledLine.cc
  glu/libnurbs/nurbtess/searchTree.cc
  )

add_library(osmesa SHARED ${osmesa_srcs} ${glu_srcs})
install(TARGETS osmesa
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})
set_property(TARGET osmesa APPEND PROPERTY COMPILE_DEFINITIONS "UTIL_ARCH_LITTLE_ENDIAN=1")
set_property(TARGET osmesa APPEND PROPERTY COMPILE_DEFINITIONS "LIBRARYBUILD")
set_property(TARGET osmesa APPEND PROPERTY COMPILE_DEFINITIONS "GL_DLL_EXPORTS")
set_property(TARGET osmesa APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "GL_DLL_IMPORTS")
if (O3_COMPILER_FLAG)
  target_compile_options(osmesa PRIVATE "-O3")
endif (O3_COMPILER_FLAG)

if (BUILD_STATIC_LIBS)
  add_library(osmesa-static STATIC ${osmesa_srcs})
  install(TARGETS osmesa-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_STATIC_LIBS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

