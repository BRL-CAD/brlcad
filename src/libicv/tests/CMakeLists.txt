brlcad_addexec(icv_format_parity format_parity.cpp "libicv;libbu;PNG::PNG" TEST)
brlcad_add_test(NAME icv_format_parity COMMAND icv_format_parity)

brlcad_addexec(icv_png_profiles png_profiles.cpp "libicv;libbu;ZLIB::ZLIB" TEST)
brlcad_add_test(NAME icv_png_profiles COMMAND icv_png_profiles)
brlcad_add_test(NAME icv_png_pipe COMMAND ${CMAKE_COMMAND}
  "-DPNG_TEST=$<TARGET_FILE:icv_png_profiles>" -P "${CMAKE_CURRENT_SOURCE_DIR}/png_pipe.cmake")
cmakefiles(png_pipe.cmake)

brlcad_addexec(icv_jpeg_profiles jpeg_profiles.cpp "libicv;libbu;JPEG::JPEG" TEST)
brlcad_add_test(NAME icv_jpeg_profiles COMMAND icv_jpeg_profiles)
brlcad_add_test(NAME icv_jpeg_pipe COMMAND ${CMAKE_COMMAND}
  "-DJPEG_TEST=$<TARGET_FILE:icv_jpeg_profiles>" -P "${CMAKE_CURRENT_SOURCE_DIR}/jpeg_pipe.cmake")
cmakefiles(jpeg_pipe.cmake)

brlcad_addexec(icv_robustness robustness.c "libicv;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME icv_robustness COMMAND icv_robustness)

brlcad_addexec(icv_demo demo.c "libicv;libbu" TEST)
brlcad_add_test(NAME icv_demo COMMAND icv_demo)

brlcad_addexec(icv_fileformat_test fileformat.c "libicv;libbu" TEST)
brlcad_add_test(NAME icv_fileformat_test COMMAND icv_fileformat_test)
if(BUILD_TESTING AND TARGET icv_fileformat_test)
  set_tests_properties(icv_fileformat_test PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()

brlcad_addexec(icv_sun_test sun.c "libicv;libbu" TEST)
brlcad_add_test(NAME icv_sun_test COMMAND icv_sun_test)

brlcad_addexec(icv_rle_profiles rle_profiles.cpp "libicv;libbu" TEST)
brlcad_add_test(NAME icv_rle_profiles COMMAND icv_rle_profiles)
brlcad_add_test(NAME icv_rle_pipe COMMAND ${CMAKE_COMMAND}
  "-DRLE_TEST=$<TARGET_FILE:icv_rle_profiles>" -P "${CMAKE_CURRENT_SOURCE_DIR}/rle_pipe.cmake")
cmakefiles(rle_pipe.cmake)

brlcad_addexec(icv_rle rle.cpp "libicv;libbu" TEST)
brlcad_add_test(NAME icv_rle  COMMAND icv_rle)
distclean(
  ${CMAKE_CURRENT_BINARY_DIR}/test_invalid_channels.rle
  ${CMAKE_CURRENT_BINARY_DIR}/test_oversized.rle
)

brlcad_addexec(icv_diff_test icv_diff_test.c "libicv;libbu" TEST)
brlcad_add_test(NAME icv_diff_test COMMAND icv_diff_test ${CMAKE_CURRENT_BINARY_DIR})

brlcad_addexec(icv_png_json_test icv_png_json_test.cpp "libicv;libbu" TEST)
brlcad_add_test(NAME icv_png_json_test COMMAND icv_png_json_test ${CMAKE_CURRENT_BINARY_DIR})

brlcad_addexec(icv_pimghash "pimghash.cpp;../PImgHash.cpp" "libicv;libbu" TEST)
brlcad_add_test(NAME icv_pimghash COMMAND icv_pimghash)

brlcad_addexec(icv_apngmini apngmini.cpp "libicv;libbu;PNG::PNG;ZLIB::ZLIB" TEST)
brlcad_add_test(NAME icv_apngmini COMMAND icv_apngmini)

add_library(icv-codec-fixture MODULE EXCLUDE_FROM_ALL codec_plugin.c)
target_link_libraries(icv-codec-fixture libicv libbu)
target_compile_definitions(icv-codec-fixture PRIVATE BRLCADBUILD HAVE_CONFIG_H)
brlcad_addexec(icv_codec_test codec.c "libicv;libbu" TEST)
if(TARGET icv_codec_test)
  add_dependencies(icv_codec_test icv-codec-fixture)
endif()
brlcad_add_test(NAME icv_codec COMMAND icv_codec_test $<TARGET_FILE:icv-codec-fixture>)
brlcad_addexec(icv_sequence_profiles sequence_profiles.cpp "libicv;libbu" TEST)
if(TARGET icv_sequence_profiles)
  add_dependencies(icv_sequence_profiles icv-codec-fixture)
endif()
brlcad_add_test(NAME icv_sequence_profiles COMMAND icv_sequence_profiles $<TARGET_FILE:icv-codec-fixture>)
brlcad_add_test(NAME icv_codec_pipe COMMAND ${CMAKE_COMMAND}
  "-DICV_CODEC_TEST=$<TARGET_FILE:icv_codec_test>" -P "${CMAKE_CURRENT_SOURCE_DIR}/codec_pipe.cmake")
cmakefiles(codec_plugin.c codec_pipe.cmake codec_cli.cmake)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
