/** *****************************************************************
** \file classes_attribute.c
** FedEx parser output module for generating C++  class definitions
**
** Development of FedEx was funded by the United States Government,
** and is not subject to copyright.

*******************************************************************
The conventions used in this binding follow the proposed specification
for the STEP Standard Data Access Interface as defined in document
N350 ( August 31, 1993 ) of ISO 10303 TC184/SC4/WG7.
*******************************************************************/
#include <sc_memmgr.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sc_mkdir.h>
#include <ordered_attrs.h>
#include <type.h>
#include "classes.h"
#include "classes_attribute.h"

#include <sc_trace_fprintf.h>

#if defined(_MSC_VER) && _MSC_VER < 1900
#  include "sc_stdio.h"
#  define snprintf c99_snprintf
#endif

extern int old_accessors;
extern int print_logging;

/**************************************************************//**
 ** Procedure:  generate_attribute_name
 ** Parameters:  Variable a, an Express attribute; char *out, the C++ name
 ** Description:  converts an Express name into the corresponding C++ name
 **       see relation to generate_dict_attr_name() DAS
 ** Side Effects:
 ** Status:  complete 8/5/93
 ******************************************************************/
char *generate_attribute_name(Variable a, char *out)
{
    char *temp, *q;
    const char *p;
    int i;

    temp = EXPRto_string(VARget_name(a));
    p = StrToLower(temp);
    if(! strncmp(p, "self\\", 5)) {
        p += 5;
    }
    /*  copy p to out  */
    /* DAR - fixed so that '\n's removed */
    for(i = 0, q = out; *p != '\0' && i < BUFSIZ; p++) {
        /* copy p to out, 1 char at time.  Skip \n's and spaces, convert
         *  '.' to '_', and convert to lowercase. */
        if((*p != '\n') && (*p != ' ')) {
            if(*p == '.') {
                *q = '_';
            } else {
                *q = *p;
            }
            i++;
            q++;
        }
    }
    *q = '\0';
    sc_free(temp);
    return out;
}

char *generate_attribute_func_name(Variable a, char *out)
{
    generate_attribute_name(a, out);
    strncpy(out, StrToLower(out), BUFSIZ);
    if(old_accessors) {
        out[0] = toupper(out[0]);
    } else {
        out[strlen(out)] = '_';
    }
    return out;
}

/* return true if attr needs const and non-const getters */
bool attrIsObj(Type t)
{
    /* if( TYPEis_select( t ) || TYPEis_aggregate( t ) ) {
        / * const doesn't make sense for pointer types * /
        return false;
    } else */
    Class_Of_Type class = TYPEget_type(t);
    switch(class) {
        case number_:
        case real_:
        case integer_:
        case boolean_:
        case logical_:
            return false;
        default:
            return true;
    }
}

/** print attr descriptors to the namespace
 * \p attr_count_tmp is a _copy_ of attr_count
 */
void ATTRnames_print(Entity entity, FILE *file)
{
    char attrnm [BUFSIZ+1];

    LISTdo(ENTITYget_attributes(entity), v, Variable) {
        generate_attribute_name(v, attrnm);
        fprintf(file, "    extern %s *%s%d%s%s;\n",
                (VARget_inverse(v) ? "Inverse_attribute" : (VARis_derived(v) ? "Derived_attribute" : "AttrDescriptor")),
                ATTR_PREFIX, v->idx,
                (VARis_derived(v) ? "D" : (VARis_type_shifter(v) ? "R" : (VARget_inverse(v) ? "I" : ""))),
                attrnm);
    }
    LISTod
}

/** prints out the current attribute for an entity's c++ class definition
 * skips inverse attrs, since they are now stored in a map
 * \param entity  entity being processed
 * \param a attribute being processed
 * \param file file being written to
 */
