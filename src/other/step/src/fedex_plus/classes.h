/*
** Fed-x parser output module for generating C++  class definitions
** December  5, 1989
** release 2 17-Feb-1992
** release 3 March 1993
** release 4 December 1993
** K. C. Morris
**
** Development of Fed-x was funded by the United States Government,
** and is not subject to copyright.

*******************************************************************
The conventions used in this binding follow the proposed specification
for the STEP Standard Data Access Interface as defined in document
N350 ( August 31, 1993 ) of ISO 10303 TC184/SC4/WG7.
*******************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>


#include "express.h"
#include "exppp.h"
#include "dict.h"

#ifdef __CENTERLINE__
#define CONST
#else
#define CONST const
#endif

#define MAX_LEN		240
#define DEBUG		if (0) printf 

/* Values for multiple schema support: */
#define NOTKNOWN    1
#define UNPROCESSED 2
#define CANTPROCESS 3
#define CANPROCESS  4
#define PROCESSED   5

#define TD_PREFIX	"t_"
#define ATTR_PREFIX	"a_"
#define ENT_PREFIX	"e_"
#define SCHEMA_PREFIX	"s_"

#define TYPEprefix(t)	     (TYPEis_entity (t) ? ENT_PREFIX : TD_PREFIX)

#define SCHEMA_FILE_PREFIX	"Sdai"
#define TYPE_PREFIX   "Sdai"
#define ENTITYCLASS_PREFIX	TYPE_PREFIX
/*#define ENUM_PREFIX	"SDAI_"*/
#define ENUM_PREFIX	""

#define move(b)		(b = (b + strlen(b)))
#define TYPEtd_name(t)	TypeDescriptorName (t)

typedef  struct file_holder  {
    FILE*  inc;		/*  include file  */
    FILE*  lib;		/*  library file  */
    FILE*  incall;	/*  include file for collecting all include files  */
    FILE*  initall;	/*  for registering all entities from all schemas  */
    FILE*  make;	/*  for indicating schema source files in makefile  */
    FILE*  code;	
    FILE*  init;	/*  contains function to initialize program
			    to use schema's entities */
    FILE*  create;      /*  DAR - added - to create all schema & ent descrip-
			    tors.  In multiple interrelated schemas, must be
			    done before attribute descriptors and sub-super
			    links created. */
    FILE*  classes;     /*  DAR - added - a new .h file to contain declara-
			    tions of all the classes, so that all the .h files
			    can refer any of the entity classes.  Nec. if ent1
			    of schemaA has attribute ent2 from schemaB. */
}  File_holder, FILES;

typedef struct EntityTag_ *EntityTag;
struct EntityTag_ {
  /*  these fields are used so that ENTITY types are processed in order
   *  when appearing in differnt schemas   */
  unsigned int started :1; /*  marks the beginning of processing  */
  unsigned int complete :1;  /*  marks the end of processing  */

  Entity superclass;  /*  the entity being used as the supertype
			*  - with multiple inheritance only chose one */
};

Entity ENTITYget_superclass (Entity entity);
Entity ENTITYput_superclass (Entity entity);
int ENTITYhas_explicit_attributes (Entity e);
Linked_List ENTITYget_first_attribs(Entity entity, Linked_List result);

typedef struct SelectTag_ *SelectTag;
struct SelectTag_ {
  /*  these fields are used so that SELECT types are processed in order  */
  unsigned int started :1; /*  marks the beginning of processing  */
  unsigned int complete :1;  /*  marks the end of processing  */
  };

char * format_for_stringout(char *orig_buf, char* return_buf);
#define String  const char *
const char *	CheckWord( const char *);
const char * 	StrToLower(const char *);
const char *	StrToUpper (const char *);
const char *	FirstToUpper (const char *);
const char *	SelectName (const char *); 
FILE  * FILEcreate(const char *);
void	FILEclose(FILE *);
const char *  ClassName(const char *);
const char *  ENTITYget_classname(Entity);
void	ENTITYPrint(Entity,FILES *,Schema);
const char *	StrToConstant(const char *);
void    TYPEselect_print(Type, FILES *, Schema);
void    ENTITYprint_new(Entity, FILES *, Schema, int);
void	TYPEprint_definition(Type, FILES *, Schema);
void    TYPEprint_new(const Type, FILE *, Schema);
void    TYPEprint_typedefs(Type, FILE *);
void    TYPEprint_descriptions (const Type, FILES *, Schema);
void    TYPEprint_init (const Type, FILE *, Schema);
void    TYPEselect_init_print (const Type, FILE *, Schema);
void    MODELPrint(Entity, FILES*,Schema, int);
void    MODELprint_new(Entity,FILES*, Schema);
void    MODELPrintConstructorBody(Entity, FILES *,Schema/*, int*/);
const char * PrettyTmpName (const char * oldname);
const char * EnumName (const char * oldname);
const char * TypeDescriptorName (Type);
char * TypeDescription (const Type t);
const char * TypeName (Type t);
const char * AccessType (Type t);
const char * TYPEget_ctype (const Type t);
void print_file(Express);
void resolution_success(void);
void SCHEMAprint( Schema, FILES *, Express, void *, int );
Type TYPEget_ancestor( Type );

/*Variable*/
/*VARis_simple_explicit (Variable a)*/
#define VARis_simple_explicit(a)  (!VARis_type_shifter(a))

/*Variable*/
/*VARis_simple_derived (Variable a)*/
#define VARis_simple_derived(a)  (!VARis_overrider(a))

/* Added for multiple schema support: */
void print_schemas_separate( Express, void *, FILES * );
void getMCPrint( Express, FILE *, FILE * );
int sameSchema( Scope, Scope );

void
USEREFout(Schema schema, Dictionary refdict,Linked_List reflist,char *type,FILE* file);

