
/*
* NIST STEP Core Class Library
* clstepcore/sdaiApplication_instance.cc
* July, 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <map>
#include <sdai.h>
#include <instmgr.h>
#include <STEPcomplex.h>
#include <STEPattribute.h>
#include <read_func.h> //for ReadTokenSeparator, used when comments are inside entities

#include "sdaiApplication_instance.h"
#include "superInvAttrIter.h"

#include <sc_nullptr.h>

SDAI_Application_instance NilSTEPentity;

bool isNilSTEPentity( const SDAI_Application_instance * ai ) {
    if( ai && ai == &NilSTEPentity ) {
        return true;
    }
    return false;
}

/**************************************************************//**
** \file sdaiApplication_instance.cc  Functions for manipulating entities
**
**  KNOWN BUGs:  the SDAI_Application_instance is not aware of the STEPfile;
**    therefore it can not read comments which may be embedded in the instance.
**    The following are known problems:
**    -- does not handle comments embedded in an instance ==> bombs
**    -- ignores embedded entities ==> does not bomb
**    -- error reporting does not include line number information
*/

SDAI_Application_instance::SDAI_Application_instance()
    :  _cur( 0 ),
       eDesc( NULL ),
       _complex( false ),
       STEPfile_id( 0 ),
       p21Comment( std::string( "" ) ),
       headMiEntity( 0 ),
       nextMiEntity( 0 ) {
}

SDAI_Application_instance::SDAI_Application_instance( int fileid, int complex )
    :  _cur( 0 ),
       eDesc( NULL ),
       _complex( complex ),
       STEPfile_id( fileid ),
       p21Comment( std::string( "" ) ),
       headMiEntity( 0 ),
       nextMiEntity( 0 ) {
}

SDAI_Application_instance::~SDAI_Application_instance() {
    STEPattribute * attr;

    ResetAttributes();
    do {
        attr = NextAttribute();
        if( attr ) {
            attr->refCount --;
            if( attr->refCount <= 0 ) {
                delete attr;
            }
        }
    } while( attr );


    if( MultipleInheritance() ) {
        delete nextMiEntity;
    }
}


/// initialize inverse attrs
/// eDesc->InitIAttrs() must have been called previously
/// call once per instance (*not* once per class)
void SDAI_Application_instance::InitIAttrs() {
    assert( eDesc && "eDesc must be set; please report this bug." );
    InverseAItr iai( &( eDesc->InverseAttr() ) );
    const Inverse_attribute * ia;
    iAstruct s;
    memset( &s, 0, sizeof s );
    while( 0 != ( ia = iai.NextInverse_attribute() ) ) {
        iAMap.insert( iAMap_t::value_type( ia, s ) );
    }
    superInvAttrIter siai( eDesc );
    while( !siai.empty() ) {
        ia = siai.next();
        assert( ia && "Null inverse attr!" );
        iAMap.insert( iAMap_t::value_type( ia, s ) );
    }
}

SDAI_Application_instance * SDAI_Application_instance::Replicate() {
    char errStr[BUFSIZ];
    if( IsComplex() ) {
        cerr << "STEPcomplex::Replicate() should be called:  " << __FILE__
             <<  __LINE__ << "\n" << _POC_ "\n";
        sprintf( errStr,
                 "SDAI_Application_instance::Replicate(): %s - entity #%d.\n",
                 "Programming ERROR - STEPcomplex::Replicate() should be called",
                 STEPfile_id );
        _error.AppendToDetailMsg( errStr );
        _error.AppendToUserMsg( errStr );
        _error.GreaterSeverity( SEVERITY_BUG );
        return S_ENTITY_NULL;
    } else {
        if( !eDesc ) {
            return S_ENTITY_NULL;
        }

        SDAI_Application_instance * seNew = eDesc->NewSTEPentity();
        seNew -> CopyAs( this );
        return seNew;
    }
}

void SDAI_Application_instance::AddP21Comment( const char * s, bool replace ) {
    if( replace ) {
        p21Comment.clear();
    }
    if( s ) {
        p21Comment += s;
    }
}

void SDAI_Application_instance::AddP21Comment( const std::string & s, bool replace ) {
    if( replace ) {
        p21Comment.clear();
    }
    p21Comment += s;
}

void SDAI_Application_instance::PrependP21Comment( const std::string & s ) {
    p21Comment.insert( 0, s );
}

