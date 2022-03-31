BRLCAD_ADDEXEC(icv_read_write read_write.c "libicv;libbu" TEST)
BRLCAD_ADDEXEC(icv_rect rect.c "libicv;libbu" TEST)
BRLCAD_ADDEXEC(icv_crop crop.c "libicv;libbu" TEST)
BRLCAD_ADDEXEC(icv_filter filter.c "libicv;libbu" TEST)
BRLCAD_ADDEXEC(icv_fade fade.c "libicv;libbu" TEST)
BRLCAD_ADDEXEC(icv_size_up size_up.c "libicv;libbu" TEST)
BRLCAD_ADDEXEC(icv_size_down size_down.c "libicv;libbu" TEST)
BRLCAD_ADDEXEC(icv_saturate saturate.c "libicv;libbu" TEST)
BRLCAD_ADDEXEC(icv_operations operations.c "libicv;libbu" TEST)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
