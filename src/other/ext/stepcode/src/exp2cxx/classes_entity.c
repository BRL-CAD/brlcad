/** *****************************************************************
** \file classes_entity.c
** FedEx parser output module for generating C++  class definitions
**
** Development of FedEx was funded by the United States Government,
** and is not subject to copyright.

*******************************************************************
The conventions used in this binding follow the proposed specification
for the STEP Standard Data Access Interface as defined in document
N350 ( August 31, 1993 ) of ISO 10303 TC184/SC4/WG7.
*******************************************************************/

/* this is used to add new dictionary calls */
/* #define NEWDICT */

#include <sc_memmgr.h>
#include <stdlib.h>
#include <sc_stdbool.h>
#include <assert.h>
#include <sc_mkdir.h>
#include "classes.h"
#include "classes_entity.h"
#include "class_strings.h"
#include "genCxxFilenames.h"
#include <ordered_attrs.h>
#include "rules.h"

#include <sc_trace_fprintf.h>

extern int multiple_inheritance;
extern int old_accessors;

/* attribute numbering used to use a global variable attr_count.
 * it could be tricky keep the numbering consistent when making
 * changes, so this has been replaced with an added member in the
 * Variable_ struct, idx, which is now set by numberAttributes()
 * in classes_wrapper
 */

/*************      Entity Generation      *************/

/** print entity descriptors to the namespace in files->names
 *
 * Nov 2011 - MAP - This function was split out of ENTITYhead_print to enable
 *                  use of a separate header with a namespace.
 */
void ENTITYnames_print(Entity entity, FILE *file)
{
    fprintf(file, "    extern EntityDescriptor *%s%s;\n", ENT_PREFIX, ENTITYget_name(entity));
}

/** declares the global pointer to the EntityDescriptor representing a particular entity
 *
 * DAS: also prints the attr descs and inverse attr descs. This function creates the storage space
 * for the externs defs that were defined in the .h file. These global vars go in the .cc file.
 *
 * \param entity entity being processed
 * \param file file being written to
 * \param schema schema being processed
 */
void LIBdescribe_entity(Entity entity, FILE *file, Schema schema)
{
    char attrnm [BUFSIZ];

    fprintf(file, "EntityDescriptor * %s::%s%s = 0;\n", SCHEMAget_name(schema), ENT_PREFIX, ENTITYget_name(entity));
    LISTdo(ENTITYget_attributes(entity), v, Variable) {
        generate_attribute_name(v, attrnm);
        fprintf(file, "%s * %s::%s%d%s%s = 0;\n",
                (VARget_inverse(v) ? "Inverse_attribute" : (VARis_derived(v) ? "Derived_attribute" : "AttrDescriptor")),
                SCHEMAget_name(schema), ATTR_PREFIX, v->idx,
                (VARis_derived(v) ? "D" : (VARis_type_shifter(v) ? "R" : (VARget_inverse(v) ? "I" : ""))), attrnm);
    }
    LISTod
    fprintf(file, "\n");
}

/** prints the member functions for the class representing an entity.  These go in the .cc file
 * \param entity entity being processed
 * \param neededAttr attr's needed but not inherited through c++
 * \param file file being written to
 * \param schema needed for name of namespace
 */
void LIBmemberFunctionPrint(Entity entity, Linked_List neededAttr, FILE *file, Schema schema)
{

    Linked_List attr_list;
    char entnm [BUFSIZ];

    strncpy(entnm, ENTITYget_classname(entity), BUFSIZ);     /*  assign entnm */

    /*  1. put in member functions which belong to all entities */
    /*  the common function are still in the class definition 17-Feb-1992 */

    /*  2. print access functions for attributes    */
    attr_list = ENTITYget_attributes(entity);
    LISTdo(attr_list, a, Variable) {
        /*  do for EXPLICIT, REDEFINED, and INVERSE attributes - but not DERIVED */
        if(! VARis_derived(a))  {

            /*  retrieval  and  assignment   */
            ATTRprint_access_methods(entnm, a, file, schema);
        }
    }
    LISTod
    /* //////////////// */
    if(multiple_inheritance) {
        LISTdo(neededAttr, attr, Variable) {
            if(! VARis_derived(attr) && ! VARis_overrider(entity, attr)) {
                ATTRprint_access_methods(entnm, attr, file, schema);
            }
        }
        LISTod
    }
    /* //////////////// */

    fprintf(file, "\n");
}

int get_attribute_number(Entity entity)
{
    int i = 0;
    int found = 0;
    Linked_List local, complete;
    complete = ENTITYget_all_attributes(entity);
    local = ENTITYget_attributes(entity);

    LISTdo(local, a, Variable) {
        /*  go to the child's first explicit attribute */
        if((! VARget_inverse(a)) && (! VARis_derived(a)))  {
            LISTdo_n(complete, p, Variable, b) {
                /*  cycle through all the explicit attributes until the
                child's attribute is found  */
                if(!found && (! VARget_inverse(p)) && (! VARis_derived(p))) {
                    if(p != a) {
                        ++i;
                    } else {
                        found = 1;
                    }
                }
            }
            LISTod
            if(found) {
                return i;
            } else {
                fprintf(stderr, "Internal error at %s:%d: attribute %s not found\n", __FILE__, __LINE__, EXPget_name(VARget_name(a)));
            }
        }
    }
    LISTod
    return -1;
}

/** prints the beginning of the entity class definition for the c++ code and the declaration
 * of attr descriptors for the registry in the .h file
 *
 * \p entity entity to print
 * \p file  file being written to
 */