void SDAI_Application_instance::STEPwrite_reference( ostream & out ) {
    out << "#" << STEPfile_id;
}

const char * SDAI_Application_instance::STEPwrite_reference( std::string & buf ) {
    char tmp[64];
    sprintf( tmp, "#%d", STEPfile_id );
    buf = tmp;
    return const_cast<char *>( buf.c_str() );
}

void SDAI_Application_instance::AppendMultInstance( SDAI_Application_instance * se ) {
    if( nextMiEntity == 0 ) {
        nextMiEntity = se;
    } else {
        SDAI_Application_instance * link = nextMiEntity;
        SDAI_Application_instance * linkTrailing = 0;
        while( link ) {
            linkTrailing = link;
            link = link->nextMiEntity;
        }
        linkTrailing->nextMiEntity = se;
    }
}

const EntityDescriptor* SDAI_Application_instance::getEDesc() const {
    return eDesc;
}

// BUG implement this -- FIXME function is never used
SDAI_Application_instance * SDAI_Application_instance::GetMiEntity( char * entName ) {
    std::string s1, s2;

    const EntityDescLinkNode * edln = 0;
    const EntityDescriptor * ed = eDesc;

    // compare up the *leftmost* parent path
    while( ed ) {
        if( !strcmp( StrToLower( ed->Name(), s1 ), StrToLower( entName, s2 ) ) ) {
            return this;    // return this parent path
        }
        edln = ( EntityDescLinkNode * )( ed->Supertypes().GetHead() );
        if( edln ) {
            ed = edln->EntityDesc();
        } else {
            ed = 0;
        }
    }
    // search alternate parent path since didn't find it in this one.
    if( nextMiEntity ) {
        return nextMiEntity->GetMiEntity( entName );
    }
    return 0;
}


/**
 * Returns a STEPattribute or NULL
 * \param nm The name to search for.
 * \param entity If not null, check that the attribute comes from this entity. When MakeDerived is called from generated code, this is used to ensure that the correct attr is marked as derived. Issue #232
 */
STEPattribute * SDAI_Application_instance::GetSTEPattribute( const char * nm, const char * entity ) {
    if( !nm ) {
        return 0;
    }
    STEPattribute * a = 0;

    ResetAttributes();
    // keep going until no more attributes, or attribute is found
    while( ( a = NextAttribute() ) ) {
        if( 0 == strcmp( nm, a ->Name() ) &&
            //if entity isn't null, check for a match. NOTE: should we use IsA(), CanBe(), or Name()?
            ( entity ? ( 0 != a->aDesc->Owner().IsA( entity ) ) : true ) ) {
            break;
        }
    }

    return a;
}

STEPattribute * SDAI_Application_instance::MakeRedefined( STEPattribute * redefiningAttr, const char * nm ) {
    // find the attribute being redefined
    STEPattribute * a = GetSTEPattribute( nm );

    // assign its pointer to the redefining attribute
    if( a ) {
        a->RedefiningAttr( redefiningAttr );
    }
    return a;
}

/**
 * Returns a STEPattribute or NULL. If found, marks as derived.
 * \param nm The name to search for.
 * \param entity If not null, check that the attribute comes from this entity. When called from generated code, this is used to ensure that the correct attr is marked as derived. Issue #232
 */
STEPattribute * SDAI_Application_instance::MakeDerived( const char * nm, const char * entity ) {
    STEPattribute * a = GetSTEPattribute( nm, entity );
    if( a ) {
        a ->Derive();
    }
    return a;
}

void SDAI_Application_instance::CopyAs( SDAI_Application_instance * other ) {
    int numAttrs = AttributeCount();
    ResetAttributes();
    other -> ResetAttributes();

    STEPattribute * this_attr = 0;
    STEPattribute * other_attr = 0;
    while( ( this_attr = NextAttribute() ) && numAttrs ) {
        other_attr = other -> NextAttribute();
        this_attr -> ShallowCopy( other_attr );
        numAttrs--;
    }
}


const char * SDAI_Application_instance::EntityName( const char * schnm ) const {
    if( !eDesc ) {
        return NULL;
    }
    return eDesc->Name( schnm );
}

/**
 * Checks if a given SDAI_Application_instance is the same
 * type as this one
 */
