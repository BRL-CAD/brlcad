
#include <ctype.h>

#include <STEPcomplex.h>
#include <complexSupport.h>
#include <STEPattribute.h>
#include <sstream>
#include "sc_memmgr.h"

extern const char *
ReadStdKeyword( istream & in, std::string & buf, int skipInitWS );


STEPcomplex::STEPcomplex( Registry * registry, int fileid )
    : SDAI_Application_instance( fileid, 1 ),  sc( 0 ), _registry( registry ), visited( 0 ) {
    head = this;
}

STEPcomplex::STEPcomplex( Registry * registry, const std::string ** names,
                          int fileid, const char * schnm )
    : SDAI_Application_instance( fileid, 1 ),  sc( 0 ), _registry( registry ), visited( 0 ) {
    char * nms[BUFSIZ];
    int j, k;

    head = this;

    // Create a char ** list of names and call Initialize to build all:
    for( j = 0; names[j]; j++ ) {
        nms[j] = new char[( names[j] )->length() + 1 ];
        strcpy( nms[j], names[j]->c_str() );
    }
    nms[j] = NULL;
    Initialize( ( const char ** )nms, schnm );
    for( k = 0; k < j; k++ ) {
        delete [] nms[k];
    }
}

STEPcomplex::STEPcomplex( Registry * registry, const char ** names, int fileid,
                          const char * schnm )
    : SDAI_Application_instance( fileid, 1 ),  sc( 0 ), _registry( registry ), visited( 0 ) {

    head = this;
    Initialize( names, schnm );
}

/**
 * Called by the STEPcomplex constructors to validate the names in our
 * list, see if they represent a legal entity combination, and build the
 * complex list.  schnm is the name of the current schema if applicable.
 * (One may be defined under the FILE_SCHEMA section of a Part 21 file
 * header.)  If the current schema is not the schema in which an entity
 * was defined, it may be referenced by a different name in this schema.
 * (This is the case if schema B USEs or REFERENCEs entity X from schema
 * A and renames it to Y.)  Registry::FindEntity() below knows how to
 * search using the current name of each entity based on schnm.
 */
void STEPcomplex::Initialize( const char ** names, const char * schnm ) {
    // Create an EntNode list consisting of all the names in the complex ent:
    EntNode * ents = new EntNode( names ),
    *eptr = ents, *prev = NULL, *enext;
    const EntityDescriptor * enDesc;
    char nm[BUFSIZ];
    bool invalid = false, outOfOrder = false;

    // Splice out the invalid names from our list:
    while( eptr ) {
        enext = eptr->next;
        enDesc = _registry->FindEntity( *eptr, schnm );
        if( enDesc ) {
            if( enDesc->Supertypes().EntryCount() > 1 ) {
                eptr->multSuprs( true );
            }
            if( StrCmpIns( *eptr, enDesc->Name() ) ) {
                // If this entity was referred by another name rather than the
                // original.  May be the case if FindEntity() determined that
                // eptr's name was a legal renaming of enDesc's.  (Entities and
                // types may be renamed with the USE/REF clause - see header
                // comments.)  If so, change eptr's name (since the complex
                // support structs only deal with the original names) and have
                // ents re-ort eptr properly in the list:
                eptr->Name( StrToLower( enDesc->Name(), nm ) );
                outOfOrder = true;
            }
            prev = eptr;
        } else {
            invalid = true;
            cerr << "ERROR: Invalid entity \"" << eptr->Name()
                 << "\" found in complex entity.\n";
            if( !prev ) {
                // if we need to delete the first node,
                ents = eptr->next;
            } else {
                prev->next = eptr->next;
            }
            eptr->next = NULL;
            // must set eptr->next to NULL or next line would del entire list
            delete eptr;
        }
        eptr = enext;
    }

    // If we changed the name of any of the entities, resort:
    if( outOfOrder ) {
        ents->sort( &ents );
        // This fn may change the value of ents if the list should have a new
        // first EndNode.
    }

    // Set error message according to results of above:
    if( invalid ) {
        if( !ents ) {
            // not a single legal name
            _error.severity( SEVERITY_WARNING );
            // SEV_WARNING - we have to skip this entity altogether, but will
            // continue with the next entity.
            _error.UserMsg( "No legal entity names found in instance" );
            return;
        }
        _error.severity( SEVERITY_INCOMPLETE );
        _error.UserMsg( "Some illegal entity names found in instance" );
        // some illegal entity names, but some legal
    }

    // Check if a complex entity can be formed from the resulting combination:
    if( !_registry->CompCol()->supports( ents ) ) {
        _error.severity( SEVERITY_WARNING );
        _error.UserMsg(
            "Entity combination does not represent a legal complex entity" );
        cerr << "ERROR: Could not create instance of the following complex"
             << " entity:" << endl;
        eptr = ents;
        while( eptr ) {
            cerr << *eptr << endl;
            eptr = eptr->next;
        }
        cerr << endl;
        return;
    }

    // Finally, build what we can:
    BuildAttrs( *ents );
    for( eptr = ents->next; eptr; eptr = eptr->next ) {
        AddEntityPart( *eptr );
    }
    AssignDerives();
    delete ents;
}

