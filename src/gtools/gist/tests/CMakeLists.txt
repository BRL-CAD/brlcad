if(OpenCV_FOUND)
  brlcad_addexec(
    test_gist_preview_resolution
    "test_preview_resolution.cpp;../IFPainter.cpp;../Options.cpp"
    "libbu;librt;${OpenCV_LIBRARIES}"
    TEST
  )

  if(TARGET test_gist_preview_resolution)
    add_dependencies(test_gist_preview_resolution libged)
    target_include_directories(test_gist_preview_resolution BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_include_directories(test_gist_preview_resolution SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
    brlcad_add_test(
      NAME gist_preview_resolution
      COMMAND test_gist_preview_resolution ${CMAKE_CURRENT_BINARY_DIR}
    )
  endif()
endif()

cmakefiles(
  CMakeLists.txt
  expected_report.png
  test_preview_resolution.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
