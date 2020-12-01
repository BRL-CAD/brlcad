#include "rules.h"
#include <express/type.h>
#include "classes.h"

#include <stdio.h>

/* print Where_rule's. for types, schema should be null - tename will include schema name */
void WHEREprint( const char * tename, Linked_List wheres, FILE * impl, Schema schema, bool needWR ) {
    if( wheres ) {
        fprintf( impl, "    %s%s%s->_where_rules = new Where_rule__list;\n", ( schema ? SCHEMAget_name( schema ) : "" ), ( schema ? "::" ENT_PREFIX : "" ), tename );
        if( needWR ) {
            fprintf( impl, "        Where_rule * wr;\n" );
        }

        LISTdo( wheres, w, Where ) {
            fprintf( impl, "        str.clear();\n");
            if( w->label ) {
                fprintf( impl, "        str.append( \"%s: (\" );\n", w->label->name );
            } else {
                /* no label */
                fprintf( impl, "        str.append( \"(\" );\n");
            }
            format_for_std_stringout( impl, EXPRto_string( w->expr ) );

            fprintf( impl, "        str.append( \");\\n\" );\n");

            fprintf( impl, "        wr = new Where_rule( str.c_str() );\n" );
            fprintf( impl, "        %s%s%s->_where_rules->Append( wr );\n", ( schema ? SCHEMAget_name( schema ) : "" ), ( schema ? "::" ENT_PREFIX : "" ), tename );

        } LISTod
    }
}

/* print Uniqueness_rule's */
void UNIQUEprint( Entity entity, FILE * impl, Schema schema ) {
    Linked_List uniqs = entity->u.entity->unique;
    if( uniqs ) {
        fprintf( impl, "        %s::%s%s->_uniqueness_rules = new Uniqueness_rule__set;\n", SCHEMAget_name( schema ), ENT_PREFIX, ENTITYget_name( entity ) );
        fprintf( impl, "        Uniqueness_rule * ur;\n" );
        LISTdo( uniqs, list, Linked_List ) {
            int i = 0;
            fprintf( impl, "        str.clear();\n");
            LISTdo_n( list, e, Expression, b ) {
                i++;
                if( i == 1 ) {
                    /* print label if present */
                    if( e ) {
                        fprintf( impl, "    str.append( \"%s : \" );\n", StrToUpper( ( ( Symbol * )e )->name ) );
                    }
                } else {
                    if( i > 2 ) {
                        fprintf( impl, "    str.append( \", \" );\n");
                    }
                    format_for_std_stringout( impl, EXPRto_string( e ) );
                }
            } LISTod
            fprintf( impl, "    ur = new Uniqueness_rule( str.c_str() );\n" );
            fprintf( impl, "    %s::%s%s->_uniqueness_rules->Append(ur);\n", SCHEMAget_name( schema ), ENT_PREFIX, ENTITYget_name( entity ) );
        } LISTod
    }
}
