BRLCAD_ADDEXEC(nurbs_tester nurbs_tests.cpp "librt;libbrep;libbu" NO_INSTALL LOCAL)

add_test(get_closest_point-distinct_points nurbs_tester ${CMAKE_CURRENT_SOURCE_DIR}/nurbs_surfaces.g 1)

CMAKEFILES(nurbs_surfaces.g)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