STEPcomplex::~STEPcomplex() {
    STEPcomplex_attr_data attr_data;

    if( sc ) {
        delete sc;
    }
    for( attr_data = _attr_data_list.begin();
            attr_data != _attr_data_list.end();
            attr_data ++ ) {
        delete *attr_data;
    }
    _attr_data_list.clear();
}

void STEPcomplex::AssignDerives() {
    STEPattribute * a = 0;
    STEPcomplex * scomp1 = head;
    STEPcomplex * scomp2;

    const AttrDescriptorList * attrList;
    AttrDescLinkNode * attrPtr;
    const AttrDescriptor * ad;

    while( scomp1 && scomp1->eDesc ) {
        a = 0;
        attrList = &( scomp1->eDesc->ExplicitAttr() );
        attrPtr = ( AttrDescLinkNode * )attrList->GetHead();

        // assign nm to be derived attr
        // while( more derived attr for entity part )
        while( attrPtr != 0 ) {
            ad = attrPtr->AttrDesc();
            if( ( ad->Derived() ) == LTrue ) {
                const char * nm = ad->Name();
                const char * attrNm = 0;
                if( strrchr( nm, '.' ) ) {
                    attrNm = strrchr( nm, '.' );
                    attrNm++;
                } else {
                    attrNm = nm;
                }
                scomp2 = head;
                while( scomp2 && !a ) {
                    if( scomp1 != scomp2 ) {
                        scomp2->MakeDerived( attrNm );
                        a = scomp2->GetSTEPattribute( attrNm );
                    }
                    scomp2 = scomp2->sc;
                }
            }
            // increment attr
            attrPtr = ( AttrDescLinkNode * )attrPtr->NextNode();
        }
        scomp1 = scomp1->sc;
    }
}

/** \fn STEPcomplex::AddEntityPart
** this function should only be called for the head entity
** in the list of entity parts.
*/
void STEPcomplex::AddEntityPart( const char * name ) {
    STEPcomplex * scomplex;

    if( name ) {
        scomplex = new STEPcomplex( _registry, STEPfile_id );
        scomplex->BuildAttrs( name );
        if( scomplex->eDesc ) {
            scomplex->head = this;
            AppendEntity( scomplex );
        } else {
            cout << scomplex->_error.DetailMsg() << endl;
            delete scomplex;
        }
    }
}

STEPcomplex * STEPcomplex::EntityPart( const char * name, const char * currSch ) {
    STEPcomplex * scomp = head;
    while( scomp ) {
        if( scomp->eDesc ) {
            if( scomp->eDesc->CurrName( name, currSch ) ) {
                return scomp;
            }
        } else
            cout << "Bug in STEPcomplex::EntityPart(): entity part has "
                 << "no EntityDescriptor\n";
        scomp = scomp->sc;
    }
    return 0;
}

