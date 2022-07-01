set(ASSIMP_SRCS
  assimp_read.cpp
  assimp_write.cpp
  )

find_package(Assimp)

if(Assimp_LIBRARY AND Assimp_INCLUDE_DIR)
  message("FOUND assimp")
  include_directories(SYSTEM ${Assimp_INCLUDE_DIR})

  gcv_plugin_library(gcv-assimp SHARED ${ASSIMP_SRCS})
  target_link_libraries(gcv-assimp libwdb librt libbu ${Assimp_LIBRARY})
  set_property(TARGET gcv-assimp APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(gcv-assimp "${ASSIMP_SRCS}")
  PLUGIN_SETUP(gcv-assimp gcv)

endif(Assimp_LIBRARY AND Assimp_INCLUDE_DIR)

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
