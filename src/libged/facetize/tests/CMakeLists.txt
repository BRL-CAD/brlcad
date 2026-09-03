brlcad_addexec(facetize_worker_protocol_test "worker.cpp;../worker.cpp" libbu TEST)
brlcad_add_test(NAME facetize_worker_protocol COMMAND facetize_worker_protocol_test)

cmakefiles(
  CMakeLists.txt
  worker.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
