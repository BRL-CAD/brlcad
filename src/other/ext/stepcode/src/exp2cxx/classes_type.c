/** *****************************************************************
** \file classes_type.c
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
#include <path2str.h>
#include <stdlib.h>
#include <assert.h>
#include <sc_mkdir.h>
#include "classes.h"
#include "class_strings.h"
#include "genCxxFilenames.h"
#include <ordered_attrs.h>
#include "rules.h"

#include <sc_trace_fprintf.h>

static int type_count;  /**< number each temporary type for same reason as \sa attr_count */

extern char *non_unique_types_string(const Type type);

static void printEnumCreateHdr(FILE *, const Type);
static void printEnumCreateBody(FILE *, const Type);
static void printEnumAggrCrHdr(FILE *, const Type);
static void printEnumAggrCrBody(FILE *, const Type);

int TYPEget_RefTypeVarNm(const Type t, char *buf, Schema schema);

int isMultiDimAggregateType(const Type t);

void Type_Description(const Type, char *);
void TypeBody_Description(TypeBody body, char *buf);

/** write representation of expression to end of buf
 *
 * TODO: add buflen arg and check for overflow
 */
void strcat_expr(Expression e, char *buf)
{
    if(e == LITERAL_INFINITY) {
        strcat(buf, "?");
    } else if(e == LITERAL_PI) {
        strcat(buf, "PI");
    } else if(e == LITERAL_E) {
        strcat(buf, "E");
    } else if(e == LITERAL_ZERO) {
        strcat(buf, "0");
    } else if(e == LITERAL_ONE) {
        strcat(buf, "1");
    } else if(TYPEget_name(e)) {
        strcat(buf, TYPEget_name(e));
    } else if(TYPEget_body(e->type)->type == integer_) {
        char tmpbuf[30];
        sprintf(tmpbuf, "%d", e->u.integer);
        strcat(buf, tmpbuf);
    } else {
        strcat(buf, "??");
    }
}

/** print t's bounds to end of buf
 *
 * TODO: add buflen arg and check for overflow
 */
void strcat_bounds(TypeBody b, char *buf)
{
    if(!b->upper) {
        return;
    }

    strcat(buf, " [");
    strcat_expr(b->lower, buf);
    strcat(buf, ":");
    strcat_expr(b->upper, buf);
    strcat(buf, "]");
}

/******************************************************************
 ** Procedure:  TYPEprint_enum
 ** Parameters: const Type type - type to print
 **     FILE*      f    - file on which to print
 ** Returns:
 ** Requires:   TYPEget_class(type) == TYPE_ENUM
 ** Description:  prints code to represent an enumerated type in c++
 ** Side Effects:  prints to header file
 ** Status:  ok 1/15/91
 ** Changes: Modified to check for appropriate key words as described
 **          in "SDAI C++ Binding for PDES, Inc. Prototyping" by
 **          Stephen Clark.
 ** - Changed to match CD2 Part 23, 1/14/97 DAS
 ** Change Date: 5/22/91  CD
 ******************************************************************/
const char *EnumCElementName(Type type, Expression expr)
{
    static char buf [BUFSIZ+1];
    sprintf(buf, "%s__",
            EnumName(TYPEget_name(type)));
    strncat(buf, StrToLower(EXPget_name(expr)), BUFSIZ);

    return buf;
}

char *CheckEnumSymbol(char *s)
{
    static char b [BUFSIZ+1];
    if(strcmp(s, "sdaiTRUE")
            && strcmp(s, "sdaiFALSE")
            && strcmp(s, "sdaiUNKNOWN")) {
        /*  if the symbol is not a reserved one */
        return (s);

    } else {
        strncpy(b, s, BUFSIZ-1);
        strcat(b, "_");
        fprintf(stderr, "Warning in %s: the enumerated value %s is already being used and has been changed to %s\n", __func__, s, b);
        return (b);
    }
}

/**
 * return printable version of entire type definition
 * return it in static buffer
 */
char *TypeDescription(const Type t)
{
    static char buf[6000];

    buf[0] = '\0';

    if(TYPEget_head(t)) {
        Type_Description(TYPEget_head(t), buf);
    } else {
        TypeBody_Description(TYPEget_body(t), buf);
    }

    /* should also print out where clause here */

    return buf + 1;
}

/**************************************************************//**
 ** Procedure:   TYPEenum_inc_print
 ** Description: Writes enum type descriptors and classes.
 ** Change Date:
 ********************************************************************/
void TYPEenum_inc_print(const Type type, FILE *inc)
{
    Expression expr;

    char tdnm[BUFSIZ+1],
         enumAggrNm[BUFSIZ+1];
    const char *n;   /*  pointer to class name  */
    int cnt = 0;

    /*  print c++ enumerated values for class   */
    fprintf(inc, "enum %s {\n", EnumName(TYPEget_name(type)));

    LISTdo_links(TYPEget_body(type)->list, link)
    /*  print the elements of the c++ enum type  */
    expr = (Expression)link->data;
    if(cnt != 0) {
        fprintf(inc, ",\n");
    }
    ++cnt;
    fprintf(inc, "        %s", EnumCElementName(type, expr));

    LISTod

    fprintf(inc, ",\n        %s_unset\n};\n", EnumName(TYPEget_name(type)));

    /*  print class for enumeration */
    n = TYPEget_ctype(type);
    fprintf(inc, "\nclass SC_SCHEMA_EXPORT %s  :  public SDAI_Enum  {\n", n);

    fprintf(inc, "  protected:\n        EnumTypeDescriptor *type;\n\n");

    /*  constructors    */
    strncpy(tdnm, TYPEtd_name(type), BUFSIZ);
    tdnm[BUFSIZ] = '\0';
    fprintf(inc, "  public:\n        %s (const char * n =0, EnumTypeDescriptor *et =%s);\n", n, tdnm);
    fprintf(inc, "        %s (%s e, EnumTypeDescriptor *et =%s)\n"
            "                : type(et) {  set_value (e);  }\n",
            n, EnumName(TYPEget_name(type)), tdnm);
    fprintf(inc, "        %s (const %s &e) { set_value(e); }\n", n, TYPEget_ctype(type));

    /*  destructor  */
    fprintf(inc, "        ~%s () { }\n", n);

    /*      operator =      */
    fprintf(inc, "        %s& operator= (const %s& e)\n",
            n, TYPEget_ctype(type));
    fprintf(inc, "                {  set_value (e);  return *this;  }\n");

    /*      operator to cast to an enumerated type  */
    fprintf(inc, "        operator %s () const;\n",
            EnumName(TYPEget_name(type)));

    /*      others          */
    fprintf(inc, "\n        inline virtual const char * Name () const\n");
    fprintf(inc, "                {  return type->Name();  }\n");
    fprintf(inc, "        inline virtual int no_elements () const"
            "  {  return %d;  }\n", cnt);
    fprintf(inc, "        virtual const char * element_at (int n) const;\n");

    /*  end class definition  */
    fprintf(inc, "};\n");

    fprintf(inc, "\ntypedef       %s *   %s_ptr;\n", n, n);
    fprintf(inc, "\ntypedef const %s *   %s_ptr_c;\n", n, n);


    /*  Print ObjectStore Access Hook function  */
    printEnumCreateHdr(inc, type);

    /* DAS brandnew above */

    /*  print things for aggregate class  */
    sprintf(enumAggrNm, "%s_agg", n);

    fprintf(inc, "\nclass %s_agg  :  public EnumAggregate  {\n", n);

    fprintf(inc, "  protected:\n    EnumTypeDescriptor *enum_type;\n\n");
    fprintf(inc, "  public:\n");
    fprintf(inc, "    %s_agg( EnumTypeDescriptor * =%s);\n", n, tdnm);
    fprintf(inc, "    virtual ~%s_agg();\n", n);
    fprintf(inc, "    virtual SingleLinkNode * NewNode()\n");
    fprintf(inc, "        { return new EnumNode (new %s( \"\", enum_type )); }"
            "\n", n);

    fprintf(inc, "};\n");

    fprintf(inc, "\ntypedef       %s_agg *   %s_agg_ptr;\n", n, n);
    fprintf(inc, "\ntypedef const %s_agg *   %s_agg_ptr_c;\n", n, n);

    /* DAS brandnew below */

    /* DAS creation function for enum aggregate class */
    printEnumAggrCrHdr(inc, type);

    /* DAS brandnew above */
}