void ENTITYhead_print(Entity entity, FILE *file)
{
    char entnm [BUFSIZ];
    Linked_List list;
    Entity super = 0;

    strncpy(entnm, ENTITYget_classname(entity), BUFSIZ);
    entnm[BUFSIZ - 1] = '\0';

    /* inherit from either supertype entity class or root class of
       all - i.e. SDAI_Application_instance */

    if(multiple_inheritance) {
        list = ENTITYget_supertypes(entity);
        if(! LISTempty(list)) {
            super = (Entity)LISTpeek_first(list);
        }
    } else { /* the old way */
        super = ENTITYput_superclass(entity);
    }

    fprintf(file, "class SC_SCHEMA_EXPORT %s : ", entnm);
    if(super) {
        fprintf(file, "public %s {\n ", ENTITYget_classname(super));
    } else {
        fprintf(file, "public SDAI_Application_instance {\n");
    }
}

/** print an attr initializer
 * skip inverse attrs
 */
void DataMemberInit(bool *first, Variable a, FILE *lib)
{
    char attrnm [BUFSIZ];
    if(VARis_derived(a) || VARget_inverse(a)) {
        return;
    }
    if(TYPEis_entity(VARget_type(a)) || TYPEis_aggregate(VARget_type(a))) {
        if(*first) {
            *first = false;
            fprintf(lib, " :");
        } else {
            fprintf(lib, ",");
        }
        generate_attribute_name(a, attrnm);
        fprintf(lib, " _%s( 0 )", attrnm);
    }
}

/** print attribute initializers; call before printing constructor body
 * \param first true if this is the first initializer
 */
void DataMemberInitializers(Entity entity, bool *first, Linked_List neededAttr, FILE *lib)
{
    Linked_List attr_list = ENTITYget_attributes(entity);
    LISTdo(attr_list, attr, Variable) {
        DataMemberInit(first, attr, lib);
    }
    LISTod;
    if(multiple_inheritance) {
        LISTdo(neededAttr, attr, Variable) {
            DataMemberInit(first, attr, lib);
        }
        LISTod
    }
}

/** prints out the data members for an entity's c++ class definition
 * \param entity entity being processed
 * \param file file being written to
 */
void DataMemberPrint(Entity entity, Linked_List neededAttr, FILE *file)
{
    Linked_List attr_list;

    /*  print list of attributes in the protected access area   */
    fprintf(file, "    protected:\n");

    attr_list = ENTITYget_attributes(entity);
    LISTdo(attr_list, attr, Variable) {
        DataMemberPrintAttr(entity, attr, file);
    }
    LISTod;

    /*  add attributes for parent attributes not inherited through C++ inheritance. */
    if(multiple_inheritance) {
        LISTdo(neededAttr, attr, Variable) {
            DataMemberPrintAttr(entity, attr, file);
        }
        LISTod;
    }
}

/** prints the signature for member functions of an entity's class definition
 * \param entity entity being processed
 * \param file file being written to
 */
void MemberFunctionSign(Entity entity, Linked_List neededAttr, FILE *file)
{

    Linked_List attr_list;
    static int entcode = 0;
    char entnm [BUFSIZ];

    strncpy(entnm, ENTITYget_classname(entity), BUFSIZ);     /*  assign entnm  */
    entnm[BUFSIZ - 1] = '\0';

    fprintf(file, "    public: \n");

    /*  put in member functions which belong to all entities    */
    /*  constructors:    */
    fprintf(file, "        %s();\n", entnm);
    fprintf(file, "        %s( SDAI_Application_instance *se, bool addAttrs = true );\n", entnm);
    /*  copy constructor */
    fprintf(file, "        %s( %s & e );\n", entnm, entnm);
    /*  destructor: */
    fprintf(file, "        ~%s();\n", entnm);

    fprintf(file, "        int opcode() {\n            return %d;\n        }\n", entcode++);

    /*  print signature of access functions for attributes      */
    attr_list = ENTITYget_attributes(entity);
    LISTdo(attr_list, a, Variable) {
        if(VARget_initializer(a) == EXPRESSION_NULL) {
            /*  retrieval  and  assignment  */
            ATTRsign_access_methods(a, file);
        }
    }
    LISTod

    /* //////////////// */
    if(multiple_inheritance) {
        /*  add the EXPRESS inherited attributes which are non */
        /*  inherited in C++ */
        LISTdo(neededAttr, attr, Variable) {
            if(! VARis_derived(attr) && ! VARis_overrider(entity, attr)) {
                ATTRsign_access_methods(attr, file);
            }
        }
        LISTod
    }
    /* //////////////// */
    fprintf(file, "};\n\n");

    /*  print creation function for class */
    fprintf(file, "inline %s * create_%s() {\n    return new %s;\n}\n\n", entnm, entnm, entnm);
}

/** drives the generation of the c++ class definition code
 *
 * Side Effects:  prints segment of the c++ .h file
 *
 * \param entity entity being processed
 * \param file  file being written to
 */
void ENTITYinc_print(Entity entity, Linked_List neededAttr, FILE *file)
{
    ENTITYhead_print(entity, file);
    DataMemberPrint(entity, neededAttr, file);
    MemberFunctionSign(entity, neededAttr, file);
}

/** initialize attributes in the constructor; used for two different constructors */
void initializeAttrs(Entity e, FILE *file)
{
    const orderedAttr *oa;
    orderedAttrsInit(e);
    while(0 != (oa = nextAttr())) {
        if(oa->deriver) {
            fprintf(file, "    MakeDerived( \"%s\", \"%s\" );\n", oa->attr->name->symbol.name, oa->creator->symbol.name);
        }
    }
    orderedAttrsCleanup();
}

/** prints the c++ code for entity class's constructor and destructor.  goes to .cc file
 * Side Effects:  generates codes segment in c++ .cc file
 *
 * Changes: Modified generator to initialize attributes to NULL based
 *          on the NULL symbols defined in "SDAI C++ Binding for PDES,
 *          Inc. Prototyping" by Stephen Clark.
 * Change Date: 5/22/91 CD
 * Changes: Modified STEPattribute constructors to take fewer arguments
 *     21-Dec-1992 -kcm
 * \param entity entity being processed
 * \param file  file being written to
 */
