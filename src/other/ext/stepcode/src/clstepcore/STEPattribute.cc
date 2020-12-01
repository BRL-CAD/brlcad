/*
* NIST STEP Core Class Library
* clstepcore/STEPattribute.cc
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <iomanip>
#include <string>

#include <read_func.h>
#include <STEPattribute.h>
#include <instmgr.h>
#include <STEPundefined.h>
#include <STEPaggregate.h>
#include <ExpDict.h>
#include <sdai.h>
#include "sc_memmgr.h"

// REAL_NUM_PRECISION is defined in STEPattribute.h, and is also used
// in aggregate real handling (STEPaggregate.cc)  -- IMS 6 Jun 95
const int Real_Num_Precision = REAL_NUM_PRECISION;

/**
** \class STEPattribute
**    Functions for manipulating attribute
**
**  FIXME KNOWN BUGS:
**    -- error reporting does not include line number information
**    -- null attributes are only handled through the attribute pointer class
**       direct access of a null attribute will not indicate whether the
**       attribute is null in all cases (although a null value is available
**       for entities and enumerations.)
**/


/// the value of the attribute is assigned from the supplied string
Severity STEPattribute::StrToVal( const char * s, InstMgrBase * instances, int addFileId ) {
    if( _redefAttr )  {
        return _redefAttr->StrToVal( s, instances, addFileId );
    }

    _error.ClearErrorMsg(); // also sets Severity to SEVERITY_NULL

    //  set the value to be null (reinitialize the attribute value)
    set_null();

    int nullable = ( aDesc->Optional().asInt() == BTrue );

    // an empty str gets assigned NULL
    if( !s ) {
        if( nullable || IsDerived() ) { // if it is derived it doesn't
            return SEVERITY_NULL;    // matter if it is null DAS
        } else {
            _error.severity( SEVERITY_INCOMPLETE );
            return SEVERITY_INCOMPLETE;
        }
    }
    if( s[0] == '\0' ) {
        if( NonRefType() == STRING_TYPE ) {
            // this is interpreted as a string with no value i.e. "".
            *( ptr.S ) = s; // using string class - don't need to declare space
            return SEVERITY_NULL;
        }
        if( nullable || IsDerived() ) { // if it is derived it doesn't
            return SEVERITY_NULL;    // matter if it is null DAS
        } else  {
            _error.severity( SEVERITY_INCOMPLETE );
            return SEVERITY_INCOMPLETE;
        }
    }

    //  an overridden attribute always has a \'*\' value
    if( IsDerived() ) { // check to see if value contains: optional space,
        // followed by *, followed by optional space
        const char * tmpSptr = s;
        while( isspace( *tmpSptr ) ) {
            tmpSptr++;
        }
        if( *tmpSptr == '*' ) {
            tmpSptr++;
            char tmpC;
            int charsFound = sscanf( tmpSptr, "%c", &tmpC );
            if( charsFound == EOF ) { // no non-white chars followed the *
                return SEVERITY_NULL;
            }
        }
        _error.AppendToDetailMsg(
            "Derived attribute must have \'*\' for its value.\n" );
        return _error.severity( SEVERITY_INPUT_ERROR );
    }

    istringstream in( ( char * )s ); // sz defaults to length of s

    // read in value for attribute
    switch( NonRefType() ) {
        case INTEGER_TYPE: {
            ReadInteger( *( ptr.i ), s, &_error, 0 );
            break;
        }
        case REAL_TYPE: {
            ReadReal( *( ptr.r ), s, &_error, 0 );
            break;
        }
        case NUMBER_TYPE: {
            ReadNumber( *( ptr.r ), s, &_error, 0 );
            break;
        }

        case ENTITY_TYPE: {
            STEPentity * se = ReadEntityRef( s, &_error, 0, instances, addFileId );
            if( se != S_ENTITY_NULL ) {
                if( EntityValidLevel( se, aDesc->NonRefTypeDescriptor(),
                                      &_error )
                        == SEVERITY_NULL ) {
                    *( ptr.c ) = se;
                } else {
                    *( ptr.c ) = S_ENTITY_NULL;
                }
            } else {
                *( ptr.c ) = S_ENTITY_NULL;
            }
            break;
        }

        case BINARY_TYPE: {
            ptr.b->StrToVal( s, &_error ); // call class SDAI_Binary::StrToVal()
            break;
        }
        case STRING_TYPE: {
            *( ptr.S ) = s; // using string class - don't need to declare space
            break;
        }

        case BOOLEAN_TYPE:
        case LOGICAL_TYPE:
        case ENUM_TYPE: {
            ptr.e->StrToVal( s, &_error, nullable );
            break;
        }

        case AGGREGATE_TYPE:
        case ARRAY_TYPE:      // DAS
        case BAG_TYPE:        // DAS
        case SET_TYPE:        // DAS
        case LIST_TYPE:       // DAS
            ptr.a -> StrToVal( s, &_error,
                               aDesc -> AggrElemTypeDescriptor(),
                               instances, addFileId );
            break;

        case SELECT_TYPE:
            if( _error.severity( ptr.sh->STEPread( in, &_error, instances, 0 ) )
                    != SEVERITY_NULL ) {
                _error.AppendToDetailMsg( ptr.sh ->Error() );
            }
            break;

//      case UNKNOWN_TYPE: // should never have this case
        case GENERIC_TYPE:
        default:
            // other cases are the same for StrToVal and file
            return STEPread( in, instances, addFileId );
    }
    return _error.severity();
}


