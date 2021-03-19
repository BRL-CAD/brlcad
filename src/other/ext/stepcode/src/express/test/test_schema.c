#include <assert.h>

#include "express/schema.h"

/* core */
#include "express/hash.h"
#include "express/linklist.h"


/* non-core */
#include "express/variable.h"
#include "express/scope.h"

#include "driver.h"
#include "fff.h"

/*
 * mock globals
 */
char *EXPRESSprogram_name;
int yylineno;

int ENTITY_MARK;


/*
 * mock functions
 */

DEFINE_FFF_GLOBALS

FAKE_VALUE_FUNC(int, EXPRESS_fail, Express)
FAKE_VOID_FUNC(SCOPE_get_entities, Scope, Linked_List)
FAKE_VALUE_FUNC(Variable, ENTITYfind_inherited_attribute, struct Scope_ *, char *, struct Symbol_ **)

void setup()
{
    RESET_FAKE(EXPRESS_fail)
    RESET_FAKE(SCOPE_get_entities)
    RESET_FAKE(ENTITYfind_inherited_attribute)
}

int test_schema_define_ref()
{
    Schema cur_schema, ref_schema;
    Rename *ref_rename;
    Symbol *cur_schema_id, *ent_ref, *ref_schema_id;

    cur_schema_id = SYMBOLcreate("cur_schema", 1, "cur.exp");
    ent_ref = SYMBOLcreate("line", 1, "ref.exp");
    ref_schema_id = SYMBOLcreate("ref_schema", 1, "ref.exp");

    cur_schema = SCHEMAcreate();
    cur_schema->symbol = *cur_schema_id;
    cur_schema->u.schema->refdict = DICTcreate(20);

    ref_schema = SCHEMAcreate();
    ref_schema->symbol = *ref_schema_id;

    ref_rename = REN_new();
    ref_rename->schema_sym = ref_schema_id;
    ref_rename->old = ent_ref;
    ref_rename->nnew = ent_ref;
    ref_rename->rename_type = ref;
    ref_rename->schema = ref_schema;
    DICTdefine(cur_schema->u.schema->refdict, ent_ref->name, ref_rename, ent_ref, OBJ_RENAME);

    SCHEMAdefine_reference(cur_schema, ref_rename);

    assert(cur_schema->u.schema->refdict->KeyCount == 1);

    return 0;
}

int test_schema_define_use()
{
    Schema cur_schema, ref_schema;
    Rename *use_rename;
    Symbol *cur_schema_id, *ent_ref, *ref_schema_id;

    cur_schema_id = SYMBOLcreate("cur_schema", 1, "cur.exp");
    ent_ref = SYMBOLcreate("line", 1, "ref.exp");
    ref_schema_id = SYMBOLcreate("ref_schema", 1, "ref.exp");

    cur_schema = SCHEMAcreate();
    cur_schema->symbol = *cur_schema_id;
    cur_schema->u.schema->usedict = DICTcreate(20);

    ref_schema = SCHEMAcreate();
    ref_schema->symbol = *ref_schema_id;

    use_rename = REN_new();
    use_rename->schema_sym = ref_schema_id;
    use_rename->old = ent_ref;
    use_rename->nnew = ent_ref;
    use_rename->rename_type = use;
    use_rename->schema = ref_schema;
    DICTdefine(cur_schema->u.schema->usedict, ent_ref->name, use_rename, ent_ref, OBJ_RENAME);

    SCHEMAdefine_use(cur_schema, use_rename);

    assert(cur_schema->u.schema->usedict->KeyCount == 1);

    return 0;
}

/* TODO:
 * currently this function expects OBJ_RENAME stored as OBJ_ENTITY
 * (to indicate partial reference)
 */
int test_schema_get_entities_ref()
{
    Schema cur_schema, ref_schema;
    Rename *ref_rename;
    Entity ent;
    Symbol *cur_schema_id, *ent_id, *ent_ref, *ref_schema_id;
    Linked_List r;

    cur_schema_id = SYMBOLcreate("cur_schema", 1, "cur.exp");
    ent_id = SYMBOLcreate("line", 1, "cur.exp");
    ent_ref = SYMBOLcreate("line", 1, "ref.exp");
    ref_schema_id = SYMBOLcreate("ref_schema", 1, "ref.exp");

    cur_schema = SCHEMAcreate();
    cur_schema->symbol = *cur_schema_id;
    cur_schema->u.schema->refdict = DICTcreate(20);

    ref_schema = SCHEMAcreate();
    ref_schema->symbol = *ref_schema_id;

    ent = ENTITYcreate(ent_id);
    DICTdefine(ref_schema->symbol_table, ent_id->name, ent, ent_id, OBJ_ENTITY);

    ref_rename = REN_new();
    ref_rename->schema_sym = ref_schema_id;
    ref_rename->old = ent_ref;
    ref_rename->nnew = ent_ref;
    ref_rename->rename_type = ref;
    ref_rename->schema = ref_schema;
    ref_rename->object = ent;
    DICTdefine(cur_schema->u.schema->refdict, ent_ref->name, ref_rename, ent_ref, OBJ_ENTITY);

    r = LISTcreate();
    cur_schema->search_id = -1;
    SCHEMA_get_entities_ref(cur_schema, r);

    assert(LISTget_length(r) == 1);

    return 0;
}

Variable
ENTITY_find_attr_handler(struct Scope_ *entity, char *name, struct Symbol_ **down_sym)
{
    Variable r;
    (void) down_sym;
    r = DICTlookup(entity->symbol_table, name);
    return r;
}

int test_var_find()
{
    Schema scope;
    Symbol *e_type_id, *attr_id;
    Entity ent;
    Type attr_typ;
    TypeBody tb;
    Expression exp_attr;
    Variable var_attr, var_ref;
    Linked_List explicit_attr_list;

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

    ENTITYfind_inherited_attribute_fake.custom_fake = ENTITY_find_attr_handler;

    var_ref = VARfind(ent, "attr1", 1);

    assert(var_ref != NULL);

    return 0;
}

struct test_def tests[] = {
    {"schema_define_ref", test_schema_define_ref},
    {"schema_define_use", test_schema_define_use},
    {"schema_get_entities_ref", test_schema_get_entities_ref},
    {"var_find", test_var_find},
    {NULL}
};