const EntityDescriptor * SDAI_Application_instance::IsA( const EntityDescriptor * ed ) const {
    if( !eDesc ) {
        return NULL;
    }
    return ( eDesc->IsA( ed ) );
}

/**
 * Checks the validity of the current attribute values for the entity
 */
Severity SDAI_Application_instance::ValidLevel( ErrorDescriptor * error, InstMgrBase * im,
        int clearError ) {
    ErrorDescriptor err;
    if( clearError ) {
        ClearError();
    }
    int n = attributes.list_length();
    for( int i = 0 ; i < n; i++ ) {
        if( !( attributes[i].aDesc->AttrType() == AttrType_Redefining ) )
            error->GreaterSeverity( attributes[i].ValidLevel(
                                        attributes[i].asStr().c_str(), &err, im, 0 ) );
    }
    return error->severity();
}

/**
 * clears all attr's errors
 */
void SDAI_Application_instance::ClearAttrError() {
    int n = attributes.list_length();
    for( int i = 0 ; i < n; i++ ) {
        attributes[i].Error().ClearErrorMsg();
    }
}

/**
 * clears entity's error and optionally all attr's errors
 */
void SDAI_Application_instance::ClearError( int clearAttrs ) {
    _error.ClearErrorMsg();
    if( clearAttrs ) {
        ClearAttrError();
    }
}

/**************************************************************//**
** \param out -- stream to write to
** \details
** Side Effects:  writes out the SCOPE section for an entity
** Status:  stub FIXME
*******************************************************************/
void SDAI_Application_instance::beginSTEPwrite( ostream & out ) {
    out << "begin STEPwrite ... \n" ;
    out.flush();

    int n = attributes.list_length();
    for( int i = 0 ; i < n; i++ ) {
        if( attributes[i].Type() == ENTITY_TYPE
                && *( attributes[i].ptr.c ) != S_ENTITY_NULL ) {
            ( *( attributes[i].ptr.c ) ) -> STEPwrite();
        }
    }
}

/**************************************************************//**
** \param out -- stream to write to
** \details
** Side Effects:  writes out the data associated with an instance
**                  in STEP format
** Problems:  does not print out the SCOPE section of an entity
**
*******************************************************************/
void SDAI_Application_instance::STEPwrite( ostream & out, const char * currSch,
        int writeComments ) {
    std::string tmp;
    if( writeComments && !p21Comment.empty() ) {
        out << p21Comment;
    }
    out << "#" << STEPfile_id << "=" << StrToUpper( EntityName( currSch ), tmp )
        << "(";
    int n = attributes.list_length();

    for( int i = 0 ; i < n; i++ ) {
        if( !( attributes[i].aDesc->AttrType() == AttrType_Redefining ) ) {
            if( i > 0 ) {
                out << ",";
            }
            ( attributes[i] ).STEPwrite( out, currSch );
        }
    }
    out << ");\n";
}

void SDAI_Application_instance::endSTEPwrite( ostream & out ) {
    out << "end STEPwrite ... \n" ;
    out.flush();
}

void SDAI_Application_instance::WriteValuePairs( ostream & out,
        const char * currSch,
        int writeComments, int mixedCase ) {
    std::string s, tmp, tmp2;

    if( writeComments && !p21Comment.empty() ) {
        out << p21Comment;
    }

    if( eDesc ) {
        if( mixedCase ) {
            out << "#" << STEPfile_id << " "
                << eDesc->QualifiedName( s ) << endl;
        } else {
            out << "#" << STEPfile_id << " "
                << StrToUpper( eDesc->QualifiedName( s ), tmp ) << endl;
        }
    }

    int n = attributes.list_length();

    for( int i = 0 ; i < n; i++ ) {
        if( !( attributes[i].aDesc->AttrType() == AttrType_Redefining ) ) {
            if( mixedCase ) {
                out << "\t"
                    << attributes[i].aDesc->Owner().Name( s.c_str() )
                    << "." << attributes[i].aDesc->Name() << " ";
            } else {
                out << "\t"
                    << StrToUpper( attributes[i].aDesc->Owner().Name( s.c_str() ), tmp )
                    << "." << StrToUpper( attributes[i].aDesc->Name(), tmp2 ) << " ";
            }
            ( attributes[i] ).STEPwrite( out, currSch );
            out << endl;
        }
    }
    out << endl;
}


