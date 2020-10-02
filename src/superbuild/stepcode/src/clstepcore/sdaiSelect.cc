
/*
* NIST STEP Core Class Library
* clstepcore/sdaiSelect.cc
* April 1997
* Dave Helfrick
* KC Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <stdio.h> // to get the BUFSIZ #define
#include <ExpDict.h>
#include <sstream>
#include <string>
#include <sdai.h>
#include <STEPattribute.h>

#ifdef  SC_LOGGING
#include <fstream.h>
extern ofstream * logStream;
#endif

/**********
    (member) functions for the select class SDAI_Select
**********/
SDAI_Select::SDAI_Select( const SelectTypeDescriptor * s,
                          const TypeDescriptor * td )
    : _type( s ), underlying_type( td ) {
#ifdef SC_LOGGING
    *logStream << "Exiting SDAI_Select constructor." << endl;
#endif
}

SDAI_Select::~SDAI_Select() {
}

Severity SDAI_Select::severity() const {
    return _error.severity();
}

Severity  SDAI_Select::severity( Severity s ) {
    return _error.severity( s );
}

std::string SDAI_Select::Error() {
    return _error.DetailMsg();
}

void SDAI_Select::Error( const char * e ) {
    _error.DetailMsg( e );
}

void  SDAI_Select::ClearError() {
    _error.ClearErrorMsg();
}

const TypeDescriptor *
SDAI_Select::CanBe( const char * n ) const {
    return _type -> CanBe( n );
}


const TypeDescriptor *
SDAI_Select::CanBe( BASE_TYPE bt ) const {
    const TypeDescLinkNode * tdn =
        ( const TypeDescLinkNode * ) _type -> GetElements().GetHead();
    const TypeDescriptor * td = tdn -> TypeDesc();
    BASE_TYPE bt_thisnode;

    while( tdn )  {
        td = tdn -> TypeDesc();
        if( ( ( bt_thisnode = td -> NonRefType() ) == bt ) ||
                ( bt == AGGREGATE_TYPE && ( ( bt_thisnode == ARRAY_TYPE ) ||
                                            ( bt_thisnode == LIST_TYPE ) ||
                                            ( bt_thisnode == SET_TYPE ) ||
                                            ( bt_thisnode == BAG_TYPE ) ) ) ) {
            return td;    // they are the same
        }
        tdn = ( TypeDescLinkNode * )( tdn -> NextNode() );
    }
    return 0;
}

const TypeDescriptor *
SDAI_Select::CanBe( const TypeDescriptor * td ) const {
    return _type -> CanBe( td );
}

const TypeDescriptor *
SDAI_Select::CanBeSet( const char * n, const char * schnm ) const {
    return _type -> CanBeSet( n, schnm );
}

int
SDAI_Select::IsUnique( const BASE_TYPE bt ) const {
    if( bt == ARRAY_TYPE ||
            bt == LIST_TYPE ||
            bt == BAG_TYPE ||
            bt == SET_TYPE ) {
        return ( ( _type->UniqueElements() ) & AGGREGATE_TYPE );
    } else {
        return ( ( _type->UniqueElements() ) & bt );
    }
}


SDAI_String SDAI_Select::UnderlyingTypeName() const {
    return underlying_type -> Name();
}

const TypeDescriptor  * SDAI_Select::CurrentUnderlyingType() const {
    return underlying_type;
}

const TypeDescriptor *
SDAI_Select::SetUnderlyingType( const TypeDescriptor * td ) {
    //  don\'t do anything if the descriptor is bad
    if( !td || !( _type -> CanBe( td ) ) ) {
        return 0;
    }

    base_type = td -> NonRefType();

    return underlying_type = td;
}

bool SDAI_Select::exists() const {
    return underlying_type != NULL;
}

void SDAI_Select::nullify() {
    underlying_type = 0;
}

