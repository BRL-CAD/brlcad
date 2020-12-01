#include "typeDescriptor.h"

TypeDescriptor::TypeDescriptor( )
    : _name( 0 ), altNames( 0 ), _fundamentalType( UNKNOWN_TYPE ),
      _originatingSchema( 0 ), _referentType( 0 ), _description( 0 ), _where_rules( 0 ) {
}

TypeDescriptor::TypeDescriptor
( const char * nm, PrimitiveType ft, Schema * origSchema,
  const char * d )
    :  _name( nm ), altNames( 0 ), _fundamentalType( ft ),
       _originatingSchema( origSchema ), _referentType( 0 ), _description( d ),
       _where_rules( 0 ) {
}

TypeDescriptor::~TypeDescriptor() {
    if( _where_rules ) {
        delete _where_rules;
    }
}

/**
 * Determines the current name of this.  Normally, this is simply _name.
 * If "schnm" is set to a value, however, then this function becomes a
 * request for our name when referenced by schnm.  (This will be diff from
 * our original name if schnm USEs or REFERENCEs us and renames us (e.g.,
 * "USE (xx as yy)").)  In such a case, this function searches through our
 * altNames list to see if schema schnm refers to us with a different name
 * and returns the new name if found.  (See header comments to function
 * SchRename::rename().)
 */
const char * TypeDescriptor::Name( const char * schnm ) const {
    if( schnm == NULL ) {
        return _name;
    }
    if( altNames && altNames->rename( schnm, ( char * )_altname ) ) {
        // If our altNames list has an alternate for schnm, copy it into
        // _altname, and return it:
        return _altname;
    }
    return _name;
}

const char * TypeDescriptor::BaseTypeName()  const {
    return BaseTypeDescriptor() ?  BaseTypeDescriptor() -> Name() : 0;
}

const TypeDescriptor * TypeDescriptor::BaseTypeIsA( const TypeDescriptor * td ) const {
    switch( NonRefType() ) {
        case AGGREGATE_TYPE:
            return AggrElemTypeDescriptor() -> IsA( td );
        case ENTITY_TYPE:
        case SELECT_TYPE:
        default:
            return IsA( td );
    }
}

/**
 * Check if our "current" name = other.  CurrName may be different from
 * _name if schNm tells us the current schema is a different one from the
 * one in which we're defined.  If so, we may have an alternate name for
 * that schema (it may be listed in our altNames list).  This would be the
 * case if schNm USEs or REFERENCEs type and renames it in the process
 * (e.g., "USE (X as Y)".
 */
bool TypeDescriptor::CurrName( const char * other, const char * schNm ) const {
    if( !schNm || *schNm == '\0' ) {
        // If there's no current schema, accept any possible name of this.
        // (I.e., accept its actual name or any substitute):
        return ( PossName( other ) );
    }
    if( altNames && altNames->rename( schNm, ( char * )_altname ) ) {
        // If we have a different name when the current schema = schNm, then
        // other better = the alt name.
        return ( !StrCmpIns( _altname, other ) );
    } else {
        // If we have no desginated alternate name when the current schema =
        // schNm, other must = our _name.
        return ( OurName( other ) );
    }
}

/**
 * return true if nm is either our name or one of the possible alternates.
 */
bool TypeDescriptor::PossName( const char * nm ) const {
    return ( OurName( nm ) || AltName( nm ) );
}

bool TypeDescriptor::OurName( const char * nm ) const {
    return !StrCmpIns( nm, _name );
}

bool TypeDescriptor::AltName( const char * nm ) const {
    if( altNames ) {
        return ( altNames->choice( nm ) );
    }
    return false;
}

/**
 * Creates a SchRename consisting of schnm & newnm.  Places it in alphabe-
 * tical order in this's altNames list.
 */
void TypeDescriptor::addAltName( const char * schnm, const char * newnm ) {
    SchRename * newpair = new SchRename( schnm, newnm ),
    *node = ( SchRename * )altNames, *prev = NULL;

    while( node && *node < *newpair ) {
        prev = node;
        node = node->next;
    }
    newpair->next = node; // node may = NULL
    if( prev ) {
        // Will be the case if new node should not be first (and above while
        // loop was entered).
        prev->next = newpair;
    } else {
        // put newpair first
        altNames = newpair;
    }
}

