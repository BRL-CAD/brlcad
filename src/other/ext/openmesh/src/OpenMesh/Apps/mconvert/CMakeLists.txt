include (VCICommon)

include_directories (
  ../../..
)

vci_add_executable (mconvert mconvert.cc)

target_link_libraries (mconvert
  OpenMeshCore
  OpenMeshTools
)

