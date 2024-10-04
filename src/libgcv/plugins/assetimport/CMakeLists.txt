set(ASSETIMPORT_SRCS assetimport_read.cpp assetimport_write.cpp)

brlcad_find_package(assimp)

if(BRLCAD_ENABLE_ASSETIMPORT)
  if(TARGET assimp::assimp)
    gcv_plugin_library(gcv-assetimport SHARED ${ASSETIMPORT_SRCS})
    target_link_libraries(
      gcv-assetimport
      libgcv
      libwdb
      librt
      libbu
      assimp::assimp
    )
    set_property(TARGET gcv-assetimport APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
    validate_style(gcv-assetimport "${ASSETIMPORT_SRCS}")
    plugin_setup(gcv-assetimport gcv)
  else()
    message("Warning - Asset Import support enabled, but Asset Import Library not found - disabling GCV plugin\n")
  endif()
endif(BRLCAD_ENABLE_ASSETIMPORT)

cmakefiles(
  ${ASSETIMPORT_SRCS}
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
