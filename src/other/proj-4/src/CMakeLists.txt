include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/../
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

set(pj_srcs
  nad_init.c
  PJ_aea.c
  PJ_aeqd.c
  PJ_airy.c
  PJ_aitoff.c
  PJ_august.c
  PJ_bacon.c
  PJ_bipc.c
  PJ_boggs.c
  PJ_bonne.c
  PJ_calcofi.c
  PJ_cass.c
  PJ_cc.c
  PJ_cea.c
  PJ_chamb.c
  PJ_collg.c
  PJ_comill.c
  PJ_crast.c
  PJ_denoy.c
  PJ_eck1.c
  PJ_eck2.c
  PJ_eck3.c
  PJ_eck4.c
  PJ_eck5.c
  PJ_eqc.c
  PJ_eqdc.c
  PJ_fahey.c
  PJ_fouc_s.c
  PJ_gall.c
  PJ_geos.c
  PJ_gins8.c
  PJ_gnom.c
  PJ_gn_sinu.c
  PJ_goode.c
  PJ_gstmerc.c
  PJ_hammer.c
  PJ_hatano.c
  PJ_igh.c
  PJ_isea.c
  PJ_imw_p.c
  PJ_krovak.c
  PJ_labrd.c
  PJ_laea.c
  PJ_lagrng.c
  PJ_larr.c
  PJ_lask.c
  PJ_lcca.c
  PJ_lcc.c
  PJ_loxim.c
  PJ_lsat.c
  PJ_misrsom.c
  PJ_mbt_fps.c
  PJ_mbtfpp.c
  PJ_mbtfpq.c
  PJ_merc.c
  PJ_mill.c
  PJ_mod_ster.c
  PJ_moll.c
  PJ_natearth.c
  PJ_natearth2.c
  PJ_nell.c
  PJ_nell_h.c
  PJ_nocol.c
  PJ_nsper.c
  PJ_nzmg.c
  PJ_ob_tran.c
  PJ_ocea.c
  PJ_oea.c
  PJ_omerc.c
  PJ_ortho.c
  PJ_patterson.c
  PJ_poly.c
  PJ_putp2.c
  PJ_putp3.c
  PJ_putp4p.c
  PJ_putp5.c
  PJ_putp6.c
  PJ_qsc.c
  PJ_robin.c
  PJ_rpoly.c
  PJ_sch.c
  PJ_sconics.c
  PJ_somerc.c
  PJ_sterea.c
  PJ_stere.c
  PJ_sts.c
  PJ_tcc.c
  PJ_tcea.c
  PJ_times.c
  PJ_tmerc.c
  PJ_tpeqd.c
  PJ_urm5.c
  PJ_urmfps.c
  PJ_vandg.c
  PJ_vandg2.c
  PJ_vandg4.c
  PJ_wag2.c
  PJ_wag3.c
  PJ_wag7.c
  PJ_wink1.c
  PJ_wink2.c
  proj_etmerc.c
  )

set(core_srcs
  aasincos.c
  adjlon.c
  bch2bps.c
  bchgen.c
  biveval.c
  dmstor.c
  emess.c
  emess.h
  geocent.c
  geocent.h
  geodesic.c
  mk_cheby.c
  nad_cvt.c
  nad_init.c
  nad_intr.c
  pj_apply_gridshift.c
  pj_apply_vgridshift.c
  pj_auth.c
  pj_ctx.c
  pj_fileapi.c
  pj_datum_set.c
  pj_datums.c
  pj_deriv.c
  pj_ell_set.c
  pj_ellps.c
  pj_errno.c
  pj_factors.c
  pj_fwd.c
  pj_fwd3d.c
  pj_gauss.c
  pj_gc_reader.c
  pj_generic_selftest.c
  pj_geocent.c
  pj_gridcatalog.c
  pj_gridinfo.c
  pj_gridlist.c
  PJ_healpix.c
  pj_init.c
  pj_initcache.c
  pj_inv.c
  pj_inv3d.c
  pj_latlong.c
  pj_list.c
  pj_list.h
  pj_log.c
  pj_malloc.c
  pj_mlfn.c
  pj_msfn.c
  pj_mutex.c
  pj_open_lib.c
  pj_param.c
  pj_phi2.c
  pj_pr_list.c
  pj_qsfn.c
  pj_release.c
  pj_run_selftests.c
  pj_strerrno.c
  pj_transform.c
  pj_tsfn.c
  pj_units.c
  pj_utils.c
  pj_zpoly1.c
  proj_mdist.c
  proj_rouss.c
  rtodms.c
  vector1.c
  pj_strtod.c
  ${CMAKE_CURRENT_BINARY_DIR}/../proj_config.h
  )

set(hdr_files
  projects.h
  proj_api.h
  geodesic.h
  )

# Embed PROJ_LIB data files location
if (NOT DEFINED PROJ_LIB_DIR)
  add_definitions(-DPROJ_LIB="${CMAKE_INSTALL_PREFIX}/${DATA_DIR}/proj")
else (NOT DEFINED PROJ_LIB_DIR)
  add_definitions(-DPROJ_LIB="${PROJ_LIB_DIR}")
endif (NOT DEFINED PROJ_LIB_DIR)

#################################################
set(all_srcs ${pj_srcs} ${core_srcs})
if(WIN32 AND BUILD_SHARED_LIBS)
  set(all_srcs ${all_srcs} proj.def )
endif(WIN32 AND BUILD_SHARED_LIBS)

if (BUILD_SHARED_LIBS)
  add_library(proj SHARED ${all_srcs} ${hdr_files})
  set_target_properties(proj PROPERTIES VERSION "${PROJ_BUILD_VERSION}" SOVERSION "${PROJ_API_VERSION}")
  set_target_properties(proj PROPERTIES LINKER_LANGUAGE C)
  set(proj_libs ${M_LIBRARY} Threads::Threads)
  if(proj_libs)
    target_link_libraries(proj ${proj_libs})
  endif(proj_libs)

  install(TARGETS proj
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR}
    )
endif (BUILD_SHARED_LIBS)

if (BUILD_STATIC_LIBS)
  add_library(proj-static STATIC ${all_srcs} ${hdr_files})
  if (MSVC)
    # msvc does not append 'lib' - do it here to have consistent name
    set_target_properties(proj-static PROPERTIES PREFIX "lib")
  else (MSVC)
    set_target_properties(proj-static PROPERTIES OUTPUT_NAME "proj")
  endif (MSVC)
  install(TARGETS proj-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR}
    )
endif (BUILD_STATIC_LIBS)

if (NOT SKIP_INSTALL_HEADERS)
  install(FILES ${hdr_files} DESTINATION ${INCLUDE_DIR}/proj)
endif (NOT SKIP_INSTALL_HEADERS)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

