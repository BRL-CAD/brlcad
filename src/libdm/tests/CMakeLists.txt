#  ************ libdm tests *************

brlcad_addexec(dm_test dm_test.c "libdm;libbu" TEST)

# dm_dlist_sensor register/fire/clear
brlcad_addexec(test_dm_dlist_sensor test_dlist_sensor.c "libdm;libbv;libbu" TEST)
brlcad_add_test(NAME dm_dlist_sensor COMMAND test_dm_dlist_sensor)

cmakefiles(CMakeLists.txt)
cmakefiles(
  dm_test.c
  test_dlist_sensor.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
