#  ************ libdm tests *************

brlcad_addexec(dm_test dm_test.c "libdm;libbu" TEST)

# dm_dlist_sensor register/fire/clear
brlcad_addexec(test_dm_dlist_sensor test_dlist_sensor.c "libdm;libbv;libbu" TEST)
brlcad_add_test(NAME dm_dlist_sensor COMMAND test_dm_dlist_sensor)

# Fontstash is a private libdm implementation detail.  Exercise it with a
# renderer mock so atlas, ownership, and UTF-8 behavior do not require OpenGL.
set_source_files_properties(fontstash_test.c PROPERTIES LANGUAGE CXX)
brlcad_addexec(test_fontstash fontstash_test.c "libbu;${M_LIBRARY}" TEST)
target_include_directories(
  test_fontstash
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/libbu
    ${CMAKE_SOURCE_DIR}/src/libdm/fontstash
)
brlcad_add_test(
  NAME dm_fontstash
  COMMAND test_fontstash ${CMAKE_SOURCE_DIR}/misc/osifont/osifont-lgpl3fe.ttf
)

cmakefiles(CMakeLists.txt)
cmakefiles(
  dm_test.c
  fontstash_test.c
  test_dlist_sensor.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
