set(SIG_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${DM_INCLUDE_DIRS}
  ${FFT_INCLUDE_DIRS}
  )
LIST(REMOVE_DUPLICATES SIG_INCLUDE_DIRS)
include_directories(${SIG_INCLUDE_DIRS})

BRLCAD_ADDEXEC(damdf damdf.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(dauto dauto.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(dauto2 dauto2.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(dconv dconv.c "libbu;libfft" FOLDER Sig)
BRLCAD_ADDEXEC(ddisp ddisp.c libdm FOLDER Sig)
ADD_TARGET_DEPS(ddisp dm_plugins)
BRLCAD_ADDEXEC(dfft "dfft.c;interp.c;butter.c" "libbu;libfft" FOLDER Sig)
BRLCAD_ADDEXEC(dmod dmod.c "libbu;${M_LIBRARY}" FOLDER Sig)
BRLCAD_ADDEXEC(dpeak dpeak.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(dsel dsel.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(dstats dstats.c "libbu;${M_LIBRARY}" FOLDER Sig)
BRLCAD_ADDEXEC(dwin "dwin.c;coswin.c;hamwin.c" "libbu;${M_LIBRARY}" FOLDER Sig)
BRLCAD_ADDEXEC(ihist ihist.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(imod imod.c "libbu;${M_LIBRARY}" FOLDER Sig)
BRLCAD_ADDEXEC(istats istats.c "libbu;${M_LIBRARY}" FOLDER Sig)
BRLCAD_ADDEXEC(smod smod.c "libbu;${M_LIBRARY}" FOLDER Sig)
BRLCAD_ADDEXEC(umod umod.c "libbu;${M_LIBRARY}" FOLDER Sig)
BRLCAD_ADDEXEC(ustats ustats.c "libbu;${M_LIBRARY}" FOLDER Sig)

# Deprecated
BRLCAD_ADDEXEC(a-d a-d.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(bw-d bw-d.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(c-d c-d.c "libbu;${M_LIBRARY}" FOLDER Sig)
BRLCAD_ADDEXEC(d-a d-a.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(d-bw d-bw.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(d-f d-f.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(d-i d-i.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(d-u d-u.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(d2-c d2-c.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(f-d f-d.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(f-i f-i.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(i-a i-a.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(i-d i-d.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(i-f i-f.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(u-a u-a.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(u-bw u-bw.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(u-d u-d.c libbu FOLDER Sig)
BRLCAD_ADDEXEC(u-f u-f.c libbu FOLDER Sig)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