/******************************************************************
 ** Procedure:  STEPwrite
 ** Problems:  does not print out the SCOPE section of an entity
 ******************************************************************/
const char * SDAI_Application_instance::STEPwrite( std::string & buf, const char * currSch ) {
    buf.clear();

    char instanceInfo[BUFSIZ];

    std::string tmp;
    sprintf( instanceInfo, "#%d=%s(", STEPfile_id, StrToUpper( EntityName( currSch ), tmp ) );
    buf.append( instanceInfo );

    int n = attributes.list_length();

    for( int i = 0 ; i < n; i++ ) {
        if( !( attributes[i].aDesc->AttrType() == AttrType_Redefining ) ) {
            if( i > 0 ) {
                buf.append( "," );
            }
            tmp = attributes[i].asStr( currSch ) ;
            buf.append( tmp );
        }
    }
    buf.append( ");" );
    return const_cast<char *>( buf.c_str() );
}

void SDAI_Application_instance::PrependEntityErrMsg() {
    char errStr[BUFSIZ];
    errStr[0] = '\0';

    if( _error.severity() == SEVERITY_NULL ) {
        //  if there is not an error already
        sprintf( errStr, "\nERROR:  ENTITY #%d %s\n", GetFileId(),
                 EntityName() );
        _error.PrependToDetailMsg( errStr );
    }
}

/**************************************************************//**
 ** \param c --  character which caused error
 ** \param i --  index of attribute which caused error
 ** \param in -- (used in STEPcomplex) input stream for recovery
 ** Reports the error found, reads until it finds the end of an
 ** instance. i.e. a close quote followed by a semicolon optionally
 ** having whitespace between them.
 ******************************************************************/
void SDAI_Application_instance::STEPread_error( char c, int i, istream & in, const char * schnm ) {
    (void) in;
    char errStr[BUFSIZ];
    errStr[0] = '\0';

    if( _error.severity() == SEVERITY_NULL ) {
        //  if there is not an error already
        sprintf( errStr, "\nERROR:  ENTITY #%d %s\n", GetFileId(),
                 EntityName() );
        _error.PrependToDetailMsg( errStr );
    }

    if( ( i >= 0 ) && ( i < attributes.list_length() ) ) { // i is an attribute
        Error().GreaterSeverity( SEVERITY_WARNING );
        sprintf( errStr, "  invalid data before type \'%s\'\n",
                 attributes[i].TypeName() );
        _error.AppendToDetailMsg( errStr );
    } else {
        Error().GreaterSeverity( SEVERITY_INPUT_ERROR );
        _error.AppendToDetailMsg( "  No more attributes were expected.\n" );
    }

    std::string tmp;
    STEPwrite( tmp, schnm ); // STEPwrite writes to a static buffer inside function
    _error.AppendToDetailMsg( "  The invalid instance to this point looks like :\n" );
    _error.AppendToDetailMsg( tmp );
    _error.AppendToDetailMsg( "\nUnexpected character: " );
    _error.AppendToDetailMsg( c );
    _error.AppendToDetailMsg( '\n' );

    sprintf( errStr, "\nfinished reading #%d\n", STEPfile_id );
    _error.AppendToDetailMsg( errStr );
    return;
}

/**************************************************************//**
 ** \returns Severity, error information
 **          SEVERITY_NULL - no errors
 **          SEVERITY_USERMSG - checked as much as possible, could still
 **            be error - e.g. entity didn't match base entity type.
 **          SEVERITY_INCOMPLETE - data is missing and required.
 **          SEVERITY_WARNING - errors, but can recover
 **            <= SEVERITY_INPUT_ERROR - fatal error, can't recover
 ** \details  reads the values for an entity from an input stream
 **            in STEP file format starting at the open paren and
 **            ending with the semi-colon
 ** Side Effects:  gobbles up input stream
 ** Status:
 ******************************************************************/