void LIBstructor_print(Entity entity, Linked_List neededAttr, FILE *file, Schema schema)
{
    Linked_List attr_list;
    Type t;
    char attrnm [BUFSIZ];

    Linked_List list;
    Entity principalSuper = 0;

    const char *entnm = ENTITYget_classname(entity);
    bool first = true;

    /*  constructor definition  */

    /* parent class initializer (if any) and '{' printed below */
    fprintf(file, "%s::%s()", entnm, entnm);

    /* ////MULTIPLE INHERITANCE//////// */

    if(multiple_inheritance) {
        int super_cnt = 0;
        list = ENTITYget_supertypes(entity);
        if(! LISTempty(list)) {
            LISTdo(list, e, Entity) {
                /*  if there's no super class yet,
                    or the super class doesn't have any attributes
                */

                super_cnt++;
                if(super_cnt == 1) {
                    bool firstInitializer = false;
                    /* ignore the 1st parent */
                    const char *parent = ENTITYget_classname(e);

                    /* parent class initializer */
                    fprintf(file, ": %s()", parent);
                    DataMemberInitializers(entity, &firstInitializer, neededAttr, file);
                    fprintf(file, " {\n");
                    fprintf(file, "        /*  parent: %s  */\n%s\n%s\n", parent,
                            "        /* Ignore the first parent since it is */",
                            "        /* part of the main inheritance hierarchy */");
                    principalSuper = e; /* principal SUPERTYPE */
                } else {
                    fprintf(file, "        /*  parent: %s  */\n", ENTITYget_classname(e));
                    fprintf(file, "    HeadEntity(this);\n");
                    fprintf(file, "    AppendMultInstance(new %s(this));\n",
                            ENTITYget_classname(e));

                    if(super_cnt == 2) {
                        printf("\nMULTIPLE INHERITANCE for entity: %s\n",
                               ENTITYget_name(entity));
                        printf("        SUPERTYPE 1: %s (principal supertype)\n",
                               ENTITYget_name(principalSuper));
                    }
                    printf("        SUPERTYPE %d: %s\n", super_cnt, ENTITYget_name(e));
                }
            }
            LISTod;

        } else {    /*  if entity has no supertypes, it's at top of hierarchy  */
            /*  no parent class constructor has been printed, so still need an opening brace */
            bool firstInitializer = true;
            DataMemberInitializers(entity, &firstInitializer, neededAttr, file);
            fprintf(file, " {\n");
            fprintf(file, "        /*  no SuperTypes */\n");
        }
    }

    /* what if entity comes from other schema?
     * It appears that entity.superscope.symbol.name is the schema name (but only if entity.superscope.type == 's'?)  --MAP 27Nov11
     */
    fprintf(file, "\n    eDesc = %s::%s%s;\n",
            SCHEMAget_name(schema), ENT_PREFIX, ENTITYget_name(entity));

    attr_list = ENTITYget_attributes(entity);

    LISTdo(attr_list, a, Variable) {
        if(VARget_initializer(a) == EXPRESSION_NULL) {
            /*  include attribute if it is not derived  */
            generate_attribute_name(a, attrnm);
            t = VARget_type(a);

            if(!VARget_inverse(a) && !VARis_derived(a)) {
                /*  1. create a new STEPattribute */

                /*  if type is aggregate, the variable is a pointer and needs initialized */
                if(TYPEis_aggregate(t)) {
                    fprintf(file, "    _%s = new %s;\n", attrnm, TYPEget_ctype(t));
                }
                fprintf(file, "    %sa = new STEPattribute( * %s::",
                        (first ? "STEPattribute * " : ""),   /*   first time through, declare 'a' */
                        SCHEMAget_name(schema));
                fprintf(file, "%s%d%s%s", ATTR_PREFIX, a->idx, (VARis_type_shifter(a) ? "R" : ""), attrnm);
                fprintf(file, ", %s%s_%s );\n",
                        (TYPEis_entity(t) ? "( SDAI_Application_instance_ptr * ) " : ""),
                        (TYPEis_aggregate(t) ? "" : "& "), attrnm);
                if(first) {
                    first = false;
                }
                /*  2. initialize everything to NULL (even if not optional)  */

                fprintf(file, "    a->set_null();\n");

                /*  3.  put attribute on attributes list  */
                fprintf(file, "    attributes.push( a );\n");

                /* if it is redefining another attribute make connection of
                redefined attribute to redefining attribute */
                if(VARis_type_shifter(a)) {
                    fprintf(file, "    MakeRedefined( a, \"%s\" );\n",
                            VARget_simple_name(a));
                }
            }
        }
    }
    LISTod;

    initializeAttrs(entity, file);

    fprintf(file, "}\n\n");

    /*  copy constructor  */
    entnm = ENTITYget_classname(entity);
    fprintf(file, "%s::%s ( %s & e ) : ", entnm, entnm, entnm);

    /* include explicit initialization of base class */
    if(principalSuper) {
        fprintf(file, "%s()", ENTITYget_classname(principalSuper));
    } else {
        fprintf(file, "SDAI_Application_instance()");
    }

    fprintf(file, " {\n    CopyAs( ( SDAI_Application_instance_ptr ) & e );\n}\n\n");

    /*  print destructor  */
    /*  currently empty, but should check to see if any attributes need
    to be deleted -- attributes will need reference count  */

    entnm = ENTITYget_classname(entity);
    fprintf(file, "%s::~%s() {\n", entnm, entnm);

    attr_list = ENTITYget_attributes(entity);

    LISTdo(attr_list, a, Variable)
    if(VARget_initializer(a) == EXPRESSION_NULL) {
        generate_attribute_name(a, attrnm);
        t = VARget_type(a);

        if((! VARget_inverse(a)) && (! VARis_derived(a)))  {
            if(TYPEis_aggregate(t)) {
                fprintf(file, "    delete _%s;\n", attrnm);
            }
        }
    }
    LISTod;

    fprintf(file, "}\n\n");
}

