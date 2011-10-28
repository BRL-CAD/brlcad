#define	CLASSES_MISC_C
#include <stdlib.h>
#include "classes.h"
/*******************************************************************
** FedEx parser output module for generating C++  class definitions
** December  5, 1989
** release 2 17-Feb-1992
** release 3 March 1993
** release 4 December 1993
** K. C. Morris
**
** Development of FedEx was funded by the United States Government,
** and is not subject to copyright.

*******************************************************************
The conventions used in this binding follow the proposed specification
for the STEP Standard Data Access Interface as defined in document
N350 ( August 31, 1993 ) of ISO 10303 TC184/SC4/WG7.
*******************************************************************/

extern int multiple_inheritance;
/*extern int corba_binding; */

static inline
Boolean
LISTempty(Linked_List list)
{
    if (!list) return True;
    if ( list->mark->next == list->mark ) {
	return True;
    }
    return False;
}

/******************************************************************
**		The following functions will be used		***
***		through out the the program fedex_plus		***/


/******************************************************************
 ** Procedure:  CheckWord 
 ** Description: if word is a reserved word, it is capitalized
 ** Parameters:  word -- string to be checked
 ** Returns:  altered word if word is reserved;  otherwise the original word
 ** Side Effects:  the word is altered in memory
 ** Status:  started 12/1
 ******************************************************************/
const char *    
CheckWord (const char * word)
{
#ifdef NOT_USING_SDAI_BINDING
/*  obsolete with proposed c++ binding  */

    static char *reserved_words [] = {
"application_marker",  "asm",	"attributes", "auto",	
"break",	"case",	"char",	"class",	"const",	"continue",
"default",	"delete",	"do",	"double",	
"else",	"enum",	"extern",
"float",	"for",	"friend",	"goto",	"handle",	
"if",	"inline",	"instance_id",	"int",	"long",	
"new",	"nullable",	"opcode",  "operator",	"overload",	
"private",	"protected",	"public",	"register",	"return",
"shared", 	"short",	"sizeof",	"static",	"struct",	"switch",
"this",		"template",	"type",	"typedef",	"type_name",
"union",	"unsigned",	
"val", 	"virtual",	"void",	"volatile"
      };
    int nwords = (sizeof reserved_words / sizeof reserved_words[0]);
    int cond,
	i, 
        low = 0,
        high = nwords - 1;    

    /*  word is obviously not in list, if it is longer than any of the words in the list  */
    if (strlen (word) > 12)   return (word);

    while (low <= high) {
	i = (low + high) / 2;
	if ((cond = strcmp (word, reserved_words [i])) < 0) 
	    high = i -1;
	else if (cond > 0)
	    low = i +1;
	else {	/*	word is a reserved word, capitalize it	*/
		printf ("** warning: reserved word  %s  ", word);
		*(word + 0) = toupper (*(word +0));
		printf ("is changed to  %s **\n", word);
	    
	    }
    }
#endif
    return (word);

}

/******************************************************************
 ** Procedure:  string functions
 ** Description:  These functions take a character or a string and return
 **	a temporary copy of the string with the function applied to it.
 ** Parameters:  
 ** Returns:  temporary copy of characters
 ** Side Effects:  character or string returned persists until the 
 **	next invocation of the function
 ** Status:  complete
 ******************************************************************/

char
ToLower (char c)
{
    if (isupper (c)) return (tolower (c));
    else return (c);

}

char
ToUpper  (char c)
{
    if (islower (c)) return (toupper (c));
    else return (c);
}

const char *
StrToLower (const char * word)
{
    static char newword [MAX_LEN];
    int i =0;
    if (!word)  return 0;    
    while (word [i] != '\0') {
	newword [i] = ToLower (word [i]);
	++i;	
    }
    newword [i] = '\0';
    return (newword)    ;
    
}

const char * 
StrToUpper (const char * word)
{
    static char newword [MAX_LEN];
    int i =0;
    char ToUpper (char c);
    
    while (word [i] != '\0') {
	newword [i] = ToUpper (word [i]);
	++i;
	
    }
    newword [i] = '\0';
    return (newword);
}

