# To minimize the number of build targets and binaries that are created, we
# combine most (not all) of the unit tests into a single program.

set(
  bn_test_srcs
  mat.c
  sobolseq.c
  tabdata.c
)

# Generate and assemble the necessary per-test-type source code
set(BN_TEST_SRC_INCLUDES)
set(BN_TEST_SRC_SWITCHES)
set(BN_TEST_LIST)
foreach(srcfile ${bn_test_srcs})
  get_filename_component(NAME_ROOT ${srcfile} NAME_WE)
  set(BN_TEST_SRC_INCLUDES "${BN_TEST_SRC_INCLUDES}\n#include \"libbn/tests/${srcfile}\"")
  set(
    BN_TEST_SRC_SWITCHES
    "${BN_TEST_SRC_SWITCHES}\n   if (BU_STR_EQUAL(av[0], \"${NAME_ROOT}\")) return ${NAME_ROOT}_main(ac,av);"
  )
  set(BN_TEST_LIST "${BN_TEST_LIST}\\n  ${NAME_ROOT}")
endforeach(srcfile ${bn_test_srcs})

# Generate bn_test.c file and build it
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)
configure_file(bn_test.c.in "${CMAKE_CURRENT_BINARY_DIR}/bn_test.c" @ONLY)
distclean("${CMAKE_CURRENT_BINARY_DIR}/bn_test.c")
brlcad_addexec(bn_test "${CMAKE_CURRENT_BINARY_DIR}/bn_test.c" "libbn;${M_LIBRARY}" TEST)

# Housekeeping for included source files
cmakefiles(${bn_test_srcs})
validate_style("${bn_test_srcs}" bn_test)

# Higher-level public API behavior tests grouped to match libbn's existing
# per-area test layout.
set(
  bn_group_test_srcs
  test_anim.c
  test_complex.c
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
  test_wavelet.c
)

cmakefiles(${bn_group_test_srcs} test_util.c test_util.h)

foreach(srcfile ${bn_group_test_srcs})
  get_filename_component(NAME_ROOT ${srcfile} NAME_WE)
  brlcad_addexec(${NAME_ROOT} "${srcfile};test_util.c" "libbu;libbn;${M_LIBRARY}" TEST)
endforeach(srcfile ${bn_group_test_srcs})

brlcad_add_test(NAME test_version COMMAND test_version)
brlcad_add_test(NAME test_str COMMAND test_str)
brlcad_add_test(NAME test_ulp COMMAND test_ulp)
brlcad_add_test(NAME test_noise COMMAND test_noise)
brlcad_add_test(NAME test_complex COMMAND test_complex)
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

brlcad_add_test(NAME test_anim_basic COMMAND test_anim basic)
brlcad_add_test(NAME test_anim_quat COMMAND test_anim quat)
brlcad_add_test(NAME test_anim_dirs COMMAND test_anim dirs)

brlcad_add_test(NAME test_wavelet_1d COMMAND test_wavelet 1d)
brlcad_add_test(NAME test_wavelet_2d COMMAND test_wavelet 2d)

brlcad_add_test(NAME test_tabdata_io COMMAND test_tabdata io)
brlcad_add_test(NAME test_tabdata_utils COMMAND test_tabdata utils)

# Testing with Eigen
find_package_eigen(REQUIRED)
brlcad_addexec(bn_eigen eigen.cpp "libbu;libbn" TEST)
if(TARGET bn_eigen)
  target_include_directories(bn_eigen SYSTEM PRIVATE "${EIGEN3_INCLUDE_DIR}")
endif(TARGET bn_eigen)

#
#  *************** mat.c tests ***************
#
# For COMMAND bn_test mat, input is as follows:
# COMMAND bn_test mat <function_number> <args>
#
# Values supplied to the test program were verified using
# Wolfram Alpha: http://www.wolframalpha.com/
# and http://www.bluebit.gr/matrix-calculator/

# For function #4 (bn_matXvec), the <args> format is as follows:
#
# M V <expected_result>
#
# where M is the matrix
#       V is the (column) vector to which the matrix will be multiplied, MxV.
# A matrix is 16 floating point numbers separated by commas.
# A vector is 4 floating point numbers separated by commas.

brlcad_add_test(NAME bn_matXvec_id           COMMAND bn_test mat 4 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 2.78,3.45,16.7,38.22 2.78,3.45,16.7,38.22)
brlcad_add_test(NAME bn_matXvec_0_1          COMMAND bn_test mat 4 18.59,25.82,46.39,16.56,46.13,22.16,29.66,92.89,67.58,79.96,59.47,51.4,55.83,48.5,82.51,73.52 0,0,0,0 0,0,0,0)
brlcad_add_test(NAME bn_matXvec_0_2          COMMAND bn_test mat 4 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 1.1,2.2,3.3,4.4 0,0,0,0)
brlcad_add_test(NAME bn_matXvec_dif_1        COMMAND bn_test mat 4 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1,2,3,4 30,70,110,150)
brlcad_add_test(NAME bn_matXvec_dif_2        COMMAND bn_test mat 4 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 0,1,2,3 14,38,62,86)
brlcad_add_test(NAME bn_matXvec_dif_3        COMMAND bn_test mat 4 18.59,25.82,46.39,16.56,46.13,22.16,29.66,92.89,67.58,79.96,59.47,51.4,55.83,48.5,82.51,73.52 7.31,68.38,65.97,3.26  5015.7984,4112.0027,10052.4745,9407.4072)