/********************/
/** print the constructor that accepts a SDAI_Application_instance as an argument used
   when building multiply inherited entities.
   \sa LIBstructor_print()
*/
void LIBstructor_print_w_args(Entity entity, Linked_List neededAttr, FILE *file, Schema schema)
{
    Linked_List attr_list;
    Type t;
    char attrnm [BUFSIZ];

    Linked_List list;
    int super_cnt = 0;

    /* added for calling parents constructor if there is one */
    char parentnm [BUFSIZ];
    char *parent = 0;

    const char *entnm;
    bool first = true;

    if(multiple_inheritance) {
        bool firstInitializer = true;
        Entity parentEntity = 0;
        list = ENTITYget_supertypes(entity);
        if(! LISTempty(list)) {
            parentEntity = (Entity)LISTpeek_first(list);
            if(parentEntity) {
                strcpy(parentnm, ENTITYget_classname(parentEntity));
                parent = parentnm;
            } else {
                parent = 0;    /* no parent */
            }
        } else {
            parent = 0;    /* no parent */
        }

        /* ENTITYget_classname returns a static buffer so don't call it twice
           before it gets used - (I didn't write it) - I had to move it below
            the above use. DAS */
        entnm = ENTITYget_classname(entity);
        /*  constructor definition  */
        if(parent) {
            firstInitializer = false;
            fprintf(file, "%s::%s( SDAI_Application_instance * se, bool addAttrs ) : %s( se, addAttrs )", entnm, entnm, parentnm);
        } else {
            fprintf(file, "%s::%s( SDAI_Application_instance * se, bool addAttrs )", entnm, entnm);
        }
        DataMemberInitializers(entity, &firstInitializer, neededAttr, file);
        fprintf(file, " {\n");

        fprintf(file, "    /* Set this to point to the head entity. */\n");
        fprintf(file, "    HeadEntity(se);\n");
        if(!parent) {
            fprintf(file, "    ( void ) addAttrs; /* quell potentially unused var */\n\n");
        }

        list = ENTITYget_supertypes(entity);
        if(! LISTempty(list)) {
            LISTdo(list, e, Entity)
            /*  if there's no super class yet,
                or the super class doesn't have any attributes
                */
            fprintf(file, "        /* parent: %s */\n", ENTITYget_classname(e));

            super_cnt++;
            if(super_cnt == 1) {
                /* ignore the 1st parent */
                fprintf(file,
                        "        /* Ignore the first parent since it is part *\n%s\n",
                        "        ** of the main inheritance hierarchy        */");
            }  else {
                fprintf(file, "    se->AppendMultInstance( new %s( se, addAttrs ) );\n",
                        ENTITYget_classname(e));
            }
            LISTod;

        }  else {   /*  if entity has no supertypes, it's at top of hierarchy  */
            fprintf(file, "        /*  no SuperTypes */\n");
        }

        /* what if entity comes from other schema? */
        fprintf(file, "\n    eDesc = %s::%s%s;\n",
                SCHEMAget_name(schema), ENT_PREFIX, ENTITYget_name(entity));

        attr_list = ENTITYget_attributes(entity);

        LISTdo(attr_list, a, Variable) {
            if(VARget_initializer(a) == EXPRESSION_NULL) {
                /*  include attribute if it is not derived  */
                generate_attribute_name(a, attrnm);
                t = VARget_type(a);
                if(!VARget_inverse(a) && !VARis_derived(a)) {
                    /*  1. create a new STEPattribute */

                    /*  if type is aggregate, the variable is a pointer and needs initialized */
                    if(TYPEis_aggregate(t)) {
                        fprintf(file, "    _%s = new %s;\n", attrnm, TYPEget_ctype(t));
                    }
                    fprintf(file, "    %sa = new STEPattribute( * %s::%s%d%s%s, %s %s_%s );\n",
                            (first ? "STEPattribute * " : ""),   /*   first time through, declare a */
                            SCHEMAget_name(schema),
                            ATTR_PREFIX, a->idx,
                            (VARis_type_shifter(a) ? "R" : ""),
                            attrnm,
                            (TYPEis_entity(t) ? "( SDAI_Application_instance_ptr * )" : ""),
                            (TYPEis_aggregate(t) ? "" : "&"),
                            attrnm);

                    if(first) {
                        first = false;
                    }

                    fprintf(file, "        /* initialize to NULL (even if not optional)  */\n");
                    fprintf(file, "    a ->set_null();\n");

                    fprintf(file, "        /* Put attribute on this class' attributes list so the access functions still work. */\n");
                    fprintf(file, "    attributes.push( a );\n");

                    fprintf(file, "        /* Put attribute on the attributes list for the main inheritance hierarchy.  **\n");
                    fprintf(file, "        ** The push method rejects duplicates found by comparing attrDescriptor's.   */\n");
                    fprintf(file, "    if( addAttrs ) {\n");
                    fprintf(file, "        se->attributes.push( a );\n    }\n");

                    /* if it is redefining another attribute make connection of redefined attribute to redefining attribute */
                    if(VARis_type_shifter(a)) {
                        fprintf(file, "    MakeRedefined( a, \"%s\" );\n",
                                VARget_simple_name(a));
                    }
                }
            }
        }
        LISTod

        initializeAttrs(entity, file);

        fprintf(file, "}\n\n");
    } /* end if(multiple_inheritance) */

}

/** return 1 if types are predefined by us */
bool TYPEis_builtin(const Type t)
{
    switch(TYPEget_body(t)->type) {     /* dunno if correct*/
        case integer_:
        case real_:
        case string_:
        case binary_:
        case boolean_:
        case number_:
        case logical_:
            return true;
        default:
            break;
    }
    return false;
}

