/*
* NIST STEP Core Class Library
* clstepcore/Registry.inline.cc
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: Registry.inline.cc,v 3.0.1.9 1997/11/05 21:59:19 sauderd DP3.1 $  */ 

#include <ExpDict.h>
#include <Registry.h>

 const TypeDescriptor *  t_sdaiINTEGER;
 const TypeDescriptor *  t_sdaiREAL;
 const TypeDescriptor *  t_sdaiNUMBER;
 const TypeDescriptor *  t_sdaiSTRING;
 const TypeDescriptor *  t_sdaiBINARY;
 const TypeDescriptor *  t_sdaiBOOLEAN;
 const TypeDescriptor *  t_sdaiLOGICAL;

static int uniqueNames( const char *, const SchRename * );

Registry::Registry (CF_init initFunct) 
: entity_cnt(0), all_ents_cnt(0), col(0)
{ 
//    if(NilSTEPentity == 0)
//      NilSTEPentity = new SCLP23(Application_instance);

    /* Registry tables aren't always initialized */
    HASHinitialize();
    primordialSwamp = HASHcreate (1000);
    active_schemas = HASHcreate (10);
    active_types = HASHcreate (100);

    t_sdaiINTEGER = new TypeDescriptor("INTEGER",     // Name
		       sdaiINTEGER, // FundamentalType
		       0, // Originating Schema
		       "INTEGER");   // Description;

    t_sdaiREAL = new TypeDescriptor("REAL", sdaiREAL, 
				     0, // Originating Schema
				     "Real");

    t_sdaiSTRING = new TypeDescriptor("STRING", sdaiSTRING, 
				       0, // Originating Schema
				       "String");

    t_sdaiBINARY = new TypeDescriptor("BINARY", sdaiBINARY, 
				       0, // Originating Schema
				       "Binary");

    t_sdaiBOOLEAN = new TypeDescriptor("BOOLEAN", sdaiBOOLEAN, 
					0, // Originating Schema
					"Boolean");

    t_sdaiLOGICAL = new TypeDescriptor("LOGICAL", sdaiLOGICAL, 
					0, // Originating Schema
					"Logical");

    t_sdaiNUMBER = new TypeDescriptor("NUMBER", sdaiNUMBER, 
				       0, // Originating Schema
				       "Number");

/*    t_GENERIC_TYPE = new TypeDescriptor("GENERIC", GENERIC_TYPE, "Generic");*/

    initFunct (*this);  
    HASHlistinit (active_types, &cur_type);
    HASHlistinit (primordialSwamp, &cur_entity);  // initialize cur\'s
    HASHlistinit (active_schemas, &cur_schema);
}

/* inline */ 
Registry::~Registry  ()
{
    HASHdestroy (primordialSwamp);
    HASHdestroy (active_schemas);
    HASHdestroy (active_types);
    delete col;
}

void 
Registry::DeleteContents ()
{
  // entities first
  HASHlistinit (primordialSwamp, &cur_entity);
  while (HASHlist (&cur_entity))
    delete (EntityDescriptor *) cur_entity.e->data;

  // schemas
  HASHlistinit (active_schemas, &cur_schema);
  while (HASHlist (&cur_schema))
    delete (Schema *) cur_schema.e->data;

  // types 

}

