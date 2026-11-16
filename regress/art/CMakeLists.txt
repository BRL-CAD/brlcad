cmakefiles(CMakeLists.txt art_scene.cpp regress-art.cmake.in)

if(TARGET art AND BUILD_TESTING)
  brlcad_addexec(art_scene_test art_scene.cpp "libwdb;libicv" TEST)
  brlcad_regression_test(regress-art "art;art_scene_test" EXEC art VEXEC art_scene_test TIMEOUT 120)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/output")
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