/** converts an Express name into the corresponding SC
 *  dictionary name.  The difference between this and the
 *  generate_attribute_name() function is that for derived
 *  attributes the name will have the form <parent>.<attr_name>
 *  where <parent> is the name of the parent containing the
 *  attribute being derived and <attr_name> is the name of the
 *  derived attribute. Both <parent> and <attr_name> may
 *  contain underscores but <parent> and <attr_name> will be
 *  separated by a period.  generate_attribute_name() generates
 *  the same name except <parent> and <attr_name> will be
 *  separated by an underscore since it is illegal to have a
 *  period in a variable name.  This function is used for the
 *  dictionary name (a string) and generate_attribute_name()
 *  will be used for variable and access function names.
 * \param a, an Express attribute
 * \param out, the C++ name
 */
char *generate_dict_attr_name(Variable a, char *out)
{
    char *temp, *p, *q;
    int j;

    temp = EXPRto_string(VARget_name(a));
    p = temp;
    if(! strncmp(StrToLower(p), "self\\", 5)) {
        p = p + 5;
    }
    /*  copy p to out  */
    strncpy(out, StrToLower(p), BUFSIZ);
    /* DAR - fixed so that '\n's removed */
    for(j = 0, q = out; *p != '\0' && j < BUFSIZ; p++) {
        /* copy p to out, 1 char at time.  Skip \n's, and convert to lc. */
        if(*p != '\n') {
            *q = tolower(*p);
            j++;
            q++;
        }
    }
    *q = '\0';

    sc_free(temp);
    return out;
}

/** generates code to add entity to STEP registry
 *
 * \param entity entity being processed
 * \param header header being written to
 * \param impl implementation file being written to
 * \param schema schema the entity is in
 */