Severity SDAI_Application_instance::STEPread( int id,  int idIncr,
        InstMgrBase * instance_set, istream & in,
        const char * currSch, bool useTechCor, bool strict ) {
    STEPfile_id = id;
    char c = '\0';
    char errStr[BUFSIZ];
    errStr[0] = '\0';
    Severity severe;
    int i = 0;

    ClearError( 1 );

    in >> ws;
    in >> c; // read the open paren
    if( c != '(' ) {
        PrependEntityErrMsg();
        _error.AppendToDetailMsg(
            "  Missing initial open paren... Trying to recover.\n" );
        in.putback( c ); // assume you can recover by reading 1st attr value
    }
    ReadTokenSeparator( in, &p21Comment );

    int n = attributes.list_length();
    if( n == 0 ) { // no attributes
        in >> c; // look for the close paren
        if( c == ')' ) {
            return _error.severity();
        }
    }

    for( i = 0 ; i < n; i++ ) {
        ReadTokenSeparator( in, &p21Comment );
        if( attributes[i].aDesc->AttrType() == AttrType_Redefining ) {
            in >> ws;
            c = in.peek();
            if( !useTechCor ) { // i.e. use pre-technical corrigendum encoding
                in >> c; // read what should be the '*'
                in >> ws;
                if( c == '*' ) {
                    in >> c; // read the delimiter i.e. ',' or ')'
                } else {
                    severe = SEVERITY_INCOMPLETE;
                    PrependEntityErrMsg(); // adds entity info if necessary

                    // set the severity for this entity
                    _error.GreaterSeverity( severe );
                    sprintf( errStr, "  %s :  ", attributes[i].Name() );
                    _error.AppendToDetailMsg( errStr ); // add attr name
                    _error.AppendToDetailMsg(
                        "Since using pre-technical corrigendum... missing asterisk for redefined attr.\n" );
                    _error.AppendToUserMsg(
                        "Since using pre-technical corrigendum... missing asterisk for redefined attr. " );
                }
            } else { // using technical corrigendum
                // should be nothing to do except loop again unless...
                // if at end need to have read the closing paren.
                if( c == ')' ) { // assume you are at the end so read last char
                    in >> c;
                }
                cout << "Entity #" << STEPfile_id
                     << " skipping redefined attribute "
                     << attributes[i].aDesc->Name() << endl << endl << flush;
            }
            // increment counter to read following attr since these attrs
            // aren't written or read => there won't be a delimiter either
        } else {
            attributes[i].STEPread( in, instance_set, idIncr, currSch, strict );
            in >> c; // read the , or ) following the attr read

            severe = attributes[i].Error().severity();

            if( severe <= SEVERITY_USERMSG ) {
                // if there is some type of error
                PrependEntityErrMsg();

                // set the severity for this entity
                _error.GreaterSeverity( severe );
                sprintf( errStr, "  %s :  ", attributes[i].Name() );
                _error.AppendToDetailMsg( errStr ); // add attr name
                _error.AppendToDetailMsg( attributes[i].Error().DetailMsg() );  // add attr error
                _error.AppendToUserMsg( attributes[i].Error().UserMsg() );
            }
        }

        // if technical corrigendum redefined, input is at next attribute value
        // if pre-technical corrigendum redefined, don't process
        if( ( !( attributes[i].aDesc->AttrType() == AttrType_Redefining ) ||
                !useTechCor ) &&
                !( ( c == ',' ) || ( c == ')' ) ) ) { //  input is not a delimiter
            PrependEntityErrMsg();

            _error.AppendToDetailMsg(
                "Delimiter expected after attribute value.\n" );
            if( !useTechCor ) {
                _error.AppendToDetailMsg(
                    "I.e. since using pre-technical corrigendum, redefined " );
                _error.AppendToDetailMsg(
                    "attribute is mapped as an asterisk so needs delimiter.\n" );
            }
            CheckRemainingInput( in, &_error, "ENTITY", ",)" );
            if( !in.good() ) {
                return _error.severity();
            }
            if( _error.severity() <= SEVERITY_INPUT_ERROR ) {
                return _error.severity();
            }
        } else if( c == ')' ) {
            while( i < n - 1 ) {
                i++; // check if following attributes are redefined
                if( !( attributes[i].aDesc->AttrType() == AttrType_Redefining ) ) {
                    PrependEntityErrMsg();
                    _error.AppendToDetailMsg( "Missing attribute value[s].\n" );
                    // recoverable error
                    _error.GreaterSeverity( SEVERITY_WARNING );
                    return _error.severity();
                }
                i++;
            }
            return _error.severity();
        }
    }
    STEPread_error( c, i, in, currSch );
//  code fragment imported from STEPread_error
//  for some currently unknown reason it was commented out of STEPread_error
    errStr[0] = '\0';
    in.clear();
    int foundEnd = 0;
    std::string tmp;
    tmp = "";

    // Search until a close paren is found followed by (skipping optional
    // whitespace) a semicolon
    while( in.good() && !foundEnd ) {
        while( in.good() && ( c != ')' ) ) {
            in.get( c );
            tmp += c;
        }
        if( in.good() && ( c == ')' ) ) {
            in >> ws; // skip whitespace
            in.get( c );
            tmp += c;
            if( c == ';' ) {
                foundEnd = 1;
            }
        }
    }
    _error.AppendToDetailMsg( tmp.c_str() );
    sprintf( errStr, "\nfinished reading #%d\n", STEPfile_id );
    _error.AppendToDetailMsg( errStr );
// end of imported code
    return _error.severity();
}