/**************************************************************//**
** \fn STEPread
** \brief Reads attribute value from in, formatted as P21 file.
** The value of the attribute is read from istream. The format
** expected is STEP exchange file.
**
** Accepts '$' or nothing as value for OPTIONAL ATTRIBUTE, since
** this function is used for reading files in both old an new
** formats.
**
** Does not read the delimiter separating individual attributes (i.e. ',') or
** the delim separating the last attribute from the end of the entity (')').
**
** \returns  Severity, which indicates success or failure
**         value >= SEVERITY_WARNING means program can continue parsing input,
**         value <= SEVERITY_INPUT_ERROR  is fatal read error
******************************************************************/
Severity STEPattribute::STEPread( istream & in, InstMgrBase * instances, int addFileId,
                                  const char * currSch, bool strict ) {

    // The attribute has been redefined by the attribute pointed
    // to by _redefAttr so write the redefined value.
    if( _redefAttr )  {
        return _redefAttr->STEPread( in, instances, addFileId, currSch );
    }

    _error.ClearErrorMsg(); // also sets Severity to SEVERITY_NULL

    //  set the value to be null (reinitialize the attribute value)
    set_null();

    in >> ws; // skip whitespace
    char c = in.peek();

    if( IsDerived() ) {
        if( c == '*' ) {
            in.get( c ); // take * off the istream
            _error.severity( SEVERITY_NULL );
        } else {
            _error.severity( SEVERITY_WARNING );
            _error.AppendToDetailMsg( "  WARNING: attribute '" );
            _error.AppendToDetailMsg( aDesc->Name() );
            _error.AppendToDetailMsg( "' of type '" );
            _error.AppendToDetailMsg( aDesc->TypeName() );
            _error.AppendToDetailMsg( "' - missing asterisk for derived attribute.\n" );
        }
        CheckRemainingInput( in, &_error, aDesc->TypeName(), ",)" );
        return _error.severity();
    }
    PrimitiveType attrBaseType = NonRefType();

    //  check for NULL or derived attribute value, return if either
    switch( c ) {
        case '$':
        case ',':
        case ')':
            if( c == '$' ) {
                in.ignore();
                CheckRemainingInput( in, &_error, aDesc->TypeName(), ",)" );
            }
            if( Nullable() )  {
                _error.severity( SEVERITY_NULL );
            } else if( !strict ) {
                std::string fillerValue;
                // we aren't in strict mode, so find out the type of the missing attribute and insert a suitable value.
                ErrorDescriptor err; //this will be discarded
                switch( attrBaseType ) {
                    case INTEGER_TYPE: {
                        fillerValue = "'0',";
                        ReadInteger( *( ptr.i ), fillerValue.c_str(), &err, ",)" );
                        break;
                    }
                    case REAL_TYPE: {
                        fillerValue = "'0.0',";
                        ReadReal( *( ptr.r ), fillerValue.c_str(), &err, ",)" );
                        break;
                    }
                    case NUMBER_TYPE: {
                        fillerValue = "'0',";
                        ReadNumber( *( ptr.r ), fillerValue.c_str(), &err, ",)" );
                        break;
                    }
                    case STRING_TYPE: {
                        fillerValue = "'',";
                        *( ptr.S ) = "''";
                        break;
                    }
                    default: { //do not know what a good value would be for other types
                        _error.severity( SEVERITY_INCOMPLETE );
                        _error.AppendToDetailMsg( " missing and required\n" );
                        return _error.severity();
                    }
                }
                if( err.severity() <= SEVERITY_INCOMPLETE ) {
                    _error.severity( SEVERITY_BUG );
                    _error.AppendToDetailMsg( " Error in STEPattribute::STEPread()\n" );
                    return _error.severity();
                }
                //create a warning. SEVERITY_WARNING makes more sense to me, but is considered more severe than SEVERITY_INCOMPLETE
                _error.severity( SEVERITY_USERMSG );
                _error.AppendToDetailMsg( " missing and required. For compatibility, replacing with " );
                _error.AppendToDetailMsg( fillerValue.substr( 0, fillerValue.length() - 1 ) );
                _error.AppendToDetailMsg( ".\n" );
            } else {
                _error.severity( SEVERITY_INCOMPLETE );
                _error.AppendToDetailMsg( " missing and required\n" );
            }
            return _error.severity();
    }

    switch( attrBaseType ) {
        case INTEGER_TYPE: {
            ReadInteger( *( ptr.i ), in, &_error, ",)" );
            return _error.severity();
        }
        case REAL_TYPE: {
            ReadReal( *( ptr.r ), in, &_error, ",)" );
            return _error.severity();
        }
        case NUMBER_TYPE: {
            ReadNumber( *( ptr.r ), in, &_error, ",)" );
            return _error.severity();
        }
        case STRING_TYPE: {
            ptr.S->STEPread( in, &_error );
            CheckRemainingInput( in, &_error, "string", ",)" );
            return _error.severity();
        }
        case BINARY_TYPE: {
            // call class SDAI_Binary::STEPread()
            ptr.b->STEPread( in, &_error );
            CheckRemainingInput( in, &_error, "binary", ",)" );
            return _error.severity();
        }
        case BOOLEAN_TYPE: {
            ptr.e->STEPread( in, &_error,  Nullable() );
            CheckRemainingInput( in, &_error, "boolean", ",)" );
            return _error.severity();
        }
        case LOGICAL_TYPE: {
            ptr.e->STEPread( in, &_error,  Nullable() );
            CheckRemainingInput( in, &_error, "logical", ",)" );
            return _error.severity();
        }
        case ENUM_TYPE: {
            ptr.e->STEPread( in, &_error,  Nullable() );
            CheckRemainingInput( in, &_error, "enumeration", ",)" );
            return _error.severity();
        }
        case AGGREGATE_TYPE:
        case ARRAY_TYPE:      // DAS
        case BAG_TYPE:        // DAS
        case SET_TYPE:        // DAS
        case LIST_TYPE: {     // DAS
            ptr.a->STEPread( in, &_error,
                             aDesc->AggrElemTypeDescriptor(),
                             instances, addFileId, currSch );

            // cannot recover so give up and let STEPentity recover
            if( _error.severity() < SEVERITY_WARNING ) {
                return _error.severity();
            }

            // check for garbage following the aggregate
            CheckRemainingInput( in, &_error, "aggregate", ",)" );
            return _error.severity();
        }
        case ENTITY_TYPE: {
            STEPentity * se = ReadEntityRef( in, &_error, ",)", instances,
                                             addFileId );
            if( se != S_ENTITY_NULL ) {
                if( EntityValidLevel( se,
                                      aDesc->NonRefTypeDescriptor(),
                                      &_error ) == SEVERITY_NULL ) {
                    *( ptr.c ) = se;
                } else {
                    *( ptr.c ) = S_ENTITY_NULL;
                }
            } else {
                *( ptr.c ) = S_ENTITY_NULL;
            }
            return _error.severity();

        }
        case SELECT_TYPE:
            if( _error.severity( ptr.sh->STEPread( in, &_error, instances, 0,
                                                   addFileId, currSch ) )
                    != SEVERITY_NULL ) {
                _error.AppendToDetailMsg( ptr.sh ->Error() );
            }
            CheckRemainingInput( in, &_error, "select", ",)" );
            return _error.severity();

        case GENERIC_TYPE: {
            cerr << "Internal error:  " << __FILE__ <<  __LINE__
                 << "\n" << _POC_ "\n";
            _error.GreaterSeverity( SEVERITY_BUG );
            return _error.severity();
        }

        case UNKNOWN_TYPE:
        case REFERENCE_TYPE:
        default: {
            // bug
            cerr << "Internal error:  " << __FILE__ <<  __LINE__
                 << "\n" << _POC_ "\n";
            _error.GreaterSeverity( SEVERITY_BUG );
            return _error.severity();
        }
    }
}