void ENTITYincode_print(Entity entity, FILE *header, FILE *impl, Schema schema)
{
#define entity_name ENTITYget_name(entity)
#define schema_name SCHEMAget_name(schema)
    char attrnm [BUFSIZ];
    char dict_attrnm [BUFSIZ];
    const char *super_schema;
    char *tmp, *tmp2;
    bool hasInverse = false;

#ifdef NEWDICT
    /* DAS New SDAI Dictionary 5/95 */
    /* insert the entity into the schema descriptor */
    fprintf(impl,
            "        ((SDAIAGGRH(Set,EntityH))%s::schema->Entities())->Add(%s::%s%s);\n",
            schema_name, schema_name, ENT_PREFIX, entity_name);
#endif

    if(ENTITYget_abstract(entity)) {
        if(entity->u.entity->subtype_expression) {

            fprintf(impl, "    str.clear();\n    str.append( \"ABSTRACT SUPERTYPE OF ( \" );\n");

            format_for_std_stringout(impl, SUBTYPEto_string(entity->u.entity->subtype_expression));
            fprintf(impl, "    str.append( \")\" );\n");
            fprintf(impl, "    %s::%s%s->AddSupertype_Stmt( str.c_str() );\n", schema_name, ENT_PREFIX, entity_name);
        } else {
            fprintf(impl, "    %s::%s%s->AddSupertype_Stmt( \"ABSTRACT SUPERTYPE\" );\n",
                    schema_name, ENT_PREFIX, entity_name);
        }
    } else {
        if(entity->u.entity->subtype_expression) {
            fprintf(impl, "    str.clear();\n    str.append( \"SUPERTYPE OF ( \" );\n");
            format_for_std_stringout(impl, SUBTYPEto_string(entity->u.entity->subtype_expression));
            fprintf(impl, "    str.append( \")\" );\n");
            fprintf(impl, "    %s::%s%s->AddSupertype_Stmt( str.c_str() );\n", schema_name, ENT_PREFIX, entity_name);
        }
    }
    LISTdo(ENTITYget_supertypes(entity), sup, Entity)
    /*  set the owning schema of the supertype  */
    super_schema = SCHEMAget_name(ENTITYget_schema(sup));
    /* print the supertype list for this entity */
    fprintf(impl, "    %s::%s%s->AddSupertype(%s::%s%s);\n",
            schema_name, ENT_PREFIX, entity_name,
            super_schema,
            ENT_PREFIX, ENTITYget_name(sup));

    /* add this entity to the subtype list of it's supertype    */
    fprintf(impl, "    %s::%s%s->AddSubtype(%s::%s%s);\n",
            super_schema,
            ENT_PREFIX, ENTITYget_name(sup),
            schema_name, ENT_PREFIX, entity_name);
    LISTod

    LISTdo(ENTITYget_attributes(entity), v, Variable)
    if(VARget_inverse(v)) {
        hasInverse = true;
    }
    generate_attribute_name(v, attrnm);
    /*  do EXPLICIT and DERIVED attributes first  */
    /*    if  ( ! VARget_inverse (v))  {*/
    /* first make sure that type descriptor exists */
    if(TYPEget_name(v->type)) {
        if((!TYPEget_head(v->type)) &&
                (TYPEget_body(v->type)->type == entity_)) {
            fprintf(impl, "    %s::%s%d%s%s =", SCHEMAget_name(schema), ATTR_PREFIX, v->idx,
                    (VARis_derived(v) ? "D" : (VARis_type_shifter(v) ? "R" : (VARget_inverse(v) ? "I" : ""))),
                    attrnm);
            fprintf(impl, "\n      new %s( \"%s\",",
                    (VARget_inverse(v) ? "Inverse_attribute" :
                     (VARis_derived(v) ? "Derived_attribute" : "AttrDescriptor")),
                    /* attribute name param */
                    generate_dict_attr_name(v, dict_attrnm));

            /* following assumes we are not in a nested */
            /* entity otherwise we should search upward */
            /* for schema */
            /* attribute's type  */
            fprintf(impl, " %s::%s%s, %s,\n", TYPEget_name(TYPEget_body(v->type)->entity->superscope),
                    ENT_PREFIX, TYPEget_name(v->type), (VARget_optional(v) ? "LTrue" : "LFalse"));
            fprintf(impl, "       %s%s, *%s::%s%s);\n", (VARget_unique(v) ? "LTrue" : "LFalse"),
                    /* Support REDEFINED */
                    (VARget_inverse(v) ? "" : (VARis_derived(v) ? ", AttrType_Deriving" :
                                               (VARis_type_shifter(v) ? ", AttrType_Redefining" : ", AttrType_Explicit"))),
                    schema_name, ENT_PREFIX, TYPEget_name(entity));
        } else {
            /* type reference */
            fprintf(impl, "        %s::%s%d%s%s =\n          new %s"
                    "(\"%s\",%s::%s%s,\n          %s,%s%s,\n          *%s::%s%s);\n",
                    SCHEMAget_name(schema), ATTR_PREFIX, v->idx,
                    (VARis_derived(v) ? "D" :
                     (VARis_type_shifter(v) ? "R" :
                      (VARget_inverse(v) ? "I" : ""))),
                    attrnm,

                    (VARget_inverse(v) ? "Inverse_attribute" : (VARis_derived(v) ? "Derived_attribute" : "AttrDescriptor")),

                    /* attribute name param */
                    generate_dict_attr_name(v, dict_attrnm),

                    SCHEMAget_name(v->type->superscope),
                    TD_PREFIX, TYPEget_name(v->type),

                    (VARget_optional(v) ? "LTrue" : "LFalse"),

                    (VARget_unique(v) ? "LTrue" : "LFalse"),

                    (VARget_inverse(v) ? "" :
                     (VARis_derived(v) ? ", AttrType_Deriving" :
                      (VARis_type_shifter(v) ? ", AttrType_Redefining" : ", AttrType_Explicit"))),

                    schema_name, ENT_PREFIX, TYPEget_name(entity)
                   );
        }
    } else if(TYPEis_builtin(v->type)) {
        /*  the type wasn't named -- it must be built in or aggregate  */

        fprintf(impl, "        %s::%s%d%s%s =\n          new %s"
                "(\"%s\",%s%s,\n          %s,%s%s,\n          *%s::%s%s);\n",
                SCHEMAget_name(schema), ATTR_PREFIX, v->idx,
                (VARis_derived(v) ? "D" :
                 (VARis_type_shifter(v) ? "R" :
                  (VARget_inverse(v) ? "I" : ""))),
                attrnm,
                (VARget_inverse(v) ? "Inverse_attribute" : (VARis_derived(v) ? "Derived_attribute" : "AttrDescriptor")),
                /* attribute name param */
                generate_dict_attr_name(v, dict_attrnm),
                /* not sure about 0 here */ TD_PREFIX, FundamentalType(v->type, 0),
                (VARget_optional(v) ? "LTrue" :
                 "LFalse"),
                (VARget_unique(v) ? "LTrue" :
                 "LFalse"),
                (VARget_inverse(v) ? "" :
                 (VARis_derived(v) ? ", AttrType_Deriving" :
                  (VARis_type_shifter(v) ?
                   ", AttrType_Redefining" :
                   ", AttrType_Explicit"))),
                schema_name, ENT_PREFIX, TYPEget_name(entity)
               );
    } else {
        /* manufacture new one(s) on the spot */
        char typename_buf[MAX_LEN];
        print_typechain(header, impl, v->type, typename_buf, schema, v->name->symbol.name);
        fprintf(impl, "        %s::%s%d%s%s =\n          new %s"
                "(\"%s\",%s,%s,%s%s,\n          *%s::%s%s);\n",
                SCHEMAget_name(schema), ATTR_PREFIX, v->idx,
                (VARis_derived(v) ? "D" :
                 (VARis_type_shifter(v) ? "R" :
                  (VARget_inverse(v) ? "I" : ""))),
                attrnm,
                (VARget_inverse(v) ? "Inverse_attribute" : (VARis_derived(v) ? "Derived_attribute" : "AttrDescriptor")),
                /* attribute name param */
                generate_dict_attr_name(v, dict_attrnm),
                typename_buf,
                (VARget_optional(v) ? "LTrue" :
                 "LFalse"),
                (VARget_unique(v) ? "LTrue" :
                 "LFalse"),
                (VARget_inverse(v) ? "" :
                 (VARis_derived(v) ? ", AttrType_Deriving" :
                  (VARis_type_shifter(v) ?
                   ", AttrType_Redefining" :
                   ", AttrType_Explicit"))),
                schema_name, ENT_PREFIX, TYPEget_name(entity)
               );
    }

    fprintf(impl, "        %s::%s%s->Add%sAttr (%s::%s%d%s%s);\n",
            schema_name, ENT_PREFIX, TYPEget_name(entity),
            (VARget_inverse(v) ? "Inverse" : "Explicit"),
            SCHEMAget_name(schema), ATTR_PREFIX, v->idx,
            (VARis_derived(v) ? "D" :
             (VARis_type_shifter(v) ? "R" :
              (VARget_inverse(v) ? "I" : ""))),
            attrnm);

    if(VARis_derived(v) && v->initializer) {
        tmp = EXPRto_string(v->initializer);
        tmp2 = (char *)sc_malloc(sizeof(char) * (strlen(tmp) + BUFSIZ));
        fprintf(impl, "        %s::%s%d%s%s->initializer_(\"%s\");\n",
                schema_name, ATTR_PREFIX, v->idx,
                (VARis_derived(v) ? "D" :
                 (VARis_type_shifter(v) ? "R" :
                  (VARget_inverse(v) ? "I" : ""))),
                attrnm, format_for_stringout(tmp, tmp2));
        sc_free(tmp);
        sc_free(tmp2);
    }
    if(VARget_inverse(v)) {
        fprintf(impl, "        %s::%s%d%s%s->inverted_attr_id_(\"%s\");\n",
                schema_name, ATTR_PREFIX, v->idx,
                (VARis_derived(v) ? "D" :
                 (VARis_type_shifter(v) ? "R" :
                  (VARget_inverse(v) ? "I" : ""))),
                attrnm, v->inverse_attribute->name->symbol.name);
        if(v->type->symbol.name) {
            fprintf(impl,
                    "        %s::%s%d%s%s->inverted_entity_id_(\"%s\");\n",
                    schema_name, ATTR_PREFIX, v->idx,
                    (VARis_derived(v) ? "D" :
                     (VARis_type_shifter(v) ? "R" :
                      (VARget_inverse(v) ? "I" : ""))), attrnm,
                    v->type->symbol.name);
            fprintf(impl, "// inverse entity 1 %s\n", v->type->symbol.name);
        } else {
            switch(TYPEget_body(v->type)->type) {
                case entity_:
                    fprintf(impl,
                            "        %s%d%s%s->inverted_entity_id_(\"%s\");\n",
                            ATTR_PREFIX, v->idx,
                            (VARis_derived(v) ? "D" :
                             (VARis_type_shifter(v) ? "R" :
                              (VARget_inverse(v) ? "I" : ""))), attrnm,
                            TYPEget_body(v->type)->entity->symbol.name);
                    fprintf(impl, "// inverse entity 2 %s\n", TYPEget_body(v->type)->entity->symbol.name);
                    break;
                case aggregate_:
                case array_:
                case bag_:
                case set_:
                case list_:
                    fprintf(impl,
                            "        %s::%s%d%s%s->inverted_entity_id_(\"%s\");\n",
                            schema_name, ATTR_PREFIX, v->idx,
                            (VARis_derived(v) ? "D" :
                             (VARis_type_shifter(v) ? "R" :
                              (VARget_inverse(v) ? "I" : ""))), attrnm,
                            TYPEget_body(v->type)->base->symbol.name);
                    fprintf(impl, "// inverse entity 3 %s\n", TYPEget_body(v->type)->base->symbol.name);
                    break;
                default:
                    fprintf(stderr, "Error: reached default case at %s:%d", __FILE__, __LINE__);
                    abort();
            }
        }
    }

    LISTod

    fprintf(impl, "        reg.AddEntity( *%s::%s%s );\n", schema_name, ENT_PREFIX, entity_name);
    if(hasInverse) {
        fprintf(impl, "        %s::schema->AddEntityWInverse( %s::%s%s );\n", schema_name, schema_name, ENT_PREFIX, entity_name);
    }

#undef schema_name
}

