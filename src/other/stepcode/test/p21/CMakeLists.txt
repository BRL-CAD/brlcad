CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
#test part 21 files

#necessary macros won't already be defined if SC_BUILD_SCHEMAS is set to ""
if(NOT COMMAND SCHEMA_CMLIST)
  include(${SC_CMAKE_DIR}/schema_scanner/schemaScanner.cmake)
endif(NOT COMMAND SCHEMA_CMLIST)

#used to create a number of small files at test-time. you probably don't want to modify.
set(P21_FILE_HEAD "
ISO-10303-21;
HEADER;
FILE_DESCRIPTION((''),'2;1');
FILE_NAME('', '', (''), (''), '', '', '');
FILE_SCHEMA(('")
set(P21_FILE_MID "'));
ENDSEC;
DATA;
")
set(P21_FILE_TAIL "
ENDSEC;
END-ISO-10303-21;
")

# used in INSTANCE_TEST
set(SI_TEST_COUNTER 0)

#MACRO INSTANCE_TEST - create a file with one or more instances and test with p21read
#sname is the schema name
#instance is a p21 instance, without the number
#test_mode is PASS or FAIL - whether the test should pass or fail. affects the WILL_FAIL property
macro(INSTANCE_TEST sname test_mode instance)
  if(${test_mode} STREQUAL "PASS")
    set(_fail "FALSE")
  elseif(${test_mode} STREQUAL "FAIL")
    set(_fail "TRUE")
  else()
    message(FATAL_ERROR "** Invalid test_mode passed to INSTANCE_TEST macro: ${test_mode} **")
  endif(${test_mode} STREQUAL "PASS")
  math(EXPR SI_TEST_COUNTER "${SI_TEST_COUNTER}+1")
  set(test_file "${CMAKE_CURRENT_BINARY_DIR}/${sname}_${SI_TEST_COUNTER}.stp")
  file(WRITE ${test_file} "${P21_FILE_HEAD}${sname}${P21_FILE_MID}
  #${SI_TEST_COUNTER}=${instance}    /* should ${test_mode} */
  ${P21_FILE_TAIL}")
  add_test(test_${sname}_${SI_TEST_COUNTER} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/p21read_sdai_${sname} ${test_file})
  set_tests_properties(test_${sname}_${SI_TEST_COUNTER} PROPERTIES DEPENDS build_cpp_sdai_${sname} LABELS exchange_file WILL_FAIL ${_fail})
endmacro(INSTANCE_TEST sname test_mode instance)


# test schema names
set(sn_insts "
#1=DRAUGHTING_PRE_DEFINED_COLOUR('green');
#2=COLOUR_RGB('',1.1E-2,1.2E-2,1.E0);
")
set(schema_name_good     "AUTOMOTIVE_DESIGN")
set(schema_name_good_asn "AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }")
set(schema_name_mismatch "DOES_NOT_MATCH")
set(p21read_ap214 "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/p21read_sdai_ap214e3")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/exch_file_good.p21      "${P21_FILE_HEAD}${schema_name_good}${P21_FILE_MID}
${sn_insts}${P21_FILE_TAIL}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/exch_file_good_asn.p21  "${P21_FILE_HEAD}${schema_name_good_asn}${P21_FILE_MID}
${sn_insts}${P21_FILE_TAIL}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/exch_file_mismatch.p21  "${P21_FILE_HEAD}${schema_name_mismatch}${P21_FILE_MID}
${sn_insts}${P21_FILE_TAIL}")

#compare the schema name in a p21 file with than in the sdai lib
add_test(test_good_schema_name      ${p21read_ap214}    ${CMAKE_CURRENT_BINARY_DIR}/exch_file_good.p21)
add_test(test_good_schema_name_asn  ${p21read_ap214}    ${CMAKE_CURRENT_BINARY_DIR}/exch_file_good_asn.p21)
add_test(test_mismatch_schema_name  ${p21read_ap214}    ${CMAKE_CURRENT_BINARY_DIR}/exch_file_mismatch.p21)
add_test(test_ignore_schema_name    ${p21read_ap214} -i ${CMAKE_CURRENT_BINARY_DIR}/exch_file_mismatch.p21)

#these test for required attrs that are missing
add_test(test_missing_and_required        ${p21read_ap214}    ${CMAKE_CURRENT_SOURCE_DIR}/missing_and_required.p21)
add_test(test_missing_and_required_strict ${p21read_ap214} -s ${CMAKE_CURRENT_SOURCE_DIR}/missing_and_required.p21)

#test acceptance of comments within p21 entity, i.e. FILE_NAME(/* name */ 'ferrari sharknose', ...);
add_test(test_p21_entity_internal_comment ${p21read_ap214}    ${CMAKE_CURRENT_SOURCE_DIR}/comments.p21)

set_tests_properties(test_good_schema_name test_good_schema_name_asn test_mismatch_schema_name
  test_ignore_schema_name test_missing_and_required test_missing_and_required_strict test_p21_entity_internal_comment
  PROPERTIES DEPENDS build_cpp_sdai_ap214e3 LABELS exchange_file)

set_tests_properties(test_mismatch_schema_name test_missing_and_required_strict PROPERTIES WILL_FAIL TRUE)

#not sure how multiple schema names should be handled for testing, by p21read, or by the schema and stepeditor libs
#add_test(multi_schema)
#rotator_asm:
# FILE_SCHEMA(('CONFIG_CONTROL_DESIGN', 'GEOMETRIC_VALIDATION_PROPERTIES_MIM','SHAPE_APPEARANCE_LAYER_MIM'));

#check for a segmentation fault with selects. the files are a subset of the 210e3 schema and of SurfaceMountFlasher.stp, respectively
#p21read will report errors with the file, but they don't matter as long as there isn't a segfault.
SCHEMA_CMLIST(${SC_SOURCE_DIR}/test/misc/select_segfault_210e3.exp)
add_test(test_select_segfault ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/p21read_sdai_select_segfault_210e3
  ${CMAKE_CURRENT_SOURCE_DIR}/select_segfault_210e3.stp)
set_tests_properties(test_select_segfault PROPERTIES DEPENDS build_cpp_sdai_select_segfault_210e3 LABELS exchange_file WILL_FAIL TRUE)

set(mid "multiple_inheritance_derived")
#schema for multiple inheritance where two attrs with same name are inherited; one is derived
SCHEMA_CMLIST(${SC_SOURCE_DIR}/test/unitary_schemas/${mid}.exp)

INSTANCE_TEST(${mid} PASS "A();"                             )
INSTANCE_TEST(${mid} FAIL "A(*);"                            )
INSTANCE_TEST(${mid} PASS "B(.BEE.);"                        )
INSTANCE_TEST(${mid} FAIL "B();"                             )
INSTANCE_TEST(${mid} PASS "AB(.BEE.);"                       )
INSTANCE_TEST(${mid} FAIL "AB(*);"                           )
INSTANCE_TEST(${mid} PASS "BA(.B.);"                         )
INSTANCE_TEST(${mid} FAIL "BA(*);"                           )
INSTANCE_TEST(${mid} PASS "AC();"                            )
INSTANCE_TEST(${mid} FAIL "AC(3.0);"                         )
INSTANCE_TEST(${mid} FAIL "AC('ac');"                        )
INSTANCE_TEST(${mid} PASS "DD(*);"                           )
INSTANCE_TEST(${mid} FAIL "DD();"                            )
INSTANCE_TEST(${mid} FAIL "DD(3);"                           )
INSTANCE_TEST(${mid} PASS "AA(4);"                           )
INSTANCE_TEST(${mid} FAIL "AA();"                            )
INSTANCE_TEST(${mid} PASS "BB(.BEE.,4.5);"                   )
INSTANCE_TEST(${mid} FAIL "BB(4.5);"                         )
INSTANCE_TEST(${mid} PASS "(A()B(.BEE.)AA(4)BB(4.5)AABB());" )
INSTANCE_TEST(${mid} FAIL "(A(*)B(.BEE.)AA(4)BB(4.5)AABB());")
INSTANCE_TEST(${mid} FAIL "AABB(.BEE.,4,4.5);"               )
INSTANCE_TEST(${mid} FAIL "AABB(4,4.5);"                     )
INSTANCE_TEST(${mid} FAIL "AABB();"                          )
INSTANCE_TEST(${mid} FAIL "AABB('z',2,.B.,1.2);"             )

#p21read reports no errors on this one; I'm not certain whether that is right or wrong.
#INSTANCE_TEST(${mid} FAIL "AABB(4,.BEE.,4.5);"               )

SCHEMA_CMLIST(${SC_SOURCE_DIR}/test/unitary_schemas/select_segfault_2.exp)
add_test( test_select_segfault_2 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/p21read_sdai_select_segfault_2
                               ${CMAKE_CURRENT_SOURCE_DIR}/select_segfault_2.stp )
set_tests_properties( test_select_segfault_2 PROPERTIES DEPENDS build_cpp_sdai_select_segfault_2 LABELS exchange_file )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
