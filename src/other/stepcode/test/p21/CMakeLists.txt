CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

#test part 21 files
#currently, the only tests are for the schema name

set( schema_name_tests_head "
ISO-10303-21;
HEADER;
FILE_DESCRIPTION(('SCL test file'),'2;1');
FILE_NAME('test.stp','2011-11-29T',('mp'),(''),'0','1','2');
")
set( schema_name_tests_tail "
ENDSEC;
DATA;
#1=DRAUGHTING_PRE_DEFINED_COLOUR('green');
#2=COLOUR_RGB('',1.1E-2,1.2E-2,1.E0);
ENDSEC;
END-ISO-10303-21;
")

set( schema_name_good     "FILE_SCHEMA(('AUTOMOTIVE_DESIGN'));" )
set( schema_name_good_asn "FILE_SCHEMA(('AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }'));" )
set( schema_name_mismatch "FILE_SCHEMA(('DOES_NOT_MATCH'));" )

set( p21read_ap214 "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/p21read_sdai_AP214E3_2010" )

file( WRITE ${CMAKE_CURRENT_BINARY_DIR}/exch_file_good.p21      "${schema_name_tests_head}${schema_name_good}${schema_name_tests_tail}" )
file( WRITE ${CMAKE_CURRENT_BINARY_DIR}/exch_file_good_asn.p21  "${schema_name_tests_head}${schema_name_good_asn}${schema_name_tests_tail}" )
file( WRITE ${CMAKE_CURRENT_BINARY_DIR}/exch_file_mismatch.p21  "${schema_name_tests_head}${schema_name_mismatch}${schema_name_tests_tail}" )

#these compare the schema name in a p21 file with than in the sdai lib
add_test( test_good_schema_name      ${p21read_ap214}    ${CMAKE_CURRENT_BINARY_DIR}/exch_file_good.p21 )
add_test( test_good_schema_name_asn  ${p21read_ap214}    ${CMAKE_CURRENT_BINARY_DIR}/exch_file_good_asn.p21 )
add_test( test_mismatch_schema_name  ${p21read_ap214}    ${CMAKE_CURRENT_BINARY_DIR}/exch_file_mismatch.p21 )
add_test( test_ignore_schema_name    ${p21read_ap214} -i ${CMAKE_CURRENT_BINARY_DIR}/exch_file_mismatch.p21 )

#these test for required attrs that are missing
add_test( test_missing_and_required        ${p21read_ap214}    ${CMAKE_CURRENT_SOURCE_DIR}/missing_and_required.p21 )
add_test( test_missing_and_required_strict ${p21read_ap214} -s ${CMAKE_CURRENT_SOURCE_DIR}/missing_and_required.p21 )

#test acceptance of comments within p21 entity, i.e. FILE_NAME( /* name */ 'ferrari sharknose', ...);
add_test( test_p21_entity_internal_comment ${p21read_ap214}    ${CMAKE_CURRENT_SOURCE_DIR}/comments.p21 )


#not sure how multiple schema names should be handled for testing, by p21read, or by the schema and stepeditor libs
#add_test( multi_schema )
#rotator_asm:
# FILE_SCHEMA(('CONFIG_CONTROL_DESIGN', 'GEOMETRIC_VALIDATION_PROPERTIES_MIM','SHAPE_APPEARANCE_LAYER_MIM'));

set_tests_properties( test_good_schema_name test_good_schema_name_asn test_mismatch_schema_name
                      test_ignore_schema_name test_missing_and_required test_missing_and_required_strict test_p21_entity_internal_comment
                      PROPERTIES DEPENDS build_cpp_sdai_AP214E3_2010 LABELS exchange_file )

set_tests_properties( test_mismatch_schema_name test_missing_and_required_strict PROPERTIES WILL_FAIL TRUE )
