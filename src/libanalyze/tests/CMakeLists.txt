brlcad_addexec(analyze_raydiff raydiff.c "libanalyze;libbu" TEST)
brlcad_addexec(analyze_sp solid_partitions.c "libanalyze;libbu" TEST)
brlcad_addexec(analyze_nhit nhit.cpp "libanalyze;libbu" TEST_USESDATA)

#####################################
#      analyze_densities testing    #
#####################################
brlcad_addexec(analyze_densities density.cpp "libanalyze;libbu" TEST)

brlcad_add_test(NAME analyze_densities_null        COMMAND analyze_densities)
brlcad_add_test(NAME analyze_densities_std        COMMAND analyze_densities std)

cmakefiles(raydiff.g)
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