/* inline */ const EntityDescriptor *
Registry::FindEntity (const char *e, const char *schNm, int check_case) const
    /*
     * schNm refers to the current schema.  This will have a value if we are
     * reading from a Part 21 file (using a STEPfile object), and the file
     * declares a schema name in the File_Schema section of the Header.  (If
     * >1 schema names are declared, the first is taken.)  The schema name is
     * significant because of the USE and REFERENCE clause.  Say schema X USEs
     * entity A from schema Y and renames it to B, X should only refer to A as
     * B.  Thus, if schNm here = "X", only e="B" would be valid but not e="A".
     */
{
    const EntityDescriptor *entd;
    const SchRename *altlist;
    char schformat[BUFSIZ], altName[BUFSIZ];

    if (check_case) {
	entd = (EntityDescriptor *)HASHfind (primordialSwamp, (char *)e);
    } else {
	entd = (EntityDescriptor *)HASHfind (primordialSwamp,
					     (char *)PrettyTmpName (e));
    }
    if ( entd && schNm ) {
	// We've now found an entity.  If schNm has a value, we must ensure we
	// have a valid name.
	strcpy( schformat, PrettyTmpName( schNm ) );
	if (    ( (altlist = entd->AltNameList()) != 0 )
	     && ( altlist->rename( schformat, altName ) ) ) {
	    // If entd has other name choices, and entd is referred to with a
	    // new name by schema schNm, then e had better = the new name.
	    if ( !StrCmpIns( e, altName ) ) {
		return entd;
	    }
	    return NULL;
	} else if ( FindSchema( schformat, 1 ) ) {
	    // If schema schNm exists but we had no conditions above to use an
	    // altName, we must use the original name:
	    if ( !StrCmpIns( e, entd->Name() ) ) {
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

/* inline */ const Schema *
Registry::FindSchema (const char * n, int check_case) const
{
    if (check_case) 
      return (const Schema *) HASHfind (active_schemas, (char *) n);

    return (const Schema *) HASHfind (active_schemas, 
						   (char *)PrettyTmpName (n));
}

/* inline */ const TypeDescriptor *
Registry::FindType (const char * n, int check_case) const
{
   if (check_case)
       return (const TypeDescriptor *) HASHfind (active_types, (char *) n);
   return (const TypeDescriptor *) HASHfind (active_types,
                                                 (char *)PrettyTmpName (n));
}

/* inline */ void 	
Registry::ResetTypes ()
{
    HASHlistinit (active_types, &cur_type);
}

/* inline */ const TypeDescriptor * 
Registry::NextType ()
{
  if (0 == HASHlist (&cur_type)) return 0;
  return (const TypeDescriptor *) cur_type.e->data;
}

/* inline */ void 	
Registry::AddEntity (const EntityDescriptor& e)
{
    HASHinsert (primordialSwamp, (char *) e.Name (), (EntityDescriptor *) &e);
    ++entity_cnt;
    ++all_ents_cnt;
    AddClones(e);
}
  

/* inline */ void	
Registry::AddSchema (const Schema& d)
{
    HASHinsert (active_schemas, (char *) d.Name(), (Schema *) &d);
}

/* inline */ void 	
Registry::AddType (const TypeDescriptor& d)
{
    HASHinsert (active_types, (char *) d.Name(), (TypeDescriptor *) &d);
}
    
/* inline */ void 	
Registry::AddClones (const EntityDescriptor& e)
    /*
     * Purpose is to insert e into the registry hashed according to all its
     * alternate names (the names it was renamed with when other schemas USEd
     * or REFERENCEd it).  This will make these names available to the Registry
     * so that if we comes across one of them in a Part 21 file, we'll recog-
     * nize it.
     */
{
    const SchRename *alts = e.AltNameList();

    while ( alts ) {
	HASHinsert( primordialSwamp, (char *)alts->objName(),
		    (EntityDescriptor *)&e );
	alts = alts->next;
    }
    all_ents_cnt += uniqueNames( e.Name(), e.AltNameList() );
}

static int
uniqueNames( const char *entnm, const SchRename *altlist )
    /*
     * Returns the number of unique names in an entity's _altname list.  If
     * schema B uses ent xx from schema A and renames it to yy, and schema C
     * does the same (or if C simply uses yy from B), altlist will contain 2
     * entries with the same alt name.
     */
{
    int cnt = 0;
    const SchRename *alt = altlist;

    while ( alt ) {
	if ( !(    alt->next
	        && alt->next->choice( alt->objName() )
	        || !StrCmpIns( alt->objName(), entnm ) ) ) {
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

/* inline */ void 	
Registry::RemoveEntity (const char * n)
{
    const EntityDescriptor *e = FindEntity(n);
    struct Element tmp;

    if ( e ) {
	RemoveClones( *e );
    }
    tmp.key = (char *) n;
    HASHsearch (primordialSwamp, &tmp, HASH_DELETE) ? --entity_cnt : 0;
    
}

/* inline */ void	
Registry::RemoveSchema (const char * n)
{
    struct Element tmp;
    tmp.key = (char *) n;
    HASHsearch (active_schemas, &tmp, HASH_DELETE);
}

/* inline */ void 	
Registry::RemoveType (const char * n)
{
    struct Element tmp;
    tmp.key = (char *) n;
    HASHsearch (active_types, &tmp, HASH_DELETE);
}

/* inline */ void
Registry::RemoveClones (const EntityDescriptor& e)
    /*
     * Remove all the "clones", or rename values of e.
     */
{
    const SchRename *alts = e.AltNameList();
    struct Element *tmp;

    while ( alts ) {
	tmp = new Element;
	tmp->key = (char *) alts->objName();
	HASHsearch (primordialSwamp, tmp, HASH_DELETE);
	alts = alts->next;
    }
}


SCLP23(Application_instance) *
Registry::ObjCreate (const char *nm, const char *schnm, int check_case) const
{
    const EntityDescriptor *  entd = FindEntity (nm, schnm, check_case);
    if (entd) {
	SCLP23(Application_instance) *se = 
	  ((EntityDescriptor *)entd) -> NewSTEPentity ();

	// See comment in previous function.
	if ( entd->AbstractEntity().asInt() == TRUE ) {
	    se->Error().severity( SEVERITY_WARNING );
	    se->Error().UserMsg( "ENTITY is abstract supertype" );
	} else if ( entd->ExtMapping().asInt() == TRUE ) {
	    se->Error().severity( SEVERITY_WARNING );
	    se->Error().UserMsg( "ENTITY requires external mapping" );
	}
	return se;
    } else {
	return ENTITY_NULL;
    }
}


/* inline */ int
Registry::GetEntityCnt ()
{
    return entity_cnt;
}

/* inline */ void
Registry::ResetEntities ()  
{
    HASHlistinit (primordialSwamp, &cur_entity);
    
}

/* inline */ const EntityDescriptor *
Registry::NextEntity () 
{
  if (0 == HASHlist (&cur_entity)) return 0;
  return (const EntityDescriptor *) cur_entity.e->data;
}    

/* inline */ void
Registry::ResetSchemas ()  
{
    HASHlistinit (active_schemas, &cur_schema);
}

/* inline */ const Schema *
Registry::NextSchema () 
{
  if (0 == HASHlist (&cur_schema)) return 0;
  return (const Schema *) cur_schema.e->data;
}