/*****************************************************************//**
 ** \fn asStr
 ** \param currSch - used for select type writes.  See commenting in SDAI_Select::STEPwrite().
 ** \returns the value of the attribute
 ** Status:  complete 3/91
 *********************************************************************/
std::string STEPattribute::asStr( const char * currSch ) const {
    ostringstream ss;
    std::string str;

    // The attribute has been derived by a subtype's attribute
    if( IsDerived() )  {
        str = "*";
        return str;
    }

    // The attribute has been redefined by the attribute pointed
    // to by _redefAttr so write the redefined value.
    if( _redefAttr )  {
        return _redefAttr->asStr( currSch );
    }

    if( is_null() )  {
        return str;
    }

    switch( NonRefType() ) {
        case INTEGER_TYPE:
            ss << *( ptr.i );
            str += ss.str();
            break;

        case NUMBER_TYPE:
        case REAL_TYPE:

            ss.precision( ( int ) Real_Num_Precision );
            ss << *( ptr.r );
            str += ss.str();
            break;

        case ENTITY_TYPE:
            // print instance id only if not empty pointer
            // and has value assigned
            if( ( *( ptr.c ) == S_ENTITY_NULL ) || ( *( ptr.c ) == 0 ) ) {
                break;
            } else {
                ( *( ptr.c ) )->STEPwrite_reference( str );
            }
            break;

        case BINARY_TYPE:
            if( !( ptr.b->empty() ) ) {
                ptr.b->STEPwrite( str );
            }
            break;

        case STRING_TYPE:
            if( !( ( ptr.S )->empty() ) ) {
                ptr.S->asStr( str );
            }
            break;

        case AGGREGATE_TYPE:
        case ARRAY_TYPE:      // DAS
        case BAG_TYPE:        // DAS
        case SET_TYPE:        // DAS
        case LIST_TYPE:       // DAS
            ptr.a->asStr( str );
            break;

        case ENUM_TYPE:
        case BOOLEAN_TYPE:
        case LOGICAL_TYPE:
            ptr.e->asStr( str );
            break;

        case SELECT_TYPE:
            ptr.sh->STEPwrite( str, currSch );
            break;

        case REFERENCE_TYPE:
        case GENERIC_TYPE:
            cerr << "Internal error:  " << __FILE__ <<  __LINE__
                 << "\n" << _POC_ "\n";
            str.clear();
            break;

        case UNKNOWN_TYPE:
        default:
            ptr.u->asStr( str );
    }
    return str;
}

/// write '$' to out, put message in error, write brief error to stderr
void STEPattribute::STEPwriteError( ostream & out, unsigned int line, const char* desc ) {
    out << "$";
    cerr << "Internal error:  " << __FILE__ << ":" << line << "\n" << _POC_ "\n";

    _error.GreaterSeverity( SEVERITY_BUG );
    std::stringstream ss;
    ss << " Warning: attribute '" << Name() << " : " << TypeName() << "' " << desc << std::endl;
    _error.AppendToUserMsg( ss.str() );
    _error.AppendToDetailMsg( ss.str() );
}

/**
 * The value of the attribute is printed to the output stream specified by out.
 * The output is in physical file format.
 *
 */
