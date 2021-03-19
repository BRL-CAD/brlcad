include_directories(..)

if(SC_GENERATE_LP_SOURCES)
  include_directories("${PERPLEX_ExpScanner_INCLUDE_DIR}" "${LEMON_ExpParser_INCLUDE_DIR}")
endif(SC_GENERATE_LP_SOURCES)

add_executable(test_expr driver.c test_expr.c)
target_link_libraries(test_expr express)
add_test(NAME exp_resolve_select_enum_member COMMAND test_expr resolve_select_enum_member)
add_test(NAME exp_resolve_entity_attribute COMMAND test_expr resolve_entity_attribute)

add_executable(test_express driver.c test_express.c)
target_link_libraries(test_express express)
add_test(NAME express_rename_resolve COMMAND test_express express_rename_resolve)

add_executable(test_resolve driver.c test_resolve.c)
target_link_libraries(test_resolve express)

add_test(NAME exp_resolve_bad_func_call COMMAND test_resolve exp_resolve_bad_func_call)
add_test(NAME exp_resolve_func_call COMMAND test_resolve exp_resolve_func_call)
add_test(NAME exp_resolve_local_identifier COMMAND test_resolve exp_resolve_local_identifier)
add_test(NAME entity_resolve_subtype_expr_entity COMMAND test_resolve entity_resolve_subtype_expr_entity)
add_test(NAME type_resolve_entity COMMAND test_resolve type_resolve_entity)
add_test(NAME stmt_resolve_pcall_proc COMMAND test_resolve stmt_resolve_pcall_proc)
add_test(NAME scope_resolve_named_types COMMAND test_resolve scope_resolve_named_types)
add_test(NAME entity_resolve_supertypes_entity COMMAND test_resolve entity_resolve_supertypes_entity)

add_executable(test_resolve2 driver.c test_resolve2.c)
target_link_libraries(test_resolve2 express)
add_test(NAME scope_resolve_expr_stmt COMMAND test_resolve2 scope_resolve_expr_stmt)
add_test(NAME scope_resolve_subsupers COMMAND test_resolve2 scope_resolve_subsupers)

add_executable(test_schema driver.c test_schema.c)
target_link_libraries(test_schema express)
add_test(NAME schema_define_ref COMMAND test_schema schema_define_ref)
add_test(NAME schema_define_use COMMAND test_schema schema_define_use)
add_test(NAME schema_get_entities_ref COMMAND test_schema schema_get_entities_ref)
add_test(NAME var_find COMMAND test_schema var_find)

add_executable(test_scope driver.c test_scope.c)
target_link_libraries(test_scope express)
add_test(NAME scope_find COMMAND test_scope scope_find)

add_executable(test_type driver.c test_type.c)
target_link_libraries(test_type express)
add_test(NAME type_create_user_defined_tag COMMAND test_type type_create_user_defined_tag)

add_test(NAME build_check_express
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} --build .
  --target check-express
  --config $<CONFIGURATION>
  )

add_test(NAME test_plib_parse_err
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMAND $<TARGET_FILE:check-express> "${CMAKE_CURRENT_SOURCE_DIR}/plib_parse_err.exp"
  )

set_tests_properties( test_plib_parse_err PROPERTIES DEPENDS "build_check_express;$<TARGET_NAME:check-express>" )
set_tests_properties( test_plib_parse_err build_check_express PROPERTIES LABELS parser )

sc_addexec(print_schemas SOURCES ../fedex.c print_schemas.c LINK_LIBRARIES express base)
sc_addexec(print_attrs SOURCES ../fedex.c print_attrs.c LINK_LIBRARIES express base)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