const char * 
StrToConstant (const char * word)
{
    static char newword [MAX_LEN];
    int i =0;
    
    while (word [i] != '\0') {
	if (word [i] == '/' || word [i] == '.') newword [i] = '_';
	else newword [i] = ToUpper (word [i]);
	++i;
	
    }
    newword [i] = '\0';
    return (newword);
}

/******************************************************************
 ** Procedure:  FILEcreate  
 ** Description:  creates a file for c++ with header definitions
 ** Parameters:  filename
 ** Returns:  FILE*	pointer to file created or NULL
 ** Side Effects:  creates a file with name filename
 ** Status:  complete
 ******************************************************************/

FILE*
FILEcreate (const char * filename)
{
    FILE* file;
    const char * fn;
    
    if ((file = fopen (filename, "w")) == NULL) {
	printf ("**Error in SCHEMAprint:  unable to create file %s ** \n", filename);
	return (NULL);
    }

    fprintf (file, "#ifndef  %s\n", fn = StrToConstant (filename));
    fprintf (file, "#define  %s\n", fn );

	fprintf(file,"// This file was generated by fedex_plus.  You probably don't want to edit\n");
	fprintf(file,"// it since your modifications will be lost if fedex_plus is used to\n");
	fprintf(file,"// regenerate it.\n");
    return (file);
    
}

/******************************************************************
 ** Procedure:  FILEclose 
 ** Description:  closes a file opened with FILEcreate
 ** Parameters:  FILE*  file  --  pointer to file to close
 ** Returns:  
 ** Side Effects:  
 ** Status:  complete
 ******************************************************************/

void
FILEclose (FILE* file)
{
    fprintf (file, "#endif\n");
    fclose (file);
}


/******************************************************************
 ** Procedure:  isAggregate
 ** Parameters:  Attribute a
 ** Returns:  int 	indicates whether the attribute is an aggregate
 ** Description:  	indicates whether the attribute is an aggregate
 ** Side Effects:  none
 ** Status:  complete 1/15/91
 ******************************************************************/
 
int
isAggregate  (Variable a)
{
	return(TYPEinherits_from(VARget_type(a),aggregate_));
}

int 
isAggregateType (const Type t)
{
	return(TYPEinherits_from(t,aggregate_));
}


/******************************************************************
 ** Procedure:  TYPEget_ctype
 ** Parameters:  const Type t --  type for attribute
 ** Returns:  a string which is the type of the data member in the c++ class
 ** Description:  supplies the type of a data member for the c++ class
 ** Side Effects:  
 ** Status:  complete 1/15/90
 ** Changes: Modified by CD to return the appropriate types as outlined in
 **          "SDAI C++ Binding for PDES, Inc. Prototyping" by Stephen Clark
 ** Change Date: 5/22/91 CD
 ** Change Date: 28-Sep-1993 made entities use their real type instead of base
 **              class STEPentityH 
 ******************************************************************/