void TYPEenum_lib_print(const Type type, FILE *f)
{
    DictionaryEntry de;
    Expression expr;
    const char *n;    /*  pointer to class name  */
    char c_enum_ele [BUFSIZ+1];

    n = TYPEget_ctype(type);

    /*  set up the dictionary info  */

    fprintf(f, "const char *\n%s::element_at (int n) const  {\n", n);
    fprintf(f, "  switch (n)  {\n");
    DICTdo_type_init(ENUM_TYPEget_items(type), &de, OBJ_ENUM);
    while(0 != (expr = (Expression)DICTdo(&de))) {
        strncpy(c_enum_ele, EnumCElementName(type, expr), BUFSIZ);
        c_enum_ele[BUFSIZ] = '\0';
        fprintf(f, "  case %s:  return \"%s\";\n",
                c_enum_ele,
                StrToUpper(EXPget_name(expr)));
    }
    fprintf(f, "  case %s_unset        :\n", EnumName(TYPEget_name(type)));
    fprintf(f, "  default                :  return \"UNSET\";\n  }\n}\n");

    /*    constructors    */
    /*    construct with character string  */
    fprintf(f, "\n%s::%s (const char * n, EnumTypeDescriptor *et)\n"
            "  : type(et)\n{\n", n, n);
    fprintf(f, "  set_value (n);\n}\n");

    /*      cast operator to an enumerated type  */
    fprintf(f, "\n%s::operator %s () const {\n", n,
            EnumName(TYPEget_name(type)));
    fprintf(f, "  switch (v) {\n");
    DICTdo_type_init(ENUM_TYPEget_items(type), &de, OBJ_ENUM);
    while(0 != (expr = (Expression)DICTdo(&de))) {
        strncpy(c_enum_ele, EnumCElementName(type, expr), BUFSIZ);
        fprintf(f, "        case %s        :  ", c_enum_ele);
        fprintf(f, "return %s;\n", c_enum_ele);
    }
    /*  print the last case with the default so sun c++ doesn't complain */
    fprintf(f, "        case %s_unset        :\n", EnumName(TYPEget_name(type)));
    fprintf(f, "        default                :  return %s_unset;\n  }\n}\n", EnumName(TYPEget_name(type)));

    printEnumCreateBody(f, type);

    /* print the enum aggregate functions */

    fprintf(f, "\n%s_agg::%s_agg( EnumTypeDescriptor *et )\n", n, n);
    fprintf(f, "    : enum_type(et)\n{\n}\n\n");
    fprintf(f, "%s_agg::~%s_agg()\n{\n}\n", n, n);

    printEnumAggrCrBody(f, type);
}

void TYPEPrint_h(const Type type, FILE *file)
{
    DEBUG("Entering TYPEPrint_h for %s\n", TYPEget_ctype(type));

    if(TYPEis_enumeration(type)) {
        TYPEenum_inc_print(type, file);
    } else if(TYPEis_select(type)) {
        TYPEselect_inc_print(type, file);
    }

    fprintf(file, "void init_%s(Registry& reg);\n\n", TYPEget_ctype(type));

    DEBUG("DONE TYPEPrint_h\n");
}

void TYPEPrint_cc(const Type type, const filenames_t *names, FILE *hdr, FILE *impl, Schema schema)
{
    DEBUG("Entering TYPEPrint_cc for %s\n", names->impl);

    fprintf(impl, "#include \"schema.h\"\n");
    fprintf(impl, "#include \"sc_memmgr.h\"\n");
    fprintf(impl, "#include \"%s\"\n\n", names->header);

    if(TYPEis_enumeration(type)) {
        TYPEenum_lib_print(type, impl);
    } else if(TYPEis_select(type)) {
        TYPEselect_lib_print(type, impl);
    }

    fprintf(impl, "\nvoid init_%s( Registry& reg ) {\n", TYPEget_ctype(type));
    fprintf(impl, "    std::string str;\n");
    /* moved from SCOPEPrint in classes_wrapper */
    TYPEprint_new(type, impl, schema, true);
    TYPEprint_init(type, hdr, impl, schema);
    fprintf(impl, "}\n\n");

    DEBUG("DONE TYPEPrint_cc\n");
}

