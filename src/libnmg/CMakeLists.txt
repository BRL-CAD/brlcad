# Include directories needed by libnmg users
set(NMG_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${OPENNURBS_INCLUDE_DIR}
  )
BRLCAD_LIB_INCLUDE_DIRS(nmg NMG_INCLUDE_DIRS "")


set(LIBNMG_SOURCES
  nmg_bool.c
  nmg_brep.cpp
  nmg_ck.c
  nmg_class.c
  nmg_copy.c
  nmg_eval.c
  nmg_extrude.c
  nmg_fcut.c
  nmg_fuse.c
  nmg_index.c
  nmg_info.c
  nmg_inter.c
  nmg_manif.c
  nmg_mesh.c
  nmg_mirror.c
  nmg_misc.c
  nmg_mk.c
  nmg_mod.c
  nmg_pr.c
  nmg_pt_fu.c
  nmg_plot.c
  nmg_tri.c
  nmg_visit.c
  )


BRLCAD_ADDLIB(libnmg "${LIBNMG_SOURCES}" "libbu;libbn")
set_target_properties(libnmg PROPERTIES VERSION 20.0.1 SOVERSION 20)


CMAKEFILES(nmg_extracted.c)
