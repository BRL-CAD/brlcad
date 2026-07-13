brlcad_addexec(icv_robustness robustness.c "libicv;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME icv_robustness COMMAND icv_robustness)

brlcad_addexec(icv_demo demo.c "libicv;libbu" TEST)
brlcad_add_test(NAME icv_demo COMMAND icv_demo)

brlcad_addexec(icv_fileformat_test fileformat.c "libicv;libbu" TEST)
brlcad_add_test(NAME icv_fileformat_test COMMAND icv_fileformat_test)
set_tests_properties(icv_fileformat_test PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

brlcad_addexec(icv_rle rle.cpp "libicv;libbu" TEST)
brlcad_add_test(NAME icv_rle  COMMAND icv_rle)

brlcad_addexec(icv_diff_test icv_diff_test.c "libicv;libbu" TEST)
brlcad_add_test(NAME icv_diff_test COMMAND icv_diff_test ${CMAKE_CURRENT_BINARY_DIR})

brlcad_addexec(icv_png_json_test icv_png_json_test.cpp "libicv;libbu" TEST)
brlcad_add_test(NAME icv_png_json_test COMMAND icv_png_json_test ${CMAKE_CURRENT_BINARY_DIR})

brlcad_addexec(icv_pimghash "pimghash.cpp;../PImgHash.cpp" "libicv;libbu" TEST)
brlcad_add_test(NAME icv_pimghash COMMAND icv_pimghash)

brlcad_addexec(icv_apngmini apngmini.cpp "libicv;libbu;PNG::PNG;ZLIB::ZLIB" TEST)
brlcad_add_test(NAME icv_apngmini COMMAND icv_apngmini)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
