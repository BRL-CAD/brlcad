set(SIG_INCLUDE_DIRS ${BU_INCLUDE_DIRS} ${DM_INCLUDE_DIRS} ${FFT_INCLUDE_DIRS})
list(REMOVE_DUPLICATES SIG_INCLUDE_DIRS)
include_directories(${SIG_INCLUDE_DIRS})

brlcad_addexec(damdf damdf.c libbu FOLDER Sig)
brlcad_addexec(dauto dauto.c libbu FOLDER Sig)
brlcad_addexec(dauto2 dauto2.c libbu FOLDER Sig)
brlcad_addexec(dconv dconv.c "libbu;libfft" FOLDER Sig)
brlcad_addexec(ddisp ddisp.c libdm FOLDER Sig)
add_target_deps(ddisp dm_plugins)
brlcad_addexec(dfft "dfft.c;interp.c;butter.c" "libbu;libfft" FOLDER Sig)
brlcad_addexec(dmod dmod.c "libbu;${M_LIBRARY}" FOLDER Sig)
brlcad_addexec(dpeak dpeak.c libbu FOLDER Sig)
brlcad_addexec(dsel dsel.c libbu FOLDER Sig)
brlcad_addexec(dstats dstats.c "libbu;${M_LIBRARY}" FOLDER Sig)
brlcad_addexec(dwin "dwin.c;coswin.c;hamwin.c" "libbu;${M_LIBRARY}" FOLDER Sig)
brlcad_addexec(ihist ihist.c libbu FOLDER Sig)
brlcad_addexec(imod imod.c "libbu;${M_LIBRARY}" FOLDER Sig)
brlcad_addexec(istats istats.c "libbu;${M_LIBRARY}" FOLDER Sig)
brlcad_addexec(smod smod.c "libbu;${M_LIBRARY}" FOLDER Sig)
brlcad_addexec(umod umod.c "libbu;${M_LIBRARY}" FOLDER Sig)
brlcad_addexec(ustats ustats.c "libbu;${M_LIBRARY}" FOLDER Sig)

# Deprecated
brlcad_addexec(a-d a-d.c libbu FOLDER Sig)
brlcad_addexec(bw-d bw-d.c libbu FOLDER Sig)
brlcad_addexec(c-d c-d.c "libbu;${M_LIBRARY}" FOLDER Sig)
brlcad_addexec(d-a d-a.c libbu FOLDER Sig)
brlcad_addexec(d-bw d-bw.c libbu FOLDER Sig)
brlcad_addexec(d-f d-f.c libbu FOLDER Sig)
brlcad_addexec(d-i d-i.c libbu FOLDER Sig)
brlcad_addexec(d-u d-u.c libbu FOLDER Sig)
brlcad_addexec(d2-c d2-c.c libbu FOLDER Sig)
brlcad_addexec(f-d f-d.c libbu FOLDER Sig)
brlcad_addexec(f-i f-i.c libbu FOLDER Sig)
brlcad_addexec(i-a i-a.c libbu FOLDER Sig)
brlcad_addexec(i-d i-d.c libbu FOLDER Sig)
brlcad_addexec(i-f i-f.c libbu FOLDER Sig)
brlcad_addexec(u-a u-a.c libbu FOLDER Sig)
brlcad_addexec(u-bw u-bw.c libbu FOLDER Sig)
brlcad_addexec(u-d u-d.c libbu FOLDER Sig)
brlcad_addexec(u-f u-f.c libbu FOLDER Sig)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