void TYPEPrint(const Type type, FILES *files, Schema schema)
{
    FILE *hdr, * impl;
    filenames_t names = getTypeFilenames(type);

    fprintf(files->inc, "#include \"%s\"\n", names.header);

    fprintf(files->init, "    init_%s( reg );\n", TYPEget_ctype(type));

    if(mkDirIfNone("type") == -1) {
        fprintf(stderr, "At %s:%d - mkdir() failed with error ", __FILE__, __LINE__);
        perror(0);
        abort();
    }
    hdr = FILEcreate(names.header);
    impl = FILEcreate(names.impl);
    assert(hdr && impl && "error creating files");
    fprintf(files->unity.type.hdr, "#include \"%s\"\n", names.header);
    fprintf(files->unity.type.impl, "#include \"%s\"\n", names.impl);

    TYPEPrint_h(type, hdr);
    TYPEPrint_cc(type, &names, hdr, impl, schema);

    FILEclose(hdr);
    FILEclose(impl);
}

/**
 * Prints a bunch of lines for enumeration creation functions (i.e., "cre-
 * ate_SdaiEnum1()").  Since this is done both for an enum and for "copies"
 * of it (when "TYPE enum2 = enum1"), I placed this code in a separate fn.
 *
 * NOTE - "Print ObjectStore Access Hook function" comment seen at one of
 * the calls seems to imply it's ObjectStore specific...
 */
static void printEnumCreateHdr(FILE *inc, const Type type)
{
    const char *nm = TYPEget_ctype(type);

    fprintf(inc, "  SDAI_Enum * create_%s();\n", nm);
}

/** See header comment above by printEnumCreateHdr. */
static void printEnumCreateBody(FILE *lib, const Type type)
{
    const char *nm = TYPEget_ctype(type);
    char tdnm[BUFSIZ+1];
    tdnm[BUFSIZ] = '\0';

    strncpy(tdnm, TYPEtd_name(type), BUFSIZ);
    tdnm[BUFSIZ] = '\0';

    fprintf(lib, "\nSDAI_Enum *\ncreate_%s ()\n{\n", nm);
    fprintf(lib, "    return new %s( \"\", %s );\n}\n\n", nm, tdnm);
}

/** Similar to printEnumCreateHdr above for the enum aggregate. */
static void printEnumAggrCrHdr(FILE *inc, const Type type)
{
    const char *n = TYPEget_ctype(type);
    /*    const char *n = ClassName( TYPEget_name(type) ));*/
    fprintf(inc, "  STEPaggregate * create_%s_agg ();\n", n);
}

static void printEnumAggrCrBody(FILE *lib, const Type type)
{
    const char *n = TYPEget_ctype(type);
    char tdnm[BUFSIZ+1];

    strncpy(tdnm, TYPEtd_name(type), BUFSIZ);
    tdnm[BUFSIZ] = '\0';

    fprintf(lib, "\nSTEPaggregate *\ncreate_%s_agg ()\n{\n", n);
    fprintf(lib, "    return new %s_agg( %s );\n}\n", n, tdnm);
}

/** ************************************************************************
 ** Procedure:  TYPEprint_typedefs
 ** Parameters:  const Type type
 ** Returns:
 ** Description:
 **    Prints in Sdaiclasses.h typedefs, forward declarations, and externs
 **    for user-defined types.  Only a fraction of the typedefs and decla-
 **    rations are needed in Sdaiclasses.h.  Enum's and selects must actu-
 **    ally be defined before objects (such as entities) which use it can
 **    be defined.  So forward declarations will not serve any purpose.
 **    Other redefined types and aggregate types may be declared here.
 ** Side Effects:
 ** Status:  16-Mar-1993 kcm; updated 04-Feb-1997 dar
 ** Dec 2011 - MAP - remove goto
 **************************************************************************/
void TYPEprint_typedefs(Type t, FILE *classes)
{
    char nm [BUFSIZ+1];
    Type i;
    bool aggrNot1d = true;  /* added so I can get rid of a goto */

    /* Print the typedef statement (poss also a forward class def: */
    if(TYPEis_enumeration(t)) {
        /* For enums and sels (else clause below), we need forward decl's so
          that if we later come across a type which is an aggregate of one of
          them, we'll be able to process it.  For selects, we also need a decl
          of the class itself, while for enum's we don't.  Objects which con-
          tain an enum can't be generated until the enum is generated.  (The
          same is basically true for the select, but a sel containing an ent
          containing a sel needs the forward decl (trust me ;-) ).
         */
        if(!TYPEget_head(t)) {
            /* Only print this enum if it is an actual type and not a redefi-
              nition of another enum.  (Those are printed at the end of the
              classes file - after all the actual enum's.  They must be
              printed last since they depend on the others.)
             */
            strncpy(nm, TYPEget_ctype(t), BUFSIZ);
            nm[BUFSIZ] = '\0';
            fprintf(classes, "class %s_agg;\n", nm);
        }
    } else if(TYPEis_select(t)) {
        if(!TYPEget_head(t)) {
            /* Same comment as above. */
            strncpy(nm, SelectName(TYPEget_name(t)), BUFSIZ);
            nm[BUFSIZ] = '\0';
            fprintf(classes, "class %s;\n", nm);
            fprintf(classes, "typedef       %s *   %s_ptr;\n", nm, nm);
            fprintf(classes, "typedef const %s *   %s_ptr_c;\n", nm, nm);
            fprintf(classes, "class %s_agg;\n", nm);
            fprintf(classes, "typedef       %s_agg *   %s_agg_ptr;\n", nm, nm);
            fprintf(classes, "typedef const %s_agg *   %s_agg_ptr_c;\n", nm, nm);
        }
    } else {
        if(TYPEis_aggregate(t)) {
            i = TYPEget_base_type(t);
            if(TYPEis_enumeration(i) || TYPEis_select(i)) {
                /* One exceptional case - a 1d aggregate of an enum or select.
                   We must wait till the enum/sel itself has been processed.
                   To ensure this, we process all such 1d aggrs in a special
                   loop at the end (in multpass.c).  2d aggrs (or higher), how-
                   ever, can be processed now - they only require GenericAggr
                   for their definition here.
                 */
                aggrNot1d = false;
            }
        }
        if(aggrNot1d) {
            /* At this point, we'll print typedefs for types which are redefined
               fundamental types and their aggregates, and for 2D aggregates(aggre-
               gates of aggregates) of enum's and selects.
             */
            strncpy(nm, ClassName(TYPEget_name(t)), BUFSIZ);
            nm[BUFSIZ] = '\0';
            fprintf(classes, "typedef       %s       %s;\n", TYPEget_ctype(t), nm);
            if(TYPEis_aggregate(t)) {
                fprintf(classes, "typedef       %s *     %sH;\n", nm, nm);
                fprintf(classes, "typedef       %s *     %s_ptr;\n", nm, nm);
                fprintf(classes, "typedef const %s *     %s_ptr_c;\n", nm, nm);
                fprintf(classes, "typedef       %s_ptr   %s_var;\n", nm, nm);
            }
        }
    }

    /* Print the extern statement: */
    strncpy(nm, TYPEtd_name(t), BUFSIZ);
    fprintf(classes, "extern SC_SCHEMA_EXPORT %s         *%s;\n", GetTypeDescriptorName(t), nm);
}