/// read an entity reference and return a pointer to the SDAI_Application_instance
SDAI_Application_instance * ReadEntityRef( istream & in, ErrorDescriptor * err, const char * tokenList,
        InstMgrBase * instances, int addFileId ) {
    char c;
    char errStr[BUFSIZ];
    errStr[0] = '\0';

    in >> ws;
    in >> c;
    switch( c ) {
        case '@':
            err->AppendToDetailMsg( "Use of @ instead of # to identify entity.\n" );
            err->GreaterSeverity( SEVERITY_WARNING );
            // no break statement here on purpose
        case '#': {
            int id = -1;
            in >>  id;
            if( in.fail() ) { //  there's been an error in input
                sprintf( errStr, "Invalid entity reference value.\n" );
                err->AppendToDetailMsg( errStr );
                err->AppendToUserMsg( errStr );
                err->GreaterSeverity( SEVERITY_WARNING );
                CheckRemainingInput( in, err, "Entity Reference", tokenList );
                return S_ENTITY_NULL;
            } else { // found an entity id
                // check to make sure garbage does not follow the id
                CheckRemainingInput( in, err, "Entity Reference", tokenList );

                id += addFileId;
                if( !instances ) {
                    cerr << "Internal error:  " << __FILE__ <<  __LINE__
                         << "\n" << _POC_ "\n";
                    sprintf( errStr,
                             "STEPread_reference(): %s - entity #%d %s.\n",
                             "BUG - cannot read reference without the InstMgr",
                             id, "is unknown" );
                    err->AppendToDetailMsg( errStr );
                    err->AppendToUserMsg( errStr );
                    err->GreaterSeverity( SEVERITY_BUG );
                    return S_ENTITY_NULL;
                }

                //  lookup which object has id as its instance id
                SDAI_Application_instance * inst;
                /* If there is a ManagerNode it should have a SDAI_Application_instance */
                MgrNodeBase * mn = 0;
                mn = instances->FindFileId( id );
                if( mn ) {
                    inst =  mn->GetSTEPentity() ;
                    if( inst ) {
                        return ( inst );
                    } else {
                        cerr << "Internal error:  " << __FILE__ <<  __LINE__
                             << "\n" << _POC_ "\n";
                        sprintf( errStr,
                                 "STEPread_reference(): %s - entity #%d %s.\n",
                                 "BUG - MgrNode::GetSTEPentity returned NULL pointer",
                                 id, "is unknown" );
                        err->AppendToDetailMsg( errStr );
                        err->AppendToUserMsg( errStr );
                        err->GreaterSeverity( SEVERITY_BUG );
                        return S_ENTITY_NULL;
                    }
                } else {
                    sprintf( errStr, "Reference to non-existent ENTITY #%d.\n",
                             id );
                    err->AppendToDetailMsg( errStr );
                    err->AppendToUserMsg( errStr );
                    err->GreaterSeverity( SEVERITY_WARNING );
                    return S_ENTITY_NULL;
                }
            }
        }
        default: {
            in.putback( c );
            // read past garbage up to delim in tokenList
            // if tokenList is null it will not read anything
            CheckRemainingInput( in, err, "Entity Reference", tokenList );
            return S_ENTITY_NULL;
        }
    }
}

/// read an entity reference and return a pointer to the SDAI_Application_instance
SDAI_Application_instance * ReadEntityRef( const char * s, ErrorDescriptor * err, const char * tokenList,
        InstMgrBase * instances, int addFileId ) {
    istringstream in( ( char * )s );
    return ReadEntityRef( in, err, tokenList, instances, addFileId );
}