void STEPattribute::STEPwrite( ostream & out, const char * currSch ) {
    // The attribute has been derived by a subtype's attribute
    if( IsDerived() ) {
        out << "*";
        return;
    }
    // The attribute has been redefined by the attribute pointed
    // to by _redefAttr so write the redefined value.
    if( _redefAttr )  {
        _redefAttr->STEPwrite( out );
        return;
    }

    if( is_null() ) {
        out << "$";
        return;
    }

    switch( NonRefType() ) {
        case INTEGER_TYPE:
            out << *( ptr.i );
            break;

        case NUMBER_TYPE:
        case REAL_TYPE: {
            WriteReal( *( ptr.r ), out );
            break;
        }

        case ENTITY_TYPE:
            // print instance id only if not empty pointer
            if( ( ptr.c == 0 ) || ( *( ptr.c ) == 0 ) ||
                    // no value was assigned  <-- this would be a BUG
                    ( *( ptr.c ) == S_ENTITY_NULL ) ) {
                STEPwriteError( out, __LINE__, "is null and shouldn't be." );
            } else {
                ( *( ptr.c ) ) -> STEPwrite_reference( out );
            }
            break;

        case STRING_TYPE:
            // if null pointer or pointer to a string of length zero
            if( ptr.S ) {
                ( ptr.S ) -> STEPwrite( out );
            } else {
                STEPwriteError( out, __LINE__, "should be pointing at an SDAI_String." );
            }
            break;

        case BINARY_TYPE:
            // if null pointer or pointer to a string of length zero
            if( ptr.b ) {
                ( ptr.b ) -> STEPwrite( out );
            } else {
                STEPwriteError( out, __LINE__, "should be pointing at an SDAI_Binary." );
            }
            break;

        case AGGREGATE_TYPE:
        case ARRAY_TYPE:      // DAS
        case BAG_TYPE:        // DAS
        case SET_TYPE:        // DAS
        case LIST_TYPE:       // DAS
            ptr.a -> STEPwrite( out, currSch );
            break;

        case ENUM_TYPE:
        case BOOLEAN_TYPE:
        case LOGICAL_TYPE:
            if( ptr.e ) {
                ptr.e -> STEPwrite( out );
            } else {
                STEPwriteError( out, __LINE__, "should be pointing at a SDAI_Enum class." );
            }
            break;

        case SELECT_TYPE:
            if( ptr.sh ) {
                ptr.sh -> STEPwrite( out, currSch );
            } else {
                STEPwriteError( out, __LINE__, "should be pointing at a SDAI_Select class." );
            }
            break;

        case REFERENCE_TYPE:
        case GENERIC_TYPE:
            cerr << "Internal error:  " << __FILE__ << ":" <<  __LINE__ << "\n" << _POC_ "\n";
            _error.GreaterSeverity( SEVERITY_BUG );
            return;

        case UNKNOWN_TYPE:
        default:
            ptr.u -> STEPwrite( out );
            break;

    }
}


void STEPattribute::ShallowCopy( const STEPattribute * sa ) {
    _mustDeletePtr = false;
    aDesc = sa->aDesc;
    refCount = 0;
    _derive = sa->_derive;
    _redefAttr = sa->_redefAttr;
    if( _redefAttr )  {
        _redefAttr->ShallowCopy( sa );
    }
    //Should we just use memcpy()? That would be a true shallowCopy
    switch( sa->NonRefType() ) {
        case INTEGER_TYPE:
            ptr.i = sa->ptr.i;
            break;
        case BINARY_TYPE:
            ptr.b = sa->ptr.b;
            break;
        case STRING_TYPE:
            ptr.S = sa->ptr.S;
            break;
        case REAL_TYPE:
        case NUMBER_TYPE:
            ptr.r = sa->ptr.r;
            break;
        case ENTITY_TYPE:
            ptr.c = sa->ptr.c;
            break;
        case ARRAY_TYPE:      // DAS
        case BAG_TYPE:        // DAS
        case SET_TYPE:        // DAS
        case LIST_TYPE:       // DAS
            switch( sa->BaseType() ) {
                case sdaiAGGR:
                    ptr.a = new GenericAggregate;
                    break;
                case sdaiINSTANCE:
                    ptr.a = new EntityAggregate;
                    break;
                case sdaiSELECT:
                    ptr.a = new SelectAggregate;
                    break;
                case sdaiSTRING:
                    ptr.a = new StringAggregate;
                    break;
                case sdaiBINARY:
                    ptr.a = new BinaryAggregate;
                    break;
                case sdaiENUMERATION:
                    ptr.a = new EnumAggregate;
                    break;
                case sdaiBOOLEAN:
                    ptr.a = new BOOLEANS;
                    break;
                case sdaiLOGICAL:
                    ptr.a = new LOGICALS;
                    break;
                case sdaiREAL:
                case sdaiNUMBER:
                    ptr.a = new RealAggregate;
                    break;
                case sdaiINTEGER:
                    ptr.a = new IntAggregate;
                    break;
                default:
                    std::cerr << "WARNING: Reached default case for BaseType() in STEPattribute::"
                                << "ShallowCopy(). New attribute may be invalid." << std::endl;
                    ptr.a = new STEPaggregate;
                    break;
            }
            ptr.a->ShallowCopy( *( sa->ptr.a ) );
            _mustDeletePtr = true;
            break;
        case SELECT_TYPE:
            ptr.sh = sa->ptr.sh;
            break;
        case BOOLEAN_TYPE:
            ptr.e = new SDAI_BOOLEAN;
            ptr.e->put( sa->ptr.e->asInt() );
            _mustDeletePtr = true;
            break;
        case LOGICAL_TYPE:
            ptr.e = new SDAI_LOGICAL;
            ptr.e->put( sa->ptr.e->asInt() );
            _mustDeletePtr = true;
            break;

        case AGGREGATE_TYPE:
        case ENUM_TYPE:
        default:
            std::cerr << "WARNING: Reached default case for NonRefType() in STEPattribute::"
                        << "ShallowCopy(). New attribute may be invalid." << std::endl;
            memcpy( & ptr, & ( sa->ptr ), sizeof( sa->ptr ) );
            break;
    }
}

/**
 * for a string attribute this means, make it not exist i.e. SDAI_String
 * will exist in member variable ptr but SDAI_string will be told to report
 * as not containing a value (even a value of no chars).
 */
