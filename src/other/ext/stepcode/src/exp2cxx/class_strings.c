#include <string.h>
#include <stdlib.h>

#include "class_strings.h"
#include "express/type.h"

const char * ClassName( const char * oldname ) {
    int i = 0, j = 0;
    static char newname [BUFSIZ];
    if( !oldname ) {
        return ( "" );
    }
    strcpy( newname, ENTITYCLASS_PREFIX )    ;
    j = strlen( ENTITYCLASS_PREFIX )    ;
    newname [j] = ToUpper( oldname [i] );
    ++i;
    ++j;
    while( oldname [i] != '\0' ) {
        newname [j] = ToLower( oldname [i] );
        /*  if (oldname [i] == '_')  */
        /*  character is '_'    */
        /*      newname [++j] = ToUpper (oldname [++i]);*/
        ++i;
        ++j;
    }
    newname [j] = '\0';
    return ( newname );
}

const char * ENTITYget_classname( Entity ent ) {
    const char * oldname = ENTITYget_name( ent );
    return ( ClassName( oldname ) );
}

/** like TYPEget_ctype, but caller must alloc a buffer of at least buf_siz
 * in many circumstances, this func will return a short string rather than using that buffer
 * buf_siz is ignored in those cases since it is meaningless
 */
const char * TYPE_get_ctype( const Type t, char * retval, size_t buf_siz ) {
    const char * ptr = "_ptr", * var = "_var", * agg = "_agg";
    const char * overflowMsg = "buffer overflow detected at %s:%d!";
    Class_Of_Type ctype;
    Type bt;

    if( TYPEinherits_from( t, aggregate_ ) ) {
        bt = TYPEget_body( t )->base;
        if( TYPEinherits_from( bt, aggregate_ ) ) {
            return( "GenericAggregate" );
        }

        ctype = TYPEget_type( bt );
        if( ctype == integer_ ) {
            return ( "IntAggregate" );
        }
        if( ( ctype == number_ ) || ( ctype == real_ ) ) {
            return ( "RealAggregate" );
        }
        if( ctype == entity_ ) {
            return( "EntityAggregate" );
        }
        if( ( ctype == enumeration_ ) || ( ctype == select_ ) )  {
            const char * tmp = TYPE_get_ctype( bt, retval, buf_siz - strlen( retval ) - 1 );
            if( tmp != retval ) {
                strncpy( retval, tmp, buf_siz - strlen( retval ) - 1 );
            }
            if( strlen( retval ) + strlen( agg ) < buf_siz ) {
                strcat( retval, agg );
            } else {
                fprintf( stderr, overflowMsg, __FILE__, __LINE__ );
                abort();
            }
            return ( retval );
        }
        if( ctype == logical_ ) {
            return ( "LOGICALS" );
        }
        if( ctype == boolean_ ) {
            return ( "BOOLEANS" );
        }
        if( ctype == string_ ) {
            return( "StringAggregate" );
        }
        if( ctype == binary_ ) {
            return( "BinaryAggregate" );
        }
    }

    /*  the rest is for things that are not aggregates  */
    ctype = TYPEget_type( t );

    /*    case TYPE_LOGICAL:    */
    if( ctype == logical_ ) {
        return ( "SDAI_LOGICAL" );
    }

    /*    case TYPE_BOOLEAN:    */
    if( ctype == boolean_ ) {
        return ( "SDAI_BOOLEAN" );
    }

    /*      case TYPE_INTEGER:  */
    if( ctype == integer_ ) {
        return ( "SDAI_Integer" );
    }

    /*      case TYPE_REAL:
     *        case TYPE_NUMBER:   */
    if( ( ctype == number_ ) || ( ctype == real_ ) ) {
        return ( "SDAI_Real" );
    }

    /*      case TYPE_STRING:   */
    if( ctype == string_ ) {
        return ( "SDAI_String" );
    }

    /*      case TYPE_BINARY:   */
    if( ctype == binary_ ) {
        return ( "SDAI_Binary" );
    }

    /*      case TYPE_ENTITY:   */
    if( ctype == entity_ ) {
        strncpy( retval, TypeName( t ), buf_siz - strlen( retval ) - 1 );
        if( strlen( retval ) + strlen( ptr ) < buf_siz ) {
            strcat( retval, ptr );
        } else {
            fprintf( stderr, overflowMsg, __FILE__, __LINE__ );
            abort();
        }
        return retval;
        /*  return ("STEPentityH");    */
    }
    /*    case TYPE_ENUM:   */
    /*    case TYPE_SELECT: */
    if( ctype == enumeration_ ) {
        strncpy( retval, TypeName( t ), buf_siz - strlen( retval ) - 1 );
        if( strlen( retval ) + strlen( var ) < buf_siz ) {
            strcat( retval, var );
        } else {
            fprintf( stderr, overflowMsg, __FILE__, __LINE__ );
            abort();
        }
        return retval;
    }
    if( ctype == select_ )  {
        return ( TypeName( t ) );
    }

    /*  default returns undefined   */
    return ( "SCLundefined" );
}

const char * TYPEget_ctype( const Type t ) {
    static char retval [BUFSIZ] = {0};
    return TYPE_get_ctype( t, retval, BUFSIZ );
}

const char * TypeName( Type t ) {
    static char name [BUFSIZ];
    strcpy( name, TYPE_PREFIX );
    if( TYPEget_name( t ) ) {
        strncat( name, FirstToUpper( TYPEget_name( t ) ), BUFSIZ - strlen( TYPE_PREFIX ) - 1 );
    } else {
        return TYPEget_ctype( t );
    }
    return name;
}

char ToLower( char c ) {
    if( isupper( c ) ) {
        return ( tolower( c ) );
    } else {
        return ( c );
    }

}

char ToUpper( char c ) {
    if( islower( c ) ) {
        return ( toupper( c ) );
    } else {
        return ( c );
    }
}

const char * StrToLower( const char * word ) {
    static char newword [MAX_LEN];
    int i = 0;
    if( !word ) {
        return 0;
    }
    while( word [i] != '\0' ) {
        newword [i] = ToLower( word [i] );
        ++i;
    }
    newword [i] = '\0';
    return ( newword )    ;

}

const char * StrToUpper( const char * word ) {
    static char newword [MAX_LEN];
    int i = 0;

    while( word [i] != '\0' ) {
        newword [i] = ToUpper( word [i] );
        ++i;
    }
    newword [i] = '\0';
    return ( newword );
}

const char * StrToConstant( const char * word ) {
    static char newword[MAX_LEN];
    int i = 0;

    while( word [i] != '\0' ) {
        if( word [i] == '/' || word [i] == '.' ) {
            newword [i] = '_';
        } else {
            newword [i] = ToUpper( word [i] );
        }
        ++i;

    }
    newword [i] = '\0';
    return ( newword );
}

const char * FirstToUpper( const char * word ) {
    static char newword [MAX_LEN];

    strncpy( newword, word, MAX_LEN );
    newword[0] = ToUpper( newword[0] );
    return ( newword );
}
