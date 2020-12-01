include_directories(..)

if(SC_GENERATE_LP_SOURCES)
  include_directories("${PERPLEX_ExpScanner_INCLUDE_DIR}" "${LEMON_ExpParser_INCLUDE_DIR}")
endif(SC_GENERATE_LP_SOURCES)

set(EXPRESS_CORE_OBJ
    # base
    $<TARGET_OBJECTS:objlib_alloc_c>
    $<TARGET_OBJECTS:objlib_memory_c>
    $<TARGET_OBJECTS:objlib_hash_c>
    $<TARGET_OBJECTS:objlib_linklist_c>

    # global tables
    $<TARGET_OBJECTS:objlib_object_c>
    $<TARGET_OBJECTS:objlib_error_c>

    # AST creation
    $<TARGET_OBJECTS:objlib_factory_c>
    $<TARGET_OBJECTS:objlib_stmt_c>
    $<TARGET_OBJECTS:objlib_alg_c>
    
    # deprecated
    $<TARGET_OBJECTS:objlib_dict_c>
    $<TARGET_OBJECTS:objlib_info_c>
)

add_executable(test_expr driver.c test_expr.c $<TARGET_OBJECTS:objlib_expr_c> ${EXPRESS_CORE_OBJ})
add_test(NAME exp_resolve_select_enum_member COMMAND test_expr resolve_select_enum_member)
add_test(NAME exp_resolve_entity_attribute COMMAND test_expr resolve_entity_attribute)

add_executable(test_express driver.c test_express.c $<TARGET_OBJECTS:objlib_express_c> ${EXPRESS_CORE_OBJ})
add_test(NAME express_rename_resolve COMMAND test_express express_rename_resolve)

add_executable(test_resolve driver.c test_resolve.c $<TARGET_OBJECTS:objlib_resolve_c> ${EXPRESS_CORE_OBJ})

add_test(NAME exp_resolve_bad_func_call COMMAND test_resolve exp_resolve_bad_func_call)
add_test(NAME exp_resolve_func_call COMMAND test_resolve exp_resolve_func_call)
add_test(NAME exp_resolve_local_identifier COMMAND test_resolve exp_resolve_local_identifier)
add_test(NAME entity_resolve_subtype_expr_entity COMMAND test_resolve entity_resolve_subtype_expr_entity)
add_test(NAME type_resolve_entity COMMAND test_resolve type_resolve_entity)
add_test(NAME stmt_resolve_pcall_proc COMMAND test_resolve stmt_resolve_pcall_proc)
add_test(NAME scope_resolve_named_types COMMAND test_resolve scope_resolve_named_types)
add_test(NAME entity_resolve_supertypes_entity COMMAND test_resolve entity_resolve_supertypes_entity)

add_executable(test_resolve2 driver.c test_resolve2.c $<TARGET_OBJECTS:objlib_resolve2_c> ${EXPRESS_CORE_OBJ})
add_test(NAME scope_resolve_expr_stmt COMMAND test_resolve2 scope_resolve_expr_stmt)
add_test(NAME scope_resolve_subsupers COMMAND test_resolve2 scope_resolve_subsupers)

add_executable(test_schema driver.c test_schema.c $<TARGET_OBJECTS:objlib_schema_c> ${EXPRESS_CORE_OBJ})
add_test(NAME schema_define_ref COMMAND test_schema schema_define_ref)
add_test(NAME schema_define_use COMMAND test_schema schema_define_use)
add_test(NAME schema_get_entities_ref COMMAND test_schema schema_get_entities_ref)
add_test(NAME var_find COMMAND test_schema var_find)

add_executable(test_scope driver.c test_scope.c $<TARGET_OBJECTS:objlib_scope_c> ${EXPRESS_CORE_OBJ})
add_test(NAME scope_find COMMAND test_scope scope_find)

add_executable(test_type driver.c test_type.c $<TARGET_OBJECTS:objlib_type_c> ${EXPRESS_CORE_OBJ})
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