void DataMemberPrintAttr(Entity entity, Variable a, FILE *file)
{
    char attrnm [BUFSIZ+1];
    const char *ctype, * etype;
    if(!VARget_inverse(a) && (VARget_initializer(a) == EXPRESSION_NULL)) {
        ctype = TYPEget_ctype(VARget_type(a));
        generate_attribute_name(a, attrnm);
        if(!strcmp(ctype, "SCLundefined")) {
            fprintf(stderr, "Warning: in entity %s, the type for attribute %s is not fully implemented\n", ENTITYget_name(entity), attrnm);
        }
        if(TYPEis_entity(VARget_type(a))) {
            fprintf(file, "        SDAI_Application_instance_ptr _%s = NULL;", attrnm);
        } else if(TYPEis_aggregate(VARget_type(a))) {
            fprintf(file, "        %s_ptr _%s = NULL;", ctype, attrnm);
        } else {
            Class_Of_Type class = TYPEget_type(VARget_type(a));
	    switch (class) {
                case boolean_:
		    fprintf(file, "        %s _%s = false;", ctype, attrnm);
		    break;
                case integer_:
		    fprintf(file, "        %s _%s = 0;", ctype, attrnm);
		    break;
                case real_:
		    fprintf(file, "        %s _%s = 0.0;", ctype, attrnm);
		    break;
		default:
		    fprintf(file, "        %s _%s;", ctype, attrnm);
	    }
        }
        if(VARget_optional(a)) {
            fprintf(file, "    //  OPTIONAL");
        }
        if(isAggregate(a))        {
            /*  if it's a named type, comment the type  */
            if((etype = TYPEget_name(TYPEget_nonaggregate_base_type(VARget_type(a))))) {
                fprintf(file, "          //  of  %s\n", etype);
            }
        }
        fprintf(file, "\n");
    }
}

/**************************************************************//**
 ** Procedure:  ATTRsign_access_methods
 ** Parameters:  const Variable a --  attribute to print
                                      access method signature for
 ** FILE* file  --  file being written to
 ** Returns:  nothing
 ** Description:  prints the signature for an access method
 **               based on the attribute type
 **       DAS i.e. prints the header for the attr. access functions
 **       (get and put attr value) in the entity class def in .h file
 ** Side Effects:
 ** Status:  complete 17-Feb-1992
 ******************************************************************/
void ATTRsign_access_methods(Variable a, FILE *file)
{

    Type t = VARget_type(a);
    char ctype [BUFSIZ+1];
    char attrnm [BUFSIZ+1];

    generate_attribute_func_name(a, attrnm);

    strncpy(ctype, AccessType(t), BUFSIZ);
    ctype[BUFSIZ] = '\0';

    if(attrIsObj(t)) {
        /* object or pointer, so provide const and non-const methods */
        if(TYPEis_entity(t) || TYPEis_select(t) || TYPEis_aggregate(t)) {
            /* it's a typedef, so prefacing with 'const' won't do what we desire */
            fprintf(file, "        %s_c %s() const;\n", ctype, attrnm);
        } else {
            fprintf(file, "  const %s   %s() const;\n", ctype, attrnm);
        }
        fprintf(file, "        %s   %s();\n", ctype, attrnm);
    } else {
        fprintf(file, "        %s   %s() const;\n", ctype, attrnm);
    }
    fprintf(file, "        void %s( const %s x );\n", attrnm, ctype);
    fprintf(file, "\n");
    return;
}

/**************************************************************//**
 ** Procedure:  ATTRprint_access_methods_get_head
 ** Parameters:  const Variable a --  attribute to find the type for
 ** FILE* file  --  file being written
 ** Type t - type of the attribute
 ** Class_Of_Type class -- type name of the class
 ** const char *attrnm -- name of the attribute
 ** char *ctype -- (possibly returned) name of the attribute c++ type
 ** Returns:  name to be used for the type of the c++ access functions
 ** Description:  prints the access method get head based on the attribute type
 **     DAS which being translated is it prints the function header
 **     for the get attr value access function defined for an
 **     entity class. This is the .cc file version.
 ** Side Effects:
 ** Status:  complete 7/15/93       by DDH
 ******************************************************************/
void ATTRprint_access_methods_get_head(const char *classnm, Variable a, FILE *file, bool returnsConst)
{
    Type t = VARget_type(a);
    char ctype [BUFSIZ+1];   /*  return type of the get function  */
    char funcnm [BUFSIZ+1];  /*  name of member function  */
    generate_attribute_func_name(a, funcnm);
    strncpy(ctype, AccessType(t), BUFSIZ);
    ctype[BUFSIZ] = '\0';
    if(TYPEis_entity(t) || TYPEis_select(t) || TYPEis_aggregate(t)) {
        fprintf(file, "\n%s%s %s::%s() ", ctype, (returnsConst ? "_c" : ""), classnm, funcnm);
    } else {
        fprintf(file, "\n%s%s %s::%s() ", (returnsConst ? "const " : ""), ctype, classnm, funcnm);
    }
    return;
}

