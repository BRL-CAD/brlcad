set(IGES_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  )
list(REMOVE_DUPLICATES IGES_INCLUDE_DIRS)
BRLCAD_INCLUDE_DIRS(IGES_INCLUDE_DIRS)

#add_executable(iges
#    BrepHandler.cpp
#    brlcad_brep.cpp
#    n_iges.cpp
#    n_main.cpp
#)
#target_link_libraries(iges libwdb librt libbu ${OPENNURBS_LIBRARIES} ${WIN32LIBS} ${M_LIBRARY} ${WINSOCK_LIB} ${RPCRT_LIB})
#install(TARGETS iges RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

set(iges-g_SRCS
  add_face.c
  add_inner_shell.c
  add_loop.c
  b-spline.c
  block2.c
  brep.c
  check_names.c
  cone.c
  conv_drawings.c
  convassem.c
  convinst.c
  convsolids.c
  convsurf.c
  convtree.c
  cyl.c
  do_subfigs.c
  docolor.c
  ell.c
  evalxform.c
  extrudcirc.c
  extrudcon.c
  extrude.c
  findp.c
  get_att.c
  get_cnurb_curve.c
  get_edge_list.c
  get_iges_vertex.c
  get_outer_shell.c
  get_vertex.c
  get_vertex_list.c
  getcurve.c
  iges_types.c
  main.c
  make_face.c
  make_nurb_face.c
  makedir.c
  makegroup.c
  matmult.c
  orient_loops.c
  read_att.c
  read_edge_list.c
  read_vertex_list.c
  readcnv.c
  readcols.c
  readdbl.c
  readflt.c
  readglobal.c
  readint.c
  readmatrix.c
  readname.c
  readrec.c
  readstart.c
  readstrg.c
  readtime.c
  readtree.c
  recsize.c
  revolve.c
  showtree.c
  sphere.c
  spline.c
  splinef.c
  stack.c
  summary.c
  tor.c
  trimsurf.c
  usage.c
  wedge.c
  )
BRLCAD_ADDEXEC(iges-g "${iges-g_SRCS}" "libwdb;librt;libnmg;libbn;libbu;${M_LIBRARY}" FOLDER Conv)

BRLCAD_ADDEXEC(g-iges "g-iges.c;iges.c" "librt;libnmg;libbu;${M_LIBRARY}" FOLDER Conv)

set(iges_ignore_files
  BrepHandler.cpp
  CMakeLists.txt
  PullbackCurve.cpp
  PullbackCurve.h
  add_face.cpp
  arrange.c
  brep.cpp
  brlcad_brep.cpp
  brlcad_brep.hpp
  bubbleup.c
  copytree.c
  ge6.igs
  get_outer_shell.cpp
  iges.h
  iges_extern.h
  iges_struct.h
  iges_types.h
  n_iges.cpp
  n_iges.hpp
  n_main.cpp
  wing128.igs
  woodie.igs
  )
CMAKEFILES(${iges_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
