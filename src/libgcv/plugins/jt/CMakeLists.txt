set(JT_SRCS jt_parser.cpp jt_read.cpp jt_topology.cpp)

gcv_plugin_library(gcv-jt SHARED ${JT_SRCS})
target_link_libraries(gcv-jt libgcv libwdb librt ZLIB::ZLIB)
set_property(TARGET gcv-jt APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(gcv-jt "${JT_SRCS}")
plugin_setup(gcv-jt gcv)

brlcad_addexec(test_jt_parser "test_jt_parser.cpp;jt_parser.cpp" "libbu;ZLIB::ZLIB" NO_INSTALL)
brlcad_add_test(NAME jt_parser_test COMMAND test_jt_parser)

brlcad_addexec(test_jt_topology "test_jt_topology.cpp;jt_topology.cpp" "libbu" NO_INSTALL)
brlcad_add_test(NAME jt_topology_test COMMAND test_jt_topology)

cmakefiles(CMakeLists.txt jt_parser.h jt_topology.h ${JT_SRCS} test_jt_parser.cpp test_jt_topology.cpp)
