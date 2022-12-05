include (VCICommon)

include_directories (
  ../../..
)

vci_add_executable (Dualizer dualizer.cc)

target_link_libraries (Dualizer
  OpenMeshCore
  OpenMeshTools
)