void TypeDescriptor::AttrTypeName( std::string & buf, const char * schnm ) const {
    const char * sn = Name( schnm );
    if( sn ) {
        StrToLower( sn , buf );
    } else {
        buf = _description;
    }
}

const char * TypeDescriptor::GenerateExpress( std::string & buf ) const {
    char tmp[BUFSIZ];
    buf = "TYPE ";
    buf.append( StrToLower( Name(), tmp ) );
    buf.append( " = " );
    const char * desc = Description();
    const char * ptr = desc;

    while( *ptr != '\0' ) {
        if( *ptr == ',' ) {
            buf.append( ",\n  " );
        } else if( *ptr == '(' ) {
            buf.append( "\n  (" );
        } else if( isupper( *ptr ) ) {
            buf += ( char )tolower( *ptr );
        } else {
            buf += *ptr;
        }
        ptr++;
    }
    buf.append( ";\n" );
    ///////////////
    // count is # of WHERE rules
    if( _where_rules != 0 ) {
        int all_comments = 1;
        int count = _where_rules->Count();
        for( int i = 0; i < count; i++ ) { // print out each UNIQUE rule
            if( !( *( _where_rules ) )[i]->_label.size() ) {
                all_comments = 0;
            }
        }

        if( all_comments ) {
            buf.append( "  (* WHERE *)\n" );
        } else {
            buf.append( "    WHERE\n" );
        }

        for( int i = 0; i < count; i++ ) { // print out each WHERE rule
            if( !( *( _where_rules ) )[i]->_comment.empty() ) {
                buf.append( "    " );
                buf.append( ( *( _where_rules ) )[i]->comment_() );
            }
            if( ( *( _where_rules ) )[i]->_label.size() ) {
                buf.append( "      " );
                buf.append( ( *( _where_rules ) )[i]->label_() );
            }
        }
    }

    buf.append( "END_TYPE;\n" );
    return const_cast<char *>( buf.c_str() );
}

/**
 * This is a fully expanded description of the type.
 * This returns a string like the _description member variable
 * except it is more thorough of a description where possible
 * e.g. if the description contains a TYPE name it will also
 * be explained.
 */
const char * TypeDescriptor::TypeString( std::string & s ) const {
    switch( Type() ) {
        case REFERENCE_TYPE:
            if( Name() ) {
                s.append( "TYPE " );
                s.append( Name() );
                s.append( " = " );
            }
            if( Description() ) {
                s.append( Description() );
            }
            if( ReferentType() ) {
                s.append( " -- " );
                std::string tmp;
                s.append( ReferentType()->TypeString( tmp ) );
            }
            return const_cast<char *>( s.c_str() );

        case INTEGER_TYPE:
            s.clear();
            if( _referentType != 0 ) {
                s = "TYPE ";
                s.append( Name() );
                s.append( " = " );
            }
            s.append( "Integer" );
            break;

        case STRING_TYPE:
            s.clear();
            if( _referentType != 0 ) {
                s = "TYPE ";
                s.append( Name() );
                s.append( " = " );
            }
            s.append( "String" );
            break;

        case REAL_TYPE:
            s.clear();
            if( _referentType != 0 ) {
                s = "TYPE ";
                s.append( Name() );
                s.append( " = " );
            }
            s.append( "Real" );
            break;

        case ENUM_TYPE:
            s = "Enumeration: ";
            if( Name() ) {
                s.append( "TYPE " );
                s.append( Name() );
                s.append( " = " );
            }
            if( Description() ) {
                s.append( Description() );
            }
            break;

        case BOOLEAN_TYPE:
            s.clear();
            if( _referentType != 0 ) {
                s = "TYPE ";
                s.append( Name() );
                s.append( " = " );
            }
            s.append( "Boolean: F, T" );
            break;
        case LOGICAL_TYPE:
            s.clear();
            if( _referentType != 0 ) {
                s = "TYPE ";
                s.append( Name() );
                s.append( " = " );
            }
            s.append( "Logical: F, T, U" );
            break;
        case NUMBER_TYPE:
            s.clear();
            if( _referentType != 0 ) {
                s = "TYPE ";
                s.append( Name() );
                s.append( " = " );
            }
            s.append( "Number" );
            break;
        case BINARY_TYPE:
            s.clear();
            if( _referentType != 0 ) {
                s = "TYPE ";
                s.append( Name() );
                s.append( " = " );
            }
            s.append( "Binary" );
            break;
        case ENTITY_TYPE:
            s = "Entity: ";
            if( Name() ) {
                s.append( Name() );
            }
            break;
        case AGGREGATE_TYPE:
        case ARRAY_TYPE:      // DAS
        case BAG_TYPE:        // DAS
        case SET_TYPE:        // DAS
        case LIST_TYPE:       // DAS
            s = Description();
            if( ReferentType() ) {
                s.append( " -- " );
                std::string tmp;
                s.append( ReferentType()->TypeString( tmp ) );
            }
            break;
        case SELECT_TYPE:
            s.append( Description() );
            break;
        case GENERIC_TYPE:
        case UNKNOWN_TYPE:
            s = "Unknown";
            break;
    } // end switch
    return const_cast<char *>( s.c_str() );

}

