#include <assert.h>

#include "express/expr.h"

/* core */
#include "express/hash.h"
#include "express/linklist.h"

/* non-core */
#include "express/dict.h"
#include "express/variable.h"

#include "driver.h"
#include "fff.h"

/*
 * mock globals
 */

char * EXPRESSprogram_name;
int yylineno;
int __SCOPE_search_id;

Error ERROR_warn_unsupported_lang_feat;
Error WARNING_case_skip_label;
Error ERROR_undefined_attribute;

/*
 * mock functions
 */

DEFINE_FFF_GLOBALS

FAKE_VALUE_FUNC(int, EXPRESS_fail, Express)
FAKE_VALUE_FUNC(Variable, ENTITYfind_inherited_attribute, struct Scope_ *, char *, struct Symbol_ **)
FAKE_VALUE_FUNC(Variable, ENTITYresolve_attr_ref, Entity, Symbol *, Symbol *)
FAKE_VALUE_FUNC(struct Scope_ *, ENTITYfind_inherited_entity, struct Scope_ *, char *, int)
FAKE_VOID_FUNC(EXP_resolve, Expression, Scope, Type)

void setup() {
    EXPinitialize();
    
    RESET_FAKE(EXPRESS_fail);
    RESET_FAKE(ENTITYfind_inherited_attribute);
    RESET_FAKE(ENTITYresolve_attr_ref);
    RESET_FAKE(ENTITYfind_inherited_entity);
    RESET_FAKE(EXP_resolve);
}

/* TODO: remove DICTlookup after eliminating DICT_type */
void EXP_resolve_type_handler(Expression exp, Scope cxt, Type typ) {
    (void) typ;
    Type res_typ = DICTlookup(cxt->symbol_table, exp->symbol.name); 
    exp->type = res_typ;
    exp->return_type = res_typ;
    exp->symbol.resolved = RESOLVED;
}

int test_resolve_select_enum_member() {
    Schema scope;
    Symbol *e_type_id, *enum_id, *s_type_id;
    Type enum_typ, select_typ, chk_typ;
    TypeBody tb;
    Expression expr, op1, op2, exp_enum_id;

    /*
     * boilerplate: create objects, populate symbol tables
     */
    scope = SCHEMAcreate();

    s_type_id = SYMBOLcreate("sel1", 1, "test1");
    e_type_id = SYMBOLcreate("enum1", 1, "test1");
    enum_id = SYMBOLcreate("val1", 1, "test1");
    
    enum_typ = TYPEcreate_name(e_type_id);
    enum_typ->symbol_table = DICTcreate(50);
    
    exp_enum_id = EXPcreate(enum_typ);
    exp_enum_id->symbol = *enum_id;
    exp_enum_id->u.integer = 1;
    
    tb = TYPEBODYcreate(enumeration_);
    tb->list = LISTcreate();
    LISTadd_last(tb->list, enum_id);
    enum_typ->u.type->body = tb;
    
    DICT_define(scope->symbol_table, e_type_id->name, enum_typ, &enum_typ->symbol, OBJ_TYPE);
    
    /* TODO: OBJ_ENUM / OBJ_EXPRESSION are used interchangeably, this is confusing. */
    DICT_define(scope->enum_table, exp_enum_id->symbol.name, exp_enum_id, &exp_enum_id->symbol, OBJ_EXPRESSION);
    DICT_define(enum_typ->symbol_table, enum_id->name, exp_enum_id, enum_id, OBJ_EXPRESSION);
    
    select_typ = TYPEcreate_name(s_type_id);
    tb = TYPEBODYcreate(select_);
    tb->list = LISTcreate();
    LISTadd_last(tb->list, enum_typ);
    select_typ->u.type->body = tb;
    DICT_define(scope->symbol_table, s_type_id->name, select_typ, &select_typ->symbol, OBJ_TYPE);
    
    op1 = EXPcreate_from_symbol(Type_Identifier, s_type_id);
    op2 = EXPcreate_from_symbol(Type_Identifier, enum_id);
    expr = BIN_EXPcreate(OP_DOT, op1, op2);    

    /*
     * test: sel_ref '.' enum_id
     * expectation: enum_typ
     */
    EXP_resolve_fake.custom_fake = EXP_resolve_type_handler;
    
    chk_typ = EXPresolve_op_dot(expr, scope);

    assert(EXP_resolve_fake.call_count == 1);
    assert(expr->e.op1->type == select_typ);
    assert(chk_typ == enum_typ);
    
    /* in case of error SIGABRT will be raised (and non-zero returned) */
    
    return 0;
}

/* TODO: remove DICTlookup after eliminating DICT_type */
void EXP_resolve_entity_handler(Expression exp, Scope cxt, Type unused) {
    (void) unused;
    Entity ent = DICTlookup(cxt->symbol_table, exp->symbol.name);
    Type typ = ent->u.entity->type;
    exp->type = typ;
    exp->return_type = typ;
    exp->symbol.resolved = RESOLVED;
}

Variable ENTITY_resolve_attr_handler(Entity ent, Symbol *grp_ref, Symbol *attr_ref) {
    (void) grp_ref;
    Variable v = DICTlookup(ent->symbol_table, attr_ref->name);
    return v;   
}

int test_resolve_entity_attribute() {
    Schema scope;
    Symbol *e_type_id, *attr_id;
    Entity ent;
    Type attr_typ, chk_typ;
    TypeBody tb;
    Expression exp_attr, expr, op1, op2;
    Variable var_attr;
    Linked_List explicit_attr_list;

    /*
     * boilerplate: create objects, populate symbol tables
     */
    scope = SCHEMAcreate();

    e_type_id = SYMBOLcreate("entity1", 1, "test2");
    ent = ENTITYcreate(e_type_id);
    DICT_define(scope->symbol_table, e_type_id->name, ent, &ent->symbol, OBJ_ENTITY);

    attr_id = SYMBOLcreate("attr1", 1, "test2");
    exp_attr = EXPcreate_from_symbol(Type_Attribute, attr_id);    
    tb = TYPEBODYcreate(number_);
    attr_typ = TYPEcreate_from_body_anonymously(tb);
    attr_typ->superscope = ent;
    var_attr = VARcreate(exp_attr, attr_typ);
    var_attr->flags.attribute = 1;
    explicit_attr_list = LISTcreate();
    LISTadd_last(explicit_attr_list, var_attr);
    
    LISTadd_last(ent->u.entity->attributes, explicit_attr_list);
    DICTdefine(ent->symbol_table, attr_id->name, var_attr, &var_attr->name->symbol, OBJ_VARIABLE);

    op1 = EXPcreate_from_symbol(Type_Identifier, e_type_id);
    op2 = EXPcreate_from_symbol(Type_Attribute, attr_id);
    expr = BIN_EXPcreate(OP_DOT, op1, op2);
    
    /*
     * test: entity_ref '.' attribute_id
     * expectation: attr_typ
     */
    EXP_resolve_fake.custom_fake = EXP_resolve_entity_handler;
    ENTITYresolve_attr_ref_fake.custom_fake = ENTITY_resolve_attr_handler;
    
    chk_typ = EXPresolve_op_dot(expr, scope);

    assert(EXP_resolve_fake.call_count == 1);
    assert(expr->e.op1->type == ent->u.entity->type);
    assert(chk_typ == attr_typ);
    
    /* in case of error SIGABRT will be raised (and non-zero returned) */
    
    return 0;
}

struct test_def tests[] = {
    {"resolve_select_enum_member", test_resolve_select_enum_member},
    {"resolve_entity_attribute", test_resolve_entity_attribute},
    {NULL}
};