/**************************************************************//**
 ** Procedure:  ATTRprint_access_methods_put_head
 ** Parameters:  const Variable a --  attribute to find the type for
 ** FILE* file  --  file being written to
 ** Type t - type of the attribute
 ** Class_Of_Type class -- type name of the class
 ** const char *attrnm -- name of the attribute
 ** char *ctype -- name of the attribute c++ type
 ** Returns:  name to be used for the type of the c++ access functions
 ** Description:  prints the access method put head based on the attribute type
 **     DAS which being translated is it prints the function header
 **     for the put attr value access function defined for an
 **     entity class. This is the .cc file version.
 ** Side Effects:
 ** Status:  complete 7/15/93       by DDH
 ******************************************************************/
void ATTRprint_access_methods_put_head(const char *entnm, Variable a, FILE *file)
{

    Type t = VARget_type(a);
    char ctype [BUFSIZ+1];
    char funcnm [BUFSIZ+1];

    generate_attribute_func_name(a, funcnm);

    strncpy(ctype, AccessType(t), BUFSIZ);
    ctype[BUFSIZ] = '\0';
    fprintf(file, "\nvoid %s::%s( const %s x ) ", entnm, funcnm, ctype);

    return;
}

/** print access methods for aggregate attribute */
void AGGRprint_access_methods(const char *entnm, Variable a, FILE *file,
                              char *ctype, char *attrnm)
{
    ATTRprint_access_methods_get_head(entnm, a, file, false);
    fprintf(file, "{\n    if( !_%s ) {\n        _%s = new %s;\n    }\n", attrnm, attrnm, TypeName(a->type));
    fprintf(file, "    return ( %s ) %s_%s;\n}\n", ctype, ((a->type->u.type->body->base) ? "" : "& "), attrnm);
    ATTRprint_access_methods_get_head(entnm, a, file, true);
    fprintf(file, "const {\n");
    fprintf(file, "    return ( %s ) %s_%s;\n}\n", ctype, ((a->type->u.type->body->base) ? "" : "& "), attrnm);
    ATTRprint_access_methods_put_head(entnm, a, file);
    fprintf(file, "{\n    if( !_%s ) {\n        _%s = new %s;\n    }\n", attrnm, attrnm, TypeName(a->type));
    fprintf(file, "    _%s%sShallowCopy( * x );\n}\n", attrnm, ((a->type->u.type->body->base) ? "->" : "."));
    return;
}

/** print logging code for access methods for attrs that are entities
 * \p var is the variable name, minus preceding underscore, or null if 'x' is to be used
 * \p dir is either "returned" or "assigned"
 */
void ATTRprint_access_methods_entity_logging(const char *entnm, const char *funcnm, const char *nm,
        const char *var, const char *dir, FILE *file)
{
    if(print_logging) {
        fprintf(file, "#ifdef SC_LOGGING\n");
        fprintf(file, "    if( *logStream ) {\n");
        fprintf(file, "        logStream -> open( SCLLOGFILE, ios::app );\n");
        fprintf(file, "        if( !( %s%s == S_ENTITY_NULL ) )\n        {\n", (var ? "_" : ""), (var ? var : "x"));
        fprintf(file, "            *logStream << time( NULL ) << \" SDAI %s::%s() %s: \";\n", entnm, funcnm, dir);
        fprintf(file, "            *logStream << \"reference to Sdai%s entity #\"", nm);
        fprintf(file,                        " << %s%s->STEPfile_id << std::endl;\n", (var ? "_" : ""), (var ? var : "x"));
        fprintf(file, "        } else {\n");
        fprintf(file, "            *logStream << time( NULL ) << \" SDAI %s::%s() %s: \";\n", entnm, funcnm, dir);
        fprintf(file, "            *logStream << \"null entity\" << std::endl;\n        }\n");
        fprintf(file, "        logStream->close();\n");
        fprintf(file, "    }\n");
        fprintf(file, "#endif\n");
    }
}

/** print access methods for attrs that are entities
 * prints const and non-const getters and a setter
 */