int STEPcomplex::EntityExists( const char * name, const char * currSch ) {
    return ( EntityPart( name, currSch ) ? 1 : 0 );
}

/**
** Check if a given entity (by descriptor) is the same as this one.
** For a complex entity, we'll check the EntityDescriptor of each entity
** in the complex 'chain'
*/
const EntityDescriptor * STEPcomplex::IsA( const EntityDescriptor * ed ) const {
    const EntityDescriptor * return_ed = eDesc->IsA( ed );

    if( !return_ed && sc ) {
        return sc->IsA( ed );
    } else {
        return return_ed;
    }
}

Severity STEPcomplex::ValidLevel( ErrorDescriptor * error, InstMgr * im,
                                  int clearError ) {
    cout << "STEPcomplex::ValidLevel() not implemented.\n";
    return SEVERITY_NULL;
}

void STEPcomplex::AppendEntity( STEPcomplex * stepc ) {
    if( sc ) {
        sc->AppendEntity( stepc );
    } else {
        sc = stepc;
    }
}

// READ
Severity STEPcomplex::STEPread( int id, int addFileId, class InstMgr * instance_set,
                                istream & in, const char * currSch, bool /*useTechCor*/, bool /*strict*/ ) {
    char c;
    std::string typeNm;
    STEPcomplex * stepc = 0;

    ClearError( 1 );
    STEPfile_id = id;

    stepc = head;
    while( stepc ) {
        stepc->visited = 0;
        stepc = stepc->sc;
    }

    in >> ws;
    in.get( c );
    if( c == '(' ) { // opening paren for subsuperRecord
        in >> ws;
        c = in.peek();
        while( c != ')' ) {
            typeNm.clear();
            in >> ws;
            ReadStdKeyword( in, typeNm, 1 ); // read the type name
            in >> ws;
            c = in.peek();
            if( c != '(' ) {
                _error.AppendToDetailMsg(
                    "Missing open paren before entity attr values.\n" );
                cout << "ERROR: missing open paren\n";
                _error.GreaterSeverity( SEVERITY_INPUT_ERROR );
                STEPread_error( c, 0, in, currSch );
                return _error.severity();
            }

            stepc = EntityPart( typeNm.c_str(), currSch );
            if( stepc )
                stepc->SDAI_Application_instance::STEPread( id, addFileId,
                        instance_set, in,
                        currSch );
            else {
                cout << "ERROR: complex entity part \"" << typeNm
                     << "\" does not exist.\n";
                _error.AppendToDetailMsg(
                    "Complex entity part of instance does not exist.\n" );
                _error.GreaterSeverity( SEVERITY_INPUT_ERROR );
                STEPread_error( c, 0, in, currSch );
                return _error.severity();
            }
            in >> ws;
            c = in.peek();
        }
        if( c != ')' )
            cout <<
                 "ERROR: missing ending paren for complex entity instance.\n";
        else {
            in.get( c );    // read the closing paren
        }
    }
    return _error.severity();
}