/// return SEVERITY_NULL if se's entity type matches the supplied entity type
Severity EntityValidLevel( SDAI_Application_instance * se,
                           const TypeDescriptor * ed, // entity type that entity se needs
                           // to match. (this must be an
                           // EntityDescriptor)
                           ErrorDescriptor * err ) {
    char messageBuf [BUFSIZ];
    messageBuf[0] = '\0';

    if( !ed || ( ed->NonRefType() != ENTITY_TYPE ) ) {
        err->GreaterSeverity( SEVERITY_BUG );
        sprintf( messageBuf,
                 " BUG: EntityValidLevel() called with %s",
                 "missing or invalid EntityDescriptor\n" );
        err->AppendToUserMsg( messageBuf );
        err->AppendToDetailMsg( messageBuf );
        cerr << "Internal error:  " << __FILE__ << ":" <<  __LINE__
             << "\n" << _POC_ "\n";
        return SEVERITY_BUG;
    }
    if( !se || ( se == S_ENTITY_NULL ) ) {
        err->GreaterSeverity( SEVERITY_BUG );
        sprintf( messageBuf,
                 " BUG: EntityValidLevel() called with null pointer %s\n",
                 "for SDAI_Application_instance argument." );
        err->AppendToUserMsg( messageBuf );
        err->AppendToDetailMsg( messageBuf );
        cerr << "Internal error:  " << __FILE__ << ":" <<  __LINE__
             << "\n" << _POC_ "\n";
        return SEVERITY_BUG;
    }

    // DAVE: Can an entity be used in an Express TYPE so that this
    // EntityDescriptor would have type REFERENCE_TYPE -- it looks like NO

    else if( se->getEDesc() ) {
        // is se a descendant of ed?
        if( se->getEDesc()->IsA( ed ) ) {
            return SEVERITY_NULL;
        } else {
            if( se->IsComplex() ) {
                // This way assumes that the complex has all it's parts i.e. the
                // complete hierarchy so it should be able to find ed's part if
                // it is an ed.
                STEPcomplex * sc = ( ( STEPcomplex * )se )->sc;
                if( sc->EntityExists( ed->Name() ) ) {
                    return SEVERITY_NULL;
                }
            }
            err->GreaterSeverity( SEVERITY_WARNING );
            sprintf( messageBuf,
                     " Entity #%d exists but is not a %s or descendant.\n",
                     se->STEPfile_id, ed->Name() );
            err->AppendToUserMsg( messageBuf );
            err->AppendToDetailMsg( messageBuf );
            return SEVERITY_WARNING;
        }
    } else {
        err->GreaterSeverity( SEVERITY_BUG );
        sprintf( messageBuf,
                 " BUG: EntityValidLevel(): SDAI_Application_instance #%d has a %s",
                 se->STEPfile_id, "missing or invalid EntityDescriptor\n" );
        err->AppendToUserMsg( messageBuf );
        err->AppendToDetailMsg( messageBuf );
        cerr << "Internal error:  " << __FILE__ <<  __LINE__
             << "\n" << _POC_ "\n";
        return SEVERITY_BUG;
    }
}

/**
 * return 1 if attrValue has the equivalent of a null value.
 * DAVE: Is this needed will sscanf return 1 if assignment suppression is used?
 */
int SetErrOnNull( const char * attrValue, ErrorDescriptor * error ) {
    char scanBuf[BUFSIZ];
    scanBuf[0] = '\0';

    std::stringstream fmtstr;
    fmtstr << " %" << BUFSIZ -1 << "s ";
    //fmtstr contains " %ns " where n is BUFSIZ -1

    int numFound = sscanf( ( char * )attrValue , fmtstr.str().c_str() , scanBuf );

    if( numFound == EOF ) {
        error->GreaterSeverity( SEVERITY_INCOMPLETE );
        return 1;
    }
    return 0;
}