/** **
   print stuff for types that are declared in Express TYPE statements... i.e.
   extern descriptor declaration in .h file - MOVED BY DAR to TYPEprint_type-
       defs - in order to print all the Sdaiclasses.h stuff in exp2cxx's
       first pass through each schema.
   descriptor definition in the .cc file
   initialize it in the .init.cc file (DAR - all initialization done in fn
       TYPEprint_init() (below) which is done in exp2cxx's 1st pass only.)
*****/
void TYPEprint_descriptions(const Type type, FILES *files, Schema schema)
{
    char tdnm [BUFSIZ+1],
         typename_buf [MAX_LEN],
         base [BUFSIZ+1],
         nm [BUFSIZ+1];
    Type i;

    strncpy(tdnm, TYPEtd_name(type), BUFSIZ);
    tdnm[BUFSIZ] = '\0';

    /* define type descriptor pointer */
    /*  in source - declare the real definition of the pointer */
    /*  i.e. in the .cc file                                   */
    fprintf(files -> lib, "%s         *%s;\n", GetTypeDescriptorName(type), tdnm);

    if(isAggregateType(type)) {
        const char *ctype = TYPEget_ctype(type);

        fprintf(files->inc, "STEPaggregate * create_%s ();\n\n",
                ClassName(TYPEget_name(type)));

        fprintf(files->lib,
                "STEPaggregate *\ncreate_%s () {  return create_%s();  }\n",
                ClassName(TYPEget_name(type)), ctype);

        /* this function is assigned to the aggrCreator var in TYPEprint_new */
    }

    if(TYPEis_enumeration(type) && (i = TYPEget_ancestor(type)) != NULL) {
        /* If we're a renamed enum type, just print a few typedef's to the
         * original and some specialized create functions:
         */
        strncpy(base, EnumName(TYPEget_name(i)), BUFSIZ);
        strncpy(nm, EnumName(TYPEget_name(type)), BUFSIZ);
        fprintf(files->inc, "typedef %s %s;\n", base, nm);
        strncpy(base, TYPEget_ctype(i), BUFSIZ);
        strncpy(nm, TYPEget_ctype(type), BUFSIZ);
        fprintf(files->inc, "typedef %s %s;\n", base, nm);
        printEnumCreateHdr(files->inc, type);
        printEnumCreateBody(files->lib, type);
        fprintf(files->inc, "typedef       %s_agg *       %s_agg_ptr;\n", nm, nm);
        fprintf(files->inc, "typedef const %s_agg *       %s_agg_ptr_c;\n", nm, nm);
        printEnumAggrCrHdr(files->inc, type);
        printEnumAggrCrBody(files->lib, type);
        return;
    }

    if(!TYPEget_RefTypeVarNm(type, typename_buf, schema)) {
        if(TYPEis_enumeration(type)) {
            TYPEPrint(type, files, schema);
        } /* so we don't do anything for non-enums??? */
    } else {
        TYPEprint_new(type, files->create, schema, false);
        TYPEprint_init(type, files->inc, files->init, schema);
    }
}

void TYPEprint_init(const Type type, FILE *header, FILE *impl, Schema schema)
{
    char tdnm [BUFSIZ+1];
    char typename_buf[MAX_LEN];

    strncpy(tdnm, TYPEtd_name(type), BUFSIZ);
    tdnm[BUFSIZ] = '\0';

    if(isAggregateType(type)) {
        AGGRprint_init(header, impl, type, tdnm, type->symbol.name);
    }

    /* fill in the TD's values in the SchemaInit function (it is already
    declared with basic values) */

    if(TYPEget_RefTypeVarNm(type, typename_buf, schema)) {
        fprintf(impl, "        %s->ReferentType(%s);\n", tdnm, typename_buf);
    } else {
        switch(TYPEget_body(type)->type) {
            case aggregate_: /* aggregate_ should not happen? DAS */
            case array_:
            case bag_:
            case set_:
            case list_: {
                if(isMultiDimAggregateType(type)) {
                    print_typechain(header, impl, TYPEget_body(type)->base,
                                    typename_buf, schema, type->symbol.name);
                    fprintf(impl, "        %s->ReferentType(%s);\n", tdnm,
                            typename_buf);
                }
                break;
            }
            default:
                break;
        }
    }

    /* DAR - moved fn call below from TYPEselect_print to here to put all init
    ** info together. */
    if(TYPEis_select(type)) {
        TYPEselect_init_print(type, impl);
    }
#ifdef NEWDICT
    /* DAS New SDAI Dictionary 5/95 */
    /* insert the type into the schema descriptor */
    fprintf(impl,
            "        ((SDAIAGGRH(Set,DefinedTypeH))%s::schema->Types())->Add((DefinedTypeH)%s);\n",
            SCHEMAget_name(schema), tdnm);
#endif
    /* insert into type dictionary */
    fprintf(impl, "    reg.AddType (*%s);\n", tdnm);
}

/** print name, fundamental type, and description initialization function calls */
void TYPEprint_nm_ft_desc(Schema schema, const Type type, FILE *f, char *endChars)
{
    fprintf(f, "                  \"%s\",        // Name\n", PrettyTmpName(TYPEget_name(type)));
    fprintf(f, "                  %s,        // FundamentalType\n", FundamentalType(type, 1));
    fprintf(f, "                  %s::schema,        // Originating Schema\n", SCHEMAget_name(schema));
    fprintf(f, "                  \"%s\"%s        // Description\n", TypeDescription(type), endChars);
}

/** new space for a variable of type TypeDescriptor (or subtype).  This
 *  function is called for Types that have an Express name.
 */
