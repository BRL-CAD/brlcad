#include "express/memory.h"

#include "express/alloc.h"

#include "express/hash.h"
#include "express/symbol.h"
#include "express/schema.h"
#include "express/type.h"

struct freelist_head HASH_Table_fl;
struct freelist_head HASH_Element_fl;

struct freelist_head LINK_fl;
struct freelist_head LIST_fl;

struct freelist_head ERROR_OPT_fl;

struct freelist_head SYMBOL_fl;

struct freelist_head SCOPE_fl;
struct freelist_head SCHEMA_fl;
struct freelist_head REN_fl;

struct freelist_head TYPEHEAD_fl;
struct freelist_head TYPEBODY_fl;

struct freelist_head VAR_fl;

struct freelist_head FUNC_fl;
struct freelist_head RULE_fl;
struct freelist_head PROC_fl;
struct freelist_head WHERE_fl;

struct freelist_head ENTITY_fl;

struct freelist_head CASE_IT_fl;

struct freelist_head EXP_fl;
struct freelist_head OP_fl;
struct freelist_head QUERY_fl;
struct freelist_head QUAL_ATTR_fl;

struct freelist_head STMT_fl;
struct freelist_head ALIAS_fl;
struct freelist_head ASSIGN_fl;
struct freelist_head CASE_fl;
struct freelist_head COMP_STMT_fl;
struct freelist_head COND_fl;
struct freelist_head LOOP_fl;
struct freelist_head PCALL_fl;
struct freelist_head RET_fl;
struct freelist_head INCR_fl;

void MEMORYinitialize()
{
    _ALLOCinitialize();

    ALLOCinitialize(&HASH_Table_fl, sizeof(struct Hash_Table_), 50, 50);
    ALLOCinitialize(&HASH_Element_fl, sizeof(struct Element_), 500, 100);

    ALLOCinitialize(&LINK_fl, sizeof(struct Link_), 500, 100);
    ALLOCinitialize(&LIST_fl, sizeof(struct Linked_List_), 100, 50);

    ALLOCinitialize(&SYMBOL_fl, sizeof(struct Symbol_), 100, 100);

    ALLOCinitialize(&SCOPE_fl, sizeof(struct Scope_), 100, 50);

    ALLOCinitialize(&TYPEHEAD_fl, sizeof(struct TypeHead_), 500, 100);
    ALLOCinitialize(&TYPEBODY_fl, sizeof(struct TypeBody_), 200, 100);

    ALLOCinitialize(&VAR_fl, sizeof(struct Variable_), 100, 50);

    ALLOCinitialize(&FUNC_fl, sizeof(struct Function_),  100, 50);
    ALLOCinitialize(&RULE_fl, sizeof(struct Rule_),      100, 50);
    ALLOCinitialize(&PROC_fl, sizeof(struct Procedure_), 100, 50);
    ALLOCinitialize(&WHERE_fl, sizeof(struct Where_),    100, 50);

    ALLOCinitialize(&ENTITY_fl, sizeof(struct Entity_), 500, 100);

    ALLOCinitialize(&SCHEMA_fl, sizeof(struct Schema_), 40, 20);
    ALLOCinitialize(&REN_fl, sizeof(struct Rename), 30, 30);

    ALLOCinitialize(&CASE_IT_fl, sizeof(struct Case_Item_), 500, 100);

    ALLOCinitialize(&EXP_fl, sizeof(struct Expression_), 500, 200);
    ALLOCinitialize(&OP_fl, sizeof(struct Op_Subexpression), 500, 100);
    ALLOCinitialize(&QUERY_fl, sizeof(struct Query_), 50, 10);
    ALLOCinitialize(&QUAL_ATTR_fl, sizeof(struct Query_), 20, 10);

    ALLOCinitialize(&STMT_fl, sizeof(struct Statement_), 500, 100);

    ALLOCinitialize(&ALIAS_fl, sizeof(struct Alias_), 10, 10);
    ALLOCinitialize(&ASSIGN_fl, sizeof(struct Assignment_), 100, 30);
    ALLOCinitialize(&CASE_fl, sizeof(struct Case_Statement_), 100, 30);
    ALLOCinitialize(&COMP_STMT_fl, sizeof(struct Compound_Statement_), 100, 30);
    ALLOCinitialize(&COND_fl, sizeof(struct Conditional_), 100, 30);
    ALLOCinitialize(&LOOP_fl, sizeof(struct Loop_), 100, 30);
    ALLOCinitialize(&PCALL_fl, sizeof(struct Procedure_Call_), 100, 30);
    ALLOCinitialize(&RET_fl, sizeof(struct Return_Statement_), 100, 30);
    ALLOCinitialize(&INCR_fl, sizeof(struct Increment_), 100, 30);

}