const char *
TYPEget_ctype (const Type t)
{      
    Class_Of_Type class, class2;
    Type bt;
    static char retval [BUFSIZ];
    char *n;
    

    /*  aggregates are based on their base type  
    case TYPE_ARRAY:
    case TYPE_BAG:
    case TYPE_LIST:
    case TYPE_SET:
    */
    if (isAggregateType (t)) {
	bt = TYPEget_body(t)->base;

        if (isAggregateType(bt)) {
		return("GenericAggregate");
	}

	class = TYPEget_type(bt);
	    
	/*      case TYPE_INTEGER:	*/
	if (class == Class_Integer_Type) return ( "IntAggregate" );
	
	/*      case TYPE_REAL:
		case TYPE_NUMBER:	*/
	if ((class == Class_Number_Type) || ( class == Class_Real_Type))
	    return ( "RealAggregate" );

	/*      case TYPE_ENTITY:	*/
	if (class == Class_Entity_Type) return( "EntityAggregate" );
	
	/*      case TYPE_ENUM:		*/
        /*	case TYPE_SELECT:	*/
	if ((class == Class_Enumeration_Type) 
	    || (class == Class_Select_Type) )  {
/*
	    strcpy (retval, ClassName (TYPEget_name (bt)));
*/
	    strcpy (retval, TYPEget_ctype(bt));
	    strcat (retval, "s");
	    return (retval);
	}

        /*	case TYPE_LOGICAL:	*/
	if (class == Class_Logical_Type)
	    return ("LOGICALS"); 

        /*	case TYPE_BOOLEAN:	*/
	if (class == Class_Boolean_Type)
	    return ("BOOLEANS"); 

        /*	case TYPE_STRING:	*/
	if (class == Class_String_Type) return("StringAggregate");

        /*	case TYPE_BINARY:	*/
	if (class == Class_Binary_Type) return("BinaryAggregate");
    }

    /*  the rest is for things that are not aggregates	*/
    
    class = TYPEget_type(t);

    /*    case TYPE_LOGICAL:	*/
    if ( class == Class_Logical_Type)
        return ("SCLP23(LOGICAL)"); 

    /*    case TYPE_BOOLEAN:	*/
    if (class == Class_Boolean_Type)
        return ("SCLP23(BOOLEAN)"); 

    /*      case TYPE_INTEGER:	*/
    if ( class == Class_Integer_Type)
	return ("SCLP23(Integer)");

    /*      case TYPE_REAL:
	    case TYPE_NUMBER:	*/
    if ((class == Class_Number_Type) || ( class == Class_Real_Type))
	return ("SCLP23(Real)")      ; 

    /*	    case TYPE_STRING:	*/
    if (class == Class_String_Type)
	return ("SCLP23(String)");

    /*	    case TYPE_BINARY:	*/
    if ( class == Class_Binary_Type)
	return ("SCLP23(Binary)");

    /*      case TYPE_ENTITY:	*/
    if ( class == Class_Entity_Type)
      {
      strncpy (retval, TypeName (t), BUFSIZ-2);
      strcat (retval, "_ptr");
      return retval;
/*	return ("STEPentityH");    */
    }
    /*    case TYPE_ENUM:	*/
    /*    case TYPE_SELECT:	*/
    if (class == Class_Enumeration_Type)
    {
      strncpy (retval, TypeName (t), BUFSIZ-2);
      strcat (retval, "_var");
      return retval;
    }
    if (class == Class_Select_Type)  {
      return (TypeName (t));
    }

    /*  default returns undefined   */
    return ("SCLundefined");
}    

/******************************************************************
 ** Procedure:  TypeName
 ** Parameters:  Type t
 ** Returns:  name of type as defined in SDAI C++ binding  4-Nov-1993
 ** Status:   4-Nov-1993
 ******************************************************************/
const char *
TypeName (Type t)  
{
  static char name [BUFSIZ];
  strcpy (name, TYPE_PREFIX);
  if (TYPEget_name (t)) 
    strncat (name, FirstToUpper (TYPEget_name (t)), BUFSIZ -strlen(TYPE_PREFIX) -1);
  else return TYPEget_ctype (t);
  return name; 
}

/******************************************************************
 ** Procedure:  AccessType
 ** Parameters:  const Type t --  type for attribute
 ** Returns:  a string which is the type used by the access functions to
              the data member in the c++ class
 ** Description:  supplies the type for access functions in a c++ class
 ** Side Effects:  
 ** Status:    3-Nov-1993 -kcm
******************************************************************/

const char *
AccessType (Type t)
{
    Class_Of_Type class;
    static char nm [BUFSIZ];
    strncpy (nm, TypeName (t), BUFSIZ -4);
    if (TYPEis_entity(t) )
    {
/*	if(corba_binding)
	{
	    if (TYPEget_name (t)) 
	      strncpy (nm, FirstToUpper (TYPEget_name (t)), BUFSIZ-1);
	}
*/
	strcat (nm, "_ptr");
	return nm;
    }
    else if ( TYPEis_select (t) || TYPEis_aggregate(t) ) 
    {
	strcat (nm, "_ptr");
	return nm;
    }
    class = TYPEget_type(t);
    if (class == Class_Enumeration_Type)
    {
      strncpy (nm, TypeName (t), BUFSIZ-2);
      strcat (nm, "_var");
      return nm;
    }
    if ( class == Class_Logical_Type)
        strncpy (nm, "Logical", BUFSIZ -2);

    /*    case TYPE_BOOLEAN:	*/
    if (class == Class_Boolean_Type)
        strncpy (nm, "Boolean", BUFSIZ -2);
    return nm;
}