Severity SDAI_Select::SelectValidLevel( const char * attrValue, ErrorDescriptor * err,
                                        InstMgr * im, int clearError ) {
    SDAI_Select * tmp = NewSelect();
    Severity s = SEVERITY_NULL;

    istringstream strtmp( attrValue );
    s = tmp -> STEPread( strtmp, err, im );
    delete tmp;
    return s;
}

Severity SDAI_Select::StrToVal( const char * Val, const char * selectType,
                                ErrorDescriptor * err, InstMgr * instances ) {
    severity( SEVERITY_NULL );
    if( SetUnderlyingType( CanBe( selectType ) ) )

        //  the underlying type is set to a valid type
        // call read on underlying type in subclass

        switch( base_type )  {
            case ENTITY_TYPE: {
                STEPentity * tmp =
                    ReadEntityRef( Val, err, ",)", instances, 0 );
                if( tmp && ( tmp != ENTITY_NULL ) ) {
                    AssignEntity( tmp );
                    return severity();
                } else {
                    err->AppendToDetailMsg(
                        "Reference to entity that is not a valid type for SELECT.\n" );
                    nullify();
                    err->GreaterSeverity( SEVERITY_WARNING );
                    return SEVERITY_WARNING;
                }
            }

            // call StrToVal on the contents
            case STRING_TYPE:
            case AGGREGATE_TYPE:
            case ARRAY_TYPE:      // DAS
            case BAG_TYPE:        // DAS
            case SET_TYPE:        // DAS
            case LIST_TYPE:       // DAS
            case ENUM_TYPE:
            case SELECT_TYPE:
            case BOOLEAN_TYPE:
            case LOGICAL_TYPE: {
                err->GreaterSeverity( StrToVal_content( Val, instances ) );
                if( _error.severity() != SEVERITY_NULL ) {
                    err->AppendFromErrorArg( &_error );
                }
                return err->severity();
            }

            // do the same as STEPread_content
            case BINARY_TYPE:
            case NUMBER_TYPE:
            case REAL_TYPE:
            case INTEGER_TYPE:
            default: {
                istringstream strtmp( Val );
                err->GreaterSeverity( STEPread_content( strtmp ) );
                if( _error.severity() != SEVERITY_NULL ) {
                    err->AppendFromErrorArg( &_error );
                }
                return err->severity();
            }
        }
    return SEVERITY_INPUT_ERROR;
}

/** updated to Technical Corrigendum. DAS 2/4/97
 * This function does the following:
 */
