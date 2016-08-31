BRLCAD_ADDEXEC(tester_density density.c "libanalyze;libbu" NO_INSTALL)
BRLCAD_ADDEXEC(tester_raydiff raydiff.c "libanalyze;libbu" NO_INSTALL)
BRLCAD_ADDEXEC(tester_sp solid_partitions.c "libanalyze;libbu" NO_INSTALL)

CMAKEFILES(raydiff.g)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
