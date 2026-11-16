cmakefiles(CMakeLists.txt art_framebuffer_test.cpp regress-art-display.cmake.in)
if(TARGET art AND BUILD_TESTING)
  brlcad_addexec(art_framebuffer_test art_framebuffer_test.cpp "libicv" TEST)
  configure_file(regress-art-display.cmake.in regress-art-display.cmake @ONLY)
  brlcad_add_test(NAME regress-art-display COMMAND "${CMAKE_COMMAND}"
    -DEXEC=$<TARGET_FILE:art> -DVEXEC=$<TARGET_FILE:art_scene_test>
    -DFEXEC=$<TARGET_FILE:art_framebuffer_test>
    -P "${CMAKE_CURRENT_BINARY_DIR}/regress-art-display.cmake")
  brlcad_regression_test(regress-art-display "art;art_scene_test;art_framebuffer_test"
    TEST_DEFINED TIMEOUT 120)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/regress-art-display.cmake")
  distclean("${CMAKE_CURRENT_BINARY_DIR}/display-output")
endif()