void ATTRprint_access_methods_entity(const char *entnm, const char *attrnm, const char *funcnm, const char *nm,
                                     const char *ctype, Variable a, FILE *file)
{
    fprintf(file, "{\n");
    ATTRprint_access_methods_entity_logging(entnm, funcnm, nm, attrnm, "returned", file);
    fprintf(file, "    if( !_%s ) {\n        _%s = new %s;\n    }\n", attrnm, attrnm, TypeName(a->type));
    fprintf(file, "    return (%s) _%s;\n}\n", ctype, attrnm);

    ATTRprint_access_methods_get_head(entnm, a, file, true);
    fprintf(file, "const {\n");
    ATTRprint_access_methods_entity_logging(entnm, funcnm, nm, attrnm, "returned", file);
    fprintf(file, "    return (%s) _%s;\n}\n", ctype, attrnm);

    ATTRprint_access_methods_put_head(entnm, a, file);
    fprintf(file, "{\n");
    ATTRprint_access_methods_entity_logging(entnm, funcnm, nm, 0, "assigned", file);
    fprintf(file, "    _%s = x;\n}\n", attrnm);
    return;
}

/** logging code for string and binary attribute access methods */
void ATTRprint_access_methods_str_bin_logging(const char *entnm, const char *attrnm, const char *funcnm, FILE *file, bool setter)
{
    if(print_logging) {
        const char *direction = (setter ? "assigned" : "returned");
        fprintf(file, "#ifdef SC_LOGGING\n");
        fprintf(file, "    if(*logStream)\n    {\n");
        if(setter) {
            fprintf(file, "        if(!_%s.is_null())\n        {\n", attrnm);
        } else {
            fprintf(file, "        if(x)\n        {\n");
        }
        fprintf(file, "            *logStream << time(NULL) << \" SDAI %s::%s() %s: \";\n", entnm, funcnm, direction);
        if(setter) {
            fprintf(file, "            *logStream << _%s << std::endl;\n", attrnm);
        } else {
            fprintf(file, "            *logStream << x << std::endl;\n");
        }
        fprintf(file, "        }\n        else\n        {\n");
        fprintf(file, "            *logStream << time(NULL) << \" SDAI %s::%s() %s: \";\n", entnm, funcnm, direction);
        fprintf(file, "            *logStream << \"unset\" << std::endl;\n        }\n    }\n");
        fprintf(file, "#endif\n");
    }
}

/** print access methods for string or bin attribute */
void ATTRprint_access_methods_str_bin(const char *entnm, const char *attrnm, const char *funcnm,
                                      const char *ctype, Variable a, FILE *file)
{
    fprintf(file, "{\n");
    ATTRprint_access_methods_str_bin_logging(entnm, attrnm, funcnm, file, true);
    fprintf(file, "    return _%s;\n}\n", attrnm);
    ATTRprint_access_methods_get_head(entnm, a, file, true);
    fprintf(file, "const {\n");
    ATTRprint_access_methods_str_bin_logging(entnm, attrnm, funcnm, file, true);
    fprintf(file, "    return (%s) _%s;\n}\n", ctype, attrnm);
    ATTRprint_access_methods_put_head(entnm, a, file);
    fprintf(file, "{\n");
    ATTRprint_access_methods_str_bin_logging(entnm, attrnm, funcnm, file, false);
    fprintf(file, "    _%s = x;\n}\n", attrnm);
    return;
}

/** print logging code for access methods for enumeration attribute */
void ATTRprint_access_methods_enum_logging(const char *entnm, const char *attrnm, const char *funcnm, FILE *file, bool setter)
{
    if(print_logging) {
        const char *direction = (setter ? "assigned" : "returned");
        fprintf(file, "#ifdef SC_LOGGING\n");
        fprintf(file, "    if(*logStream)\n    {\n");
        if(!setter) {
            fprintf(file, "        if(!_%s.is_null())\n        {\n", attrnm);
        }
        fprintf(file, "            *logStream << time(NULL) << \" SDAI %s::%s() %s: \";\n", entnm, funcnm, direction);
        if(setter) {
            fprintf(file, "            *logStream << _%s.element_at(x) << std::endl;\n", attrnm);
        } else {
            fprintf(file, "            *logStream << _%s.element_at(_%s.asInt()) << std::endl;\n", attrnm, attrnm);
            fprintf(file, "        }\n        else\n        {\n");
            fprintf(file, "            *logStream << time(NULL) << \" SDAI %s::%s() %s: \";\n", entnm, funcnm, direction);
            fprintf(file, "            *logStream << \"unset\" << std::endl;\n        }\n");
        }
        fprintf(file, "    }\n#endif\n");
    }
}