/******************************************************************
 ** Procedure:  ClassName
 ** Parameters:  const char * oldname
 ** Returns:  temporary copy of name suitable for use as a class name
 ** Side Effects:  erases the name created by a previous call to this function
 ** Status:  complete
 ******************************************************************/

const char *
ClassName (const char * oldname)
{
    int i= 0, j = 0;
    static char newname [BUFSIZ];
    if (!oldname)  return ("");
    
    
    strcpy (newname, ENTITYCLASS_PREFIX)    ;
    j = strlen (ENTITYCLASS_PREFIX)    ;
    newname [j] = ToUpper (oldname [i]);
    ++i; ++j;
    while ( oldname [i] != '\0') {
	newname [j] = ToLower (oldname [i]);
/*	if (oldname [i] == '_')  */
/*  character is '_'	*/
/*	    newname [++j] = ToUpper (oldname [++i]);*/
	++i;
	++j;
    }
    newname [j] = '\0';
    return (newname);
    
/******  This procedure gets rid of '_' and is no longer being used
	if (oldname [i] != '_') newname [j] = ToLower (oldname [i]);
	else {	*//*  character is '_'	*//*
	    newname [j] = ToUpper (oldname [++i]);
	    if (oldname [i] == '\0') --i;
	}
	++i;
	++j;
*******/
}

const char *
ENTITYget_CORBAname (Entity ent)
{
    static char newname [BUFSIZ];
    strcpy( newname, ENTITYget_name(ent) );
    newname[0] = ToUpper (newname [0]);
    return newname;
}

/******************************************************************
 ** Procedure:  ENTITYget_classname	
 ** Parameters:  Entity ent
 ** Returns:  the name of the c++ class representing the entity 
 ** Status:  complete
 ******************************************************************/

const char *
ENTITYget_classname (Entity ent)
{
    const char * oldname = ENTITYget_name (ent);
    return (ClassName (oldname));
}

/******************************************************************
 ** Procedure:  PrettyTmpName (char * oldname)
 ** Procedure:  PrettyNewName (char * oldname)
 ** Parameters:  oldname
 ** Returns:  a new capitalized name 
 ** Description:  creates a new name with first character's in caps
 ** Side Effects:  PrettyNewName allocates memory for the new name
 ** Status:   OK  7-Oct-1992 kcm 
 ******************************************************************/
const char *
PrettyTmpName (const char * oldname)
{
    int i= 0;
    static char newname [BUFSIZ];
    newname [0] = '\0';
    while (( oldname [i] != '\0') && (i < BUFSIZ)) {
        newname [i] = ToLower (oldname [i]);
        if (oldname [i] == '_')  /*  character is '_'   */  
	  {
	      ++i;
	      newname [i] = ToUpper (oldname [i]);  
	  }
        if (oldname [i] != '\0') ++i;
    }

    newname [0] = ToUpper (oldname [0]);
    newname [i] = '\0';
    return newname;
}

/* This function is out of date DAS */
const char *
EnumName (const char * oldname)  
{
    int i= 0, j = 0;
    static char newname [MAX_LEN];
    if (!oldname)  return ("");
    
    strcpy (newname, ENUM_PREFIX)    ;
    j = strlen (ENUM_PREFIX)    ;
    newname [j] = ToUpper (oldname [0]);
    strncpy (newname+j+1, StrToLower (oldname +1), MAX_LEN -j);
    j = strlen (newname);
    newname [j] = '\0';
    return (newname);
}

const char *
SelectName (const char * oldname)  
{
    int i= 0, j = 0;
    static char newname [MAX_LEN];
    if (!oldname)  return ("");
    
    strcpy (newname, TYPE_PREFIX);
    newname [0] = ToUpper( newname [0] );
    j = strlen (TYPE_PREFIX);
    newname [j] = ToUpper (oldname [0]);
    strncpy (newname+j+1, StrToLower (oldname +1), MAX_LEN -j);
    j = strlen (newname);
    newname [j] = '\0';
    return (newname);
}

