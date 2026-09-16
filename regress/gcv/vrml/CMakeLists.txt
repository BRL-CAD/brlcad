gcv_regress_util("vrml" "vrml_example.wrl")
gcv_regress_util(
  "vrml1"
  "vrml1_example.wrl"
  DEPS "gcv;gcv-vrml"
  EXPECTED_FILE_SIZE "28464"
)

cmakefiles(
  CMakeLists.txt
  vrml1_example.wrl
  vrml_example.wrl
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