/**
** return SEVERITY_NULL if attrValue has a valid entity reference
** This function accepts an entity reference in two forms that is with or
** without the # sign: e.g. either #23 or 23 will be read.
** If non-whitespace characters follow the entity reference an error is set.
*/
Severity EntityValidLevel( const char * attrValue, // string contain entity ref
                           const TypeDescriptor * ed, // entity type that entity in
                           // attrValue (if it exists) needs
                           // to match. (this must be an
                           // EntityDescriptor)
                           ErrorDescriptor * err, InstMgrBase * im, int clearError ) {
    char tmp [BUFSIZ];
    tmp[0] = '\0';
    char messageBuf [BUFSIZ];
    messageBuf[0] = '\0';
    std::stringstream fmtstr1, fmtstr2;

    if( clearError ) {
        err->ClearErrorMsg();
    }

    int fileId;
    MgrNodeBase * mn = 0;

    // fmtstr1 contains "#%d %ns" where n is BUFSIZ-1
    fmtstr1 << " #%d %" << BUFSIZ - 1 << "s ";

    // fmtstr2 contains "%d %ns" where n is BUFSIZ-1
    fmtstr2 << " %d %" << BUFSIZ - 1 << "s ";
 
    // check for both forms:  #id or id
    int found1 = sscanf( ( char * )attrValue, fmtstr1.str().c_str() , &fileId, tmp );
    int found2 = sscanf( ( char * )attrValue, fmtstr2.str().c_str() , &fileId, tmp );

    if( ( found1 > 0 ) || ( found2 > 0 ) ) {
        if( ( found1 == 2 ) || ( found2 == 2 ) ) {
            sprintf( messageBuf,
                     " Attribute's Entity Reference %s is %s data \'%s\'.\n",
                     attrValue, "followed by invalid", tmp );
            err->AppendToUserMsg( messageBuf );
            err->AppendToDetailMsg( messageBuf );
            err->GreaterSeverity( SEVERITY_WARNING );
        }
        mn = im->FindFileId( fileId );
        if( mn ) {
            SDAI_Application_instance * se = mn->GetSTEPentity();
            return EntityValidLevel( se, ed, err );
        } else {
            sprintf( messageBuf,
                     " Attribute's Entity Reference %s does not exist.\n",
                     attrValue );
            err->AppendToUserMsg( messageBuf );
            err->AppendToDetailMsg( messageBuf );
            err->GreaterSeverity( SEVERITY_WARNING );
            return SEVERITY_WARNING;
        }
    }

    // if the attrValue contains no value return
    if( SetErrOnNull( attrValue, err ) ) {
        return err->severity();
    }

    sprintf( messageBuf, "Invalid attribute entity reference value: '%s'.\n",
             attrValue );
    err->AppendToUserMsg( messageBuf );
    err->AppendToDetailMsg( messageBuf );
    err->GreaterSeverity( SEVERITY_WARNING );
    return SEVERITY_WARNING;
}

/**************************************************************//**
** Description:  used to cycle through the list of attributes
** Side Effects:  increments the current position in the attribute list
** Status:  untested 7/31/90
** \Returns  reference to an attribute pointer
******************************************************************/
STEPattribute * SDAI_Application_instance::NextAttribute()  {
    int i = AttributeCount();
    ++_cur;
    if( i < _cur ) {
        return 0;
    }
    return &attributes [_cur - 1];
}

int SDAI_Application_instance::AttributeCount()  {
    return  attributes.list_length();
}

const iAstruct SDAI_Application_instance::getInvAttr( const Inverse_attribute * const ia ) const {
    iAstruct ias;
    memset( &ias, 0, sizeof ias );
    iAMap_t::const_iterator it = iAMap.find( ia );
    if( it != iAMap.end() ) {
        ias = (*it).second;
    }
    return ias;
}

const SDAI_Application_instance::iAMap_t::value_type SDAI_Application_instance::getInvAttr( const char * name ) const {
    iAMap_t::const_iterator it = iAMap.begin();
    for( ; it != iAMap.end(); ++it ) {
        if( 0 == strcmp( it->first->Name(), name) ) {
            return *it;
        }
    }
    iAstruct z;
    memset( &z, 0, sizeof z );
    iAMap_t::value_type nil( (Inverse_attribute *) nullptr, z );
    return nil;
}

void SDAI_Application_instance::setInvAttr( const Inverse_attribute * const ia, const iAstruct ias )  {
    iAMap_t::iterator it = iAMap.find(ia);
    if( it != iAMap.end() ) {
        it->second = ias;
    } else {
        iAMap.insert( iAMap_t::value_type( ia, ias ) );
    }
}
