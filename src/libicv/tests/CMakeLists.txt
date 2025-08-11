brlcad_addexec(icv_ascii ascii.c "libicv;libbu" TEST)
brlcad_addexec(icv_read_write read_write.c "libicv;libbu" TEST)
brlcad_addexec(icv_rect rect.c "libicv;libbu" TEST)
brlcad_addexec(icv_crop crop.c "libicv;libbu" TEST)
brlcad_addexec(icv_filter filter.c "libicv;libbu" TEST)
brlcad_addexec(icv_fade fade.c "libicv;libbu" TEST)
brlcad_addexec(icv_size_up size_up.c "libicv;libbu" TEST)
brlcad_addexec(icv_size_down size_down.c "libicv;libbu" TEST)
brlcad_addexec(icv_saturate saturate.c "libicv;libbu" TEST)
brlcad_addexec(icv_operations operations.c "libicv;libbu" TEST)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
