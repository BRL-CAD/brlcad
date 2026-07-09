brlcad_addexec(analyze_raydiff raydiff.c "libanalyze;librt;libbu" TEST)
brlcad_addexec(analyze_sp solid_partitions.c "libanalyze;librt;libbu" TEST)
brlcad_addexec(analyze_nhit nhit.cpp "libanalyze;librt;libbu" TEST_USESDATA)

# Sampler comparison / stress-test program (run manually with havoc.g)
brlcad_addexec(analyze_sampler_compare analyze_sampler_compare.cpp "libanalyze;librt;libbu" TEST)

#####################################
#      analyze_densities testing    #
#####################################
brlcad_addexec(analyze_densities density.cpp "libanalyze;libbu" TEST)

brlcad_add_test(NAME analyze_densities_null        COMMAND analyze_densities)
brlcad_add_test(NAME analyze_densities_std        COMMAND analyze_densities std)

cmakefiles(
  CMakeLists.txt
  arbs.g
  raydiff.g
  analyze_sampler_compare.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
