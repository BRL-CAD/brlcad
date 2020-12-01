#ifndef SCHRENAME_H
#define SCHRENAME_H

#include <cstdio>
#include <cstring>

#include "Str.h"

#include "sc_export.h"

/** \class SchRename
 * SchRename is a structure which partially support the concept of USE and RE-
 * FERENCE in EXPRESS.  Say schema A USEs object X from schema B and renames it
 * to Y (i.e., "USE (X as Y);").  SchRename stores the name of the schema (B)
 * plus the new object name for that schema (Y).  Each TypeDescriptor has a
 * SchRename object (actually a linked list of SchRenames) corresponding to all
 * the possible different names of itself depending on the current schema (the
 * schema which is currently reading or writing this object).  (The current
 * schema is determined by the file schema section of the header section of a
 * part21 file (the _headerInstances of STEPfile).
 */
class SC_CORE_EXPORT SchRename {
public:
    SchRename( const char * sch = "\0", const char * newnm = "\0" ) : next( 0 ) {
        strcpy( schName, sch );
        strcpy( newName, newnm );
    }
    ~SchRename() {
        delete next;
    }
    const char * objName() const {
        return newName;
    }
    int operator< ( SchRename & schrnm ) {
        return ( strcmp( schName, schrnm.schName ) < 0 );
    }
    bool choice( const char * nm ) const;
    // is nm one of our possible choices?
    char * rename( const char * schm, char * newnm ) const;
    // given a schema name, returns new object name if exists
    SchRename * next;

private:
    char schName[BUFSIZ];
    char newName[BUFSIZ];
};


#endif //SCHRENAME_H
