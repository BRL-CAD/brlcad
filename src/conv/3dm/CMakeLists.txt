brlcad_addexec(3dm-g 3dm-g.c "libgcv" FOLDER Conv)
add_dependencies(3dm-g gcv_plugins)

if(TARGET argdoc AND NOT CMAKE_CROSSCOMPILING)
  brlcad_add_test(
    NAME argdoc_3dm_g_options
    COMMAND
      argdoc --check
      --man-page "${CMAKE_SOURCE_DIR}/doc/asciidoc/system/man1/3dm-g.adoc"
      -- "$<TARGET_FILE:3dm-g>" --command-schema
  )
endif()

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