//FIXME delete this?
#ifdef buildwhileread
// READ
Severity STEPcomplex::STEPread( int id, int addFileId, class InstMgr * instance_set,
                                istream & in, const char * currSch ) {
    ClearError( 1 );
    STEPfile_id = id;

    STEPcomplex stepc = head;
    while( stepc ) {
        stepc->visited = 0;
        stepc = stepc->sc;
    }

    char c;
    in >> ws;
    in.get( c );
    if( c == '(' ) {
        std::string s;
        in >> ws;
        in.get( c );
        while( in && ( c != '(' ) && !isspace( c ) ) { // get the entity name
            s.Append( c );
            in.get( c );
        }
        if( isspace( c ) ) {
            in >> ws;
            in.get( c );
        }

        if( c != '(' ) {
            _error.AppendToDetailMsg(
                "Missing open paren before entity attr values.\n" );
            cout << "ERROR: missing open paren\n";
            _error.GreaterSeverity( SEVERITY_INPUT_ERROR );
            STEPread_error( c, 0, in, currSch );
            return _error.severity();
        } else { // c == '('
            in.putback( c );
        }

        cout << s << endl;
        BuildAttrs( s.c_str() );
        SDAI_Application_instance::STEPread( id, addFileId, instance_set,
                                             in, currSch );

        in >> ws;
        in.get( c );
        while( c != ')' ) {
            s.set_null();
            while( in && ( c != '(' ) && !isspace( c ) ) { // get the entity name
                s.Append( c );
                in.get( c );
            }
            if( isspace( c ) ) {
                in >> ws;
                in.get( c );
            }
            if( c != '(' ) {
                _error.AppendToDetailMsg(
                    "Missing open paren before entity attr values.\n" );
                cout << "ERROR: missing open paren\n";
                _error.GreaterSeverity( SEVERITY_INPUT_ERROR );
                STEPread_error( c, 0, in, currSch );
                return _error.severity();
            } else { // c == '('
                in.putback( c );
            }

            cout << s << endl; // diagnostics DAS

            STEPcomplex * stepc = new STEPcomplex( _registry );
            AppendEntity( stepc );
            stepc->BuildAttrs( s.c_str() );
            stepc->SDAI_Application_instance::STEPread( id, addFileId,
                    instance_set, in,
                    currSch );
            in >> ws;
            in.get( c );
        }
    }
    return _error.severity();
}

#endif

void STEPcomplex::BuildAttrs( const char * s ) {

    // assign inherited member variable
    eDesc = ( class EntityDescriptor * )_registry->FindEntity( s );

    if( eDesc ) {
        const AttrDescriptorList * attrList;
        SDAI_Integer * integer_data;
        SDAI_String * string_data;
        SDAI_Binary * binary_data;
        SDAI_Real * real_data;
        SDAI_BOOLEAN * boolean_data;
        SDAI_LOGICAL * logical_data;
        SDAI_Application_instance ** entity_data;
        SDAI_Enum * enum_data;
        SDAI_Select * select_data;
        STEPaggregate * aggr_data;
        attrList = &( eDesc->ExplicitAttr() );

        //////////////////////////////////////////////
        // find out how many attrs there are
        //////////////////////////////////////////////

        STEPattribute * a = 0;

        AttrDescLinkNode * attrPtr = ( AttrDescLinkNode * )attrList->GetHead();
        while( attrPtr != 0 ) {
            const AttrDescriptor * ad = attrPtr->AttrDesc();

            if( ( ad->Derived() ) != LTrue ) {

                switch( ad->NonRefType() ) {
                    case INTEGER_TYPE:
                        integer_data = new SDAI_Integer;
                        _attr_data_list.push_back( ( void * ) integer_data );
                        a = new STEPattribute( *ad, integer_data );
                        break;

                    case STRING_TYPE:
                        string_data = new SDAI_String;
                        _attr_data_list.push_back( ( void * ) string_data );
                        a = new STEPattribute( *ad, string_data );
                        break;

                    case BINARY_TYPE:
                        binary_data = new SDAI_Binary;
                        _attr_data_list.push_back( ( void * ) binary_data );
                        a = new STEPattribute( *ad, binary_data );
                        break;

                    case REAL_TYPE:
                    case NUMBER_TYPE:
                        real_data = new SDAI_Real;
                        _attr_data_list.push_back( ( void * ) real_data );
                        a = new STEPattribute( *ad,  real_data );
                        break;

                    case BOOLEAN_TYPE:
                        boolean_data = new SDAI_BOOLEAN;
                        _attr_data_list.push_back( ( void * ) boolean_data );
                        a = new STEPattribute( *ad,  boolean_data );
                        break;

                    case LOGICAL_TYPE:
                        logical_data = new SDAI_LOGICAL;
                        _attr_data_list.push_back( ( void * ) logical_data );
                        a = new STEPattribute( *ad,  logical_data );
                        break;

                    case ENTITY_TYPE:
                        entity_data = new( SDAI_Application_instance * );
                        _attr_data_list.push_back( ( void * ) entity_data );
                        a = new STEPattribute( *ad, entity_data );
                        break;

                    case ENUM_TYPE: {
                        EnumTypeDescriptor * enumD =
                            ( EnumTypeDescriptor * )ad->ReferentType();
                        enum_data = enumD->CreateEnum();
                        _attr_data_list.push_back( ( void * ) enum_data );
                        a = new STEPattribute( *ad, enum_data );
                        break;
                    }
                    case SELECT_TYPE: {
                        SelectTypeDescriptor * selectD =
                            ( SelectTypeDescriptor * )ad->ReferentType();
                        select_data = selectD->CreateSelect();
                        _attr_data_list.push_back( ( void * ) select_data );
                        a = new STEPattribute( *ad, select_data );
                        break;
                    }
                    case AGGREGATE_TYPE:
                    case ARRAY_TYPE:      // DAS
                    case BAG_TYPE:        // DAS
                    case SET_TYPE:        // DAS
                    case LIST_TYPE: {     // DAS
                        AggrTypeDescriptor * aggrD =
                            ( AggrTypeDescriptor * )ad->ReferentType();
                        aggr_data = aggrD->CreateAggregate();
                        //_attr_data_list.push_back( ( void * ) aggr_data );
                        _attr_data_list.push_back( aggr_data );
                        a = new STEPattribute( *ad, aggr_data );
                        break;
                    }
                    default:
                        _error.AppendToDetailMsg( "STEPcomplex::BuildAttrs: Found attribute of unknown type. Creating default attribute.\n" );
                        _error.GreaterSeverity( SEVERITY_WARNING );
                        a = new STEPattribute();
                }

                a -> set_null();
                attributes.push( a );
            }
            attrPtr = ( AttrDescLinkNode * )attrPtr->NextNode();
        }
    } else {
        _error.AppendToDetailMsg( "Entity does not exist.\n" );
        _error.GreaterSeverity( SEVERITY_INPUT_ERROR );
    }
}