const char *
FirstToUpper (const char * word)
{
    static char newword [MAX_LEN];
    int i =0;
    
    strncpy( newword, word, MAX_LEN );
    newword[0] = ToUpper( newword[0] );
    return (newword);
}

/* return fundamental type but as the string which corresponds to */
/* the appropriate type descriptor */
/* if report_reftypes is true, report REFERENCE_TYPE when appropriate */
char *
FundamentalType(const Type t,int report_reftypes) {
	if (report_reftypes && TYPEget_head(t)) return("REFERENCE_TYPE");
	switch (TYPEget_body(t)->type) {
	case integer_:		return("sdaiINTEGER");
	case real_:		return("sdaiREAL");
	case string_:		return("sdaiSTRING");
	case binary_:		return("sdaiBINARY");
	case boolean_:		return("sdaiBOOLEAN");
	case logical_:		return("sdaiLOGICAL");
	case number_:		return("sdaiNUMBER");
	case generic_:		return("GENERIC_TYPE");
	case aggregate_:	return("AGGREGATE_TYPE");
	case array_:		return("ARRAY_TYPE");
	case bag_:		return("BAG_TYPE");
	case set_:		return("SET_TYPE");
	case list_:		return("LIST_TYPE");
	case entity_:		return("sdaiINSTANCE");
	case enumeration_:	return("sdaiENUMERATION");
	case select_:           return ("sdaiSELECT");
	default:		return("UNKNOWN_TYPE");
	}
}

/* this actually gets you the name of the variable that will be generated to 
   be a TypeDescriptor or subtype of TypeDescriptor to represent Type t in 
   the dictionary. */

const char *
TypeDescriptorName (Type t )
{
  static char b [BUFSIZ];
  Schema parent = t->superscope;
  /* NOTE - I corrected a prev bug here in which the *current* schema was
  ** passed to this function.  Now we take "parent" - the schema in which 
  ** Type t was defined - which was actually used to create t's name. DAR */

  if ( !parent ) {
      parent = TYPEget_body(t)->entity->superscope;
      /* This works in certain cases that don't work otherwise (basically a
      ** kludge).  For some reason types which are really entity choices of
      ** a select have no superscope value, but their super may be tracked
      ** by following through the entity they reference, as above. */
  }

      sprintf ( b, "%s%s%s", SCHEMAget_name(parent), TYPEprefix (t),
	       TYPEget_name(t) );
  return b;
}

/* this gets you the name of the type of TypeDescriptor (or subtype) that a
   variable generated to represent Type t would be an instance of. */

const char *
GetTypeDescriptorName( Type t )
{
    switch (TYPEget_body(t)->type)
    {
      case aggregate_:
	return "AggrTypeDescriptor";

      case list_:
	return "ListTypeDescriptor";

      case set_:
	return "SetTypeDescriptor";

      case bag_:
	return "BagTypeDescriptor";

      case array_:
	return "ArrayTypeDescriptor";

      case select_:
	return "SelectTypeDescriptor";

      case boolean_:
      case logical_:
      case enumeration_:
	return "EnumTypeDescriptor";

      case entity_:
	return "EntityDescriptor";

      case integer_:
      case real_:
      case string_:
      case binary_:
      case number_:
      case generic_:
	return "TypeDescriptor";
    }
}

int
ENTITYhas_explicit_attributes (Entity e)
{
  Linked_List l =ENTITYget_attributes (e);
  int cnt =0;
  LISTdo (l, a, Variable)
    if (VARget_initializer (a) == EXPRESSION_NULL) 
      ++cnt;
  LISTod;
  return cnt;

}