void TYPEprint_new(const Type type, FILE *create, Schema schema, bool needWR)
{
    Type tmpType = TYPEget_head(type);
    Type bodyType = tmpType;

    /* define type definition */
    /*  in source - the real definition of the TypeDescriptor   */

    if(TYPEis_select(type)) {
        char *temp;
        temp = non_unique_types_string(type);
        fprintf(create, "        %s = new SelectTypeDescriptor (\n                  ~%s,        //unique elements,\n", TYPEtd_name(type), temp);
        sc_free(temp);
        TYPEprint_nm_ft_desc(schema, type, create, ",");
        fprintf(create, "                  (SelectCreator) create_%s);        // Creator function\n", SelectName(TYPEget_name(type)));
    } else {
        switch(TYPEget_body(type)->type) {
            case boolean_:
                fprintf(create, "        %s = new EnumTypeDescriptor (\n", TYPEtd_name(type));
                TYPEprint_nm_ft_desc(schema, type, create, ",");
                fprintf(create, "                  (EnumCreator) create_BOOLEAN);        // Creator function\n");
                break;
            case logical_:
                fprintf(create, "        %s = new EnumTypeDescriptor (\n", TYPEtd_name(type));
                TYPEprint_nm_ft_desc(schema, type, create, ",");
                fprintf(create, "                  (EnumCreator) create_LOGICAL);        // Creator function\n");
                break;
            case enumeration_:
                fprintf(create, "        %s = new EnumTypeDescriptor (\n", TYPEtd_name(type));
                TYPEprint_nm_ft_desc(schema, type, create, ",");
                /* get the type name of the underlying type - it is the type that needs to get created */
                tmpType = TYPEget_head(type);
                if(tmpType) {
                    bodyType = tmpType;
                    while(tmpType) {
                        bodyType = tmpType;
                        tmpType = TYPEget_head(tmpType);
                    }
                    fprintf(create, "                  (EnumCreator) create_%s);        // Creator function\n", TYPEget_ctype(bodyType));
                } else {
                    fprintf(create, "                  (EnumCreator) create_%s);        // Creator function\n", TYPEget_ctype(type));
                }
                break;
            case aggregate_:
            case array_:
            case bag_:
            case set_:
            case list_:
                fprintf(create, "\n        %s = new %s (\n", TYPEtd_name(type), GetTypeDescriptorName(type));
                TYPEprint_nm_ft_desc(schema, type, create, ",");
                fprintf(create, "                  (AggregateCreator) create_%s);        // Creator function\n\n", ClassName(TYPEget_name(type)));
                break;
            default:
                fprintf(create, "        %s = new TypeDescriptor (\n", TYPEtd_name(type));
                TYPEprint_nm_ft_desc(schema, type, create, ");");
                break;
        }
    }
    /* add the type to the Schema dictionary entry */
    fprintf(create, "        %s::schema->AddType(%s);\n", SCHEMAget_name(schema), TYPEtd_name(type));

    WHEREprint(TYPEtd_name(type), type->where, create, 0, needWR);
}

/** Get the TypeDescriptor variable name that t's TypeDescriptor references (if
   possible).
   pass space in through buf, buff will be filled in with the name of the
   TypeDescriptor (TD) that needs to be referenced by the TD that is
   generated for Type t.  Return 1 if buf has been filled in with the name
   of a TD.  Return 0 if it hasn't for these reasons: Enumeration TDs don't
   reference another TD, select TDs reference several TDs and are handled
   separately, Multidimensional aggregates generate unique intermediate TD
   variables that are referenced - when these don't have an Express related
   name this function can't know about them - e.g.
   TYPE listSetAggr = LIST OF SET OF STRING;  This function won't fill in the
   name that listSetAggr's ListTypeDescriptor will reference.
   TYPE arrListSetAggr = ARRAY [1:4] of listSetAggr;  This function will
   return the name of the TD that arrlistSetAggr's ArrayTypeDescriptor should
   reference since it has an Express name associated with it.
   *
   Nov 2011 - MAP - modified to insert scope operator into variable name.
   Reason: use of namespace for global variables
*/
int TYPEget_RefTypeVarNm(const Type t, char *buf, Schema schema)
{

    /* It looks like TYPEget_head(t) is true when processing a type
       that refers to another type. e.g. when processing "name" in:
       TYPE name = label; ENDTYPE; TYPE label = STRING; ENDTYPE; DAS */
    if(TYPEget_head(t)) {
        /* this means that it is defined in an Express TYPE stmt and
           it refers to another Express TYPE stmt */
        /*  it would be a reference_ type */
        /*  a TypeDescriptor of the form <schema_name>t_<type_name_referred_to> */
        sprintf(buf, "%s::%s%s",
                SCHEMAget_name(TYPEget_head(t)->superscope),
                TYPEprefix(t), TYPEget_name(TYPEget_head(t)));
        return 1;
    } else {
        switch(TYPEget_body(t)->type) {
            case integer_:
            case real_:
            case boolean_:
            case logical_:
            case string_:
            case binary_:
            case number_:
                /* one of the SCL builtin TypeDescriptors of the form
                   t_STRING_TYPE, or t_REAL_TYPE */
                sprintf(buf, "%s%s", TD_PREFIX, FundamentalType(t, 0));
                return 1;
                break;

            case enumeration_: /* enums don't have a referent type */
            case select_:  /* selects are handled differently elsewhere, they
                refer to several TypeDescriptors */
                return 0;
                break;

            case entity_:
                sprintf(buf, "%s", TYPEtd_name(t));
                /* following assumes we are not in a nested entity */
                /* otherwise we should search upward for schema */
                return 1;
                break;

            case aggregate_:
            case array_:
            case bag_:
            case set_:
            case list_:
                /* referent TypeDescriptor will be the one for the element unless it
                   is a multidimensional aggregate then return 0 */

                if(isMultiDimAggregateType(t)) {
                    if(TYPEget_name(TYPEget_body(t)->base)) {
                        sprintf(buf, "%s::%s%s",
                                SCHEMAget_name(TYPEget_body(t)->base->superscope),
                                TYPEprefix(t), TYPEget_name(TYPEget_body(t)->base));
                        return 1;
                    }

                    /* if the multi aggr doesn't have a name then we are out of scope
                       of what this function can do */
                    return 0;
                } else {
                    /* for a single dimensional aggregate return TypeDescriptor
                       for element */
                    /* being an aggregate implies that base below is not 0 */

                    if(TYPEget_body(TYPEget_body(t)->base)->type == enumeration_ ||
                            TYPEget_body(TYPEget_body(t)->base)->type == select_) {

                        sprintf(buf, "%s", TYPEtd_name(TYPEget_body(t)->base));
                        return 1;
                    } else if(TYPEget_name(TYPEget_body(t)->base)) {
                        if(TYPEget_body(TYPEget_body(t)->base)->type == entity_) {
                            sprintf(buf, "%s", TYPEtd_name(TYPEget_body(t)->base));
                            return 1;
                        }
                        sprintf(buf, "%s::%s%s",
                                SCHEMAget_name(TYPEget_body(t)->base->superscope),
                                TYPEprefix(t), TYPEget_name(TYPEget_body(t)->base));
                        return 1;
                    }
                    return TYPEget_RefTypeVarNm(TYPEget_body(t)->base, buf, schema);
                }
                break;
            default:
                return 0;
        }
    }
    /* NOTREACHED */
    return 0;
}