Severity STEPattribute::set_null() {
    if( _redefAttr )  {
        return _redefAttr->set_null();
    }
    switch( NonRefType() ) {
        case INTEGER_TYPE:
            *( ptr.i ) = S_INT_NULL;
            break;

        case NUMBER_TYPE:
            *( ptr.r ) = S_NUMBER_NULL;
            break;

        case REAL_TYPE:
            *( ptr.r ) = S_REAL_NULL;
            break;

        case ENTITY_TYPE:
            *( ptr.c ) = S_ENTITY_NULL;
            break;

        case STRING_TYPE:
            ptr.S->clear();
            break;

        case BINARY_TYPE:
            ptr.b ->clear();
            break;

        case AGGREGATE_TYPE:
        case ARRAY_TYPE:      // DAS
        case BAG_TYPE:        // DAS
        case SET_TYPE:        // DAS
        case LIST_TYPE: {     // DAS
            ptr.a -> Empty();
            break;
        }

        case ENUM_TYPE:
        case BOOLEAN_TYPE:
        case LOGICAL_TYPE:
            ptr.e -> set_null();
            break;

        case SELECT_TYPE:
            ptr.sh -> set_null();
            break;

        case REFERENCE_TYPE:
        case GENERIC_TYPE:
            cerr << "Internal error:  " << __FILE__ << ":" <<  __LINE__ << "\n" << _POC_ "\n";
            return SEVERITY_BUG;

        case UNKNOWN_TYPE:
        default:
            ptr.u -> set_null();
            std::stringstream err;
            err << " Warning: attribute '" << Name() << " : " << TypeName() << " : ";
            err << Type() << "' - " << "Don't know how to make attribute NULL" << std::endl;
            _error.AppendToDetailMsg( err.str() );
            _error.GreaterSeverity( SEVERITY_WARNING );
            return SEVERITY_WARNING;
    }
    if( Nullable() ) {
        return SEVERITY_NULL;
    } else {
        return SEVERITY_INCOMPLETE;
    }
}

/**
 * For a string value this reports whether the string exists (as reported by
 * SDAI_String ) not whether SDAI_String contains a null string.
 */
bool STEPattribute::is_null()  const {
    if( _redefAttr )  {
        return _redefAttr->is_null();
    }
    /* for NUMBER_TYPE and REAL_TYPE, we want an exact comparison. however, doing so causes a compiler warning.
     * workaround is to use memcmp. need a variable, but can't declare it within the switch without errors.
     */
    SDAI_Real z;
    switch( NonRefType() ) {
        case INTEGER_TYPE:
            return ( *( ptr.i ) == S_INT_NULL );

        case NUMBER_TYPE:
            z = S_NUMBER_NULL;
            return ( 0 == memcmp( ptr.r, &z, sizeof z ) );

        case REAL_TYPE:
            z = S_REAL_NULL;
            return ( 0 == memcmp( ptr.r, &z, sizeof z ) );

        case ENTITY_TYPE:
            return ( *( ptr.c ) == S_ENTITY_NULL );

        case STRING_TYPE:
            return ( *( ptr.S ) == S_STRING_NULL );

        case BINARY_TYPE:
            return ptr.b->empty();

        case AGGREGATE_TYPE:
        case ARRAY_TYPE:        // DAS
        case BAG_TYPE:      // DAS
        case SET_TYPE:      // DAS
        case LIST_TYPE: {   // DAS
            return ( ptr.a -> is_null() );
        }

        case ENUM_TYPE:
        case BOOLEAN_TYPE:
        case LOGICAL_TYPE:
            return ( ptr.e -> is_null() );

        case SELECT_TYPE:
            return( ptr.sh->is_null() );

        case REFERENCE_TYPE:
        case GENERIC_TYPE:
            //should be an error, but this is a const function - no way to report.
            return true;
        case UNKNOWN_TYPE:
        default:
            return ( ptr.u -> is_null() );
    }
}

// these get the attr value

SDAI_Integer * STEPattribute::Integer(){
    if( NonRefType() == INTEGER_TYPE ) {
        return ptr.i;
    }
    return 0;
}

SDAI_Real * STEPattribute::Number() {
    if( NonRefType() == NUMBER_TYPE ) {
        return ptr.r;
    }
    return 0;
}

SDAI_Real * STEPattribute::Real() {
    if( NonRefType() == REAL_TYPE ) {
        return ptr.r;
    }
    return 0;
}

SDAI_Application_instance * STEPattribute::Entity() {
    if( NonRefType() == ENTITY_TYPE ) {
        return *( ptr.c );
    }
    return 0;
}

SDAI_String * STEPattribute::String() {
    if( NonRefType() == STRING_TYPE ) {
        return ptr.S;
    }
    return 0;
}

SDAI_Binary * STEPattribute::Binary() {
    if( NonRefType() == BINARY_TYPE ) {
        return ptr.b;
    }
    return 0;
}

STEPaggregate * STEPattribute::Aggregate() {
    if( ( NonRefType() == AGGREGATE_TYPE ) || ( NonRefType() == ARRAY_TYPE ) || ( NonRefType() == BAG_TYPE )
        || ( NonRefType() == SET_TYPE ) || ( NonRefType() == LIST_TYPE ) ) {
        return ptr.a;
    }
    return 0;
}

SDAI_BOOLEAN * STEPattribute::Boolean() {
    if( NonRefType() == BOOLEAN_TYPE ) {
        return ( SDAI_BOOLEAN * ) ptr.e;
    }
    return 0;
}

SDAI_LOGICAL * STEPattribute::Logical() {
    if( NonRefType() == LOGICAL_TYPE ) {
        return ( SDAI_LOGICAL * ) ptr.e;
    }
    return 0;
}

SDAI_Enum * STEPattribute::Enum() {
    if( NonRefType() == ENUM_TYPE ) {
        return ptr.e;
    }
    return 0;
}

