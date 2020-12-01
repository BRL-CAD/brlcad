

/**
** Module:  Entity \file entity.c
*
** This module is used to represent Express entity definitions.
**  An entity definition consists of a name, a set of attributes, a
**  collection of uniqueness sets, a specification of the entity's
**  position in a class hierarchy (i.e., sub- and supertypes), and
**  a list of constraints which must be satisfied by instances of
**  the entity.  A uniquess set is a set of one or more attributes
**  which, when taken together, uniquely identify a specific instance
**  of the entity.  Thus, the uniqueness set { name, ssn } indicates
**  that no two instances of the entity may share both the same
**  values for both name and ssn.
*
** Constants:
**  ENTITY_NULL - the null entity
**
************************************************************************/

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: entity.c,v $
 * Revision 1.11  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.10  1995/03/09  18:43:03  clark
 * various fixes for caddetc - before interface clause changes
 *
 * Revision 1.9  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.8  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.6  1993/02/16  03:14:47  libes
 * simplified find calls
 *
 * Revision 1.5  1993/01/19  22:45:07  libes
 * *** empty log message ***
 *
 * Revision 1.4  1992/08/18  17:13:43  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:06:57  libes
 * prettied up interface to print_objects_when_running
 *
 * Revision 1.2  1992/05/31  08:35:51  libes
 * multiple files
 *
 * Revision 1.1  1992/05/28  03:55:04  libes
 * Initial revision
 *
 * Revision 1.7  1992/05/10  06:01:29  libes
 * cleaned up OBJget_symbol
 *
 * Revision 1.6  1992/05/05  19:51:47  libes
 * final alpha
 *
 * Revision 1.5  1992/02/19  15:48:18  libes
 * changed types to enums & flags
 *
 * Revision 1.4  1992/02/17  14:32:57  libes
 * lazy ref/use evaluation now working
 *
 * Revision 1.3  1992/02/12  07:05:23  libes
 * y
 * do sub/supertype
 *
 * Revision 1.2  1992/02/09  00:49:53  libes
 * does ref/use correctly
 *
 * Revision 1.1  1992/02/05  08:34:24  libes
 * Initial revision
 *
 * Revision 1.0.1.1  1992/01/22  02:47:57  libes
 * copied from ~pdes
 *
 * Revision 4.7  1991/09/20  06:20:43  libes
 * fixed bug that printed out entity->attributes incorrectly
 * assumed they were objects rather than strings
 *
 * Revision 4.6  1991/09/16  23:13:12  libes
 * added print functionsalgorithm.c
 *
 * Revision 4.5  1991/07/18  05:07:08  libes
 * added SCOPEget_use
 *
 * Revision 4.4  1991/07/13  05:04:17  libes
 * added ENTITYget_supertype and ..get_subtype
 *
 * Revision 4.3  1991/01/24  22:20:36  silver
 * merged changes from NIST and SCRA
 * SCRA changes are due to DEC ANSI C compiler tests.
 *
 * Revision 4.3  91/01/08  18:55:42  pdesadmn
 * Initial - Beta checkin at SCRA
 *
 * Revision 4.2  90/11/23  16:33:43  clark
 * Initial checkin at SCRA
 *
 * Revision 4.2  90/11/23  16:33:43  clark
 * initial checkin at SCRA
 *
 * Revision 4.2  90/11/23  16:33:43  clark
 * Fixes for better error handling on supertype lists
 *
 * Revision 4.1  90/09/13  15:13:08  clark
 * BPR 2.1 alpha
 *
 */

#include "express/entity.h"
#include "express/express.h"
#include "express/object.h"

int ENTITY_MARK = 0;

/** returns true if variable is declared (or redeclared) directly by entity */
int ENTITYdeclares_variable( Entity e, Variable v ) {
    LISTdo( e->u.entity->attributes, attr, Variable )
    if( attr == v ) {
        return true;
    }
    LISTod;

    return false;
}

static Entity ENTITY_find_inherited_entity( Entity entity, char * name, int down ) {
    Entity result;

    /* avoid searching scopes that we've already searched */
    /* this can happen due to several things */
    /* if A ref's B which ref's C, and A ref's C.  Then C */
    /* can be searched twice by A.  Similar problem with */
    /* sub/super inheritance. */
    if( entity->search_id == __SCOPE_search_id ) {
        return NULL;
    }
    entity->search_id = __SCOPE_search_id;

    LISTdo( entity->u.entity->supertypes, super, Entity )
    if( !strcmp( super->symbol.name, name ) ) {
        return super;
    }
    LISTod

    LISTdo( entity->u.entity->supertypes, super, Entity )
    result = ENTITY_find_inherited_entity( super, name, down );
    if( result ) {
        return result;
    }
    LISTod;

    if( down ) {
        LISTdo( entity->u.entity->subtypes, sub, Entity )
        if( !strcmp( sub->symbol.name, name ) ) {
            return sub;
        }
        LISTod;

        LISTdo( entity->u.entity->subtypes, sub, Entity )
        result = ENTITY_find_inherited_entity( sub, name, down );
        if( result ) {
            return result;
        }
        LISTod;
    }

    return 0;
}