void ENTITYPrint_h(const Entity entity, FILE *header, Linked_List neededAttr, Schema schema)
{
    const char *name = ENTITYget_classname(entity);
    DEBUG("Entering ENTITYPrint_h for %s\n", name);

    ENTITYhead_print(entity, header);
    DataMemberPrint(entity, neededAttr, header);
    MemberFunctionSign(entity, neededAttr, header);

    fprintf(header, "void init_%s(Registry& reg);\n\n", name);

    fprintf(header, "namespace %s {\n", SCHEMAget_name(schema));
    ENTITYnames_print(entity, header);
    ATTRnames_print(entity, header);
    fprintf(header, "}\n\n");

    DEBUG("DONE ENTITYPrint_h\n");
}

void ENTITYPrint_cc(const Entity entity, FILE *createall, FILE *header, FILE *impl, Linked_List neededAttr, Schema schema, bool externMap)
{
    const char *name = ENTITYget_classname(entity);

    DEBUG("Entering ENTITYPrint_cc for %s\n", name);

    fprintf(impl, "#include \"schema.h\"\n");
    fprintf(impl, "#include \"sc_memmgr.h\"\n");
    fprintf(impl, "#include \"entity/%s.h\"\n\n", name);

    LIBdescribe_entity(entity, impl, schema);
    LIBstructor_print(entity, neededAttr, impl, schema);
    if(multiple_inheritance) {
        LIBstructor_print_w_args(entity, neededAttr, impl, schema);
    }
    LIBmemberFunctionPrint(entity, neededAttr, impl, schema);

    fprintf(impl, "void init_%s( Registry& reg ) {\n", name);
    fprintf(impl, "    std::string str;\n\n");
    ENTITYprint_descriptors(entity, createall, impl, schema, externMap);
    ENTITYincode_print(entity, header, impl, schema);
    fprintf(impl, "}\n\n");

    DEBUG("DONE ENTITYPrint_cc\n");
}

/** \sa collectAttributes */
enum CollectType { ALL, ALL_BUT_FIRST, FIRST_ONLY };

/** Retrieve the list of inherited attributes of an entity
 * \param curList current list to store the attributes
 * \param curEntity current Entity being processed
 * \param collect selects attrs to be collected
 */
static void collectAttributes(Linked_List curList, const Entity curEntity, enum CollectType collect)
{
    Linked_List parent_list = ENTITYget_supertypes(curEntity);

    if(! LISTempty(parent_list)) {
        if(collect != FIRST_ONLY) {
            /*  collect attributes from parents and their supertypes */
            LISTdo(parent_list, e, Entity) {
                if(collect == ALL_BUT_FIRST) {
                    /*  skip first and collect from the rest */
                    collect = ALL;
                } else {
                    /*  collect attributes of this parent and its supertypes */
                    collectAttributes(curList, e, ALL);
                }
            }
            LISTod;
        } else {
            /*  collect attributes of only first parent and its supertypes */
            collectAttributes(curList, (Entity) LISTpeek_first(parent_list), ALL);
        }
    }
    /*  prepend this entity's attributes to the result list */
    LISTdo(ENTITYget_attributes(curEntity), attr, Variable) {
        LISTadd_first(curList, attr);
    }
    LISTod;
}