/** print access methods for enumeration attribute */
void ATTRprint_access_methods_enum(const char *entnm, const char *attrnm, const char *funcnm,
                                   Variable a, Type t, FILE *file)
{
    fprintf(file, "{\n");
    ATTRprint_access_methods_enum_logging(entnm, attrnm, funcnm, file, false);
    fprintf(file, "    return (%s) _%s;\n}\n", EnumName(TYPEget_name(t)), attrnm);

    ATTRprint_access_methods_get_head(entnm, a, file, true);
    fprintf(file, "const {\n");
    ATTRprint_access_methods_enum_logging(entnm, attrnm, funcnm, file, false);
    fprintf(file, "    return (%s) _%s;\n}\n", EnumName(TYPEget_name(t)), attrnm);

    ATTRprint_access_methods_put_head(entnm, a, file);
    fprintf(file, "{\n");
    ATTRprint_access_methods_enum_logging(entnm, attrnm, funcnm, file, true);
    fprintf(file, "    _%s.put( x );\n}\n", attrnm);
    return;
}

/** print logging code for access methods for logical or boolean attribute */
void ATTRprint_access_methods_log_bool_logging(const char *entnm, const char *attrnm, const char *funcnm, FILE *file, bool setter)
{
    if(print_logging) {
        const char *direction = (setter ? "assigned" : "returned");
        fprintf(file, "#ifdef SC_LOGGING\n");
        fprintf(file, "    if(*logStream)\n    {\n");
        if(!setter) {
            /* fprintf( file, "        logStream->open(SCLLOGFILE,ios::app);\n" ); */
            fprintf(file, "        if(!_%s.is_null())\n        {\n", attrnm);
        }
        fprintf(file, "            *logStream << time(NULL) << \" SDAI %s::%s() %s: \";\n", entnm, funcnm, direction);
        if(setter) {
            fprintf(file, "        *logStream << _%s.element_at(x) << std::endl;\n", attrnm);
        } else {
            fprintf(file, "            *logStream << _%s.element_at(_%s.asInt()) << std::endl;\n", attrnm, attrnm);
            fprintf(file, "        }\n        else\n        {\n");
            fprintf(file, "            *logStream << time(NULL) << \" SDAI %s::%s() %s: \";\n", entnm, funcnm, direction);
            fprintf(file, "            *logStream << \"unset\" << std::endl;\n        }\n");
            /* fprintf( file, "            logStream->close();\n" ); */
        }
        fprintf(file, "    }\n");
        fprintf(file, "#endif\n");
    }
}

/** print access methods for logical or boolean attribute */
void ATTRprint_access_methods_log_bool(const char *entnm, const char *attrnm, const char *funcnm,
                                       const char *ctype, Variable a, FILE *file)
{
    fprintf(file, "const {\n");
    ATTRprint_access_methods_log_bool_logging(entnm, attrnm, funcnm, file, false);
    fprintf(file, "    return (%s) _%s;\n}\n", ctype, attrnm);

    /* don't need a const method for logical or boolean
     * ATTRprint_access_methods_get_head( entnm, a, file, true );
     * fprintf( file, "const {\n" );
     * ATTRprint_access_methods_log_bool_logging( entnm, attrnm, funcnm, file, false );
     * fprintf( file, "    return (const %s) _%s;\n}\n", ctype, attrnm );
    */
    ATTRprint_access_methods_put_head(entnm, a, file);
    fprintf(file, "{\n");
    ATTRprint_access_methods_log_bool_logging(entnm, attrnm, funcnm, file, true);
    fprintf(file, "    _%s.put (x);\n}\n", attrnm);
    return;
}

