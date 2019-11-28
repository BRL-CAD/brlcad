BRLCAD_ADDEXEC(tester_raydiff raydiff.c "libanalyze;libbu" NO_INSTALL)
BRLCAD_ADDEXEC(tester_sp solid_partitions.c "libanalyze;libbu" NO_INSTALL)

#####################################
#      analyze_densities testing    #
#####################################
BRLCAD_ADDEXEC(analyze_densities density.cpp "libanalyze;libbu" NO_INSTALL)

add_test(NAME analyze_densities_null        COMMAND analyze_densities)
add_test(NAME analyze_densities_std        COMMAND analyze_densities std)


CMAKEFILES(raydiff.g)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