Entity
ENTITYput_superclass (Entity entity)
{
#define ENTITYget_type(e)  ((e)->u.entity->type)

  Linked_List l =ENTITYget_supertypes (entity);
  Entity super =0;
  Entity ignore =0;
  int super_cnt =0;
  EntityTag tag;

  Linked_List list = 0;

  if (! LISTempty (l)) {

    if(multiple_inheritance)
    {
    list = ENTITYget_supertypes (entity);
    if (! LISTempty (list)) {
      /* assign superclass to be the first one on the list of parents */
      super = (Entity)LISTpeek_first(list);
    }
    }
    else
    { /* find the first parent that has attributes (in the parent or any of its
	 ancestors).  Make super point at that parent and print warnings for
	 all the rest of the parents. DAS */
    LISTdo (l, e, Entity)
      /*  if there's no super class yet, 
	  or if the entity super class [the variable] super is pointing at 
	  doesn't have any attributes: make super point at the current parent.
	  As soon as the parent pointed to by super has attributes, stop 
	  assigning super and print ignore messages for the remaining parents.
	*/
      if ( (! super) || (! ENTITYhas_explicit_attributes (super) )) {
	ignore = super;
	super = e;
	++ super_cnt;
      }  else {
	ignore = e;
      }
      if (ignore) {
	printf ("WARNING:  multiple inheritance not implemented.\n");
	printf ("\tin ENTITY %s\n\tSUPERTYPE %s IGNORED.\n\n", 
		ENTITYget_name (entity), ENTITYget_name (e));
      }
    LISTod;
    }

    tag = (EntityTag ) malloc (sizeof (struct EntityTag_));
    tag -> superclass = super;
    TYPEput_clientData (ENTITYget_type (entity), tag);
    return super;
  } return 0;
}

Entity
ENTITYget_superclass (Entity entity)
{
  EntityTag tag;
  tag = TYPEget_clientData (ENTITYget_type (entity));
  return (tag ? tag -> superclass : 0);
}

Linked_List
ENTITYget_first_attribs(Entity entity, Linked_List result)
{
    Linked_List supers;

    LISTdo( ENTITYget_attributes(entity), attr, Generic )
      LISTadd_last( result, attr );
    LISTod;
    supers = ENTITYget_supertypes( entity );
    if ( supers ) {
	ENTITYget_first_attribs( (Entity)LISTget_first( supers ), result);
    }
}

/* Attributes are divided into four categories: 
** these are not exclusive as far as I can tell! I added defs below DAS
**
**      . simple explicit
**	. type shifters    // not DERIVEd - redefines type in ancestor
			   // VARget_initializer(v) returns null

**      . simple derived   // DERIVEd - is calculated - VARget_initializer(v)
			   // returns non-zero, VARis_derived(v) is non-zero

**	. overriding	   // includes type shifters and derived
**
** All of them are added to the dictionary.
** Only type shifters generate a new STEPattribute.
** Type shifters generate access functions and data members, for now. 
** Overriding generate access functions and data members, for now. ???? DAS

** //  type shifting attributes
** //  ------------------------
** // before printing new STEPattribute
** // check to see if it\'s already printed in supertype
** // still add new access function
**
** //  overriding attributes
** //  ---------------------
** // go through derived attributes
** // if STEPattribute found with same name
** // tell it to be * for reading and writing
**/

Variable
VARis_type_shifter (Variable a)
{
  char *temp;

  if (VARis_derived(a) || VARget_inverse (a))  return 0;

  temp = EXPRto_string( VARget_name(a) );
  if (! strncmp (StrToLower (temp), "self\\", 5)) {
    /*    a is a type shifter */
    free (temp);
    return a;
  }  
  free (temp);
  return 0;
}

Variable
VARis_overrider (Entity e, Variable a)
{  

  Variable other;
  char * tmp;

  tmp = VARget_simple_name(a);

  LISTdo (ENTITYget_supertypes (e), s, Entity) 
  if ((other = ENTITYget_named_attribute (s, tmp)) 
      && other != a)
    return other;
  LISTod;
  return 0;
}

Type
TYPEget_ancestor( Type t )
    /*
     * For a renamed type, returns the original (ancestor) type from which t
     * descends.  Return NULL if t is top level.
     */
{
    Type i = t;

    if ( !TYPEget_head(i) ) {
        return NULL;
    }

    while ( TYPEget_head(i) ) {
	i = TYPEget_head(i);
    }

    return i;
}