/** print access methods for inverse attrs, using iAMap */
void INVprint_access_methods(const char *entnm, const char *attrnm, const char *funcnm, const char *nm,
                             const char *ctype, Variable a, FILE *file, Schema schema)
{
    char iaName[BUFSIZ+1] = {0};
    snprintf(iaName, BUFSIZ, "%s::%s%d%s%s", SCHEMAget_name(schema), ATTR_PREFIX, a->idx,
             /* can it ever be anything but "I"? */
             (VARis_derived(a) ? "D" : (VARis_type_shifter(a) ? "R" : (VARget_inverse(a) ? "I" : ""))), attrnm);

    if(isAggregate(a)) {
        /* following started as AGGRprint_access_methods() */
        ATTRprint_access_methods_get_head(entnm, a, file, false);
        fprintf(file, "{\n    iAstruct ias = getInvAttr( %s );\n    if( !ias.a ) {\n", iaName);
        fprintf(file, "        ias.a = new EntityAggregate;\n        setInvAttr( %s, ias );\n    }\n", iaName);
        fprintf(file, "    return ias.a;\n}\n");
        ATTRprint_access_methods_get_head(entnm, a, file, true);
        fprintf(file, "const {\n");
        fprintf(file, "    return getInvAttr( %s ).a;\n}\n", iaName);
        ATTRprint_access_methods_put_head(entnm, a, file);
        fprintf(file, "{\n    iAstruct ias;\n    ias.a = x;\n    setInvAttr( %s, ias );\n}\n", iaName);
    } else {
        ATTRprint_access_methods_get_head(entnm, a, file, false);
        /* following started as ATTRprint_access_methods_entity() */
        fprintf(file, "{\n");
        ATTRprint_access_methods_entity_logging(entnm, funcnm, nm, attrnm, "returned", file);
        fprintf(file, "    iAstruct ias = getInvAttr( %s );\n", iaName);
        fprintf(file, "    /* no 'new' - doesn't make sense to create an SDAI_Application_instance\n     * since it isn't generic like EntityAggregate */\n");
        fprintf(file, "    return (%s) ias.i;\n}\n", ctype);

        ATTRprint_access_methods_get_head(entnm, a, file, true);
        fprintf(file, "const {\n");
        ATTRprint_access_methods_entity_logging(entnm, funcnm, nm, attrnm, "returned", file);
        fprintf(file, "    iAstruct ias = getInvAttr( %s );\n", iaName);
        fprintf(file, "    return (%s) ias.i;\n}\n", ctype);

        ATTRprint_access_methods_put_head(entnm, a, file);
        fprintf(file, "{\n");
        ATTRprint_access_methods_entity_logging(entnm, funcnm, nm, 0, "assigned", file);
        fprintf(file, "    iAstruct ias;\n    ias.i = x;    setInvAttr( %s, ias );\n}\n", iaName);
    }
}

/** prints the access method based on the attribute type
 *  i.e. get and put value access functions defined in a class
 *  generated for an entity.
 * \param entnm the name of the current entity
 * \param a attribute to print methods for
 * \param file file being written to
 */
