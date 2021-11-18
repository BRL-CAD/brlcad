set(GLTF_SRCS
  gltf_read.c
  )

gcv_plugin_library(gcv-gltf SHARED ${GLTF_SRCS})
target_link_libraries(gcv-gltf libgcv libwdb librt)
set_property(TARGET gcv-gltf APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(gcv-gltf "${GLTF_SRCS}")
PLUGIN_SETUP(gcv-gltf gcv)

CMAKEFILES(
  ${GLTF_SRCS}
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