Severity SDAI_Select::STEPread( istream & in, ErrorDescriptor * err,
                                InstMgr * instances, const char * utype,
                                int addFileId, const char * currSch ) {
    char c = '\0';
    std::string tmp;

#ifdef SC_LOGGING
//    *logStream << "DAVE ERR Entering SDAI_Select::STEPread." << endl;
#endif
    // find out what case we have
    //  NOTE case C falls out of recursive calls in cases A and B

    /**
    ** This section of code is used to read a value belonging to a select
    ** contained in another select. If you have read the text part of the
    ** TYPED_PARAMETER and it needs to fall down thru some levels of contained
    ** select types, then the text is passed down to each select in the utype
    ** parameter as STEPread is called on each contained select type.DAS 2/4/97
    */
    if( utype ) {
        if( SetUnderlyingType( CanBeSet( utype, currSch ) ) ) {
            //  assign the value to the underlying type
            in >> ws; // skip white space
            if( ( underlying_type->Type() == REFERENCE_TYPE ) &&
                    ( underlying_type->NonRefType() == sdaiSELECT ) ) {
                // See comments below for a similar code segment.
                STEPread_content( in, instances, 0, addFileId, currSch );
            } else {
                STEPread_content( in, instances, utype, addFileId, currSch );
            }
            err->AppendToDetailMsg( Error() );
            err->GreaterSeverity( severity() );
        }
        return err->severity();
    }
    in >> ws;
    in >> c;

    /**
    ** the if part of this code reads a value according to the Technical
    ** Corrigendum for Part 21 (except for Entity instance refs which are read
    ** in the else part - everything else in the else stmt is invalid). The
    ** idea here is to read text and find out if it is specifying the final
    ** type or is a simple defined type which is typed to be a select some
    ** number of levels down. The first case will cause the value to be read
    ** directly by STEPread_content() or will cause STEPread_content() to pass
    ** the text naming the type to the underlying Select containing it. When
    ** the latter is the case, STEPread_content calls STEPread for the
    ** contained select and passes in the text. The convoluted case is when
    ** the text describes a simple defined type that translates directly into
    ** a select some number of levels down. In this case there will be more
    ** text inside the containing parens to indicate the type. Since the text
    ** specifying the type still needs to be read utype is passed as null which
    ** will cause the contained Select STEPread function to read it. DAS 2/4/97
    */

    if( isalpha( c ) ) { //  case B
        int eot = 0; // end of token flag
        //  token is a type name - get the type
        while( ( c != '(' ) && in.good() ) {
            if( !eot && !( eot = isspace( c ) ) )
                // as long as eot hasn\'t been reached keep appending
            {
                tmp += c;
            }
            in >> c;
        }

        //  check for valid type and set the underlying type
        if( SetUnderlyingType( CanBeSet( tmp.c_str(), currSch ) ) ) {
            /**
            ** Assign the value to the underlying type.  CanBeSet() is a
            ** slightly modified CanBe().  It ensures that a renamed select
            ** type (see big comment below) "CantBe" one of its member items.
            ** This is because such a select type requires its own name to
            ** appear first ("selX("), so even if we read a valid element of
            ** selX, selX can't be the underlying type.  That can only be the
            ** case if "selX" appears first and is what we just read.
            */
            in >> ws; // skip white space
            if( ( underlying_type->Type() == REFERENCE_TYPE ) &&
                    ( underlying_type->NonRefType() == sdaiSELECT ) ) {
                /**
                 * This means (1) that the underlying type is itself a select
                ** (cond 2), and (2) it's not defined in the EXPRESS as a
                ** select, but as a renamed select.  (E.g., TYPE sel2 = sel1.)
                ** In such a case, according to the TC, "sel2(" must appear in
                ** the text.  (We must have found one for SetUnderlyingType()
                ** above to set underlying_type to sel2.)  If all of the above
                ** is true, we read "sel2" but not the value of sel2.  We
                ** therefore do not pass down what we read to STEPread_content
                ** below, so that we can still read the value of sel2.  If,
                ** however, under_type was a non-renamed select, no "sel1"
                ** would appear (according to TC) and we already read the value
                ** of sel1.  If so, we pass the already-read value down.
                 */
                STEPread_content( in, instances, 0, addFileId, currSch );
            } else {
                /**
                ** In most cases (see above note), we've already read the value
                ** we're interested in.
                ** This also handles all other cases? other than the if part
                ** above and elements of type entity ref?
                */
                STEPread_content( in, instances, tmp.c_str(), addFileId,
                                  currSch );
                // STEPread_content uses the ErrorDesc data member from the
                // SDAI_Select class
            }
            err->AppendToDetailMsg( Error() );
            err->GreaterSeverity( severity() );
            in >> ws >> c;
            if( c != ')' ) {
                err->AppendToDetailMsg(
                    "Bad data or missing closing ')' for SELECT type.\n" );
                err->GreaterSeverity( SEVERITY_WARNING );
                in.putback( c );
#ifdef SC_LOGGING
//    *logStream << "DAVE ERR Exiting SDAI_Select::STEPread for " << _type->Name() << endl;
#endif
                return SEVERITY_WARNING;
            }
#ifdef SC_LOGGING
//    *logStream << "DAVE ERR Exiting SDAI_Select::STEPread for " << _type->Name() << endl;
#endif
            return err->severity();
        } else { // ERROR  -- the type wasn't one of the choices
            if( !in.good() ) {
                err->GreaterSeverity( SEVERITY_INPUT_ERROR );
#ifdef SC_LOGGING
//    *logStream << "DAVE ERR Exiting SDAI_Select::STEPread for " << _type->Name() << endl;
#endif
                return SEVERITY_INPUT_ERROR;
            } else {
                err->AppendToDetailMsg(
                    "The type name for the SELECT type is not valid.\n" );
                err->GreaterSeverity( SEVERITY_WARNING );
#ifdef SC_LOGGING
//    *logStream << "DAVE ERR Exiting SDAI_Select::STEPread for " << _type->Name() << endl;
#endif
                return SEVERITY_WARNING;
            }
        }
    }
    /**
    ** NOTE **** anything that gets read below here is invalid according to the
    ** Technical Corrigendum for Part 21 (except for reading an entity instance
    ** where below is the only place they get read). I am leaving all of this
    ** here since it is valuable to 'read what you can' and flag an error which
    ** is what it does (for every type but entity instance refs). DAS 2/4/97
    */

    else { /// case A
        switch( c ) {
            case '$':
                nullify();
                err->GreaterSeverity( SEVERITY_INCOMPLETE );
#ifdef SC_LOGGING
//    *logStream << "DAVE ERR Exiting SDAI_Select::STEPread for " << _type->Name() << endl;
#endif
                return SEVERITY_INCOMPLETE;

            case ',':
            case '\0':
                // ERROR  IN INPUT
                in.putback( c );
                err->AppendToDetailMsg( "No value found for SELECT type.\n" );
                err->GreaterSeverity( SEVERITY_WARNING );
#ifdef SC_LOGGING
//    *logStream << "DAVE ERR Exiting SDAI_Select::STEPread for " << _type->Name() << endl;
#endif
                return SEVERITY_WARNING;

            case '.': // assign enum
                base_type = ENUM_TYPE;
                err->AppendToDetailMsg( "Invalid Enumeration, Logical, or Boolean value in SELECT type.\n" );
                err->GreaterSeverity( SEVERITY_WARNING );
                break;
                // set the underlying type
                // call STEPread
                // return

            case '\'': // assign string
                base_type = STRING_TYPE;
                err->AppendToDetailMsg( "Invalid String value in SELECT type.\n" );
                err->GreaterSeverity( SEVERITY_WARNING );
                break;

            case '"': // assign string
                base_type = BINARY_TYPE;
                err->AppendToDetailMsg( "Invalid Binary value in SELECT type.\n" );
                err->GreaterSeverity( SEVERITY_WARNING );
                break;

            case '#':
                base_type = ENTITY_TYPE;
                break;
                // call STEPread_reference
                // set the underlying type

                // assign entity
                // read the reference
                // match type to underlying type
                // assign the value
                // set the underlying type

            case '(': {
                err->AppendToDetailMsg( "Invalid aggregate value in SELECT type.\n" );
                err->GreaterSeverity( SEVERITY_WARNING );
                char n;
                in >> n;
                in.putback( n );
                if( isalpha( n ) ) {
                    base_type = SELECT_TYPE;
                } else {
                    base_type = AGGREGATE_TYPE;
                }
                break;
            }

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '-':
                err->AppendToDetailMsg( "Invalid Integer or Real value in SELECT type.\n" );
                err->GreaterSeverity( SEVERITY_WARNING );
                if( CanBe( REAL_TYPE ) ) {
                    base_type = REAL_TYPE;
                } else {
                    base_type = INTEGER_TYPE;
                }
                break;

            default:
                // ambiguous - ERROR:  underlying type should have been set
                err->AppendToDetailMsg(
                    "type for SELECT could not be determined from value.\n" );
                nullify();
                in.putback( c );
                err->GreaterSeverity( SEVERITY_WARNING );
#ifdef SC_LOGGING
//    *logStream << "DAVE ERR Exiting SDAI_Select::STEPread for " << _type->Name() << endl;
#endif
                return SEVERITY_WARNING;
        }

        in.putback( c );

        // now the type descriptor should be derivable from the base_type

        // if it's not issue a warning
        if( _type && !( IsUnique( base_type ) ) )  {
            err->AppendToDetailMsg( "Value for SELECT will be assigned to first possible choice.\n" );
            err->GreaterSeverity( SEVERITY_USERMSG );
        }

        if( base_type == ENTITY_TYPE ) {
            // you don\'t know if this is an ENTITY or a SELECT
            // have to do this here - not in STEPread_content
            STEPentity * tmp =
                ReadEntityRef( in, err, ",)", instances, addFileId );
            if( tmp && ( tmp != ENTITY_NULL ) && AssignEntity( tmp ) ) {
#ifdef SC_LOGGING
//    *logStream << "DAVE ERR Exiting SDAI_Select::STEPread for " << _type->Name() << endl;
#endif
                return SEVERITY_NULL;
            } else {
                err->AppendToDetailMsg(
                    "Reference to entity that is not a valid type for SELECT.\n" );
                nullify();
                err->GreaterSeverity( SEVERITY_WARNING );
#ifdef SC_LOGGING
//    *logStream << "DAVE ERR Exiting SDAI_Select::STEPread for " << _type->Name() << endl;
#endif
                return SEVERITY_WARNING;
            }
        } else if( SetUnderlyingType( CanBe( base_type ) ) ) {
            STEPread_content( in, instances, 0, addFileId );
        }

        else { // ERROR  -- the type wasn\'t one of the choices
            err->AppendToDetailMsg(
                "The type of the SELECT type is not valid.\n" );
            err->GreaterSeverity( SEVERITY_WARNING );
#ifdef SC_LOGGING
//    *logStream << "DAVE ERR Exiting SDAI_Select::STEPread for " << _type->Name() << endl;
#endif
            return SEVERITY_WARNING;
        }
    }
//    if (!in.good())   severity (SEVERITY_INPUT_ERROR);
#ifdef SC_LOGGING
//    *logStream << "DAVE ERR Exiting SDAI_Select::STEPread for " << _type->Name() << endl;
#endif
    return err->severity();
}


