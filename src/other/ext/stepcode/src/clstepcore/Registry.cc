/*
* NIST STEP Core Class Library
* clstepcore/Registry.cc
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <ExpDict.h>
#include <Registry.h>
#include "sc_memmgr.h"

/* these may be shared between multiple Registry instances, so don't create/destroy in Registry ctor/dtor
 *                                                        Name, FundamentalType, Originating Schema, Description */
const TypeDescriptor *const t_sdaiINTEGER  = new TypeDescriptor("INTEGER", sdaiINTEGER, 0, "INTEGER");
const TypeDescriptor *const t_sdaiREAL     = new TypeDescriptor("REAL",    sdaiREAL,    0, "Real");
const TypeDescriptor *const t_sdaiNUMBER   = new TypeDescriptor("NUMBER",  sdaiNUMBER,  0, "Number");
const TypeDescriptor *const t_sdaiSTRING   = new TypeDescriptor("STRING",  sdaiSTRING,  0, "String");
const TypeDescriptor *const t_sdaiBINARY   = new TypeDescriptor("BINARY",  sdaiBINARY,  0, "Binary");
const TypeDescriptor *const t_sdaiBOOLEAN  = new TypeDescriptor("BOOLEAN", sdaiBOOLEAN, 0, "Boolean");
const TypeDescriptor *const t_sdaiLOGICAL  = new TypeDescriptor("LOGICAL", sdaiLOGICAL, 0, "Logical");

static int uniqueNames(const char *, const SchRename *);

Registry::Registry(CF_init initFunct)
    : col(0), entity_cnt(0), all_ents_cnt(0)
{

    primordialSwamp = SC_HASHcreate(1000);
    active_schemas = SC_HASHcreate(10);
    active_types = SC_HASHcreate(100);

    initFunct(*this);
    SC_HASHlistinit(active_types, &cur_type);
    SC_HASHlistinit(primordialSwamp, &cur_entity);   // initialize cur's
    SC_HASHlistinit(active_schemas, &cur_schema);
}

Registry::~Registry()
{
    DeleteContents();

    SC_HASHdestroy(primordialSwamp);
    SC_HASHdestroy(active_schemas);
    SC_HASHdestroy(active_types);
    delete col;
}

void Registry::DeleteContents()
{
    // entities first
    SC_HASHlistinit(primordialSwamp, &cur_entity);
    while(SC_HASHlist(&cur_entity)) {
        delete(EntityDescriptor *) cur_entity.e->data;
    }

    // schemas
    SC_HASHlistinit(active_schemas, &cur_schema);
    while(SC_HASHlist(&cur_schema)) {
        delete(Schema *) cur_schema.e->data;
    }

    // types
    SC_HASHlistinit(active_types, &cur_type);
    while(SC_HASHlist(&cur_type)) {
        delete(TypeDescriptor *) cur_type.e->data;
    }
}

/**
 * schNm refers to the current schema.  This will have a value if we are
 * reading from a Part 21 file (using a STEPfile object), and the file
 * declares a schema name in the File_Schema section of the Header.  (If
 * >1 schema names are declared, the first is taken.)  The schema name is
 * significant because of the USE and REFERENCE clause.  Say schema X USEs
 * entity A from schema Y and renames it to B, X should only refer to A as
 * B.  Thus, if schNm here = "X", only e="B" would be valid but not e="A".
 */
const EntityDescriptor *Registry::FindEntity(const char *e, const char *schNm, int check_case) const
{
    const EntityDescriptor *entd;
    const SchRename *altlist;
    char schformat[BUFSIZ], altName[BUFSIZ];

    if(check_case) {
        entd = (EntityDescriptor *)SC_HASHfind(primordialSwamp, (char *)e);
    } else {
        entd = (EntityDescriptor *)SC_HASHfind(primordialSwamp,
                                               (char *)PrettyTmpName(e));
    }
    if(entd && schNm) {
        // We've now found an entity.  If schNm has a value, we must ensure we
        // have a valid name.
        strcpy(schformat, PrettyTmpName(schNm));
        if(((altlist = entd->AltNameList()) != 0)
                && (altlist->rename(schformat, altName))) {
            // If entd has other name choices, and entd is referred to with a
            // new name by schema schNm, then e had better = the new name.
            if(!StrCmpIns(e, altName)) {
                return entd;
            }
            return NULL;
        } else if(FindSchema(schformat, 1)) {
            // If schema schNm exists but we had no conditions above to use an
            // altName, we must use the original name:
            if(!StrCmpIns(e, entd->Name())) {
                return entd;
            }
            return NULL;
        } else {
            // Last choice: schNm does not exist at all.  The user must have
            // typed something wrong.  Don't penalize him for it (so even if
            // we have an altName of entd, accept it):
            return entd;
        }
    }
    return entd;
}

const Schema *Registry::FindSchema(const char *n, int check_case) const
{
    if(check_case) {
        return (const Schema *) SC_HASHfind(active_schemas, (char *) n);
    }

    return (const Schema *) SC_HASHfind(active_schemas,
                                        (char *)PrettyTmpName(n));
}

const TypeDescriptor *Registry::FindType(const char *n, int check_case) const
{
    if(check_case) {
        return (const TypeDescriptor *) SC_HASHfind(active_types, (char *) n);
    }
    return (const TypeDescriptor *) SC_HASHfind(active_types,
            (char *)PrettyTmpName(n));
}

