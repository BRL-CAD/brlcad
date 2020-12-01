#include "enumTypeDescriptor.h"

/*
 * why have EnumTypeDescriptor + EnumerationTypeDescriptor ???
 * this was in ExpDict.cc before splitting it up
 */
#ifdef NOT_YET
EnumerationTypeDescriptor::EnumerationTypeDescriptor( ) {
    _elements = new StringAggregate;
}
#endif

EnumTypeDescriptor::EnumTypeDescriptor( const char * nm, PrimitiveType ft,
                                        Schema * origSchema,
                                        const char * d, EnumCreator f )
    : TypeDescriptor( nm, ft, origSchema, d ), CreateNewEnum( f ) {
}

SDAI_Enum * EnumTypeDescriptor::CreateEnum() {
    if( CreateNewEnum ) {
        return CreateNewEnum();
    } else {
        return 0;
    }
}

const char * EnumTypeDescriptor::GenerateExpress( std::string & buf ) const {
    char tmp[BUFSIZ];
    buf = "TYPE ";
    buf.append( StrToLower( Name(), tmp ) );
    buf.append( " = ENUMERATION OF \n  (" );
    const char * desc = Description();
    const char * ptr = &( desc[16] );

    while( *ptr != '\0' ) {
        if( *ptr == ',' ) {
            buf.append( ",\n  " );
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
            buf.append( "  WHERE\n" );
        }

        for( int i = 0; i < count; i++ ) { // print out each WHERE rule
            if( !( *( _where_rules ) )[i]->_comment.empty() ) {
                buf.append( "    " );
                buf.append( ( *( _where_rules ) )[i]->comment_() );
            }
            if( ( *( _where_rules ) )[i]->_label.size() ) {
                buf.append( "    " );
                buf.append( ( *( _where_rules ) )[i]->label_() );
            }
        }
    }

    buf.append( "END_TYPE;\n" );
    return const_cast<char *>( buf.c_str() );
}