void ATTRprint_access_methods(const char *entnm, Variable a, FILE *file, Schema schema)
{
    Type t = VARget_type(a);
    Class_Of_Type classType;
    char ctype [BUFSIZ+1];  /*  type of data member  */
    char attrnm [BUFSIZ+1];
    char membernm[BUFSIZ+1];
    char funcnm [BUFSIZ+1];  /*  name of member function  */

    char nm [BUFSIZ+1];
    /* I believe nm has the name of the underlying type without Sdai in front of it */
    if(TYPEget_name(t)) {
        strncpy(nm, FirstToUpper(TYPEget_name(t)), BUFSIZ);
    }

    generate_attribute_func_name(a, funcnm);
    generate_attribute_name(a, attrnm);
    strcpy(membernm, attrnm);
    membernm[0] = toupper(membernm[0]);
    classType = TYPEget_type(t);
    strncpy(ctype, AccessType(t), BUFSIZ);
    if(VARget_inverse(a)) {
        INVprint_access_methods(entnm, attrnm, funcnm, nm, ctype, a, file, schema);
        return;
    }
    if(isAggregate(a)) {
        AGGRprint_access_methods(entnm, a, file, ctype, attrnm);
        return;
    }
    ATTRprint_access_methods_get_head(entnm, a, file, false);

    /*      case TYPE_ENTITY:   */
    if(classType == entity_)  {
        ATTRprint_access_methods_entity(entnm, attrnm, funcnm, nm, ctype, a, file);
        return;
    }
    /*    case TYPE_LOGICAL:    */
    if((classType == boolean_) || (classType == logical_))  {
        ATTRprint_access_methods_log_bool(entnm, attrnm, funcnm, ctype, a, file);
    }
    /*    case TYPE_ENUM:   */
    if(classType == enumeration_)  {
        ATTRprint_access_methods_enum(entnm, attrnm, funcnm, a, t, file);
    }
    /*    case TYPE_SELECT: */
    if(classType == select_)  {
        fprintf(file, " {\n    return &_%s;\n}\n", attrnm);
        ATTRprint_access_methods_get_head(entnm, a, file, true);
        fprintf(file, "const {\n    return (%s) &_%s;\n}\n",  ctype, attrnm);
        ATTRprint_access_methods_put_head(entnm, a, file);
        fprintf(file, " {\n    _%s = x;\n}\n", attrnm);
        return;
    }
    /*    case TYPE_AGGRETATES: */
    /* handled in AGGRprint_access_methods(entnm, a, file, t, ctype, attrnm) */


    /*  case STRING:*/
    /*      case TYPE_BINARY:   */
    if((classType == string_) || (classType == binary_))  {
        ATTRprint_access_methods_str_bin(entnm, attrnm, funcnm, ctype, a, file);
    }
    /*      case TYPE_INTEGER:  */
    if(classType == integer_) {
        fprintf(file, "const {\n");
        if(print_logging) {
            fprintf(file, "#ifdef SC_LOGGING\n");
            fprintf(file, "    if(*logStream)\n    {\n");
            fprintf(file, "        if(!(_%s == S_INT_NULL) )\n        {\n", attrnm);
            fprintf(file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                    entnm, funcnm);
            fprintf(file,
                    "            *logStream << _%s << std::endl;\n", attrnm);
            fprintf(file, "        }\n        else\n        {\n");
            fprintf(file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                    entnm, funcnm);
            fprintf(file,
                    "            *logStream << \"unset\" << std::endl;\n        }\n    }\n");
            fprintf(file, "#endif\n");
        }
        /*  default:  INTEGER   */
        /*  is the same type as the data member  */
        fprintf(file, "    return (%s) _%s;\n}\n", ctype, attrnm);
        ATTRprint_access_methods_put_head(entnm, a, file);
        fprintf(file, "{\n");
        if(print_logging) {
            fprintf(file, "#ifdef SC_LOGGING\n");
            fprintf(file, "    if(*logStream)\n    {\n");
            fprintf(file, "        if(!(x == S_INT_NULL) )\n        {\n");
            fprintf(file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", entnm, funcnm);
            fprintf(file, "            *logStream << x << std::endl;\n");
            fprintf(file, "        }\n        else\n        {\n");
            fprintf(file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", entnm, funcnm);
            fprintf(file, "            *logStream << \"unset\" << std::endl;\n        }\n    }\n");
            fprintf(file, "#endif\n");
            /*  default:  INTEGER   */
            /*  is the same type as the data member  */
        }
        fprintf(file, "    _%s = x;\n}\n", attrnm);
    }

    /*      case TYPE_REAL:
        case TYPE_NUMBER:   */
    if((classType == number_) || (classType == real_)) {
        fprintf(file, "const {\n");
        if(print_logging) {
            fprintf(file, "#ifdef SC_LOGGING\n");
            fprintf(file, "    if(*logStream)\n    {\n");
            fprintf(file, "        if(!(_%s == S_REAL_NULL) )\n        {\n", attrnm);
            fprintf(file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", entnm, funcnm);
            fprintf(file, "            *logStream << _%s << std::endl;\n", attrnm);
            fprintf(file, "        }\n        else\n        {\n");
            fprintf(file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", entnm, funcnm);
            fprintf(file, "            *logStream << \"unset\" << std::endl;\n        }\n    }\n");
            fprintf(file, "#endif\n");
        }
        fprintf(file, "    return (%s) _%s;\n}\n", ctype, attrnm);
        ATTRprint_access_methods_put_head(entnm, a, file);
        fprintf(file, "{\n");
        if(print_logging) {
            fprintf(file, "#ifdef SC_LOGGING\n");
            fprintf(file, "    if(*logStream)\n    {\n");
            fprintf(file, "        if(!(_%s == S_REAL_NULL) )\n        {\n", attrnm);
            fprintf(file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", entnm, funcnm);
            fprintf(file, "            *logStream << _%s << std::endl;\n", attrnm);
            fprintf(file, "        }\n        else\n        {\n");
            fprintf(file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", entnm, funcnm);
            fprintf(file, "            *logStream << \"unset\" << std::endl;\n        }\n    }\n");
            fprintf(file, "#endif\n");
        }
        fprintf(file, "    _%s = x;\n}\n", attrnm);
    }
}