void STEPcomplex::STEPread_error( char c, int index, istream & in, const char * schnm ) {
    (void) schnm; //unused
    cout << "STEPcomplex::STEPread_error(), index=" << index << ", entity #" << STEPfile_id << "." << endl;
    streampos p = in.tellg();
    std::string q, r;
    getline( in, q );
    getline( in, r );
    cout << "Remainder of this line:" << endl << c << q << endl << "Next line:" << endl << r << endl;
    in.seekg( p );
}

/**
** These functions take into account the current schema, so that if an entity
** or attribute type is renamed in this schema (using the USE/REF clause) the
** new name is written out.  They do not, however, comply with the requirement
** in the standard that the entity parts of a complex entity must be printed in
** alphabetical order.  The nodes are sorted alphabetically according to their
** original names but not according to their renamed names (DAR 6/5/97).
*/
void STEPcomplex::STEPwrite( ostream & out, const char * currSch, int writeComment ) {
    if( writeComment && !p21Comment.empty() ) {
        out << p21Comment;
    }
    out << "#" << STEPfile_id << "=(\n";
    WriteExtMapEntities( out, currSch );
    out << ");\n";
}

const char * STEPcomplex::STEPwrite( std::string & buf, const char * currSch ) {
    buf.clear();

    stringstream ss;
    ss << "#" << STEPfile_id << "=(";
    WriteExtMapEntities( ss, currSch );
    ss << ");";
    ss << ends;

    buf.append( ss.str() );

    return const_cast<char *>( buf.c_str() );
}

