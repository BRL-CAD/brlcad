set(ASSIMP_SRCS
  assimp_read.cpp
  assimp_write.cpp
  )

find_package(ASSIMP)

if(BRLCAD_ENABLE_ASSIMP AND ASSIMP_LIBRARY AND ASSIMP_INCLUDE_DIR)
  message("assimp plugin FOUND and ENABLED")
  include_directories(SYSTEM ${ASSIMP_INCLUDE_DIR})

  gcv_plugin_library(gcv-assimp SHARED ${ASSIMP_SRCS})
  target_link_libraries(gcv-assimp libgcv libwdb librt libbu ${ASSIMP_LIBRARY})
  set_property(TARGET gcv-assimp APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(gcv-assimp "${ASSIMP_SRCS}")
  PLUGIN_SETUP(gcv-assimp gcv)

endif(BRLCAD_ENABLE_ASSIMP AND ASSIMP_LIBRARY AND ASSIMP_INCLUDE_DIR)

CMAKEFILES(
  ${ASSIMP_SRCS}
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