/** go down through a type's base type chain,
   make and print new TypeDescriptors for each type with no name.

   This function should only be called for types that don't have an
   associated Express name.  Currently this only includes aggregates.
   If this changes this function needs to be changed to support the type
   that changed.  This function prints TypeDescriptors for types
   without names and it will go down through the type chain until it hits
   a type that has a name.  i.e. when it hits a type with a name it stops.
   There are only two places where a type can not have a name - both
   cases are aggregate types.
   1. an aggregate created in an attr declaration
      e.g. names : ARRAY [1:3] of STRING;
   2. an aggregate that is an element of another aggregate.
      e.g. TYPE Label = STRING; END_TYPE;
           TYPE listSetOfLabel = LIST of SET of Label; END_TYPE;
      LIST of SET of Label has a name i.e. listSetOfReal
      SET of Label does not have a name and this function should be called
         to generate one.
      This function will not generate the code to handle Label.

      Type t contains the Type with no Express name that needs to have
        TypeDecriptor[s] generated for it.
      buf needs to have space declared enough to hold the name of the var
        that can be referenced to refer to the type that was created for
    Type t.
*/
void print_typechain(FILE *header, FILE *impl, const Type t, char *buf, Schema schema, const char *type_name)
{
    /* if we've been called, current type has no name */
    /* nor is it a built-in type */
    /* the type_count variable is there for debugging purposes  */

    const char *ctype = TYPEget_ctype(t);
    int count = type_count++;
    char name_buf[MAX_LEN];
    int s;

    switch(TYPEget_body(t)->type) {
        case aggregate_:
        case array_:
        case bag_:
        case set_:
        case list_:
            /* create a new TypeDescriptor variable, e.g. t1, and new space for it */
            fprintf(impl, "        %s * %s%d = new %s;\n",
                    GetTypeDescriptorName(t), TD_PREFIX, count,
                    GetTypeDescriptorName(t));

            fprintf(impl,
                    "        %s%d->AssignAggrCreator((AggregateCreator) create_%s);%s",
                    TD_PREFIX, count, ctype, "        // Creator function\n");

            s = sprintf(name_buf, "%s%d", TD_PREFIX, count);
            assert((s > 0) && (s < MAX_LEN));
            AGGRprint_init(header, impl, t, name_buf, type_name);

            break;

        default: /* this should not happen since only aggregates are allowed to
          not have a name. This funct should only be called for aggrs
          without names. */
            fprintf(impl, "        TypeDescriptor * %s%d = new TypeDescriptor;\n",
                    TD_PREFIX, count);
    }

    /* there is no name so name doesn't need to be initialized */

    fprintf(impl, "        %s%d->FundamentalType(%s);\n", TD_PREFIX, count,
            FundamentalType(t, 1));
    fprintf(impl, "        %s%d->Description(\"%s\");\n", TD_PREFIX, count,
            TypeDescription(t));

    /* DAS ORIG SCHEMA FIX */
    fprintf(impl, "        %s%d->OriginatingSchema(%s::schema);\n", TD_PREFIX, count, SCHEMAget_name(schema));

    if(TYPEget_RefTypeVarNm(t, name_buf, schema)) {
        fprintf(impl, "        %s%d->ReferentType(%s);\n", TD_PREFIX, count, name_buf);
    } else {
        Type base = 0;
        /* no name, recurse */
        char callee_buffer[MAX_LEN];
        if(TYPEget_body(t)) {
            base = TYPEget_body(t)->base;
        }
        print_typechain(header, impl, base, callee_buffer, schema, type_name);
        fprintf(impl, "        %s%d->ReferentType(%s);\n", TD_PREFIX, count, callee_buffer);
    }
    sprintf(buf, "%s%d", TD_PREFIX, count);

    /* Types */
    fprintf(impl, "        %s::schema->AddUnnamedType(%s%d);\n", SCHEMAget_name(schema), TD_PREFIX, count);
}

/** return 1 if it is a multidimensional aggregate at the level passed in
   otherwise return 0;  If it refers to a type that is a multidimensional
   aggregate 0 is still returned. */
int isMultiDimAggregateType(const Type t)
{
    if(TYPEget_body(t)->base)
        if(isAggregateType(TYPEget_body(t)->base)) {
            return 1;
        }
    return 0;
}

void Type_Description(const Type t, char *buf)
{
    if(TYPEget_name(t)) {
        strcat(buf, " ");
        strcat(buf, TYPEget_name(t));
    } else {
        TypeBody_Description(TYPEget_body(t), buf);
    }
}

