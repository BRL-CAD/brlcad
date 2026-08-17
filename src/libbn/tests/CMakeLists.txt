# Higher-level public API behavior tests grouped to match libbn's existing
# per-area test layout.

set(
  bn_group_test_srcs
  test_anim.c
  test_complex.c
  test_eigen.cpp
  test_mat.c
  test_msr.c
  test_noise.c
  test_poly.c
  test_qmath.c
  test_rand.c
  test_sobol.c
  test_str.c
  test_tabdata.c
  test_ulp.c
  test_version.c
  test_vmath.c
  test_wavelet.c
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)
find_package_eigen(REQUIRED)

cmakefiles(${bn_group_test_srcs} test_util.c test_util.h)

foreach(srcfile ${bn_group_test_srcs})
  get_filename_component(NAME_ROOT ${srcfile} NAME_WE)
  brlcad_addexec(${NAME_ROOT} "${srcfile};test_util.c" "libbu;libbn;${M_LIBRARY}" TEST)
endforeach(srcfile ${bn_group_test_srcs})

if(TARGET test_eigen)
  target_include_directories(test_eigen SYSTEM PRIVATE "${EIGEN3_INCLUDE_DIR}")
endif(TARGET test_eigen)

brlcad_add_test(NAME test_version COMMAND test_version)
brlcad_add_test(NAME test_str COMMAND test_str)
brlcad_add_test(NAME test_ulp COMMAND test_ulp)
brlcad_add_test(NAME test_vmath COMMAND test_vmath)
brlcad_add_test(NAME test_noise COMMAND test_noise)
brlcad_add_test(NAME test_complex COMMAND test_complex)
brlcad_add_test(NAME test_eigen COMMAND test_eigen)
brlcad_add_test(NAME test_mat COMMAND test_mat)
brlcad_add_test(NAME test_poly COMMAND test_poly)
brlcad_add_test(NAME test_qmath COMMAND test_qmath)

brlcad_add_test(NAME test_rand_table COMMAND test_rand table)
brlcad_add_test(NAME test_rand_randmt COMMAND test_rand randmt)
brlcad_add_test(NAME test_rand_sphere COMMAND test_rand sphere)

brlcad_add_test(NAME test_msr_uniform COMMAND test_msr uniform)
brlcad_add_test(NAME test_msr_gaussian COMMAND test_msr gaussian)

brlcad_add_test(NAME test_sobol_skip COMMAND test_sobol skip)
brlcad_add_test(NAME test_sobol_bounds COMMAND test_sobol bounds)
brlcad_add_test(NAME test_sobol_sphere COMMAND test_sobol sphere)
brlcad_add_test(NAME test_sobol_integrand COMMAND test_sobol integrand)

brlcad_add_test(NAME test_anim_basic COMMAND test_anim basic)
brlcad_add_test(NAME test_anim_quat COMMAND test_anim quat)
brlcad_add_test(NAME test_anim_dirs COMMAND test_anim dirs)

brlcad_add_test(NAME test_wavelet_1d COMMAND test_wavelet 1d)
brlcad_add_test(NAME test_wavelet_2d COMMAND test_wavelet 2d)

brlcad_add_test(NAME test_tabdata_io COMMAND test_tabdata io)
brlcad_add_test(NAME test_tabdata_ops COMMAND test_tabdata ops)
brlcad_add_test(NAME test_tabdata_utils COMMAND test_tabdata utils)

cmakefiles(
  CMakeLists.txt
  dbl_to_from_long_test.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