SDAI_Select * STEPattribute::Select() {
    if( NonRefType() == SELECT_TYPE ) {
        return ptr.sh;
    }
    return 0;
}

SCLundefined * STEPattribute::Undefined() {
    if( ( NonRefType() != REFERENCE_TYPE ) && ( NonRefType() != GENERIC_TYPE ) ) {
        return ptr.u;
    }
    return 0;
}

// these set the attr value

void STEPattribute::Integer( SDAI_Integer * n ) {
    assert( NonRefType() == INTEGER_TYPE );
    if( ptr.i ) {
        *( ptr.i ) = * n;
    } else {
        ptr.i = n;
    }
}

void STEPattribute::Real( SDAI_Real * n ) {
    assert( NonRefType() == REAL_TYPE );
    if( ptr.r ) {
        *( ptr.r ) = * n;
    } else {
        ptr.r = n;
    }
}

void STEPattribute::Number( SDAI_Real * n ) {
    assert( NonRefType() == NUMBER_TYPE );
    if( ptr.r ) {
        *( ptr.r ) = * n;
    } else {
        ptr.r = n;
    }
}

void STEPattribute::String( SDAI_String * str ) {
    assert( NonRefType() == STRING_TYPE );
    if( ptr.S ) {
        *( ptr.S ) = * str;
    } else {
        ptr.S = str;
    }
}

void STEPattribute::Binary( SDAI_Binary * bin ) {
    assert( NonRefType() == BINARY_TYPE );
    if( ptr.b ) {
        *( ptr.b ) = * bin;
    } else {
        ptr.b = bin;
    }
}

void STEPattribute::Entity( SDAI_Application_instance * ent ) {
    assert( NonRefType() == ENTITY_TYPE );
    if( ptr.c ) {
        delete ptr.c;
    }
    ptr.c = new (SDAI_Application_instance * );
    *( ptr.c ) = ent;
}

void STEPattribute::Aggregate( STEPaggregate * aggr ) {
    assert( ( NonRefType() == AGGREGATE_TYPE ) || ( NonRefType() == ARRAY_TYPE ) || ( NonRefType() == BAG_TYPE )
    || ( NonRefType() == SET_TYPE ) || ( NonRefType() == LIST_TYPE ) );
    if( ptr.a ) {
        *( ptr.a ) = * aggr;
    } else {
        ptr.a = aggr;
    }
}

void STEPattribute::Enum( SDAI_Enum * enu ) {
    assert( NonRefType() == ENUM_TYPE );
    if( ptr.e ) {
        ptr.e->set_null();
        *( ptr.e ) = * enu;
    } else {
        ptr.e = enu;
    }
}

void STEPattribute::Logical( SDAI_LOGICAL * log ) {
    assert( NonRefType() == LOGICAL_TYPE );
    if( ptr.e ) {
        ptr.e->set_null();
        *( ptr.e ) = * log;
    } else {
        ptr.e = log;
    }
}

void STEPattribute::Boolean( SDAI_BOOLEAN * boo ) {
    assert( NonRefType() == BOOLEAN_TYPE );
    if( ptr.e ) {
        ptr.e->set_null();
        *( ptr.e ) = *boo;
    } else {
        ptr.e = boo;
    }
}

void STEPattribute::Select( SDAI_Select * sel ) {
    assert( NonRefType() == SELECT_TYPE );
    if( ptr.sh ) {
        ptr.sh->set_null();
        *( ptr.sh ) = * sel;
    } else {
        ptr.sh = sel;
    }
}

void STEPattribute::Undefined( SCLundefined * undef ) {
    //FIXME is this right, or is the Undefined() above right?
    assert( NonRefType() == REFERENCE_TYPE || NonRefType() == UNKNOWN_TYPE );
    if( ptr.u ) {
        *( ptr.u ) = * undef;
    } else {
        ptr.u = undef;
    }
}


/** evaluate the equality of two attributes
 * ignores _error and refCount, since those are ancillary
 *  \return true if equal
 */
bool operator == ( const STEPattribute & a1, const STEPattribute & a2 ) {
    if( & a1 == & a2 ) {
        return true;
    }
    if( a1._derive == a2._derive && a1.aDesc == a2.aDesc && a1._redefAttr == a2._redefAttr ){
        if( 0 == memcmp( & a1.ptr, & a2.ptr, sizeof( a1.ptr ) ) ) {
            return true;
        } else {
            //ptr differs between a1 and a2, but contents aren't necessarily different
            return a1.asStr() == a2.asStr();
        }
    }
    return false;
}

/** evaluate the equality of two attributes
 * ignores _error and refCount, since those are ancillary
 *  \return true if not equal
 */
bool operator != ( const STEPattribute & a1, const STEPattribute & a2 ) {
  return !( a1 == a2 );
}

/// return true if the attr descriptor is identical
bool sameADesc( const STEPattribute & a1, const STEPattribute & a2 ) {
    return a1.aDesc == a2.aDesc;
}