void TypeBody_Description(TypeBody body, char *buf)
{
    char *s;

    switch(body->type) {
        case integer_:
            strcat(buf, " INTEGER");
            break;
        case real_:
            strcat(buf, " REAL");
            break;
        case string_:
            strcat(buf, " STRING");
            break;
        case binary_:
            strcat(buf, " BINARY");
            break;
        case boolean_:
            strcat(buf, " BOOLEAN");
            break;
        case logical_:
            strcat(buf, " LOGICAL");
            break;
        case number_:
            strcat(buf, " NUMBER");
            break;
        case entity_:
            strcat(buf, " ");
            strcat(buf, PrettyTmpName(TYPEget_name(body->entity)));
            break;
        case aggregate_:
        case array_:
        case bag_:
        case set_:
        case list_:
            switch(body->type) {
                /* ignore the aggregate bounds for now */
                case aggregate_:
                    strcat(buf, " AGGREGATE OF");
                    break;
                case array_:
                    strcat(buf, " ARRAY");
                    strcat_bounds(body, buf);
                    strcat(buf, " OF");
                    if(body->flags.optional) {
                        strcat(buf, " OPTIONAL");
                    }
                    if(body->flags.unique) {
                        strcat(buf, " UNIQUE");
                    }
                    break;
                case bag_:
                    strcat(buf, " BAG");
                    strcat_bounds(body, buf);
                    strcat(buf, " OF");
                    break;
                case set_:
                    strcat(buf, " SET");
                    strcat_bounds(body, buf);
                    strcat(buf, " OF");
                    break;
                case list_:
                    strcat(buf, " LIST");
                    strcat_bounds(body, buf);
                    strcat(buf, " OF");
                    if(body->flags.unique) {
                        strcat(buf, " UNIQUE");
                    }
                    break;
                default:
                    fprintf(stderr, "Error: reached default case at %s:%d", __FILE__, __LINE__);
                    abort();
            }

            Type_Description(body->base, buf);
            break;
        case enumeration_:
            strcat(buf, " ENUMERATION of (");
            LISTdo(body->list, e, Expression)
            strcat(buf, ENUMget_name(e));
            strcat(buf, ", ");
            LISTod
            /* find last comma and replace with ')' */
            s = strrchr(buf, ',');
            if(s) {
                strcpy(s, ")");
            }
            break;

        case select_:
            strcat(buf, " SELECT (");
            LISTdo(body->list, t, Type)
            strcat(buf, PrettyTmpName(TYPEget_name(t)));
            strcat(buf, ", ");
            LISTod
            /* find last comma and replace with ')' */
            s = strrchr(buf, ',');
            if(s) {
                strcpy(s, ")");
            }
            break;
        default:
            strcat(buf, " UNKNOWN");
    }

    if(body->precision) {
        strcat(buf, " (");
        strcat_expr(body->precision, buf);
        strcat(buf, ")");
    }
    if(body->flags.fixed) {
        strcat(buf, " FIXED");
    }
}

const char *IdlEntityTypeName(Type t)
{
    static char name [BUFSIZ+1];
    strncpy(name, TYPE_PREFIX, BUFSIZ);
    if(TYPEget_name(t)) {
        strncpy(name, FirstToUpper(TYPEget_name(t)), BUFSIZ);
    } else {
        return TYPEget_ctype(t);
    }
    return name;
}

const char *GetAggrElemType(const Type type)
{
    Class_Of_Type class;
    Type bt;
    static char retval [BUFSIZ+1];

    if(isAggregateType(type)) {
        bt = TYPEget_nonaggregate_base_type(type);
        if(isAggregateType(bt)) {
            strcpy(retval, "ERROR_aggr_of_aggr");
        }

        class = TYPEget_type(bt);

        /*      case TYPE_INTEGER:  */
        if(class == integer_) {
            strcpy(retval, "long");
        }

        /*      case TYPE_REAL:
            case TYPE_NUMBER:   */
        if((class == number_) || (class == real_)) {
            strcpy(retval, "double");
        }

        /*      case TYPE_ENTITY:   */
        if(class == entity_) {
            strncpy(retval, IdlEntityTypeName(bt), BUFSIZ);
        }

        /*      case TYPE_ENUM:     */
        /*  case TYPE_SELECT:   */
        if((class == enumeration_)
                || (class == select_))  {
            strncpy(retval, TYPEget_ctype(bt), BUFSIZ);
        }

        /*  case TYPE_LOGICAL:  */
        if(class == logical_) {
            strcpy(retval, "Logical");
        }

        /*  case TYPE_BOOLEAN:  */
        if(class == boolean_) {
            strcpy(retval, "Bool");
        }

        /*  case TYPE_STRING:   */
        if(class == string_) {
            strcpy(retval, "string");
        }

        /*  case TYPE_BINARY:   */
        if(class == binary_) {
            strcpy(retval, "binary");
        }
    }
    return retval;
}

const char *TYPEget_idl_type(const Type t)
{
    Class_Of_Type class;
    static char retval [BUFSIZ+1];

    /*  aggregates are based on their base type
    case TYPE_ARRAY:
    case TYPE_BAG:
    case TYPE_LIST:
    case TYPE_SET:
    */

    if(isAggregateType(t)) {
        strcpy(retval, GetAggrElemType(t));

        /*  case TYPE_ARRAY:    */
        if(TYPEget_type(t) == array_) {
            strcat(retval, "__array");
        }
        /*  case TYPE_LIST: */
        if(TYPEget_type(t) == list_) {
            strcat(retval, "__list");
        }
        /*  case TYPE_SET:  */
        if(TYPEget_type(t) == set_) {
            strcat(retval, "__set");
        }
        /*  case TYPE_BAG:  */
        if(TYPEget_type(t) == bag_) {
            strcat(retval, "__bag");
        }
        return retval;
    }

    /*  the rest is for things that are not aggregates  */

    class = TYPEget_type(t);

    /*    case TYPE_LOGICAL:    */
    if(class == logical_) {
        return ("Logical");
    }

    /*    case TYPE_BOOLEAN:    */
    if(class == boolean_) {
        return ("Boolean");
    }

    /*      case TYPE_INTEGER:  */
    if(class == integer_) {
        return ("SDAI_Integer");
    }

    /*      case TYPE_REAL:
        case TYPE_NUMBER:   */
    if((class == number_) || (class == real_)) {
        return ("SDAI_Real");
    }

    /*      case TYPE_STRING:   */
    if(class == string_) {
        return ("char *");
    }

    /*      case TYPE_BINARY:   */
    if(class == binary_) {
        return (AccessType(t));
    }

    /*      case TYPE_ENTITY:   */
    if(class == entity_) {
        /* better do this because the return type might go away */
        strcpy(retval, IdlEntityTypeName(t));
        strcat(retval, "_ptr");
        return retval;
    }
    /*    case TYPE_ENUM:   */
    /*    case TYPE_SELECT: */
    if(class == enumeration_) {
        strncpy(retval, EnumName(TYPEget_name(t)), BUFSIZ - 2);

        strcat(retval, " /*");
        strcat(retval, IdlEntityTypeName(t));
        strcat(retval, "*/ ");
        return retval;
    }
    if(class == select_)  {
        return (IdlEntityTypeName(t));
    }

    /*  default returns undefined   */
    return ("SCLundefined");
}

/**************************************************************//**
 ** Procedure:  TYPEget_express_type (const Type t)
 ** Parameters:  const Type t --  type for attribute
 ** Returns:  a string which is the type as it would appear in Express
 ** Description:  supplies the type for error messages
                  and to register the entity
          - calls itself recursively to create a description of
          aggregate types
 ** Side Effects:
 ** Status:  new 1/24/91
 ******************************************************************/
