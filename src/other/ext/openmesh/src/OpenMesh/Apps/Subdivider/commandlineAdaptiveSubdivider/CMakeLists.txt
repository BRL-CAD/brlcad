include (VCICommon)

include_directories (
  ../../../..
  ${CMAKE_CURRENT_SOURCE_DIR}
)

vci_add_executable (commandlineAdaptiveSubdivider ../adaptive_subdivider.cc)

target_link_libraries (commandlineAdaptiveSubdivider
  OpenMeshCore
  OpenMeshTools
)