/// updated to Technical Corrigendum DAS Feb 4, 1997
void SDAI_Select::STEPwrite( ostream & out, const char * currSch )  const {
    if( !exists() ) {
        out << "$";
        return;
    }
    switch( underlying_type->NonRefType() ) {
        case sdaiINSTANCE: {
            STEPwrite_content( out );
            break;
        }
        case sdaiSELECT: { // The name of a select is never written DAS 1/31/97
            if( underlying_type->Type() == REFERENCE_TYPE ) {
                std::string s;
                out << StrToUpper( underlying_type->Name( currSch ), s ) << "(";
                STEPwrite_content( out, currSch );
                out << ")";
            } else {
                STEPwrite_content( out, currSch );
            }
            break;
        }
        case sdaiNUMBER:
        case sdaiREAL:
        case sdaiINTEGER:
        case sdaiSTRING:
        case sdaiBOOLEAN:
        case sdaiLOGICAL:
        case sdaiBINARY:
        case sdaiENUMERATION:
        case sdaiAGGR:
        case ARRAY_TYPE:
        case BAG_TYPE:
        case SET_TYPE:
        case LIST_TYPE: {
            STEPwrite_verbose( out, currSch );
            break;
        }
        case REFERENCE_TYPE: // this should never happen? DAS
        default:
            out << "ERROR Should not have gone here in SDAI_Select::STEPwrite()"
                << endl;
    }
}

void SDAI_Select::STEPwrite_verbose( ostream & out, const char * currSch ) const {
    std::string tmp;
    out << StrToUpper( CurrentUnderlyingType()->Name( currSch ), tmp ) << "(";
    STEPwrite_content( out );
    out << ")";
}

const char * SDAI_Select::STEPwrite( std::string & s, const char * currSch )  const {
    ostringstream buf;
    STEPwrite( buf, currSch );
    buf << ends;  // add the terminating \0 char
    s = buf.str();
    return const_cast<char *>( s.c_str() );
}

int SDAI_Select::set_null() {
    nullify();
    return 1;
}

int SDAI_Select::is_null() {
    return ( !exists() );
}