char *TYPEget_express_type(const Type t)
{
    Class_Of_Type class;
    Type bt;
    char retval [BUFSIZ+1] = {'\0'};
    char *n=NULL, *permval=NULL, *aggr_type=NULL;


    /*  1.  "DEFINED" types */
    /*    case TYPE_ENUM:   */
    /*    case TYPE_ENTITY: */
    /*  case TYPE_SELECT:       */

    n = TYPEget_name(t);
    if(n) {
        PrettyTmpName(n);
    }

    /*   2.   "BASE" types  */
    class = TYPEget_type(t);

    /*    case TYPE_LOGICAL:    */
    if((class == boolean_) || (class == logical_)) {
        return ("Logical");
    }

    /*      case TYPE_INTEGER:  */
    if(class == integer_) {
        return ("Integer ");
    }

    /*      case TYPE_REAL:
        case TYPE_NUMBER:   */
    if((class == number_) || (class == real_)) {
        return ("Real ");
    }

    /*      case TYPE_STRING:   */
    if(class == string_) {
        return ("String ")      ;
    }

    /*      case TYPE_BINARY:   */
    if(class == binary_) {
        return ("Binary ")      ;
    }

    /*  AGGREGATES
    case TYPE_ARRAY:
    case TYPE_BAG:
    case TYPE_LIST:
    case TYPE_SET:
    */
    if(isAggregateType(t)) {
        bt = TYPEget_nonaggregate_base_type(t);
        class = TYPEget_type(bt);

        /*  case TYPE_ARRAY:    */
        if(TYPEget_type(t) == array_) {
            aggr_type = "Array";
        }
        /*  case TYPE_LIST: */
        if(TYPEget_type(t) == list_) {
            aggr_type = "List";
        }
        /*  case TYPE_SET:  */
        if(TYPEget_type(t) == set_) {
            aggr_type = "Set";
        }
        /*  case TYPE_BAG:  */
        if(TYPEget_type(t) == bag_) {
            aggr_type = "Bag";
        }

        sprintf(retval, "%s of %s",
                aggr_type, TYPEget_express_type(bt));

        /*  this will declare extra memory when aggregate is > 1D  */

        permval = (char *)sc_malloc(strlen(retval) * sizeof(char) + 1);
        strcpy(permval, retval);
        return permval;

    }

    /*  default returns undefined   */

    fprintf(stderr, "Warning in %s: type %s is undefined\n", __func__, TYPEget_name(t));
    return ("SCLundefined");

}

/** Initialize an upper or lower bound for an aggregate. \sa AGGRprint_init */
void AGGRprint_bound(FILE *header, FILE *impl, const char *var_name, const char *aggr_name, const char *cname, Expression bound, int boundNr)
{
    if(bound->symbol.resolved) {
        if(bound->type == Type_Funcall) {
	    char *bound_str = EXPRto_string(bound);
            fprintf(impl, "        %s->SetBound%dFromExpressFuncall( \"%s\" );\n", var_name, boundNr, bound_str);
	    sc_free(bound_str);
        } else {
            fprintf(impl, "        %s->SetBound%d( %d );\n", var_name, boundNr, bound->u.integer);
        }
    } else { /* resolved == 0 seems to mean that this is Type_Runtime */
        assert(cname && (bound->e.op2) && (bound->e.op2->symbol.name));
        fprintf(impl, "        %s->SetBound%dFromMemberAccessor( &getBound%d_%s__%s );\n", var_name, boundNr, boundNr, cname, aggr_name);
        fprintf(header, "inline SDAI_Integer getBound%d_%s__%s( SDAI_Application_instance* this_ptr ) {\n", boundNr, cname, aggr_name);
        fprintf(header, "    %s * ths = (%s *) this_ptr;\n", cname, cname);
        fprintf(header, "    ths->ResetAttributes();\n");
        fprintf(header, "    STEPattribute * a = ths->NextAttribute();\n");
        fprintf(header, "    while( strcmp( a->Name(), \"%s\" ) != 0 ) {\n", bound->e.op2->symbol.name);
        fprintf(header, "        a = ths->NextAttribute();\n");
        fprintf(header, "        if( !a ) {\n");
        fprintf(header, "            break;\n");
        fprintf(header, "        }\n");
        fprintf(header, "    }\n");
        fprintf(header, "    assert( a->NonRefType() == INTEGER_TYPE && \"Error in schema or in exp2cxx at %s:%d %s\" );\n", path2str(__FILE__),
                __LINE__, "(incorrect assumption of integer type?) Please report error to STEPcode: scl-dev at groups.google.com.");
        fprintf(header, "    return *( a->Integer() );\n");   /* always an integer? if not, would need to translate somehow due to return type... */
        fprintf(header, "}\n");
    }

}

/**
 * For aggregates, initialize Bound1, Bound2, OptionalElements, UniqueElements.
 * Handles bounds that depend on attributes (via SetBound1FromMemberAccessor() ), or
 * on function calls (via SetBound1FromExpressFuncall() ). In C++, runtime bounds
 * look like `t_0->SetBound2FromMemberAccessor(SdaiStructured_mesh::index_count_);`
 * \param header the header to write attr callback members to
 * \param impl the file to write to
 * \param t the Type
 * \param var_name the name of the C++ variable, such as t_1 or schema::t_name
 */
void AGGRprint_init(FILE *header, FILE *impl, const Type t, const char *var_name, const char *aggr_name)
{
    if(!header) {
        fprintf(stderr, "ERROR at %s:%d! 'header' is null for aggregate %s.", __FILE__, __LINE__, t->symbol.name);
        abort();
    }
    if(!TYPEget_head(t)) {
        const char *cname = 0;
        if((t->superscope) && (t->superscope->symbol.name)) {
            cname = ClassName(t->superscope->symbol.name);
        }

        if(TYPEget_body(t)->lower) {
            AGGRprint_bound(header, impl, var_name, aggr_name, cname, TYPEget_body(t)->lower, 1);
        }
        if(TYPEget_body(t)->upper) {
            AGGRprint_bound(header, impl, var_name, aggr_name, cname, TYPEget_body(t)->upper, 2);
        }
        if(TYPEget_body(t)->flags.unique) {
            fprintf(impl, "        %s->UniqueElements(LTrue);\n", var_name);
        }
        if(TYPEget_body(t)->flags.optional) {
            fprintf(impl, "        %s->OptionalElements(LTrue);\n", var_name);
        }
    }
}