# For function #5 (bn_mat_inverse), the <args> format is as follows:
#
# 0|1 M [expected_result]
#
# where 0 or 1 indicates whether the matrix is singular
#       M is the matrix to be inverted.
# A matrix is 16 floating point numbers separated by commas.

brlcad_add_test(NAME bn_mat_inverse_0        COMMAND bn_test mat 5 1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
brlcad_add_test(NAME bn_mat_inverse_id       COMMAND bn_test mat 5 0 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
brlcad_add_test(NAME bn_mat_inverse_sing_1   COMMAND bn_test mat 5 1 1,2,2,0,1,1,3,4,2,2,1,0,0,0,0,0)
brlcad_add_test(NAME bn_mat_inverse_sing_2   COMMAND bn_test mat 5 1 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16)
brlcad_add_test(NAME bn_mat_inverse_1        COMMAND bn_test mat 5 0 14.16,71.27,95.53,77.27,91.51,26.83,17.56,6.94,30.11,40.47,38.1,39.27,56.52,90.12,87.96,84.3 0.00836179,0.0114599,0.0844829,-0.0479631,-0.0600379,-0.0107874,-0.349984,0.218954,0.0425546,0.0207413,-0.0527631,-0.0161344,0.0141745,-0.0177931,0.372557,-0.173216)

# For function #6 (bn_mat_trn), the <args> format is as follows:
#
# M <expected_result>
#
# where M is the matrix to transpose.
# A matrix is 16 floating point numbers separated by commas.

brlcad_add_test(NAME bn_mat_trn_id           COMMAND bn_test mat 6 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
brlcad_add_test(NAME bn_mat_trn_same         COMMAND bn_test mat 6 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
brlcad_add_test(NAME bn_mat_trn_1            COMMAND bn_test mat 6 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1,5,9,13,2,6,10,14,3,7,11,15,4,8,12,16)
brlcad_add_test(NAME bn_mat_trn_2            COMMAND bn_test mat 6 99.01,41.88,17.07,37.47,38.8,42.89,34.48,82.84,59.97,23.74,6.98,27.81,1.64,86.35,43.57,13.87 99.01,38.8,59.97,1.64,41.88,42.89,23.74,86.35,17.07,34.48,6.98,43.57,37.47,82.84,27.81,13.87)

# For function #7 (bn_mat_is_identity), the <args> format is as follows:
#
# M <expected_result>
#
# where M is the matrix to test.
# A matrix is 16 floating point numbers separated by commas.

brlcad_add_test(NAME bn_mat_is_identity_id   COMMAND bn_test mat 7 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1)
brlcad_add_test(NAME bn_mat_is_identity_0    COMMAND bn_test mat 7 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0)
brlcad_add_test(NAME bn_mat_is_identity_1    COMMAND bn_test mat 7 15.58,76.15,18.45,80.55,52.36,28.49,85.75,56.75,76.44,66.57,32.39,52.18,39.21,96.09,98.66,30.95 0)
brlcad_add_test(NAME bn_mat_is_identity_2    COMMAND bn_test mat 7 1.001,0,0,0,0,1.001,0,0,0,0,1.001,0,0,0,0,1.001 0)

# For function #8 (bn_mat_det3), the <args> format is as follows:
#
# M <expected_result>
#
# where M is the matrix to find the upper-left 3x3 determinant of.
# A matrix is 16 floating point numbers separated by commas.

brlcad_add_test(NAME bn_mat_det3_id          COMMAND bn_test mat 8 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1)
brlcad_add_test(NAME bn_mat_det3_0           COMMAND bn_test mat 8 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0)
brlcad_add_test(NAME bn_mat_det3_1           COMMAND bn_test mat 8 77.64,50.22,46.68,16.35,41.25,71.79,24.01,23.88,37.37,94.83,34.25,4.43,95.01,81.8,1.45,45.57 45601.558488)
brlcad_add_test(NAME bn_mat_det3_2           COMMAND bn_test mat 8 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 0)

# For function #9 (bn_mat_determinant), the <args> format is as follows:
#
# M <expected_result>
#
# where M is the matrix to find the determinant of.
# A matrix is 16 floating point numbers separated by commas.

brlcad_add_test(NAME bn_mat_determinant_id   COMMAND bn_test mat 9 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1)
brlcad_add_test(NAME bn_mat_determinant_0    COMMAND bn_test mat 9 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0)
brlcad_add_test(NAME bn_mat_determinant_1    COMMAND bn_test mat 9 80.22,28.45,95.46,74.05,53.38,74.98,71.55,24.86,65.01,46.42,64.24,49.13,75.74,79.22,64.61,17.33 2.1004520202541295e6)
brlcad_add_test(NAME bn_mat_determinant_2    COMMAND bn_test mat 9 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 0)

# For function #10 (bn_mat_is_equal), the <args> format is as follows:
#
# M1 M2 <expected_result>
#
# where M1 and M2 are the two matrices to be compared.
# A matrix is 16 floating point numbers separated by commas.

brlcad_add_test(NAME bn_mat_is_equal_1       COMMAND bn_test mat 10 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1)
brlcad_add_test(NAME bn_mat_is_equal_2       COMMAND bn_test mat 10 70.46,38.7,1.57,84.19,38.53,52.52,89.28,8.58,89.96,61.36,5.45,63.62,99.98,82.15,65.44,67.46 70.46,38.7,1.57,84.19,38.53,52.52,89.28,8.58,89.96,61.36,5.45,63.62,99.98,82.15,65.44,67.46 1)
brlcad_add_test(NAME bn_mat_is_equal_3       COMMAND bn_test mat 10 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0)
brlcad_add_test(NAME bn_mat_is_equal_4       COMMAND bn_test mat 10 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 71.53,18.85,47.48,24.56,42.34,98.18,42.1,74.61,30.47,49.18,63.06,2.11,58.31,60.61,0.62,34.23 0)

# For function #11 (bn_mat_atan2), the <args> format is as follows:
#
# y x <expected_result>
#
# where x and y are the floating point arguments to the atan2 function

brlcad_add_test(NAME bn_atan2_0_0            COMMAND bn_test mat 11 0 0 0)
brlcad_add_test(NAME bn_atan2_0_1            COMMAND bn_test mat 11 2.8 0 1.570796326794896)
brlcad_add_test(NAME bn_atan2_0_2            COMMAND bn_test mat 11 -2.8 0 -1.570796326794896)
brlcad_add_test(NAME bn_atan2_1              COMMAND bn_test mat 11 2.55 8.76 0.283268001141651)
brlcad_add_test(NAME bn_atan2_2              COMMAND bn_test mat 11 -2.8 -7.2 -2.770701364777130)

# For function #12 (bn_vtoh_move), the <args> format is as follows:
#
# V <expected_result>
#
# where V is the 3-vector to move to a homogeneous vector.
# A vector is three floating point numbers separated by commas.

brlcad_add_test(NAME bn_vtoh_move            COMMAND bn_test mat 12 2.6,-5.5,8.12 2.6,-5.5,8.12,1)

# For function #13 (bn_vtoh_move), the <args> format is as follows:
#
# H <expected_result>
#
# where H is the homogeneous vector to move to a 3-vector.
# A homogeneous vector is four floating point numbers separated by commas.

brlcad_add_test(NAME bn_htov_move            COMMAND bn_test mat 13 1.1,2.2,3.3,7.4 0.14864864864864866,0.2972972972972973,0.4459459459459459)

# For function #14 (bn_mat_ae), the <args> format is as follows:
#
# az el <expected_result>
#
# where az is the azimuth and el is the elevation to generate a
# transform matrix.
# az and el are floating point numbers and are in degrees.

brlcad_add_test(NAME bn_mat_ae               COMMAND bn_test mat 14 0 90 0,0,-1,0,0,1,0,0,1,0,0,0,0,0,0,1)

# For function #15 (bn_ae_vec), the <args> format is as follows:
#
# V <expected_az> <expected_el>
#
# where V is the vector to extract az and el info from.
#
# Functions 15-17 were verified using the website below.
# http://www.random-science-tools.com/maths/coordinate-converter.htm

brlcad_add_test(NAME bn_ae_vec_0             COMMAND bn_test mat 15 0,1,0 90 0)
brlcad_add_test(NAME bn_ae_vec_1             COMMAND bn_test mat 15 5.7,-2.34,19.61 337.68055960702 72.556852698595)
brlcad_add_test(NAME bn_ae_vec_2             COMMAND bn_test mat 15 -23.78,42,3.141 119.518125952119 3.7234738236524)

# For function #16 (bn_vec_ae), the <args> format is as follows:
#
# az el <expected_result>
#
# where az is the azimuth and el is the elevation to generate a vector
# az and el are floating point numbers and are in radians.

# These two tests fail for some reason. It could be a bug in bn_vec_ae().
# The value of the returned vector seems to always have the same z value
# irrespective of the input.

brlcad_add_test(NAME bn_vec_ae_00 COMMAND bn_test mat 16 0 0 1,0,0)
brlcad_add_test(NAME bn_vec_ae_01 COMMAND bn_test mat 16 0  1.57079632679 0,0,1)
brlcad_add_test(NAME bn_vec_ae_02 COMMAND bn_test mat 16 0 -1.57079632679 0,0,-1)
brlcad_add_test(NAME bn_vec_ae_03 COMMAND bn_test mat 16 0  3.14159265359 -1,0,0)
brlcad_add_test(NAME bn_vec_ae_04 COMMAND bn_test mat 16 0 -3.14159265359 -1,0,0)
brlcad_add_test(NAME bn_vec_ae_05 COMMAND bn_test mat 16 1.57079632679 0 0,1,0)
brlcad_add_test(NAME bn_vec_ae_06 COMMAND bn_test mat 16 1.57079632679  1.57079632679 0,0,1)
brlcad_add_test(NAME bn_vec_ae_07 COMMAND bn_test mat 16 1.57079632679 -1.57079632679 0,0,-1)
brlcad_add_test(NAME bn_vec_ae_08 COMMAND bn_test mat 16 1.57079632679  3.14159265359 0,-1,0)
brlcad_add_test(NAME bn_vec_ae_09 COMMAND bn_test mat 16 1.57079632679 -3.14159265359 0,-1,0)
brlcad_add_test(NAME bn_vec_ae_10 COMMAND bn_test mat 16 -1.57079632679 0 0,-1,0)
brlcad_add_test(NAME bn_vec_ae_11 COMMAND bn_test mat 16 -1.57079632679  1.57079632679 0,0,1)
brlcad_add_test(NAME bn_vec_ae_12 COMMAND bn_test mat 16 -1.57079632679 -1.57079632679 0,0,-1)
brlcad_add_test(NAME bn_vec_ae_13 COMMAND bn_test mat 16 -1.57079632679  3.14159265359 0,1,0)
brlcad_add_test(NAME bn_vec_ae_14 COMMAND bn_test mat 16 -1.57079632679 -3.14159265359 0,1,0)
brlcad_add_test(NAME bn_vec_ae_15 COMMAND bn_test mat 16  3.14159265359 0 -1,0,0)
brlcad_add_test(NAME bn_vec_ae_16 COMMAND bn_test mat 16  3.14159265359  1.57079632679 0,0,1)
brlcad_add_test(NAME bn_vec_ae_17 COMMAND bn_test mat 16  3.14159265359 -1.57079632679 0,0,-1)
brlcad_add_test(NAME bn_vec_ae_18 COMMAND bn_test mat 16  3.14159265359  3.14159265359 1,0,0)
brlcad_add_test(NAME bn_vec_ae_19 COMMAND bn_test mat 16  3.14159265359 -3.14159265359 1,0,0)
brlcad_add_test(NAME bn_vec_ae_20 COMMAND bn_test mat 16 -3.14159265359 0 -1,0,0)
brlcad_add_test(NAME bn_vec_ae_21 COMMAND bn_test mat 16 -3.14159265359  1.5707963267 0,0,1)
brlcad_add_test(NAME bn_vec_ae_22 COMMAND bn_test mat 16 -3.14159265359 -1.5707963267 0,0,-1)
brlcad_add_test(NAME bn_vec_ae_23 COMMAND bn_test mat 16 -3.14159265359  3.14159265359 1,0,0)
brlcad_add_test(NAME bn_vec_ae_24 COMMAND bn_test mat 16 -3.14159265359 -3.14159265359 1,0,0)
brlcad_add_test(NAME bn_vec_ae_25 COMMAND bn_test mat 16 1 1 0.291926581820,0.454648713558,0.841470984697)
brlcad_add_test(NAME bn_vec_ae_26 COMMAND bn_test mat 16 0.5 1.2 0.317998846662,0.173723561699,0.932039085893)

# For function #17 (bn_vec_aed), the <args> format is as follows:
#
# az el d <expected_result>
#
# where az is the azimuth, el is the elevation and d is the distance
# to generate a vector
# az and el are floating point numbers and are in radians.
# d is a floating point number.

brlcad_add_test(NAME bn_vec_aed_1            COMMAND bn_test mat 17 1 1 5 1.45963290910,2.27324356779,4.20735492349)
brlcad_add_test(NAME bn_vec_aed_2            COMMAND bn_test mat 17 0.3845 0.286 18.3354 16.3062567701,6.59816438267,5.17272752864)

# For function #18 (bn_mat_angles), the <args> format is as follows:
#
# x y z <expected_result>
#
# where x, y, z are the angles to rotate about their respective axes
# x, y, z are floating point numbers and are in degrees.
#
# Functions 18 and 19 were verified using the website below
# http://toolserver.org/~dschwen/tools/rotationmatrix.html

brlcad_add_test(NAME bn_mat_angles_1         COMMAND bn_test mat 18 30 40 50 0.4924038765,-0.5868240888,0.6427876097,0,0.8700019038,0.310468461,-0.3830222216,0,0.0252013863,0.7478280708,0.6634139482,0,0,0,0,1)
brlcad_add_test(NAME bn_mat_angles_2         COMMAND bn_test mat 18 84.23 19.5 38.9 0.733604282,-0.591944033,0.3338068592,0,0.3215992025,-0.1303153824,-0.9378655842,0,0.5986641049,0.7953742282,0.0947688064,0,0,0,0,1)

# For function #19 (bn_mat_angles_rad), the <args> format is as follows:
#
# x y z <expected_result>
##
# where x, y, z are the angles to rotate about their respective axes
# x, y, z are floating point numbers and are in radians.

brlcad_add_test(NAME bn_mat_angles_rad_1     COMMAND bn_test mat 19 1.14 0.856 0.321 0.6219827767,-0.2068090198,0.7552267572,0,0.7829298453,0.1797494268,-0.5955761923,0,-0.0125810482,0.9617277021,0.2737180014,0,0,0,0,1)
brlcad_add_test(NAME bn_mat_angles_rad_2     COMMAND bn_test mat 19 0.6 1 0.92 0.3273260276,-0.4298653899,0.8414709848,0,0.9444818397,0.1219905484,-0.3050776304,0,0.0284908076,0.8946139126,0.4459307359,0,0,0,0,1)

# For function #20 (bn_eigen2x2), the <args> format is as follows:
#
# a b c <expected_val1> <expected_val2> <expected_vec1> <expected_vec2>
#
# where a, b, c are the values of the symmetric 2x2 matrix.
# The eigenvalue with the lowest absolute value will be val1, and its
# eigenvector will be in vec1.
# An eigen vector is two floating point numbers separated by commas.

brlcad_add_test(NAME bn_eigen2x2_1           COMMAND bn_test mat 20 2.18 13.9 6.6 -9.68459058019096 18.464590580191 0.760598630446286,-0.649222398999938,0 0.649222398999938,0.760598630446286,0)
brlcad_add_test(NAME bn_eigen2x2_2           COMMAND bn_test mat 20 1 2 3 -0.23606797749979 4.23606797749979 0.85065080835204,-0.525731112119133,0 0.525731112119133,0.85065080835204,0)

# For function #21 (bn_mat_xrot), the <args> format is as follows:
#
# sinx cosx <expected_result>
#
# where sinx and cosx are the sine and cosine of the angle of rotation
# about the x-axis.
# sinx and cosx are floating point values.

brlcad_add_test(NAME bn_mat_xrot_1           COMMAND bn_test mat 21 0.342020143325668 0.939692620785908 1,0,0,0,0,0.9396926208,-0.3420201433,0,0,0.3420201433,0.9396926208,0,0,0,0,1)
brlcad_add_test(NAME bn_mat_xrot_2           COMMAND bn_test mat 21 -0.78260815685241 0.622514636637619 1,0,0,0,0,0.6225146366,0.7826081569,0,0,-0.7826081569,0.6225146366,0,0,0,0,1)

# For function #22 (bn_mat_yrot), the <args> format is as follows:
#
# siny cosy <expected_result>
#
# where siny and cosy are the sine and cosine of the angle of rotation
# about the y-axis.
# siny and cosy are floating point values.

brlcad_add_test(NAME bn_mat_yrot_1           COMMAND bn_test mat 22 0.30901699437494 0.951056516295153 0.9510565163,0,-0.3090169944,0,0,1,0,0,0.3090169944,0,0.9510565163,0,0,0,0,1)
brlcad_add_test(NAME bn_mat_yrot_2           COMMAND bn_test mat 22 0.17364817766693 0.984807753012208 0.9848077530122,0,-0.1736481777,0,0,1,0,0,0.1736481777,0,0.984807753,0,0,0,0,1)

# For function #23 (bn_mat_zrot), the <args> format is as follows:
#
# sinz cosz <expected_result>
#
# where sinz and cosz are the sine and cosine of the angle of rotation
# about the z-axis.
# sinz and cosz are floating point values.

brlcad_add_test(NAME bn_mat_zrot_1           COMMAND bn_test mat 23 0.669130606358858 0.743144825477394 0.743144825477394,-0.669130606358858,0,0,0.669130606358858,0.743144825477394,0,0,0,0,1,0,0,0,0,1)
brlcad_add_test(NAME bn_mat_zrot_2           COMMAND bn_test mat 23 -0.99619469809174 -0.087155742747658 -0.087155742747658,0.996194698091745,0,0,-0.996194698091746,-0.087155742747658,0,0,0,0,1,0,0,0,0,1)

# For function #24 (bn_mat_scale_about_pt), the <args> format is as follows:
#
# P s <expected_result>
#
# where P is the point to scale about and s is the uniform scale to
# be applied.
# A point is three floating point numbers separated by commas.
# Scale is a floating point number.

brlcad_add_test(NAME bn_mat_scale_about_pt_1 COMMAND bn_test mat 24 3,4,5 1.5 0 1,0,0,-1,0,1,0,-1.333333333333333,0,0,1,-1.666666666666667,0,0,0,0.666666666666667)
brlcad_add_test(NAME bn_mat_scale_about_pt_2 COMMAND bn_test mat 24 2.18,-4.55,-17.4 0.31 0 1,0,0,4.852258064516120,0,1,0,-10.127419354838700,0,0,1,-38.729032258064500,0,0,0,3.225806451612900)

# For function #25 (bn_mat_xform_about_pt), the <args> format is as follows:
#
# M P <expected_result>
#
# where M is the transform matrix, P is the point to apply from.
# A matrix is 16 floating point values separated by commas.
# A point is three floating point values separated by commas.

brlcad_add_test(NAME bn_mat_xform_about_pt_1 COMMAND bn_test mat 25 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 -2,4.5,6.7 -25,-26,-27,219.9,63.5,69,74.5,-602.65,96.1,103.8,111.5,-902.75,13,14,15,-121.5)

# For function #26 (bn_mat_ck), the <args> format is as follows:
#
# M <expected_result>
#
# where M is the matrix to check for axis perpendicularity
# A matrix is 16 floating point values separated by commas.

brlcad_add_test(NAME bn_mat_ck_1             COMMAND bn_test mat 26 -0.064769339233561,-0.740316935054896,-0.669130606358858,0,-0.920939044198179,-0.213876288688268,0.325773249374894,0,-0.384286624235859,0.637328619165857,-0.667934144677115,0,0,0,0,1 0)
brlcad_add_test(NAME bn_mat_ck_2             COMMAND bn_test mat 26 -0.384795044679446,-0.906520316365329,0.17364817766693,23.835,-0.772197474581702,0.419235049422787,0.477444272753497,-1.23,-0.505612335529687,0.049627506006009,-0.861332268528144,-46.321,0,0,0,1 0)
brlcad_add_test(NAME bn_mat_ck_3             COMMAND bn_test mat 26 -0.38457345,-0.944,0.46483,23.835,0.27474,0.4192787,0.4272753497,-1.23,-0.87,0.06009,0.999144,-46.321,0,0,0,1 -1)

#  ***************** sobolseq.c tests ***************

brlcad_add_test(NAME bn_sobol_3_1000         COMMAND bn_test sobolseq 3 1000)

#
#  *************** tabdata.c tests ***************
#
# For COMMAND bn_test tabdata, input is as follows:
# COMMAND bn_test tabdata <function_number> <args>

# For function #1 (bn_table_make_uniform), the <args> format is as follows:
#
# first last num expected
#
# where first last and num will be used to make a uniform table, and
# expected is the expected result table.  A table is a list of
# comma-separated numbers, where the first one is the number of
# entries: nx,x0,x1,...,x(nx).

brlcad_add_test(NAME bn_table_make_uniform COMMAND bn_test tabdata 1 0 100 10 10,0,10,20,30,40,50,60,70,80,90,100)

# For function #2 (bn_tabdata_add), the <args> format is as follows:
#
# table tabdata1 tabdata2 expected_tabdata
#
# where tabdata1 and tabdata2 will be added, and table is a table that
# tabdata1 and tabdata2 use.  A table is a list of comma-separated
# numbers, where the first one is the number of entries:
# nx,x0,x1,...,x(nx); a tabdata is a list of comma-separated numbers
# with the same length as the table to which it refers; if the table
# entry is nx,x0,x1,...,x(nx), then the tabdata entry is
# y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_add COMMAND bn_test tabdata 2 5,0,20,40,60,80,100 1,2,3,4,5 8,3,6,2,9 9,5,9,6,14)

# For function #3 (bn_tabdata_mul), the <args> format is as follows:
#
# table tabdata1 tabdata2 expected_tabdata
#
# where tabdata1 and tabdata2 will be multiplied, and table is a table
# that tabdata1 and tabdata2 use.  A table is a list of
# comma-separated numbers, where the first one is the number of
# entries: nx,x0,x1,...,x(nx); a tabdata is a list of comma-separated
# numbers with the same length as the table to which it refers; if the
# table entry is nx,x0,x1,...,x(nx), then the tabdata entry is
# y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_mul COMMAND bn_test tabdata 3 5,0,20,40,60,80,100 1,2,3,4,5 8,3,6,2,9 8,6,18,8,45)

# For function #4 (bn_tabdata_mul3), the <args> format is as follows:
#
# table tabdata1 tabdata2 tabdata3 expected_tabdata
#
# where tabdata1, tabdata2, and tabdata3 will be multiplied, and table
# is a table that tabdata1, tabdata2, and tabdata3 use.  A table is a
# list of comma-separated numbers, where the first one is the number
# of entries: nx,x0,x1,...,x(nx); a tabdata is a list of
# comma-separated numbers with the same length as the table to which
# it refers; if the table entry is nx,x0,x1,...,x(nx), then the
# tabdata entry is y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_mul3 COMMAND bn_test tabdata 4 5,0,20,40,60,80,100 1,2,3,4,5 8,3,6,2,9 8,6,18,8,45 64,36,324,64,2025)

# For function #5 (bn_tabdata_incr_mul3_scale), the <args> format is as follows:
#
# table tabdataout_orig tabdata1 tabdata2 tabdata3 scale expected_tabdata
#
# where tabdata1, tabdata2, and tabdata3, and scale will be
# multiplied, and the result will be added to tabdataout_orig; table
# is a table that tabdata1, tabdata2, and tabdata3 use.  A table is a
# list of comma-separated numbers, where the first one is the number
# of entries: nx,x0,x1,...,x(nx); a tabdata is a list of
# comma-separated numbers with the same length as the table to which
# it refers; if the table entry is nx,x0,x1,...,x(nx), then the
# tabdata entry is y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_incr_mul3_scale COMMAND bn_test tabdata 5 5,0,20,40,60,80,100 5,4,3,2,1 1,2,3,4,5 8,3,6,2,9 8,6,18,8,45 2 133,76,651,130,4051)

# For function #6 (bn_tabdata_incr_mul2_scale), the <args> format is as follows:
#
# table tabdataout_orig tabdata1 tabdata2 scale expected_tabdata
#
# where tabdata1 and tabdata2, and scale will be multiplied, and the
# result will be added to tabdataout_orig; table is a table that
# tabdata and tabdata2 use.  A table is a list of comma-separated
# numbers, where the first one is the number of entries:
# nx,x0,x1,...,x(nx); a tabdata is a list of comma-separated numbers
# with the same length as the table to which it refers; if the table
# entry is nx,x0,x1,...,x(nx), then the tabdata entry is
# y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_incr_mul2_scale COMMAND bn_test tabdata 6 5,0,20,40,60,80,100 5,4,3,2,1 1,2,3,4,5 8,3,6,2,9 2 21,16,39,18,91)

# For function #7 (bn_tabdata_scale), the <args> format is as follows:
#
# table tabdata scale expected_tabdata
#
# where tabdata will be scaled by scale.  A table is a list of
# comma-separated numbers, where the first one is the number of
# entries: nx,x0,x1,...,x(nx); a tabdata is a list of comma-separated
# numbers with the same length as the table to which it refers; if the
# table entry is nx,x0,x1,...,x(nx), then the tabdata entry is
# y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_scale COMMAND bn_test tabdata 7 5,0,20,40,60,80,100 1,2,3,4,5 2 2,4,6,8,10)

# For function #8 (bn_table_scale), the <args> format is as follows:
#
# table scale expected_table
#
# where table will be scaled by scale.  A table is a list of
# comma-separated numbers, where the first one is the number of
# entries: nx,x0,x1,...,x(nx).

brlcad_add_test(NAME bn_table_scale COMMAND bn_test tabdata 8 5,0,20,40,60,80,100 2 5,0,40,80,120,160,200)

# For function #9 (bn_tabdata_join1), the <args> format is as follows:
#
# table tabdata1 tabdata2 scale expected_tabdata
#
# where tabdata1, tabdata2, and scale will be combined via
# bn_tabdata_join1, and the result will be checked against
# expected_tabdata. A table is a list of comma-separated numbers,
# where the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_join1 COMMAND bn_test tabdata 9 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 2 11,10,9,8,7)

# For function #10 (bn_tabdata_join2), the <args> format is as follows:
#
# table tabdata1 tabdata2 tabdata3 scale1 scale2 expected_tabdata
#
# where tabdata1, tabdata2, and scale will be combined via
# bn_tabdata_join2, and the result will be checked against
# expected_tabdata. A table is a list of comma-separated numbers,
# where the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_join2 COMMAND bn_test tabdata 10 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 2,4,6,8,10 2 3 17,22,27,32,37)

# For function #11 (bn_tabdata_blend2), the <args> format is as follows:
#
# table tabdata1 tabdata2 scale1 scale2 expected_result
#
# where tabdata1, tabdata2, scale1, and scale2 will be combined via
# bn_tabdata_blend2, and the result will be checked against
# expected_result. A table is a list of comma-separated numbers,
# where the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_blend2 COMMAND bn_test tabdata 11 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 2 0.5 4.5,6,7.5,9,10.5)

# For function #12 (bn_tabdata_blend3), the <args> format is as follows:
#
# table tabdata1 tabdata2 tabdata3 scale1 scale2 scale3 expected_result
#
# where tabdata1, tabdata2, tabdata3, scale1, scale2, and scale3 will
# be combined via bn_tabdata_blend3, and the result will be checked
# against expected_result. A table is a list of comma-separated
# numbers, where the first one is the number of entries:
# nx,x0,x1,...,x(nx); a tabdata is a list of comma-separated numbers
# with the same length as the table to which it refers; if the table
# entry is nx,x0,x1,...,x(nx), then the tabdata entry is
# y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_blend3 COMMAND bn_test tabdata 12 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 3,2,4,1,5 2 0.5 3 13.5,12,19.5,12,25.5)

# For function #13 (bn_tabdata_area1), the <args> format is as follows:
#
# table tabdata expected_result
#
# where the area of the tabdata (which is a tabdata over table), will
# be calculated. The area will be calculated using interpretation #1
# (see bn.h). A table is a list of comma-separated numbers, where the
# first one is the number of entries: nx,x0,x1,...,x(nx); a tabdata is
# a list of comma-separated numbers with the same length as the table
# to which it refers; if the table entry is nx,x0,x1,...,x(nx), then
# the tabdata entry is y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_area1 COMMAND bn_test tabdata 13 5,0,20,40,60,80,100 1,2,3,4,5 15)

# For function #14 (bn_tabdata_area2), the <args> format is as follows:
#
# table tabdata expected_result
#
# where the area of the tabdata (which is a tabdata over table), will
# be calculated. The area will be calculated using interpretation #2
# (see bn.h). A table is a list of comma-separated numbers, where the
# first one is the number of entries: nx,x0,x1,...,x(nx); a tabdata is
# a list of comma-separated numbers with the same length as the table
# to which it refers; if the table entry is nx,x0,x1,...,x(nx), then
# the tabdata entry is y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_area2 COMMAND bn_test tabdata 14 5,0,20,40,60,80,100 1,2,3,4,5 300)

# For function #15 (bn_tabdata_mul_area1), the <args> format is as follows:
#
# table tabdata1 tabdata2 expected_result
#
# where the area of tabdata1*tabdata2 (which are tabdatas over table),
# will be calculated. The area will be calculated using interpretation
# #1 (see bn.h). A table is a list of comma-separated numbers, where
# the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_mul_area1 COMMAND bn_test tabdata 15 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 35)

# For function #16 (bn_tabdata_mul_area2), the <args> format is as follows:
#
# table tabdata expected_result
#
# where the area of tabdata1*tabdata2 (which are tabdatas over table),
# will be calculated. The area will be calculated using interpretation
# #2 (see bn.h). A table is a list of comma-separated numbers, where
# the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_mul_area2 COMMAND bn_test tabdata 16 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 700)

# For function #17 (bn_table_lin_interp), the <args> format is as follows:
#
# table tabdata wl expected_result
#
# where wl will be used to sample the tabdata via
# bn_table_lin_interp. A table is a list of comma-separated numbers,
# where the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_table_lin_interp_1 COMMAND bn_test tabdata 17 5,0,20,40,60,80,100 1,2,3,4,5 30 2.5)
brlcad_add_test(NAME bn_table_lin_interp_2 COMMAND bn_test tabdata 17 5,0,20,30,60,80,100 1,2,3,4,5 30 3)
brlcad_add_test(NAME bn_table_lin_interp_3 COMMAND bn_test tabdata 17 5,0,20,30,60,80,100 1,2,3,4,5 45 3.5)
brlcad_add_test(NAME bn_table_lin_interp_4 COMMAND bn_test tabdata 17 5,0,20,30,60,80,100 1,2,3,4,5 25 2.5)
brlcad_add_test(NAME bn_table_lin_interp_5 COMMAND bn_test tabdata 17 5,0,20,40,60,80,100 1,2,3,4,5 800 0)

# For function #18 (bn_tabdata_copy), the <args> format is as follows:
#
# table tabdata
#
# where tabdata will be copied via bn_tabdata_copy. A table is a list
# of comma-separated numbers, where the first one is the number of
# entries: nx,x0,x1,...,x(nx); a tabdata is a list of comma-separated
# numbers with the same length as the table to which it refers; if the
# table entry is nx,x0,x1,...,x(nx), then the tabdata entry is
# y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_copy COMMAND bn_test tabdata 18 5,0,20,40,60,80,100 1,2,3,4,5)

# For function #19 (bn_tabdata_dup), the <args> format is as follows:
#
# table tabdata
#
# where tabdata will be copied via bn_tabdata_dup. A table is a list
# of comma-separated numbers, where the first one is the number of
# entries: nx,x0,x1,...,x(nx); a tabdata is a list of comma-separated
# numbers with the same length as the table to which it refers; if the
# table entry is nx,x0,x1,...,x(nx), then the tabdata entry is
# y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_dup COMMAND bn_test tabdata 19 5,0,20,40,60,80,100 1,2,3,4,5)

# For function #20 (bn_tabdata_get_constval), the <args> format is as follows:
#
# table val expected_tabdata
#
# where a tabdata based on table with every item set to val will be
# produced via bn_tabdata_get_constval and checked against
# expected_tabdata. A table is a list of comma-separated numbers,
# where the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

brlcad_add_test(NAME bn_tabdata_get_constval COMMAND bn_test tabdata 20 5,0,20,40,60,80,100 3 3,3,3,3,3)

cmakefiles(
  CMakeLists.txt
  bntester.dat
  bn_test.c.in
  dbl_to_from_long_test.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