/**************************************************************//**
 * \returns the severity level that the parameter attrValue would pass if it
 * was the value for this attribute.
 * *note* for string values - (attrValue = 0) => string value does not exist,
 *       attrValue exists it is valid.
******************************************************************/
Severity STEPattribute::ValidLevel( const char * attrValue, ErrorDescriptor * error, InstMgrBase * im, bool clearError ) {
    if( clearError ) {
        ClearErrorMsg();
    }

    if( _redefAttr )  {
        return _redefAttr->ValidLevel( attrValue, error, im, clearError );
    }
    bool optional = Nullable();

    if( !attrValue ) {
        if( optional ) {
            return error->severity();
        } else {
            error->GreaterSeverity( SEVERITY_INCOMPLETE );
            return SEVERITY_INCOMPLETE;
        }
    }
    if( attrValue[0] == '\0' ) {
        if( NonRefType() == STRING_TYPE ) {
            // this is interpreted as a string with no value i.e. "".
            // Thus if it exists it has to be valid.
            return SEVERITY_NULL;
        }
        if( optional ) {
            return error->severity();
        } else {
            error->GreaterSeverity( SEVERITY_INCOMPLETE );
            return SEVERITY_INCOMPLETE;
        }
    }

    //  an overridden attribute always has a \'*\' value
    if( IsDerived() )  {
        if( !strcmp( attrValue, "*" ) ) {
            return SEVERITY_NULL;
        } else {
            _error.AppendToDetailMsg( "attr is derived - value not permitted\n" );
            return _error.severity( SEVERITY_INPUT_ERROR );
        }
    }

    switch( NonRefType() ) {
        case INTEGER_TYPE:
            return IntValidLevel( attrValue, error, clearError, optional, 0 );

        case STRING_TYPE:
            // if a value exists (checked above) then that is the string value
            return SEVERITY_NULL;

        case REAL_TYPE:
            return RealValidLevel( attrValue, error, clearError, optional, 0 );

        case NUMBER_TYPE:
            return NumberValidLevel( attrValue, error, clearError, optional, 0 );

        case ENTITY_TYPE:
            return EntityValidLevel( attrValue,
                                     aDesc->NonRefTypeDescriptor(),
                                     error, im, 0 );
        case BINARY_TYPE:
            return ptr.b->BinaryValidLevel( attrValue, &_error, optional, 0 );

        case AGGREGATE_TYPE:
        case ARRAY_TYPE:      // DAS
        case BAG_TYPE:        // DAS
        case SET_TYPE:        // DAS
        case LIST_TYPE: {     // DAS
            return ptr.a->AggrValidLevel( attrValue, error,
                                          aDesc->AggrElemTypeDescriptor(), im,
                                          optional, 0, 0, 0 );
        }
        case ENUM_TYPE:
        case BOOLEAN_TYPE:
        case LOGICAL_TYPE:
            return ptr.e->EnumValidLevel( attrValue, error, optional, 0, 0, 1 );
        case SELECT_TYPE:
            return ptr.sh->SelectValidLevel( attrValue, error, im );

        default:
            cerr << "Internal error:  " << __FILE__ <<  __LINE__
                 << "\n" << _POC_ "\n";
            return error->severity();
    }
}

/**************************************************************//**
** \param out -- output stream
** \param a -- attribute to output
** Description:  overloads the output operator to print an attribute
******************************************************************/
ostream & operator<< ( ostream & out, STEPattribute & a ) {
    a.STEPwrite( out );
    return out;
}

/**************************************************************//**
* This prepends attribute information to the detailed error msg.  This
* is intended to add information to the error msgs written by Enumerations,
* Aggregates, and SDAI_Strings which don't know they are a STEPattribute
* value.
******************************************************************/
void STEPattribute::AddErrorInfo() {
    char errStr[BUFSIZ];
    errStr[0] = '\0';
    if( SEVERITY_INPUT_ERROR < _error.severity() &&
            _error.severity() < SEVERITY_NULL ) {
        sprintf( errStr, " Warning: ATTRIBUTE '%s : %s : %d' - ",
                 Name(), TypeName(), Type() );
        _error.PrependToDetailMsg( errStr );
    } else if( _error.severity() == SEVERITY_INPUT_ERROR ) {
        sprintf( errStr, " Error: ATTRIBUTE '%s : %s : %d' - ",
                 Name(), TypeName(), Type() );
        _error.PrependToDetailMsg( errStr );
    } else if( _error.severity() <= SEVERITY_BUG ) {
        sprintf( errStr, " BUG: ATTRIBUTE '%s : %s : %d' - ",
                 Name(), TypeName(), Type() );
        _error.PrependToDetailMsg( errStr );
    }
}

/**************************************************************//**
* this function reads until it hits eof or one of the StopChars.
* if it hits one of StopChars it puts it back.
* RETURNS: the last char it read.
******************************************************************/
char STEPattribute::SkipBadAttr( istream & in, char * StopChars ) {
    ios_base::fmtflags flbuf = in.flags();
    in.unsetf( ios::skipws ); // turn skipping whitespace off

    // read bad data until end of this attribute or entity.
    char * foundCh = 0;
    char c = '\0';
    char errStr[BUFSIZ];
    errStr[0] = '\0';

    _error.GreaterSeverity( SEVERITY_WARNING );
    in >> c;
    while( !in.eof() && !( foundCh = strchr( StopChars, c ) ) ) {
        in >> c;
    }
    if( in.eof() ) {
        _error.GreaterSeverity( SEVERITY_INPUT_ERROR );
        sprintf( errStr, " Error: attribute '%s : %s : %d' - %s.\n",
                 Name(), TypeName(), Type(),
                 "Unexpected EOF when skipping bad attr value" );
        _error.AppendToDetailMsg( errStr );
    } else {
        sprintf( errStr, " Error: attribute '%s : %s : %d' - %s.\n",
                 Name(), TypeName(), Type(), "Invalid value" );
        _error.AppendToDetailMsg( errStr );
    }
    in.putback( c );
    in.flags( flbuf ); // set skip whitespace to its original state
    return c;
}


/*********************************************************************
 * The following functions were moved from clstepcore/STEPattribute.inline.cc
 *
 * NIST STEP Core Class Library
 * clstepcore/STEPattribute.inline.cc
 * April 1997
 * K. C. Morris
 * David Sauder
 *
 * Development of this software was funded by the United States Government,
 * and is not subject to copyright.
 */

