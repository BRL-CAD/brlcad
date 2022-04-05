BRLCAD_ADDEXEC(analyze_raydiff raydiff.c "libanalyze;libbu" TEST)
BRLCAD_ADDEXEC(analyze_sp solid_partitions.c "libanalyze;libbu" TEST)
BRLCAD_ADDEXEC(analyze_nhit nhit.cpp "libanalyze;libbu" TEST_USESDATA)

#####################################
#      analyze_densities testing    #
#####################################
BRLCAD_ADDEXEC(analyze_densities density.cpp "libanalyze;libbu" TEST)

BRLCAD_ADD_TEST(NAME analyze_densities_null        COMMAND analyze_densities)
BRLCAD_ADD_TEST(NAME analyze_densities_std        COMMAND analyze_densities std)


CMAKEFILES(raydiff.g)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