static bool listContainsVar(Linked_List l, Variable v)
{
    const char *vName = VARget_simple_name(v);
    LISTdo(l, curr, Variable) {
        if(!strcmp(vName, VARget_simple_name(curr))) {
            return true;
        }
    }
    LISTod;
    return false;
}

/** drives the printing of the code for the class library
 *  additional member functions can be generated by writing a routine
 *  to generate the code and calling that routine from this procedure
 *
 * ** Side Effects:  generates code segment for c++ library file
 *
 * \param entity entity being processed
 * \param file  file being written to
 */
void ENTITYlib_print(Entity entity, Linked_List neededAttr, FILE *file, Schema schema)
{
    LIBdescribe_entity(entity, file, schema);
    LIBstructor_print(entity, neededAttr, file, schema);
    if(multiple_inheritance) {
        LIBstructor_print_w_args(entity, neededAttr, file, schema);
    }
    LIBmemberFunctionPrint(entity, neededAttr, file, schema);
}

/** drives the functions for printing out code in lib,
 * include, and initialization files for a specific entity class
 * \p entity entity being processed
 * \p files  files being written to
 * \p schema name of the schema
 * \p col the ComplexCollect
 */
void ENTITYPrint(Entity entity, FILES *files, Schema schema, bool externMap)
{
    FILE *hdr, * impl;
    char *n = ENTITYget_name(entity);
    Linked_List remaining = LISTcreate();
    filenames_t names = getEntityFilenames(entity);

    DEBUG("Entering ENTITYPrint for %s\n", n);

    if(multiple_inheritance) {
        Linked_List existing = LISTcreate();
        Linked_List required = LISTcreate();

        /*  create list of attr inherited from the parents in C++ */
        collectAttributes(existing, entity, FIRST_ONLY);

        /*  create list of attr that have to be inherited in EXPRESS */
        collectAttributes(required, entity, ALL_BUT_FIRST);

        /*  build list of unique attr that are required but haven't been */
        /*  inherited */
        LISTdo(required, attr, Variable) {
            if(!listContainsVar(existing, attr) &&
                    !listContainsVar(remaining, attr)) {
                LISTadd_first(remaining, attr);
            }
        }
        LISTod;
        LIST_destroy(existing);
        LIST_destroy(required);
    }
    if(mkDirIfNone("entity") == -1) {
        fprintf(stderr, "At %s:%d - mkdir() failed with error ", __FILE__, __LINE__);
        perror(0);
        abort();
    }

    hdr = FILEcreate(names.header);
    impl = FILEcreate(names.impl);
    assert(hdr && impl && "error creating files");
    fprintf(files->unity.entity.hdr, "#include \"%s\"\n", names.header);   /* TODO this is not necessary? */
    fprintf(files->unity.entity.impl, "#include \"%s\"\n", names.impl);

    ENTITYPrint_h(entity, hdr, remaining, schema);
    ENTITYPrint_cc(entity, files->create, hdr, impl, remaining, schema, externMap);
    FILEclose(hdr);
    FILEclose(impl);

    fprintf(files->inc, "#include \"entity/%s.h\"\n", ENTITYget_classname(entity));
    fprintf(files->init, "    init_%s( reg );\n", ENTITYget_classname(entity));

    DEBUG("DONE ENTITYPrint\n");
    LIST_destroy(remaining);
}

/** create entity descriptors
 *
 * originally part of ENTITYprint_new(), along with ENTITYprint_classes()
 *
 * \p entity the entity to print
 * \p createall the file to write eDesc into
 * \p impl the file to write rules into
 * \p schema the current schema
 * \p externMap true if entity must be instantiated with external mapping (see Part 21, sect 11.2.5.1).
 *
 * eDesc is printed into createall because it must be initialized before other entity init fn's are called
 * alternative is two init fn's per ent. call init1 for each ent, then repeat with init2
 */
void ENTITYprint_descriptors(Entity entity, FILE *createall, FILE *impl, Schema schema, bool externMap)
{
    fprintf(createall, "    %s::%s%s = new EntityDescriptor( ", SCHEMAget_name(schema), ENT_PREFIX, ENTITYget_name(entity));
    fprintf(createall, "\"%s\", %s::schema, %s, ", PrettyTmpName(ENTITYget_name(entity)), SCHEMAget_name(schema), (ENTITYget_abstract(entity) ? "LTrue" : "LFalse"));
    fprintf(createall, "%s, (Creator) create_%s );\n", externMap ? "LTrue" : "LFalse", ENTITYget_classname(entity));
    /* add the entity to the Schema dictionary entry */
    fprintf(createall, "    %s::schema->AddEntity(%s::%s%s);\n", SCHEMAget_name(schema), SCHEMAget_name(schema), ENT_PREFIX, ENTITYget_name(entity));

    WHEREprint(ENTITYget_name(entity), TYPEget_where(entity), impl, schema, true);
    UNIQUEprint(entity, impl, schema);
}

/** print in classes file: class forward prototype, class typedefs
 * split out of ENTITYprint_new, which is now ENTITYprint_descriptors
 */
void ENTITYprint_classes(Entity entity, FILE *classes)
{
    const char *n = ENTITYget_classname(entity);
    fprintf(classes, "\nclass %s;\n", n);
    fprintf(classes, "typedef %s *          %sH;\n", n, n);
    fprintf(classes, "typedef %s *          %s_ptr;\n", n, n);
    fprintf(classes, "typedef const %s *    %s_ptr_c;\n", n, n);
    fprintf(classes, "typedef %s_ptr        %s_var;\n", n, n);
    fprintf(classes, "#define %s__set       SDAI_DAObject__set\n", n);
    fprintf(classes, "#define %s__set_var   SDAI_DAObject__set_var\n", n);
}