void Registry::ResetTypes()
{
    SC_HASHlistinit(active_types, &cur_type);
}

const TypeDescriptor *Registry::NextType()
{
    if(0 == SC_HASHlist(&cur_type)) {
        return 0;
    }
    return (const TypeDescriptor *) cur_type.e->data;
}

void Registry::AddEntity(const EntityDescriptor &e)
{
    SC_HASHinsert(primordialSwamp, (char *) e.Name(), (EntityDescriptor *) &e);
    ++entity_cnt;
    ++all_ents_cnt;
    AddClones(e);
}


void Registry::AddSchema(const Schema &d)
{
    SC_HASHinsert(active_schemas, (char *) d.Name(), (Schema *) &d);
}

void Registry::AddType(const TypeDescriptor &d)
{
    SC_HASHinsert(active_types, (char *) d.Name(), (TypeDescriptor *) &d);
}

/**
 * Purpose is to insert e into the registry hashed according to all its
 * alternate names (the names it was renamed with when other schemas USEd
 * or REFERENCEd it).  This will make these names available to the Registry
 * so that if we comes across one of them in a Part 21 file, we'll recog-
 * nize it.
 */
void Registry::AddClones(const EntityDescriptor &e)
{
    const SchRename *alts = e.AltNameList();

    while(alts) {
        SC_HASHinsert(primordialSwamp, (char *)alts->objName(),
                      (EntityDescriptor *)&e);
        alts = alts->next;
    }
    all_ents_cnt += uniqueNames(e.Name(), e.AltNameList());
}

/**
 * Returns the number of unique names in an entity's _altname list.  If
 * schema B uses ent xx from schema A and renames it to yy, and schema C
 * does the same (or if C simply uses yy from B), altlist will contain 2
 * entries with the same alt name.
 */
static int uniqueNames(const char *entnm, const SchRename *altlist)
{
    int cnt = 0;
    const SchRename *alt = altlist;

    while(alt) {
        if(!((alt->next && alt->next->choice(alt->objName()))
                || !StrCmpIns(alt->objName(), entnm))) {
            // alt has a unique alternate name if it's not reused by a later
            // alt.  alt->next->choice() returns 1 if one of the later alts
            // also has alt's name as its value.  The final condition checks
            // that our alt name is not the same as the original ent's (would
            // be the case if the Express file said "USE from A (xx as xx)",
            // which may not be legal and certainly isn't meaningful, but we
            // check for it just in case.  If none of the above conditions are
            // true, we have a unique.
            cnt++;
        }
        alt = alt->next;
    }
    return cnt;
}

void Registry::RemoveEntity(const char *n)
{
    const EntityDescriptor *e = FindEntity(n);
    struct Element tmp;

    if(e) {
        RemoveClones(*e);
    }
    tmp.key = (char *) n;
    SC_HASHsearch(primordialSwamp, &tmp, HASH_DELETE) ? --entity_cnt : 0;

}

void Registry::RemoveSchema(const char *n)
{
    struct Element tmp;
    tmp.key = (char *) n;
    SC_HASHsearch(active_schemas, &tmp, HASH_DELETE);
}

void Registry::RemoveType(const char *n)
{
    struct Element tmp;
    tmp.key = (char *) n;
    SC_HASHsearch(active_types, &tmp, HASH_DELETE);
}

/**
 * Remove all the "clones", or rename values of e.
 */
void Registry::RemoveClones(const EntityDescriptor &e)
{
    const SchRename *alts = e.AltNameList();

    while(alts) {
        struct Element *tmp = new Element;
        tmp->key = (char *) alts->objName();
        SC_HASHsearch(primordialSwamp, tmp, HASH_DELETE);
        alts = alts->next;
	delete tmp;
    }
}


SDAI_Application_instance *Registry::ObjCreate(const char *nm, const char *schnm, int check_case) const
{
    const EntityDescriptor   *entd = FindEntity(nm, schnm, check_case);
    if(entd) {
        SDAI_Application_instance *se =
            ((EntityDescriptor *)entd) -> NewSTEPentity();

        // See comment in previous function.
        if(entd->AbstractEntity().asInt() == 1) {
            se->Error().severity(SEVERITY_WARNING);
            se->Error().UserMsg("ENTITY is abstract supertype");
        } else if(entd->ExtMapping().asInt() == 1) {
            se->Error().severity(SEVERITY_WARNING);
            se->Error().UserMsg("ENTITY requires external mapping");
        }
        se->eDesc = entd;
        return se;
    } else {
        return ENTITY_NULL;
    }
}


int Registry::GetEntityCnt()
{
    return entity_cnt;
}

void Registry::ResetEntities()
{
    SC_HASHlistinit(primordialSwamp, &cur_entity);

}

const EntityDescriptor *Registry::NextEntity()
{
    if(0 == SC_HASHlist(&cur_entity)) {
        return 0;
    }
    return (const EntityDescriptor *) cur_entity.e->data;
}

void Registry::ResetSchemas()
{
    SC_HASHlistinit(active_schemas, &cur_schema);
}

const Schema *Registry::NextSchema()
{
    if(0 == SC_HASHlist(&cur_schema)) {
        return 0;
    }
    return (const Schema *) cur_schema.e->data;
}