/** \copydoc STEPcomplex::STEPwrite */
void STEPcomplex::WriteExtMapEntities( ostream & out, const char * currSch ) {
    std::string tmp;
    out << StrToUpper( EntityName( currSch ), tmp );
    out << "(";
    int n = attributes.list_length();

    for( int i = 0 ; i < n; i++ ) {
        ( attributes[i] ).STEPwrite( out, currSch );
        if( i < n - 1 ) {
            out << ",";
        }
    }
    out << ")\n";
    if( sc ) {
        sc->WriteExtMapEntities( out, currSch );
    }
}

/** \copydoc STEPcomplex::STEPwrite */
const char * STEPcomplex::WriteExtMapEntities( std::string & buf, const char * currSch ) {

    std::string tmp;

    buf.append( ( char * )StrToUpper( EntityName( currSch ), tmp ) );
    buf.append( "i" );

    int n = attributes.list_length();

    for( int i = 0 ; i < n; i++ ) {
        attributes[i].asStr( tmp, currSch ) ;
        buf.append( tmp );
        if( i < n - 1 ) {
            buf.append( "," );
        }
    }
    buf.append( ")\n" );

    if( sc ) {
        sc->WriteExtMapEntities( buf, currSch );
    }

    return const_cast<char *>( buf.c_str() );
}

void STEPcomplex::CopyAs( SDAI_Application_instance * se ) {
    if( !se->IsComplex() ) {
        char errStr[BUFSIZ];
        cerr << "STEPcomplex::CopyAs() called with non-complex entity:  "
             << __FILE__ <<  __LINE__ << "\n" << _POC_ "\n";
        sprintf( errStr,
                 "STEPcomplex::CopyAs(): %s - entity #%d.\n",
                 "Programming ERROR -  called with non-complex entity",
                 STEPfile_id );
        _error.AppendToDetailMsg( errStr );
        _error.AppendToUserMsg( errStr );
        _error.GreaterSeverity( SEVERITY_BUG );
        return;
    } else {
        STEPcomplex * scpartCpyTo = head;
        STEPcomplex * scpartCpyFrom = ( ( STEPcomplex * )se )->head;
        while( scpartCpyTo && scpartCpyFrom ) {
            scpartCpyTo->SDAI_Application_instance::CopyAs( scpartCpyFrom );
            scpartCpyTo = scpartCpyTo->sc;
            scpartCpyFrom = scpartCpyFrom->sc;
        }
    }
}

SDAI_Application_instance * STEPcomplex::Replicate() {
    if( !IsComplex() ) {
        return SDAI_Application_instance::Replicate();
    } else if( !_registry ) {
        return S_ENTITY_NULL;
    } else {
        int nameCount = 64;

        std::string ** nameList = new std::string *[nameCount];
        STEPcomplex * scomp = this->head;
        int i = 0;
        while( scomp && ( i < 63 ) ) {
            nameList[i] = new std::string( "" );
            nameList[i]->append( scomp->eDesc->Name() );
            i++;
            scomp = scomp->sc;
        }
        nameList[i] = ( std::string * )0;
        if( i == 63 ) {
            char errStr[BUFSIZ];
            cerr << "STEPcomplex::Replicate() name buffer too small:  "
                 << __FILE__ <<  __LINE__ << "\n" << _POC_ "\n";
            sprintf( errStr,
                     "STEPcomplex::Replicate(): %s - entity #%d.\n",
                     "Programming ERROR - name buffer too small",
                     STEPfile_id );
            _error.AppendToDetailMsg( errStr );
            _error.AppendToUserMsg( errStr );
            _error.GreaterSeverity( SEVERITY_BUG );
        }

        STEPcomplex * seNew = new STEPcomplex( _registry,
                                               ( const std::string ** )nameList,
                                               1111 );
        seNew -> CopyAs( this );
        return seNew;

        // TODO need to:
        // get registry
        // create list of entity names
        // send them to constructor for STEPcomplex
        // implement STEPcomplex::CopyAs()
        // call seNew->CopyAs()
    }
}