/// This is needed so that STEPattribute's can be passed as references to inline functions
/// NOTE this code only does shallow copies. It may be necessary to do more, in which case
/// the destructor and assignment operator will also need examined.
STEPattribute::STEPattribute( const STEPattribute & a ) : _derive( a._derive ), _mustDeletePtr( false ),
_redefAttr( a._redefAttr ), aDesc( a.aDesc ), refCount( a.refCount ) {
    ShallowCopy( & a );

    //NOTE may need to do a deep copy for the following types since they are classes
    /*
     *    switch( NonRefType() ) {
     *        case BINARY_TYPE:
     *
     *        case STRING_TYPE:
     *
     *        case ENTITY_TYPE:
     *
     *        case AGGREGATE_TYPE:
     *        case ARRAY_TYPE:      // DAS
     *        case BAG_TYPE:        // DAS
     *        case SET_TYPE:        // DAS
     *        case LIST_TYPE:       // DAS
     *
     *        case SELECT_TYPE:
     *
     *        case ENUM_TYPE:
     *        case BOOLEAN_TYPE:
     *        case LOGICAL_TYPE:
     *
     *        default:
     *            break;
}
*/
}

///  INTEGER
STEPattribute::STEPattribute( const class AttrDescriptor & d, SDAI_Integer * p ): _derive( false ),
_mustDeletePtr( false ), _redefAttr( 0 ), aDesc( &d ), refCount( 0 )  {
    ptr.i = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  BINARY
STEPattribute::STEPattribute( const class AttrDescriptor & d, SDAI_Binary * p ): _derive( false ),
_mustDeletePtr( false ), _redefAttr( 0 ), aDesc( &d ), refCount( 0 )  {
    ptr.b = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  STRING
STEPattribute::STEPattribute( const class AttrDescriptor & d, SDAI_String * p ): _derive( false ),
_mustDeletePtr( false ), _redefAttr( 0 ), aDesc( &d ), refCount( 0 )  {
    ptr.S = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  REAL & NUMBER
STEPattribute::STEPattribute( const class AttrDescriptor & d, SDAI_Real * p ): _derive( false ),
_mustDeletePtr( false ), _redefAttr( 0 ), aDesc( &d ), refCount( 0 )  {
    ptr.r = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  ENTITY
STEPattribute::STEPattribute( const class AttrDescriptor & d, SDAI_Application_instance * *p ):
_derive( false ), _mustDeletePtr( false ), _redefAttr( 0 ), aDesc( &d ), refCount( 0 )  {
    ptr.c = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  AGGREGATE
STEPattribute::STEPattribute( const class AttrDescriptor & d, STEPaggregate * p ): _derive( false ),
_mustDeletePtr( false ), _redefAttr( 0 ), aDesc( &d ), refCount( 0 )  {
    ptr.a =  p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  ENUMERATION  and Logical
STEPattribute::STEPattribute( const class AttrDescriptor & d, SDAI_Enum * p ): _derive( false ),
_mustDeletePtr( false ), _redefAttr( 0 ), aDesc( &d ), refCount( 0 )  {
    ptr.e = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  SELECT
STEPattribute::STEPattribute( const class AttrDescriptor & d, class SDAI_Select * p ): _derive( false ),
_mustDeletePtr( false ), _redefAttr( 0 ), aDesc( &d ), refCount( 0 )  {
    ptr.sh = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

///  UNDEFINED
STEPattribute::STEPattribute( const class AttrDescriptor & d, SCLundefined * p ): _derive( false ),
_mustDeletePtr( false ), _redefAttr( 0 ), aDesc( &d ), refCount( 0 )  {
    ptr.u = p;
    assert( &d ); //ensure that the AttrDescriptor is not a null pointer
}

/// the destructor conditionally deletes the object in ptr
STEPattribute::~STEPattribute() {
    if( _mustDeletePtr ) {
        switch( NonRefType() ) {
            case AGGREGATE_TYPE:
            case ARRAY_TYPE:      // DAS
            case BAG_TYPE:        // DAS
            case SET_TYPE:        // DAS
            case LIST_TYPE:       // DAS
                if( ptr.a ) {
                    delete ptr.a;
                    ptr.a = 0;
                }
                break;
            case BOOLEAN_TYPE:
                if( ptr.e ) {
                    delete ( SDAI_BOOLEAN * ) ptr.e;
                    ptr.e = 0;
                }
            case LOGICAL_TYPE:
                if( ptr.e ) {
                    delete ( SDAI_LOGICAL * ) ptr.e;
                    ptr.e = 0;
                }
                break;
            default:
                break;
        }
    }
}

/// name is the same even if redefined
const char * STEPattribute::Name() const {
    return aDesc->Name();
}

const char * STEPattribute::TypeName() const {
    if( _redefAttr )  {
        return _redefAttr->TypeName();
    }
    return aDesc->TypeName().c_str();
}

BASE_TYPE STEPattribute::Type() const {
    if( _redefAttr )  {
        return _redefAttr->Type();
    }
    return aDesc->Type();
}

BASE_TYPE STEPattribute::NonRefType() const {
    if( _redefAttr )  {
        return _redefAttr->NonRefType();
    } else if( aDesc ) {
        return aDesc->NonRefType();
    }
    return UNKNOWN_TYPE;
}

BASE_TYPE STEPattribute::BaseType() const {
    if( _redefAttr )  {
        return _redefAttr->BaseType();
    }
    return aDesc->BaseType();
}

const TypeDescriptor * STEPattribute::ReferentType() const {
    if( _redefAttr )  {
        return _redefAttr->ReferentType();
    }
    return aDesc->ReferentType();
}

bool STEPattribute::Nullable() const {
    if( _redefAttr )  {
        return _redefAttr->Nullable();
    }
    return ( aDesc->Optionality().asInt() == LTrue );
}
