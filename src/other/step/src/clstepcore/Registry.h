#ifndef _REGISTRY_H
#define _REGISTRY_H

/*
* NIST STEP Core Class Library
* clstepcore/Registry.h
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: Registry.h,v 3.0.1.7 1997/11/05 21:59:19 sauderd DP3.1 $  */ 

#include <sdai.h>
//#include <STEPentity.h>
#include <errordesc.h>
#include <scl_hash.h>
#include <Str.h>
#include <complexSupport.h>

typedef struct Hash_Table * HashTable;

class Registry;
extern char * EntityClassName ( char *);
typedef void (* CF_init) (Registry &);	//  pointer to creation initialization 

class Registry {
  protected:
    HashTable primordialSwamp;	//  dictionary of EntityDescriptors
    HashTable active_schemas;	//  dictionary of Schemas
    HashTable active_types;	//  dictionary of TypeDescriptors
    ComplexCollect *col;        //  struct containing all complex entity info
    
    int entity_cnt,
        all_ents_cnt;
    HashEntry 	cur_entity;
    HashEntry 	cur_schema;
    HashEntry   cur_type;

         // used by AddEntity() and RemoveEntity() to deal with renamings of an
         // entity done in a USE or REFERENCE clause - see header comments in
         // file Registry.inline.cc
    void        AddClones (const EntityDescriptor &);
    void        RemoveClones (const EntityDescriptor &);

  public:
    Registry (CF_init initFunct);
    ~Registry ();
    void DeleteContents ();  // CAUTION: calls delete on all the descriptors 

    const EntityDescriptor* FindEntity (const char *, const char * =0,
					int check_case =0) const;
    const Schema* FindSchema (const char *, int check_case =0) const;
    const TypeDescriptor*   FindType   (const char *, int check_case =0) const;
    
    void 	AddEntity (const EntityDescriptor&);
    void	AddSchema (const Schema&);
    void 	AddType   (const TypeDescriptor&);
    
    void 	RemoveEntity (const char *);
    void	RemoveSchema (const char *);
    void 	RemoveType   (const char *);

    int 	GetEntityCnt ();
    int 	GetFullEntCnt () { return all_ents_cnt; }

    void	ResetEntities ();
    const EntityDescriptor *	NextEntity ();

    void	ResetSchemas ();
    const Schema *	NextSchema ();

    void        ResetTypes ();
    const TypeDescriptor *      NextType ();

    const ComplexCollect *CompCol() { return col; }
    void        SetCompCollect( ComplexCollect *c ) { col = c; }

    SCLP23(Application_instance)* ObjCreate (const char * nm, const char * =0,
			   int check_case =0) const;
};

#endif  /*  _REGISTRY_H  */