struct Scope_ * ENTITYfind_inherited_entity( struct Scope_ *entity, char * name, int down ) {
    if( !strcmp( name, entity->symbol.name ) ) {
        return( entity );
    }

    __SCOPE_search_id++;
    return ENTITY_find_inherited_entity( entity, name, down );
}

/** find a (possibly inherited) attribute */
Variable ENTITY_find_inherited_attribute( Entity entity, char * name, int * down, struct Symbol_ ** where ) {
    Variable result;

    /* avoid searching scopes that we've already searched */
    /* this can happen due to several things */
    /* if A ref's B which ref's C, and A ref's C.  Then C */
    /* can be searched twice by A.  Similar problem with */
    /* sub/super inheritance. */
    if( entity->search_id == __SCOPE_search_id ) {
        return NULL;
    }
    entity->search_id = __SCOPE_search_id;

    /* first look locally */
    result = ( Variable )DICTlookup( entity->symbol_table, name );
    if( result ) {
        if( down && *down && where ) {
            *where = &entity->symbol;
        }
        return result;
    }

    /* check supertypes */
    LISTdo( entity->u.entity->supertypes, super, Entity )
    result = ENTITY_find_inherited_attribute( super, name, down, where );
    if( result ) {
        return result;
    }
    LISTod;

    /* check subtypes, if requested */
    if( down ) {
        ++*down;
        LISTdo( entity->u.entity->subtypes, sub, Entity )
        result = ENTITY_find_inherited_attribute( sub, name, down, where );
        if( result ) {
            return result;
        }
        LISTod;
        --*down;
    }

    return 0;
}

Variable ENTITYfind_inherited_attribute( struct Scope_ *entity, char * name,
        struct Symbol_ ** down_sym ) {
    extern int __SCOPE_search_id;
    int down_flag = 0;

    __SCOPE_search_id++;
    if( down_sym ) {
        return ENTITY_find_inherited_attribute( entity, name, &down_flag, down_sym );
    } else {
        return ENTITY_find_inherited_attribute( entity, name, 0, 0 );
    }
}

/** resolve a (possibly group-qualified) attribute ref.
 * report errors as appropriate
 */
Variable ENTITYresolve_attr_ref( Entity e, Symbol * grp_ref, Symbol * attr_ref ) {
    Entity ref_entity;
    Variable attr;
    struct Symbol_ *where;

    if( grp_ref ) {
        /* use entity provided in group reference */
        ref_entity = ENTITYfind_inherited_entity( e, grp_ref->name, 0 );
        if( !ref_entity ) {
            ERRORreport_with_symbol(UNKNOWN_SUPERTYPE, grp_ref, grp_ref->name, e->symbol.name );
            return 0;
        }
        attr = ( Variable )DICTlookup( ref_entity->symbol_table,
                                       attr_ref->name );
        if( !attr ) {
            ERRORreport_with_symbol(UNKNOWN_ATTR_IN_ENTITY, attr_ref, attr_ref->name,
                                     ref_entity->symbol.name );
            /*      resolve_failed(e);*/
        }
    } else {
        /* no entity provided, look through supertype chain */
        where = NULL;
        attr = ENTITYfind_inherited_attribute( e, attr_ref->name, &where );
        if( !attr /* was ref_entity? */ ) {
            ERRORreport_with_symbol(UNKNOWN_ATTR_IN_ENTITY,
                                     attr_ref, attr_ref->name,
                                     e->symbol.name );
        } else if( where != NULL ) {
            ERRORreport_with_symbol(IMPLICIT_DOWNCAST, attr_ref,
                                     where->name );
        }
    }
    return attr;
}

/**
 * currently, this is only used by USEresolve
 * low-level function for type Entity
 */
Entity ENTITYcopy( Entity e ) {
    /* for now, do a totally shallow copy */
    Entity e2 = SCOPE_new();
    *e2 = *e;
    return e2;
}

/** Initialize the Entity module. */
void ENTITYinitialize() {
}

/**
** \param entity entity to modify
** \param attr attribute to add
** Add an attribute to an entity.
*/
void ENTITYadd_attribute( Entity entity, Variable attr ) {
    int rc;

    if( attr->name->type->u.type->body->type != op_ ) {
        /* simple id */
        rc = DICTdefine( entity->symbol_table, attr->name->symbol.name,
                         attr, &attr->name->symbol, OBJ_VARIABLE );
    } else {
        /* SELF\ENTITY.SIMPLE_ID */
        rc = DICTdefine( entity->symbol_table, attr->name->e.op2->symbol.name,
                         attr, &attr->name->symbol, OBJ_VARIABLE );
    }
    if( rc == 0 ) {
        LISTadd_last( entity->u.entity->attributes, attr );
        VARput_offset( attr, entity->u.entity->attribute_count );
        entity->u.entity->attribute_count++;
    }
}

/**
** \param entity entity to modify
** \param instance new instance
** Add an item to the instance list of an entity.
*/
void ENTITYadd_instance( Entity entity, void *instance ) {
    if( entity->u.entity->instances == LIST_NULL ) {
        entity->u.entity->instances = LISTcreate();
    }
    LISTadd_last( entity->u.entity->instances, instance );
}