const TypeDescriptor * TypeDescriptor::IsA( const TypeDescriptor * other )  const {
    if( this == other ) {
        return other;
    }
    return 0;
}

const TypeDescriptor * TypeDescriptor::IsA( const char * other ) const  {
    if( !Name() ) {
        return 0;
    }
    if( !StrCmpIns( _name, other ) ) {   // this is the type
        return this;
    }
    return ( ReferentType() ? ReferentType() -> IsA( other ) : 0 );
}

/**
 * the first PrimitiveType that is not REFERENCE_TYPE (the first
 * TypeDescriptor *_referentType that does not have REFERENCE_TYPE
 * for it's fundamentalType variable).  This would return the same
 * as BaseType() for fundamental types.  An aggregate type
 * would return AGGREGATE_TYPE then you could find out the type of
 * an element by calling AggrElemType().  Select types
 * would work the same?
 */
PrimitiveType TypeDescriptor::NonRefType() const {
    const TypeDescriptor * td = NonRefTypeDescriptor();
    if( td ) {
        return td->FundamentalType();
    }
    return UNKNOWN_TYPE;
}


const TypeDescriptor * TypeDescriptor::NonRefTypeDescriptor() const {
    const TypeDescriptor * td = this;

    while( td->ReferentType() ) {
        if( td->Type() != REFERENCE_TYPE ) {
            return td;
        }
        td = td->ReferentType();
    }

    return td;
}

/// This returns the PrimitiveType of the first non-aggregate element of an aggregate
int TypeDescriptor::IsAggrType() const {
    switch( NonRefType() ) {
        case AGGREGATE_TYPE:
        case ARRAY_TYPE:      // DAS
        case BAG_TYPE:        // DAS
        case SET_TYPE:        // DAS
        case LIST_TYPE:       // DAS
            return 1;

        default:
            return 0;
    }
}

PrimitiveType TypeDescriptor::AggrElemType() const {
    const TypeDescriptor * aggrElemTD = AggrElemTypeDescriptor();
    if( aggrElemTD ) {
        return aggrElemTD->Type();
    }
    return UNKNOWN_TYPE;
}

const TypeDescriptor * TypeDescriptor::AggrElemTypeDescriptor() const {
    const TypeDescriptor * aggrTD = NonRefTypeDescriptor();
    const TypeDescriptor * aggrElemTD = aggrTD->ReferentType();
    if( aggrElemTD ) {
        aggrElemTD = aggrElemTD->NonRefTypeDescriptor();
    }
    return aggrElemTD;
}

/**
 * This is the underlying type of this type. For instance:
 * TYPE count = INTEGER;
 * TYPE ref_count = count;
 * TYPE count_set = SET OF ref_count;
 *  each of the above will generate a TypeDescriptor and for
 *  each one, PrimitiveType BaseType() will return INTEGER_TYPE
 *  TypeDescriptor *BaseTypeDescriptor() returns the TypeDescriptor
 *  for Integer
 */
PrimitiveType TypeDescriptor::BaseType() const {
    const TypeDescriptor * td = BaseTypeDescriptor();
    if( td ) {
        return td->FundamentalType();
    } else {
        return ENTITY_TYPE;
    }
}

const TypeDescriptor * TypeDescriptor::BaseTypeDescriptor() const {
    const TypeDescriptor * td = this;

    while( td -> ReferentType() ) {
        td = td->ReferentType();
    }
    return td;
}
