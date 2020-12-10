/// \file ordered_attrs.cc - create a list of attributes in the proper order for part 21, taking into account derivation, diamond inheritance, and (TODO) redefinition

#include <vector>
#include <cassert>
#include <cstring>

#include "ordered_attrs.h"

#ifdef _WIN32
#  define strcasecmp _stricmp
#endif

Entity currentEntity = 0;
oaList attrs;
unsigned int attrIndex = 0;

/// uses depth-first recursion to add attrs in order; looks for derived attrs
void populateAttrList(oaList &list, Entity ent)
{
    unsigned int attrCount = list.size(); //use to figure out how many attrs on end of list need to be checked for duplicates
    //recurse through supertypes
    LISTdo(ent->u.entity->supertypes, super, Entity) {
        populateAttrList(list, super);
    }
    LISTod
    //then look at ent's own attrs, checking against attrs with index >= attrCount
    //derivation check only - leave deduplication for later
    LISTdo(ent->u.entity->attributes, attr, Variable) {
        bool unique = true;
        for(unsigned int i = attrCount; i < list.size(); i++) {
            if(0 == strcasecmp(attr->name->symbol.name, list[i]->attr->name->symbol.name)) {
                // an attr by this name exists in a supertype
                // originally printed a warning here, but that was misleading - they have more uses than I thought
                unique = false;
                list[i]->deriver = ent;
                break;
            }
        }
        if(unique) {
            orderedAttr *oa = new orderedAttr;
            oa->attr = attr;
            oa->creator = ent;
            if(attr->initializer) {
                // attrs derived by their owner are omitted from part 21 - section 10.2.3
                oa->deriver = ent;
            } else {
                oa->deriver = 0;
            }
            oa->redefiner = 0;
            list.push_back(oa);
        }
    }
    LISTod
}

///compare attr name and creator, remove all but first occurrence
///this is necessary for diamond inheritance
void dedupList(oaList &list)
{
    oaList::iterator it, jt;
    for(it = list.begin(); it != list.end(); it++) {
        for(jt = it + 1; jt != list.end(); jt++) {
            if((0 == strcasecmp((* it)->attr->name->symbol.name, (* jt)->attr->name->symbol.name)) &&
                    (0 == strcasecmp((* it)->creator->symbol.name, (* jt)->creator->symbol.name))) {
                //fprintf( stderr, "erasing %s created by %s\n", ( * jt )->attr->name->symbol.name, ( * jt )->creator->symbol.name );
                jt--;
                list.erase(jt + 1);
            }
        }
    }
}

/// set up ordered attrs for new entity
void orderedAttrsInit(Entity e)
{
    orderedAttrsCleanup();
    attrIndex = 0;
    currentEntity = e;
    if(currentEntity) {
        populateAttrList(attrs, currentEntity);
        if(attrs.size() > 1) {
            dedupList(attrs);
        }
    }
}

void orderedAttrsCleanup()
{
    for(unsigned int i = 0; i < attrs.size(); i++) {
        delete attrs[i];
    }
    attrs.clear();
}

const orderedAttr *nextAttr()
{
    if(attrIndex < attrs.size()) {
        unsigned int i = attrIndex;
        attrIndex++;
        return attrs.at(i);
    } else {
        return 0;
    }
}