/**
** \param child entity to check parentage of
** \param parent parent to check for
** \return does child's superclass chain include parent?
** Look for a certain entity in the supertype graph of an entity.
*/
bool ENTITYhas_supertype( Entity child, Entity parent ) {
    LISTdo( child->u.entity->supertypes, entity, Entity )
    if( entity == parent ) {
        return true;
    }
    if( ENTITYhas_supertype( entity, parent ) ) {
        return true;
    }
    LISTod;
    return false;
}

/**
** \param child entity to check parentage of
** \param parent parent to check for
** \return is parent a direct supertype of child?
** Check whether an entity has a specific immediate superclass.
*/
bool ENTITYhas_immediate_supertype( Entity child, Entity parent ) {
    LISTdo( child->u.entity->supertypes, entity, Entity )
    if( entity == parent ) {
        return true;
    }
    LISTod;
    return false;
}

/** called by ENTITYget_all_attributes(). \sa ENTITYget_all_attributes */
static void ENTITY_get_all_attributes( Entity entity, Linked_List result ) {
    LISTdo( entity->u.entity->supertypes, super, Entity )
    /*  if (OBJis_kind_of(super, Class_Entity))*/
    ENTITY_get_all_attributes( super, result );
    LISTod;
    /* Gee, aren't they resolved by this time? */
    LISTdo( entity->u.entity->attributes, attr, void * )
    LISTadd_last( result, attr );
    LISTod;
}

/**
 ** \param entity  - entity to examine
 ** \return all attributes of this entity
 **
 ** Retrieve the attribute list of an entity.
 ** \sa ENTITY_get_all_attributes()
 **
 ** \note   If an entity has neither defines nor inherits any
 **      attributes, this call returns an empty list.  Note
 **      that this is distinct from the constant LIST_NULL.
 */
Linked_List ENTITYget_all_attributes( Entity entity ) {
    Linked_List result = LISTcreate();

    ENTITY_get_all_attributes( entity, result );
    return result;
}

/**
** \param entity  - entity to examine
** \param  name    - name of attribute to retrieve
** \return the named attribute of this entity
** Retrieve an entity attribute by name.
**
** \note   If the entity has no attribute with the given name,
**  VARIABLE_NULL is returned.
*/
Variable ENTITYget_named_attribute( Entity entity, char * name ) {
    Variable attribute;

    LISTdo( entity->u.entity->attributes, attr, Variable )
    if( !strcmp( VARget_simple_name( attr ), name ) ) {
        return attr;
    }
    LISTod;

    LISTdo( entity->u.entity->supertypes, super, Entity )
    /*  if (OBJis_kind_of(super, Class_Entity) && */
    if( 0 != ( attribute = ENTITYget_named_attribute( super, name ) ) ) {
        return attribute;
    }
    LISTod;
    return 0;
}

/**
** \param entity entity to examine
** \param attribute attribute to retrieve offset for
** \return offset to given attribute
** Retrieve offset to an entity attribute.
**
** \note   If the entity does not include the attribute, -1
**  is returned.
*/
int ENTITYget_attribute_offset( Entity entity, Variable attribute ) {
    int offset;
    int value;

    LISTdo( entity->u.entity->attributes, attr, Variable )
    if( attr == attribute ) {
        return entity->u.entity->inheritance + VARget_offset( attribute );
    }
    LISTod;
    offset = 0;
    LISTdo( entity->u.entity->supertypes, super, Entity )
    /*  if (OBJis_kind_of(super, Class_Entity)) {*/
    if( ( value = ENTITYget_attribute_offset( super, attribute ) ) != -1 ) {
        return value + offset;
    }
    offset += ENTITYget_initial_offset( super );
    /*  }*/
    LISTod;
    return -1;
}

/**
** \param entity entity to examine
** \param name name of attribute to retrieve
** \return offset to named attribute of this entity
** Retrieve offset to an entity attribute by name.
**
** \note   If the entity has no attribute with the given name,
**      -1 is returned.
*/
int ENTITYget_named_attribute_offset( Entity entity, char * name ) {
    int offset;
    int value;

    LISTdo( entity->u.entity->attributes, attr, Variable )
    if( !strcmp( VARget_simple_name( attr ), name ) )
        return entity->u.entity->inheritance +
               VARget_offset( ENTITY_find_inherited_attribute( entity, name, 0, 0 ) );
    LISTod;
    offset = 0;
    LISTdo( entity->u.entity->supertypes, super, Entity )
    /*  if (OBJis_kind_of(super, Class_Entity)) {*/
    if( ( value = ENTITYget_named_attribute_offset( super, name ) ) != -1 ) {
        return value + offset;
    }
    offset += ENTITYget_initial_offset( super );
    /*  }*/
    LISTod;
    return -1;
}

/**
** \param entity entity to examine
** \return number of inherited attributes
** Retrieve the initial offset to an entity's local frame.
*/
int ENTITYget_initial_offset( Entity entity ) {
    return entity->u.entity->inheritance;
}
