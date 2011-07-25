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

/**************************************************************************
********    The functions in this file generate C++ code for representing
********    EXPRESS SELECT types.
**************************************************************************/
#include <stdlib.h>
#include "classes.h"

#define BASE_SELECT "SCLP23(Select)"

#define TYPEis_primitive(t) ( !( TYPEis_entity(t)  || \
                                 TYPEis_select (t) || \
                                 TYPEis_aggregate(t) ) )

#define TYPEis_numeric(t)   ( ((t)->u.type->body->type == real_)    || \
			      ((t)->u.type->body->type == integer_) || \
			      ((t)->u.type->body->type == number_) )
#define PRINT_BUG_REPORT  \
     fprintf( f, "   std::cerr << __FILE__ << \":\" << __LINE__ <<  \":  ERROR" \
              " in schema library:  \\n\" \n\t<< _POC_ << \"\\n\\n\";\n");

#define PRINT_SELECTBUG_WARNING(f) \
     fprintf( (f), "\n   severity( SEVERITY_WARNING );\n" ); \
     fprintf( (f), "   std::cerr << __FILE__ << \":\" << __LINE__ <<  \":  "); \
     fprintf( (f), "WARNING:  possible misuse of\"\n        << \" SELECT "); \
     fprintf( (f), "TYPE from schema library.\\n\";\n"); \
     fprintf( (f), "   Error( \"Mismatch in underlying type.\" );\n" );

#define print_error_msg() \
      fprintf( f, "\n  severity( SEVERITY_BUG );\n" ); \
      PRINT_BUG_REPORT  \
      fprintf( f, "  Error( \"This 'argument' is of the incorrect type\" );\n" );

#define TRUE    1
#define FALSE  	0

static void initSelItems( const Type, FILE * );

const char *
SEL_ITEMget_enumtype (Type t)
{
  return StrToUpper(TYPEget_name (t));
}


/******************************************************************
 ** Procedure:  TYPEget_utype
 ** Parameters:  Type t
 ** Returns:  type used to represent the underlying type in a select class
 ** Description:  
 ** Side Effects:  
 ** Status:  
 ******************************************************************/

const char *
TYPEget_utype (Type t)  {
/*
  static char b [BUFSIZ];
  strncpy (b, TYPEget_ctype (t), BUFSIZ-2);
  if (TYPEis_select (t)) strcat (b, "H");
  */
  return (TYPEis_entity (t) ?  "SCLP23(Application_instance_ptr)" : TYPEget_ctype (t));
}

/*******************
LISTmember

determines if the given entity is a member of the list.  
RETURNS the member if it is a member; otherwise 0 is returned.
*******************/
Generic 
LISTmember (const Linked_List list, Generic e)
{
  Link node;
  for (node = list->mark->next; node != list->mark; node = node->next) 
    if (e == node -> data)  return e;
  return (0);
}

/*******************
 compareOrigTypes

 Specialized function to catch if two enumerations, two selects, or two aggrs
 of either, are of the same type.  The issue is that e.g. select B may be a
 rename of sel A (i.e., TYPE B = A;).  Such renamed types are implemented by
 fedex_plus with typedefs, so that they are in fact the same type.  TYPEget_-
 ctype() is used for most type comparisons and does not consider renamed types
 equivalent.  This function is called in instances when they should be consi-
 dered equivalent.  One such case is the generation of duplicate lists.
 *******************/
static int
compareOrigTypes( Type a, Type b )
{
  Type t, u;

  if (    ( TYPEis_select(a) && TYPEis_select(b) ) 
       || ( TYPEis_enumeration(a) && TYPEis_enumeration(b) ) ) {
      t = a;
      u = b;
  } else if ( TYPEis_aggregate(a) && TYPEis_aggregate(b) ) {
      t = TYPEget_base_type(a);
      u = TYPEget_base_type(b);
      if ( !(    ( TYPEis_select(t) && TYPEis_select(u) ) 
	      || ( TYPEis_enumeration(t) && TYPEis_enumeration(u) ) ) ) {
	  return FALSE;
	  /* Only go further with 1D aggregates of sels or enums.  Note that
	  // for 2D aggrs and higher we do not continue.  These are all recog-
	  // nized to be the same type ("GenericAggregate") by TYPEget_ctype(),
	  // and do not have to be handled specially here. */
      }
  } else {
      return FALSE;
  }

  if ( TYPEget_head(t) ) {
      t = TYPEget_ancestor(t);
  }
  if ( TYPEget_head(u) ) {
      u = TYPEget_ancestor(u);
  }
  return ( !strcmp( TYPEget_name(t), TYPEget_name(u) ) );
}

/*******************
 utype_member

 determines if the given "link's" underlying type is a member of the list.  
        RETURNS the underlying type if it is a member; otherwise 0 is returned.

 If "rename" is TRUE, we also consider check to match in certain cases where
 list already has an item that check is a renaming of (see header comments to
 compareOrigTypes() above).
 *******************/
const char *
utype_member( const Linked_List list, const Type check, int rename )
{
  static char r [BUFSIZ], *p;

  LISTdo( list, t, Type )
    strncpy (r, TYPEget_utype(t), BUFSIZ);
    if( strcmp(r, TYPEget_utype(check)) == 0 )
      return r;
    if( rename && compareOrigTypes( check, t ) )
      return r;
  LISTod;
  return 0;
}

/**
***  SELgetnew_dmlist (const Type type) 
***  Returns a list of types which have unique underlying types
***  The returned list includes all the types which have a data members
***  in the select type.
***
***  The list that is returned needs to be freed by the caller.
***/


Linked_List 
SELgetnew_dmlist (const Type type) 
{
  Linked_List complete = SEL_TYPEget_items(type);
  Linked_List newlist = LISTcreate ();

  LISTdo (complete, t, Type)

    /*     if t\'s underlying type is not already in newlist, */
    if ( ! utype_member (newlist, t, 0))
      LISTadd_last (newlist, t);

  LISTod; 

  return newlist;

}

const char *
SEL_ITEMget_dmtype (Type t, const Linked_List l)   
{
  const char * r = utype_member (l, t, 0);
  return StrToLower (r ? r : TYPEget_utype (t));

}


/**
***  SEL_ITEMget_dmname (Type t)   
***  Returns the name of the data member in the select class for the item of
***    the select having the type t.
***  Logical and boolean are handled as exceptions because TYPEget_utype()
***    returns "PSDAI::..." for them which is not a legal variable name.
***/

const char *
SEL_ITEMget_dmname( Type t )
{
    Class_Of_Type class = TYPEget_type(t);

    if ( class == Class_Integer_Type ) {
	return "integer";
    }
    if ( class == Class_Real_Type ) {
	return "real";
    }
    if ( class == Class_Number_Type ) {
	return "real";
    }
    if ( class == Class_String_Type ) {
	return "string";
    }
    if ( class == Class_Binary_Type ) {
	return "binary";
    }
    if ( class == Class_Logical_Type ) {
	return "logical";
    }
    if ( class == Class_Boolean_Type ) {
	return "boolean";
    }
    if ( class == Class_Entity_Type ) {
	return "app_inst";
    }
    return ( StrToLower (TYPEget_utype (t)) );
}

/*******************
duplicate_in_express_list

determines if the given "link's" underlying type is a multiple member
of the list.
	RETURNS 1 if true, else 0.
*******************/
int
duplicate_in_express_list ( const Linked_List list, const Type check )
{
  char b [BUFSIZ];

  if (TYPEis_entity (check))  return FALSE;
  /*  entities are never the same  */
  
  LISTdo( list, t, Type )
    if (t == check)  ;        /*  don\'t compare check to itself  */
    else return TRUE;   /* other things in the list conflict  */
  LISTod;
  return FALSE;
}

/*******************
unique_types ( const Linked_List list )

determines if any of the types in a select type resolve to the same
underlying Express type.  
RETURNS 1 if true, else 0.
*******************/
int
unique_types ( const Linked_List list )
{
   LISTdo( list, t, Type )
      if( duplicate_in_express_list (list, t) )
         return FALSE;
   LISTod;
   return TRUE;
}


/*******************
duplicate_utype_member 

determines if the given "link's" C++ representation is used again in the list.
	RETURNS 1 if true, else 0.
*******************/
int
duplicate_utype_member( const Linked_List list, const Type check )
{
  char b [BUFSIZ];

  if (TYPEis_entity (check))  return FALSE;
  /*  entities are never the same  */
  
  LISTdo( list, t, Type )
    if (t == check)  ;
      /*  don\'t compare check to itself  */
    else {   /*  continue looking  */
      strncpy (b, TYPEget_utype (t), BUFSIZ);
      if (    ( !strcmp(b, TYPEget_utype(check)) )
	   || ( compareOrigTypes( t, check ) ) )
	/*  if the underlying types are the same  */
	return TRUE;
      if (! strcmp(b, "SCLP23(Integer)") &&
	  (! strcmp(TYPEget_utype(check), "SCLP23(Real)") ))
	/*  integer\'s and real\'s are not unique  */
	return TRUE;
  }
  LISTod;
  return FALSE;
}

/*******************
any_duplicates_in_select 

determines if any of the types in a select type resolve to the same
C++ representation for the underlying Express type.  
RETURNS 1 if true, else 0.
*******************/
int
any_duplicates_in_select( const Linked_List list )
{
   LISTdo( list, t, Type )
      if( duplicate_utype_member(list, t) )
         return TRUE;
   LISTod;
   return FALSE;
}

/*******************
find_duplicate_list 

finds an instance of each kind of duplicate type found in the given list.  
This list is returned as dup_list.  If a duplicate exists, the function
returns TRUE, else FALSE.
list should be unbound before calling, and freed afterwards.
*******************/
int
find_duplicate_list(const Type type, Linked_List *duplicate_list )
{
  Linked_List temp;	/** temporary list for comparison **/

   *duplicate_list = LISTcreate();
   if( any_duplicates_in_select(SEL_TYPEget_items(type)) )
   {  /**  if there is a dup somewhere  **/
      temp = LISTcreate();
      LISTdo( SEL_TYPEget_items(type), u, Type )
         if( !utype_member(*duplicate_list, u, 1) )
         {  /**  if not already a duplicate  **/
            if( utype_member(temp, u, 1) )
               LISTadd_first( *duplicate_list, u );
            else
               LISTadd_first( temp, u );
         } 
      LISTod;
      LISTfree( temp );
      return TRUE;
   } 
   return FALSE;
}

/*******************
non_unique_types_string ( const Type type )

returns a string containing the non-unique EXPRESS types deriveable
from a select.  the returned string is in the form (TYPE | TYPE |...)
*******************/
/* In the functions below, we use a vector of ints to count paths in the
   select-graph to base types.  The names in this enum correspond to the
   indices in the vector, i.e., tvec[treal] == tvec[1], and contains the
   number of paths to REAL in the graph
*/

enum __types {
    tint = 0,  /* INTEGER */
    treal,     /* REAL */
    tstring,   /* STRING */
    tbinary,   /* BINARY */
    tenum,     /* ENUMERATION, also LOGICAL, BOOLEAN */
    tselect,   /* SELECT */
    tentity,   /* ENTITY */
    taggr,     /* AGGREGATE, also ARRAY, BAG, SET, LIST */
    tnumber    /* NUMBER */
};

/* This function gets called recursively, to follow a select-graph to its
   leaves.  It passes around the vector described above, to track paths to
   the leaf nodes.
*/
void
non_unique_types_vector ( const Type type, int* tvec )
{
    LISTdo( SEL_TYPEget_items(type), t, Type )
        switch (TYPEget_body(t)->type) {
        case integer_:
            tvec[tint]++;
            break;
        case real_:
            tvec[treal]++;
            break;
        case string_:
            tvec[tstring]++;
            break;
        case binary_:
            tvec[tbinary]++;
            break;
        case enumeration_:
        case logical_:
        case boolean_:
            tvec[tenum]++;
            break;
        case select_:
            /* SELECT, ergo recurse! */
            non_unique_types_vector(t, tvec);
            break;
        case entity_:
            tvec[tentity]++;
            break;
        case aggregate_:
        case array_:
        case list_:
        case bag_:
        case set_:
            tvec[taggr]++;
            break;
        case number_:
            tvec[tnumber]++;
            break;
        }
    LISTod;
}            

/* Uses non_unique_types_vector on the select to get a vector of base-type
   reference counts, then uses that to make a string of types, of the form
   (FOO_TYPE | BAR_TYPE | BAZ_TYPE), where FOO, BAR, and BAZ are EXPRESS
   types.  If all types are unique, the string (0) is generated.
*/
char*
non_unique_types_string ( const Type type )
{
    int tvec[] = {0,0,0,0,0,0,0,0,0};
    char* typestr;
    int first = 1;
    int i;
    
    non_unique_types_vector(type, tvec);

    /* build type string from vector */
    typestr = (char*)malloc(BUFSIZ); typestr[0] = '\0';
    strcat(typestr, (char*)"(");
    for (i=0; i<=tnumber; i++)
    {
        if (tvec[i] < 2) continue;  /* skip, this one is unique */
        
        if (!first)
            strcat(typestr, (char*)" | ");
        else
            first = 0;
        switch (i) {
        case tint   : strcat(typestr, (char*)"sdaiINTEGER"); break;
        case treal  : strcat(typestr, (char*)"sdaiREAL"); break;
        case tstring: strcat(typestr, (char*)"sdaiSTRING"); break;
        case tbinary: strcat(typestr, (char*)"sdaiBINARY"); break;
        case tenum  : strcat(typestr, (char*)"sdaiENUMERATION"); break;
        case tentity: strcat(typestr, (char*)"sdaiINSTANCE"); break;
        case taggr  : strcat(typestr, (char*)"sdaiAGGR"); break;
        case tnumber: strcat(typestr, (char*)"sdaiNUMBER"); break;
        }
    }
    if (first) strcat(typestr, (char*)"0");
    strcat(typestr, (char*)")");
    return typestr;
}



/******************************************************************
 ** Procedure:  ATTR_LISTmember 
 ** Parameters:  Linked_List l, Variable check
 ** Returns:  the attribute if an attribute with the same name as "check"
 **  is on the list, 0 otherwise
 ** Description:  checks to see if an attribute is a member of the list
 ** Side Effects:  
 ** Status:  26-Oct-1993 done
 ******************************************************************/

Variable
ATTR_LISTmember (Linked_List l, Variable check)
{
  char nm [BUFSIZ];
  char cur [BUFSIZ];

  generate_attribute_name (check, nm);
  LISTdo (l, a, Variable) 
    generate_attribute_name (a, cur);
    if (! strcmp (nm, cur) )
	   return check;
  LISTod;
  return (0);
}


/******************************************************************
 ** Procedure:  SEL_TYPEgetnew_attribute_list 
 ** Parameters:  const Type type
 ** Returns:  Returns a list of all the attributes for a select type.
 **   The list is the union of all the attributes of the underlying types.
 ** Description:  
 ** Side Effects:  
***  The list that is returned needs to be freed by the caller.
 ** Status:  
 ******************************************************************/

Linked_List 
SEL_TYPEgetnew_attribute_list (const Type type) 
{
  Linked_List complete = SEL_TYPEget_items(type);
  Linked_List newlist = LISTcreate ();
  Linked_List attrs;
  Entity cur;

  LISTdo (complete, t, Type)
    if( TYPEis_entity(t) )
      {  
	cur = ENT_TYPEget_entity (t);
	attrs = ENTITYget_all_attributes (cur);
	LISTdo (attrs, a, Variable)
	  if (! ATTR_LISTmember (newlist, a))
	    LISTadd_first (newlist, a);
	LISTod; 
      }
  LISTod;
  return newlist;
}
       
/*******************
TYPEselect_inc_print_vars prints the class 'definition', that is, the objects
	and the constructor(s)/destructor for a select class.
********************/
void
TYPEselect_inc_print_vars( const Type type, FILE *f, Linked_List dups)
{
  char buffer [BUFSIZ],
       *buf = buffer;
  int result, size, j;
  Linked_List data_members = SELgetnew_dmlist (type);
  char dmname [BUFSIZ],
       classnm [BUFSIZ],
       tdnm [BUFSIZ];

  buffer[0] = '\0';
  strncpy (classnm, SelectName (TYPEget_name (type)), BUFSIZ);
  strncpy (tdnm, TYPEtd_name (type), BUFSIZ);
  size = strlen(classnm) + 2; /* for formatting output */

  fprintf (f, "\n//////////  SELECT TYPE %s\n", 
	   SelectName (TYPEget_name (type)));
/*  fprintf( f, "class %s;\n"*/
/*	   "typedef %s * %sH;\n\n",*/
/*	   classnm, classnm, classnm);*/
  fprintf( f, "class %s  :  public "  BASE_SELECT " {\n", classnm);
   fprintf( f, "  protected:\n" );
/*   fprintf( f, "  \tunion {  \n" );  */
   fprintf( f, "/*  \tunion {  */\n" );  
   fprintf( f, "\t//  types in SELECT \n" );  
   LISTdo( SEL_TYPEget_items(type), t, Type )
     fprintf( f, "\t//   %s\t--  %s\n", 
	      SEL_ITEMget_enumtype (t), FundamentalType (t));
   LISTod;

   LISTdo (data_members, t, Type)
     strncpy (dmname, SEL_ITEMget_dmname (t), BUFSIZ);
     fprintf( f, "\t   %s _%s;\n", TYPEget_utype (t), dmname );
   LISTod;

/*   fprintf( f, "  \t}  ;" );*/
   fprintf( f, "/*  \t} ;  */" );
   fprintf( f, "  /* unnamed union of select items */\n" );

   fprintf (f, "\n  public:\n");
   fprintf (f, "\n#ifdef __OSTORE__\n");
   fprintf (f, "\tfriend void %s_access_hook_in(void *, \n\t\t\t\t"
	       "enum os_access_reason, void *, void *, void *);\n", classnm);
   fprintf (f, "\tstatic os_typespec* get_os_typespec();\n");
   fprintf (f, "#endif\n\n");
 
   fprintf (f, 
	   "\tvirtual const TypeDescriptor * AssignEntity (SCLP23(Application_instance) * se);\n"
	   "\tvirtual SCLP23(Select) * NewSelect ();\n"
	   );

  fprintf (f, "\n\tvirtual BASE_TYPE ValueType() const;\n");
  
  fprintf (f, "\n\n// STEP Part 21\n");
  fprintf (f, "\tvirtual void STEPwrite_content (ostream& out =std::cout,\n"
	      "\t\t\t\t\tconst char *currSch =0) const;\n");
  fprintf (f, "\tvirtual void STEPwrite_verbose (ostream& out =std::cout,\n"
	      "\t\t\t\t\tconst char *currSch =0) const;\n");
  fprintf (f, "\tvirtual Severity STEPread_content (istream& in =cin,\n"
	      "\t\tInstMgr * instances =0, const char *utype =0,\n"
	      "\t\tint addFileId =0, const char *currSch =0);\n");

	/*  read StrToVal_content   */
	fprintf (f, "\tvirtual Severity StrToVal_content "
		 "(const char *,\n\t\tInstMgr * instances =0);\n");

	/*	constructor(s)	*/
	fprintf (f, "\n// STEP Part 22:  SDAI\n");
	fprintf (f, "\n// constructors\n" );
        fprintf (f, "\t%s( const SelectTypeDescriptor * =%s );\n",
		 classnm, tdnm  );

	fprintf (f, "\t//  part 1\n" );

	LISTdo( SEL_TYPEget_items(type), t, Type )
	  if (    ( TYPEis_entity (t) )
	       || ( !utype_member(dups, t, 1) ) ) {
	    /**  if an entity or not in the dup list  **/
	    fprintf (f, "\t%s( const %s&,\n\t",
		     SelectName (TYPEget_name(type)), AccessType(t));
	    for ( j=0; j<size; j++ ) fprintf (f, " ");
	    fprintf (f, "const SelectTypeDescriptor * =%s );\n", tdnm);
	  }
	LISTod;
	LISTdo( dups, t, Type )
	  if (! TYPEis_entity (t))  {  /*  entities were done already */
	      fprintf (f, "\t%s( const %s&,\n\t",
		       SelectName (TYPEget_name(type)), 
		       isAggregateType(t) ?  AccessType(t) : TYPEget_utype(t));
	      for ( j=0; j<size; j++ ) fprintf (f, " ");
	      fprintf (f, "const SelectTypeDescriptor * =%s );\n", tdnm);
	  }
	LISTod;

             /*	destructor	*/
   fprintf( f, "\tvirtual ~%s();\n", classnm );
   LISTfree (data_members);
}

/*******************
TYPEselect_inc_print prints the class member function declarations of a select
class.
*******************/
void
TYPEselect_inc_print (const Type type, FILE* f)
{
  char n[BUFSIZ];	/*  class name  */
  char tdnm [BUFSIZ];  /*  TypeDescriptor name  */
  String attrnm;
  Linked_List dups;
  int dup_result;
  Linked_List attrs; 

   dup_result = find_duplicate_list( type, &dups );
   strncpy( n, SelectName(TYPEget_name (type)), BUFSIZ );
   strncpy( tdnm, TYPEtd_name (type), BUFSIZ );
   TYPEselect_inc_print_vars( type, f, dups );

  fprintf( f, "\n\t//  part 2\n" );

	LISTdo( SEL_TYPEget_items(type), t, Type )
	  if ( (TYPEis_entity (t))
	       || ( !utype_member(dups, t, 1) ) ) {
	    /**  if an entity or not in the dup list  **/
	    fprintf (f, "\toperator %s();\n", AccessType (t) );
	  }
	LISTod;
	LISTdo( dups, t, Type )
	  /* do the dups once only */
	  if (! TYPEis_entity (t))  /*  entities were done already */
	    fprintf ( f, "\toperator %s ();\n", 
		      (TYPEis_aggregate(t) || TYPEis_select(t)) ?
		      AccessType (t) : TYPEget_utype(t) );
	LISTod;

  fprintf( f, "\n\t//  part 3\n" );
  attrs = SEL_TYPEgetnew_attribute_list (type);  
  /* get the list of unique attributes from the entity items */
  LISTdo( attrs, a, Variable )
    if (VARget_initializer (a) == EXPRESSION_NULL)
      ATTRsign_access_methods (a, f); 
  LISTod;
  LISTfree( attrs );

  fprintf( f, "\n\t//  part 4\n" );
  LISTdo( SEL_TYPEget_items(type), t, Type )
    if (    ( TYPEis_entity (t) )
	 || ( !utype_member(dups, t, 1) ) ) {
	/**  if an entity or not in the dup list  **/
	fprintf (f, "\t%s& operator =( const %s& );\n", 
		 SelectName (TYPEget_name (type)), AccessType (t) );
    }
  LISTod;
  LISTdo( dups, t, Type )
    if (! TYPEis_entity (t))   { /*  entities were done already */
	fprintf (f, "\t%s& operator =( const %s& );\n", 
		 SelectName (TYPEget_name (type)), 
		 isAggregateType(t) ? AccessType(t) : TYPEget_utype(t));
    }
  LISTod;

  fprintf (f, "\t// not in SDAI\n"
	   "\t%s& ShallowCopy ( const %s& );\n",
	   n, n );

  fprintf( f, "\n#ifdef COMPILER_DEFINES_OPERATOR_EQ\n#else\n" );
  fprintf( f, "\t%s& operator =( %s * const & );\n", n, n );
  fprintf( f, "\tSCLP23(Select)& operator =( const SCLP23(Select)& );\n" );
/*  fprintf( f, "\t%s& operator =( const %s& );\n", n, n );*/
  fprintf( f, "#endif\n" );

  fprintf( f, "\n\t//  part 5\n" );
   LISTdo( SEL_TYPEget_items(type), t, Type )
      fprintf( f, "\tLogical Is%s() const;\n", 
	FirstToUpper(TYPEget_name(t)) );
   LISTod;

  fprintf( f, "\n\t//  part 6 ... UnderlyingTypeName () implemented in"
	      " SCLP23(Select) class ...\n" );
/*   fprintf( f, "\tSCLP23(String) UnderlyingTypeName() const;\n" );*/
/*   fprintf( f, "\tconst EntityDescriptor * CurrentUnderlyingType();\n" );*/

   if( dup_result )
   {  /**  if there are duplicate underlying types  **/
      fprintf( f, "\n\t//  part 7\n" );
      fprintf( f, "\tconst TypeDescriptor *" 
	       "SetUnderlyingType ( const TypeDescriptor * td );\n", n );
   } 
   else
      fprintf( f, "\n\t//  part 7 ... NONE\tonly for complex selects...\n" );

#ifdef PART8
  fprintf( f, "\n\t//  part 8\n" );
  fprintf( f, "\t%s* operator->();\n", n );
#endif

  fprintf( f, "};\n");

        /* Print ObjectStore Access Hook function */
  fprintf (f, "\n#ifdef __OSTORE__\n");
  fprintf (f, "void %s_access_hook_in(void *object, \n\tenum os_access"
	      "_reason reason, void *user_data, \n\tvoid *start_range, "
	      "void *end_range);\n\n", n);

	/* DAS creation function select class */
  fprintf (f, "SCLP23(Select) * create_%s (os_database *db);\n", n, n);
  fprintf (f, "#else\n");
  fprintf (f, "\ninline SCLP23(Select) * create_%s () { return new %s; }\n", n, n);
  fprintf (f, "#endif\n\n");

        /* DAR - moved from SCOPEPrint() */
  fprintf( f, "typedef %s * %sH;\n", n, n);
  fprintf( f, "typedef %s_ptr %s_var;\n\n", n, n );

  /*  print things for aggregate class  */
  fprintf (f, "\nclass %ss : public SelectAggregate {\n", n);
  fprintf (f, "  protected:\n");
  fprintf (f, "    SelectTypeDescriptor *sel_type;\n\n");
  fprintf (f, "  public:\n");
  fprintf (f, "    %ss( SelectTypeDescriptor * =%s );\n", n, tdnm);
  fprintf (f, "    ~%ss();\n", n);
  fprintf (f, "\n#ifdef __OSTORE__\n");
  fprintf (f, "    static os_typespec* get_os_typespec();\n");
  fprintf (f, "    virtual SingleLinkNode * NewNode();\n");
  fprintf (f, "#else\n");
  fprintf (f, "    virtual SingleLinkNode * NewNode()\n");
  fprintf (f, "\t { return new SelectNode (new %s( sel_type )); }\n", n);
  fprintf (f, "#endif\n");
  fprintf (f, "};\n");

	/* DAS creation function for select aggregate class */
  fprintf (f, "\n#ifdef __OSTORE__\n");
  fprintf (f, "STEPaggregate * create_%ss (os_database *db);\n", n, n);
  fprintf (f, "#else\n");
  fprintf (f, "inline STEPaggregate * create_%ss () { return new %ss; }\n",
	   n, n);
  fprintf (f, "#endif\n\n");

  fprintf( f, "typedef %ss_ptr %ss_var;\n", n, n );

  fprintf (f, "\n/////  END SELECT TYPE %s\n\n", TYPEget_name (type) );

  LISTfree( dups );
}


/*******************
TYPEselect_lib_print_part_one prints constructor(s)/destructor of a select
class.
*******************/
void
TYPEselect_lib_print_part_one( const Type type, FILE* f, Schema schema,
			       Linked_List dups, char *n )
{
#define schema_name	SCHEMAget_name(schema)
  char var,
       attrnm[BUFSIZ],
       tdnm[BUFSIZ],
       nm[BUFSIZ];
  int size = strlen(n) * 2 + 4, j; /* size - for formatting output */

  strncpy (tdnm, TYPEtd_name (type), BUFSIZ);
  strncpy (nm, SelectName( TYPEget_name(type) ), BUFSIZ);

	/* DAS creation function select class */
  fprintf (f, "\n#ifdef __OSTORE__\n");
  fprintf (f, "SCLP23(Select) * \ncreate_%s (os_database *db) \n{\n    return "
	      "new (db, %s::get_os_typespec()) \n\t\t%s ; \n}\n", nm, nm, nm);
	/* generate access hook function to reinitialize SCLP23(Select)'s pointer 
	   to the transient select dictionary entry. */
  fprintf (f, "\nvoid \n%s_access_hook_in(void *object, \n\t\t\t\tenum "
	      "os_access_reason reason, void *user_data, \n\t\t\t\tvoid *"
	      "start_range, void *end_range)\n{\n", nm);
  fprintf (f, "#ifdef SCL_LOGGING\n    if( *logStream )\n    {\n");
  fprintf (f, "\t*logStream << \"%s access hook funct called.\" << std::endl;\n",
	      nm);
  fprintf (f, "    }\n#endif\n");
/*
  fprintf (f, "    std::cout << \"%s access hook funct called.\" << std::endl;\n",
	   SelectName( TYPEget_name(type) ) );
*/
  fprintf (f, "    %s *s = (%s *) object;\n    s->_type = %s;\n", nm, nm, tdnm);
  fprintf (f, "#ifdef SCL_LOGGING\n    if( *logStream )\n    {\n");
  fprintf (f, "\t*logStream << \"underlying type set to \" << "
	      "s->UnderlyingTypeName() << std::endl << std::endl;\n");
  fprintf (f, "    }\n#endif\n");
/*
  fprintf (f, "    std::cout << \"underlying type set to \" << s->UnderlyingTypeName() << std::endl << std::endl;\n");
*/
  fprintf (f, "    const TypeDescriptor *td = s->CanBe(s->UnderlyingType"
	      "Name());\n");
  fprintf (f, "    s->SetUnderlyingType(td);\n");
  fprintf (f, "    if(!td)\n");
  fprintf (f, "\tstd::cerr << \"ERROR: can't reinitialize underlying select "
	      "TypeDescriptor.\" \n\t     << std::endl;\n");
  fprintf (f, "}\n");
  fprintf (f, "#endif\n\n");

	/* DAS creation function for select aggregate class */
  fprintf (f, "\n#ifdef __OSTORE__\n");
  fprintf (f, "STEPaggregate * \ncreate_%ss (os_database *db) \n{\n    return "
	      "new (db, %ss::get_os_typespec()) \n\t\t%ss ; \n}\n", nm, nm, nm);
  fprintf (f, "#endif\n\n");

    /*	constructor(s)	*/
	/*  null constructor  */
  fprintf( f, "\n// STEP Part 22:  SDAI\n");
  fprintf( f, "\n\t//  part 0\n" );
  fprintf( f, "%s::%s( const SelectTypeDescriptor *typedescript )\n", n, n );
  fprintf( f, "  : " BASE_SELECT " (typedescript)" );
  /* Initialize the select members with their correct typedescriptors: */
  initSelItems( type, f );
  fprintf (f, "\n{\n");
  fprintf (f, "#ifdef SCL_LOGGING\n    if( *logStream )\n    {\n");
  fprintf (f, "\t*logStream << \"DAVE ERR entering %s constructor.\" << std::endl;\n", n);
  fprintf (f, "    }\n#endif\n");
/*  fprintf( f, "#ifdef  WE_WANT_TO_HAVE_THIS_CONSTRUCTOR\n" );*/
  fprintf( f, "   nullify();\n" );
/*  fprintf( f, "#endif\n" );*/
  fprintf (f, "#ifdef SCL_LOGGING\n    if( *logStream )\n    {\n");
  fprintf (f, "//\t*logStream << \"DAVE ERR exiting %s constructor.\" << std::endl;\n", n);
  fprintf (f, "    }\n#endif\n");
  fprintf( f, "}\n" );

	/*  constructors with underlying types  */
  fprintf( f, "\n\t//  part 1\n" );
  LISTdo( SEL_TYPEget_items(type), t, Type )
    if (    ( TYPEis_entity (t) )
	 || ( !utype_member(dups, t, 1) ) ) {
	/* if there is not more than one underlying type that maps to the same
	 * base type print out the constructor using the type from the TYPE
	 * statement as the underlying type.  Also skip enums/sels which are
	 * renames of other items.  That would create redundant constructors
	 * since renames are typedef'ed to the original type.
       	 */
	fprintf( f, "%s::%s( const %s& o,\n", n, n, AccessType (t) );
	for ( j=0; j<size; j++ ) fprintf( f, " " );
	/* Did this for the heck of it, and to show how easy it would have
	   been to make it all pretty - DAR.  ;-) */
	fprintf( f, "const SelectTypeDescriptor *typedescript )\n" );

	fprintf( f, "  : " BASE_SELECT " (typedescript, %s)",
		 TYPEtd_name(t));
	initSelItems( type, f );
	fprintf (f, "\n{\n");
	fprintf (f, "#ifdef SCL_LOGGING\n    if( *logStream )\n    {\n");
	fprintf (f, 
		 "\t*logStream << \"DAVE ERR entering %s constructor.\""
		 " << std::endl;\n", n);
	fprintf (f, "    }\n#endif\n");

	if (isAggregateType (t))  
	  fprintf( f, "   _%s.ShallowCopy (*o);\n", SEL_ITEMget_dmname (t) );
	else
	  fprintf( f, "   _%s = o;\n", SEL_ITEMget_dmname (t) );
	fprintf (f, "#ifdef SCL_LOGGING\n    if( *logStream )\n    {\n");
	fprintf (f, 
		 "//\t*logStream << \"DAVE ERR exiting %s constructor.\""
		 " << std::endl;\n", n);
	fprintf (f, "    }\n#endif\n");

	fprintf( f, "}\n\n" );
      }
   LISTod;
   LISTdo( dups, t, Type )
     /* if there is more than one underlying type that maps to the
      *  same base type, print a constructor using the base type.
      */
     if (! TYPEis_entity (t))  /*  entities were done already */
	 {
	   if (isAggregateType (t))  {
	     fprintf( f, "%s::%s( const %s& o,\n", n, n, AccessType(t) );
	     for ( j=0; j<size; j++ ) fprintf( f, " " );
             fprintf( f, "const SelectTypeDescriptor *typedescript )\n" );
	     fprintf( f, "  : " BASE_SELECT " ( typedescript, %s )",
		      TYPEtd_name(t));
	     initSelItems( type, f );
	     fprintf (f, "\n{\n");
	     fprintf (f, "#ifdef SCL_LOGGING\n    if( *logStream )\n    {\n");
	     fprintf (f, 
		      "\t*logStream << \"DAVE ERR entering %s constructor.\""
		      " << std::endl;\n", n);
	     fprintf (f, "    }\n#endif\n");
	     fprintf (f, "   _%s.ShallowCopy (*o);\n", SEL_ITEMget_dmname (t));
	   }
	   else {
	     fprintf( f, "%s::%s( const %s& o,\n", n, n, TYPEget_utype(t) );
	     for ( j=0; j<size; j++ ) fprintf( f, " " );
             fprintf( f, "const SelectTypeDescriptor *typedescript )\n" );
	     fprintf( f, "  : " BASE_SELECT " ( typedescript, %s )", 
		      TYPEtd_name(t));
	     initSelItems( type, f );
	     fprintf (f, "\n{\n");
	     fprintf( f, "   _%s = o;\n", SEL_ITEMget_dmname (t));
	   }
       fprintf( f, 
	       "//  NOTE:  Underlying type defaults to %s instead of NULL\n",
	       TYPEtd_name(t));
       fprintf (f, "#ifdef SCL_LOGGING\n    if( *logStream )\n    {\n");
       fprintf (f, 
		"//\t*logStream << \"DAVE ERR exiting %s constructor.\""
		" << std::endl;\n", n);
       fprintf (f, "    }\n#endif\n");
       fprintf( f, "}\n\n" );
   }
   LISTod;

   fprintf( f, "%s::~%s()\n{\n", n, n );
   fprintf( f, "}\n\n" );

   fprintf (f, "%ss::%ss( SelectTypeDescriptor *s)\n"
	       "  : SelectAggregate(), sel_type(s)\n{\n}\n\n", n, n);
   fprintf (f, "%ss::~%ss() { }\n\n",n,n);
   fprintf (f, "\n#ifdef __OSTORE__\n");
   fprintf (f, "SingleLinkNode * \n%ss::NewNode() \n{\n",n);
   fprintf (f, "    return new( os_segment::of(this), SelectNode::get_os"
               "_typespec() )\n\tSelectNode( new( os_segment::of(this),\n"
	       "\t\t\t %s::get_os_typespec() ) \n\t\t    %s );\n}\n", n,n);
   fprintf (f, "#endif\n\n");

#undef schema_name	
}

static void
initSelItems( const Type type, FILE *f )
    /*
     * Creates initialization functions for the select items of a select.  The
     * selects must have their typedescriptors set properly.  If a select is a
     * renaming of another select ("TYPE selB = selA") its td would default to
     * selA's, so it must be set specifically.
     */
{
    Linked_List data_members = SELgetnew_dmlist(type);

    LISTdo (data_members, t, Type)
      if ( TYPEis_select(t) ) {
	  fprintf( f, ",\n    _%s (%s)", SEL_ITEMget_dmname(t),
		   TYPEtd_name(t) );
      }
    LISTod;
}

Linked_List
ENTITYget_expanded_entities  (Entity e, Linked_List l)
{
  Linked_List supers;
  int super_cnt =0;
  Entity super;

  if (! LISTmember (l, e)) 
    LISTadd_first (l, e);

  if ( multiple_inheritance ) {
      supers = ENTITYget_supertypes (e);
      LISTdo (supers, s, Entity)
	/* ignore the more than one supertype
	   since multiple inheritance isn\'t implemented  */
	if (super_cnt == 0) 
	  ENTITYget_expanded_entities (s, l);
        ++ super_cnt;
      LISTod;
  } else {
      /* ignore the more than one supertype
	 since multiple inheritance isn\'t implemented  */
      super = ENTITYget_superclass (e);
      ENTITYget_expanded_entities (super, l);
  }
  return l;
}

Linked_List 
SELget_entity_itemlist (const Type type) 
{
  Linked_List complete = SEL_TYPEget_items(type);
  Linked_List newlist = LISTcreate ();
  Entity cur;

  LISTdo (complete, t, Type)
    if( TYPEis_entity(t) )
      {  
	cur = ENT_TYPEget_entity (t);
	ENTITYget_expanded_entities (cur, newlist);
      }
  LISTod; 
  return newlist;

}

static int
memberOfEntPrimary( Entity ent, Variable uattr )
    /*
     * Specialized function used in function TYPEselect_lib_print_part_three
     * below.  Calls a function to check if an attribute of an entity belongs
     * to its primary path (is its own attr, that of its first super, that of
     * its first super's first super etc), and does necessary housekeeping.
     */
{
    Linked_List attrlist = LISTcreate();
    int result;

    ENTITYget_first_attribs( ent, attrlist );
    result = ( LISTmember( attrlist, uattr ) != 0 );
    LIST_destroy( attrlist );
    return result;
}

/*******************
TYPEselect_lib_print_part_three prints part 3) of the SDAI C++ binding for
a select class -- access functions for the data members of underlying entity
types.
*******************/
void
TYPEselect_lib_print_part_three( const Type type, FILE* f, Schema schema,
				 char *classnm )
{
#define ENTITYget_type(e)  ((e)->u.entity->type)

  char uent[BUFSIZ],  /*  name of underlying entity type  */
    utype[BUFSIZ],    /*  underlying type name  */
    attrnm [BUFSIZ],  /*  attribute name ->  data member = _attrnm  */
    funcnm[BUFSIZ];   /*  access function name = Attrnm  */
  Class_Of_Type class;
  Linked_List items = SEL_TYPEget_items (type);
                      /* all the items in the select type  */
  Linked_List attrs = SEL_TYPEgetnew_attribute_list (type);
                      /*  list of attributes with unique names */
  Linked_List supers;
  Entity super,  /*  supertype used for super class with single inheritance  */
         ent;
  Variable uattr;     /*  attribute in underlying type  */

  fprintf( f, "\n\t//  part 3\n" );

  LISTdo( attrs, a, Variable )
    /*  go through all the unique attributes  */
    if (VARget_initializer (a) == EXPRESSION_NULL)
      { /*  only do for explicit attributes  */
	generate_attribute_func_name(a, funcnm);
	generate_attribute_name( a, attrnm );
/*
	strncpy (funcnm, attrnm, BUFSIZ);
	funcnm [0] = toupper (funcnm[0]);
*/
	/*  use the ctype since utype will be the same for all entities  */
	strncpy( utype, TYPEget_ctype (VARget_type (a)), BUFSIZ );

	 /*   get method  */
	ATTRprint_access_methods_get_head (classnm, a, f);
	fprintf (f, "{\n");

	LISTdo (items, t, Type)
	  if (TYPEis_entity (t) && 
	      (uattr = ENTITYget_named_attribute
	               ((ent = ENT_TYPEget_entity (t)),
			(char *) StrToLower (attrnm))))

	  {  /*  for the select items which have the current attribute  */

         /* if ( !multiple_inheritance ) { */
	        if ( !memberOfEntPrimary( ent, uattr ) ) {
		    /* If multiple inheritance is not supported, we must addi-
		       tionally check that uattr is a member of the entity's
		       primary inheritance path (i.e., the entity, its first
		       supertype, the super's first super, etc).  The above
		       `if' is commented out, because currently mult inher is
		       not supported to the extent of handling accessor func-
		       tions for non-primary supertypes. */
		    continue;
		}
         /* } */

            if (! VARis_derived (uattr))  {

	      if (!strcmp (utype, TYPEget_ctype (VARget_type (uattr)) ))  {
		/*  check to make sure the underlying attribute\'s type is 
		    the same as the current attribute.  
		    */

		strncpy( uent, TYPEget_ctype (t), BUFSIZ );

                /*  if the underlying type is that item\'s type
		    call the underlying_item\'s member function  */
		fprintf( f,
			 "  if( CurrentUnderlyingType () == %s ) \n\t//  %s\n", 
			 TYPEtd_name(t), StrToUpper (TYPEget_name (t)));
		fprintf( f, "\treturn ((%s) _%s) ->%s();\n", 
			 uent, SEL_ITEMget_dmname (t),  funcnm );

	      } else {
		/*  types are not the same issue a warning  */
		fprintf (stderr, 
			 "WARNING: in SELECT TYPE %s: \n\tambiguous "
			 "attribute \"%s\" from underlying type \"%s\".\n\n", 
			 TYPEget_name (type), attrnm, TYPEget_name (t));
		fprintf (f, "  //  %s\n\t//  attribute access function"
			 " has a different return type\n", 
			 StrToUpper (TYPEget_name (t)));
	      }

	    } else /*  derived attributes  */
		fprintf (f, "  //  for %s  attribute is derived\n",
			 StrToUpper (TYPEget_name (t)));
	  }
	LISTod;
	PRINT_BUG_REPORT (f);

	/* If the return type is an enumeration class then you can\'t
	    return NULL.  Instead I made the return type the
	    enumeration value.  This causes a WARNING about going from
	    int (NULL) to the enumeration type.  To get rid of the
	    WARNING you could add an explicit cast (using the code
	    commented out below.

	    Another option is to have the return type be the
	    enumeration class and create special "NULL" instances of
	    the class for every enumeration.  This option was not
	    implemented.

	    kcm 28-Mar-1995
	    */
	 

 
/*	EnumName (TYPEget_name (VARget_type (a)))*/
	switch (TYPEget_body ( VARget_type (a)) -> type)
	  {
	  case enumeration_:
	    fprintf( f, "   return (%s) 0;\n}\n\n", 
		    EnumName (TYPEget_name (VARget_type (a))) );
	      break;

	  case boolean_:
	    fprintf( f, "   return (Boolean) 0;\n}\n\n" );
	      break;

	  case logical_:
	    fprintf( f, "   return (Logical) 0;\n}\n\n" );
	      break;

	  default:
	    fprintf( f, "   return 0;\n}\n\n" );
	  }

	/*   put method  */
	ATTRprint_access_methods_put_head  (classnm, a, f);
	fprintf (f, "{\n");
	LISTdo (items, t, Type)
	  if (TYPEis_entity (t) && 
	      (uattr = ENTITYget_named_attribute(
	                                  (ent = ENT_TYPEget_entity (t)),
					  (char *) StrToLower (attrnm))) )

	  {  /*  for the select items which have the current attribute  */

         /* if ( !multiple_inheritance ) { */
	        if ( !memberOfEntPrimary( ent, uattr ) ) {
		    /* See note for similar code segment in 1st part of fn. */
		    continue;
		}
         /* } */

            if (! VARis_derived (uattr))  {

	      if (!strcmp (utype, TYPEget_ctype (VARget_type (uattr)) )) {
		/*  check to make sure the underlying attribute\'s type is 
		    the same as the current attribute.  
		    */

		/*  if the underlying type is that item\'s type
		    call the underlying_item\'s member function  */
		strncpy( uent, TYPEget_ctype (t), BUFSIZ );
		fprintf( f,
			"  if( CurrentUnderlyingType () == %s ) \n\t//  %s\n",
			 TYPEtd_name(t), StrToUpper (TYPEget_name (t)));
		fprintf( f, "\t{  ((%s) _%s) ->%s( x );\n\t  return;\n\t}\n",
			 uent, SEL_ITEMget_dmname (t),  funcnm );
	      } else   /*  warning printed above  */
		fprintf (f, "  //  for %s  attribute access function"
			 " has a different argument type\n", 
			 SEL_ITEMget_enumtype (t));
	    } else /*  derived attributes  */
	      fprintf (f, "  //  for %s  attribute is derived\n",
		       SEL_ITEMget_enumtype (t));
	  }
	LISTod;
	PRINT_SELECTBUG_WARNING (f);
	fprintf( f, "}\n" );
    }
  LISTod;
  LISTfree( attrs );
}

/*******************
TYPEselect_lib_print_part_four prints part 4 of the SDAI document of a select
class.
*******************/
void
TYPEselect_lib_print_part_four( const Type type, FILE* f, Schema schema,
			        Linked_List dups, char *n )
{
  char nvar = tolower( n[4] ),
       x[BUFSIZ];
  int firsttime = 1;

  fprintf( f, "\n\t//  part 4\n" );

  LISTdo( SEL_TYPEget_items(type), t, Type )
    if (    ( TYPEis_entity (t) )
	 || ( !utype_member(dups, t, 1) ) ) {
	fprintf( f, "%s& %s::operator =( const %s& o )\n{\n"
		 "   nullify ();\n",
		 n, n, AccessType (t));

	if (isAggregateType (t))  
	  fprintf( f, "   _%s.ShallowCopy (*o);\n", SEL_ITEMget_dmname (t) );
	else
	  fprintf( f, "   _%s = o;\n", SEL_ITEMget_dmname (t) );

	fprintf( f, "   SetUnderlyingType (%s);\n", TYPEtd_name(t));
	fprintf( f, "   return *this;\n}\n\n" );
      }  
   LISTod;
   LISTdo( dups, t, Type )
     if (! TYPEis_entity (t))  /*  entities were done already */
       {
	 if (isAggregateType (t))  {
	   fprintf( f, "%s& %s::operator =( const %s& o )\n{\n",
		    n, n, AccessType (t) );
	   fprintf( f, "   _%s.ShallowCopy (*o);\n", SEL_ITEMget_dmname (t) );
	 }
	 else  {
	   fprintf( f, "%s& %s::operator =( const %s& o )\n{\n",
		    n, n, TYPEget_utype (t) );
	   fprintf( f, "   _%s = o;\n", SEL_ITEMget_dmname (t) );
	 }

	 fprintf (f, "   underlying_type = 0; // MUST BE SET BY USER\n");
	 fprintf (f, "#ifdef __OSTORE__\n");
	 fprintf (f, "  underlying_type_name.set_null(); \n");
	 fprintf (f, "#endif\n");
	 fprintf( f, "   //	discriminator = UNSET\n" );
	 fprintf( f, "   return *this;\n}\n" );
    }
   LISTod;

   fprintf( f, "\n#ifdef COMPILER_DEFINES_OPERATOR_EQ\n#else\n\n" );
   fprintf( f, "%s& %s::operator =( const %s_ptr& o )\n{\n", n,n,n );

   LISTdo( SEL_TYPEget_items(type), t, Type )
     strncpy ( x, TYPEget_name(t), BUFSIZ );
     if ( firsttime ) {
	 fprintf( f, "   " );
	 firsttime = 0;
     } else {
	 fprintf( f, "   else " );
     }
     fprintf( f, "if ( o -> CurrentUnderlyingType () == %s )\n",
	      TYPEtd_name(t));
   if( TYPEis_select(t) )
     {
         if( utype_member(dups,t,1) )
	   /**  if in the dup list  **/
	   fprintf( f, "      _%s = &(o -> _%s);\n",
		    SEL_ITEMget_dmname (t), 
		    StrToLower(TYPEget_utype(t)) );
         else
	   fprintf( f, "\t_%s =  &(o -> _%s);\n", 
		    SEL_ITEMget_dmname (t), 
		    SEL_ITEMget_dmname (t));
     }
   else
     {
         if( utype_member(dups,t,1) )
	   /**  if in the dup list  **/
	   fprintf( f, "      _%s = o -> _%s;\n", 
		    SEL_ITEMget_dmname (t), 
		    SEL_ITEMget_dmname (t) );
	/* I changed this although I'm not sure how the if and else differ */
/*		    StrToLower(TYPEget_utype(t)) ); */
	 else
	   fprintf( f, "\t_%s =  o -> _%s;\n", 
		    SEL_ITEMget_dmname (t), 
		    SEL_ITEMget_dmname (t));
     }
   LISTod;
   fprintf( f, "   underlying_type = o -> CurrentUnderlyingType ();\n" );
   fprintf (f, "#ifdef __OSTORE__\n");
   fprintf (f, "   underlying_type_name = o -> UnderlyingTypeName(); \n");
   fprintf (f, "#endif\n");
   fprintf( f, "   return *this;\n}\n\n" );

/*   fprintf( f, "%s& %s::operator =( const %s& o )\n{\n", n,n,n );*/
   fprintf( f, "SCLP23(Select)& %s::operator =( const SCLP23(Select)& o )\n{\n", n );

   firsttime = 1;
   LISTdo( SEL_TYPEget_items(type), t, Type )
     strncpy ( x, TYPEget_name(t), BUFSIZ );
     if ( firsttime ) {
	 fprintf( f, "   " );
	 firsttime = 0;
     } else {
	 fprintf( f, "   else " );
     }
     fprintf( f, "if ( o.CurrentUnderlyingType () == %s )\n",
	      TYPEtd_name(t));
     if( TYPEis_select(t) )
     {
         if( utype_member(dups,t,1) )
         /**  if in the dup list  **/
/*	   fprintf( f, "      _%s = o._%s;\n", */
	   fprintf( f, "\t_%s = ((%s&) o)._%s;\n", 
		    SEL_ITEMget_dmname (t), 
		    n,
		    SEL_ITEMget_dmname (t) );
/*		    StrToLower(TYPEget_utype(t)) ); */
         else
/*	   fprintf( f, "\t_%s =  o._%s;\n", */
	   fprintf( f, "\t_%s = &(((%s&) o)._%s);\n", 
		    SEL_ITEMget_dmname (t), 
		    n,
		    SEL_ITEMget_dmname (t));
     }
     else
     {
	 if( utype_member(dups,t,1) )
	   /**  if in the dup list  **/
/*         fprintf( f, "      _%s = o._%s;\n", */
	   fprintf( f, "\t_%s = ((%s&) o)._%s;\n", 
		   SEL_ITEMget_dmname (t), 
		   n,
		   SEL_ITEMget_dmname (t) );
/*		   StrToLower(TYPEget_utype(t)) ); */
	 else
/*         fprintf( f, "\t_%s =  o._%s;\n", */
	   fprintf( f, "\t_%s = ((%s&) o)._%s;\n", 
		    SEL_ITEMget_dmname (t), 
		    n,
		    SEL_ITEMget_dmname (t) );
     }
   LISTod;
   fprintf( f, "   underlying_type = o.CurrentUnderlyingType ();\n" );
   fprintf (f, "#ifdef __OSTORE__\n");
   fprintf (f, "   underlying_type_name = o.UnderlyingTypeName(); \n");
   fprintf (f, "#endif\n");
   fprintf( f, "   return *this;\n}\n\n" );
   fprintf( f, "#endif\n" );

#ifdef JNK
/*  define ShallowCopy because operator= does not always act virtual  */
   fprintf( f, "SCLP23(Select)& %s::ShallowCopy ( const SCLP23(Select)& o )\n{\n", n );

   LISTdo( SEL_TYPEget_items(type), t, Type )
     strncpy ( x, TYPEget_name(t), BUFSIZ );
	fprintf( f, "   if( o.CurrentUnderlyingType () == %s )\n",
		 TYPEtd_name(t));
         if( utype_member(dups,t,1) )
         /**  if in the dup list  **/
	   fprintf( f, "      _%s = o._%s;\n", 
		    SEL_ITEMget_dmname (t), 
		    StrToLower(TYPEget_utype(t)) );
         else
	   fprintf( f, "\t_%s =  o._%s;\n", 
		    SEL_ITEMget_dmname (t), 
		    SEL_ITEMget_dmname (t));
   LISTod;
   fprintf( f, "   underlying_type = o.CurrentUnderlyingType ();\n" );
   fprintf (f, "#ifdef __OSTORE__\n");
   fprintf (f, "   underlying_type_name = o.UnderlyingTypeName(); \n");
   fprintf (f, "#endif\n");
   fprintf( f, "   return *this;\n}\n" );
#endif
}


/*******************
TYPEselect_init_print prints the types that belong to the select type
*******************/

void
TYPEselect_init_print (const Type type, FILE* f, Schema schema)
{
#define schema_name	SCHEMAget_name(schema)
   LISTdo( SEL_TYPEget_items(type), t, Type )

    fprintf(f,"\t%s -> Elements ().AddNode",
	    TYPEtd_name (type));
    fprintf(f," (%s);\n", 
	    TYPEtd_name(t));
   LISTod;
#undef schema_name	
}

void
TYPEselect_lib_part21 (const Type type, FILE* f, Schema schema)
{
  char n[BUFSIZ], *z; 	/*  pointers to class name(s)  */
  const char * dm;  /*  data member name */
  Linked_List data_members = SELgetnew_dmlist (type);

  strncpy( n, SelectName(TYPEget_name(type)), BUFSIZ );

  fprintf (f, "\n\n// STEP Part 21\n");
  /*  write part 21   */
  fprintf (f, "\nvoid\n%s::STEPwrite_content (ostream& out, const char *"
	      " currSch) const\n{\n  ", n);

/*  go through the items  */
  LISTdo( SEL_TYPEget_items(type), t, Type )
    dm = SEL_ITEMget_dmname (t);
    
    fprintf( f, "if (CurrentUnderlyingType () == %s)\n", 
	     TYPEtd_name(t));

    switch (TYPEget_body(t)->type) {

      /*  if it\'s a number, just print it  */
    case integer_:
      fprintf( f, "\tout <<  _%s;\n  else ", dm);
      break;

    case number_:
    case real_:
      fprintf( f, "\tWriteReal(_%s,out);\n  else ", dm);
      break;

    case entity_:
      fprintf( f, "\t_%s -> STEPwrite_reference (out);\n  else ", dm);
      break;

    case string_:
    case enumeration_:
    case aggregate_:
    case array_:
    case bag_:
    case set_:
    case list_:
    case logical_:
    case boolean_:
    case binary_:
      /*  for string\'s, enum\'s, select\'s, binary\'s, and aggregate\'s
	  it\'ll be embedded; 
	  */
      fprintf( f, "\t_%s.STEPwrite (out);\n  else ", dm);
      break;

    case select_:
      fprintf( f, "\t_%s.STEPwrite (out, currSch);\n  else ", dm);
      /* Select type needs currSch passed too.  A Select writes the name of its
      // current choice when it writes itself out (e.g. "DATA(33.5)").  Since
      // the current choice name may depend on our current schema (it may be a
      // schema which USEs "DATA" and renames it to "VAL"), we pass currSch. */
      break;

    default:
      /*  otherwise it\'s a pointer  */
      fprintf( f, "\t_%s -> STEPwrite (out);\n  else ", dm);
      break;
    }
  LISTod;

  fprintf (f, "  {\n");
  PRINT_BUG_REPORT
  fprintf (f, "  }\n");
  fprintf (f, "  return;\n}\n");

  /* ValueType() -- get type of value stored in select */

  fprintf (f, "\nBASE_TYPE\n%s::ValueType() const\n{\n  ", n);

  LISTdo( SEL_TYPEget_items(type), t, Type )
    dm = SEL_ITEMget_dmname (t);
    fprintf(f, "if (CurrentUnderlyingType() == %s)\n",
            TYPEtd_name(t));

    switch (TYPEget_body(t)->type) {
    case select_:
      fprintf(f, "    return _%s.ValueType();\n  else ", dm);
      break;
    default:
      fprintf(f, "    return %s;\n  else ", FundamentalType(t,0));
    }
  LISTod;

  fprintf(f,"  {\n");
  PRINT_BUG_REPORT
  fprintf(f,"  }\n");
  fprintf(f,"  return (BASE_TYPE)0;\n}\n");

  /* STEPwrite_verbose() -- print value with specified type */

  fprintf (f, "\nvoid\n%s::STEPwrite_verbose (ostream& out,"
	      " const char *currSch) const\n{\n", n);

  /* Get name of typedescriptor, according to value of currSch: */
  fprintf (f, "  const TypeDescriptor *td = CurrentUnderlyingType();\n");
  fprintf (f, "  std::string tmp;\n\n");
  fprintf (f, "  if ( td ) {\n");
  fprintf (f, "    // If we have a legal underlying type, get its name acc\n");
  fprintf (f, "    // to the current schema.\n");
  fprintf (f, "    StrToUpper( td->Name(currSch), tmp );\n");
  fprintf (f, "  }\n  ");

  /* Next loop through the possible items: */
  LISTdo( SEL_TYPEget_items(type), t, Type )
    dm = SEL_ITEMget_dmname(t);
    fprintf(f, "if (td == %s)\n",
            TYPEtd_name(t));

    switch (TYPEget_body(t)->type) {
    case integer_:
/*      fprintf(f, "    out << \"%s(\" << _%s << \")\";\n  else ",
              StrToUpper(TYPEget_name(t)), dm);*/
      fprintf(f, "    out << tmp.chars() << \"(\" << _%s << \")\";\n  else ",
	      dm);
      break;

    case real_:
    case number_:
      fprintf(f, "  {\n    out << tmp.chars() << \"(\";\n");
      fprintf(f, "    WriteReal(_%s,out);\n", dm);
      fprintf(f, "    out << \")\";\n  }\n  else ");
      break;

    case entity_:
      fprintf(f, "  {\n    out <<  tmp.chars() << \"(\";\n");
      fprintf(f, "    _%s -> STEPwrite_reference (out);\n", dm);
      fprintf(f, "    out << \")\";\n  }\n  else ");
      break;
    case string_:
    case enumeration_:
    case logical_:
    case boolean_:
    case binary_:
      fprintf(f, "  {\n    out << tmp.chars() << \"(\";\n");
      fprintf(f, "    _%s.STEPwrite (out);\n", dm);
      fprintf(f, "    out << \")\";\n  }\n  else ");
      break;
    case aggregate_:
    case array_:
    case bag_:
    case set_:
    case list_:
      /* Aggregates need currSch passed since they may be aggrs of sels. */
      fprintf(f, "  {\n    out << tmp.chars() << \"(\";\n");
      fprintf(f, "    _%s.STEPwrite (out, currSch);\n", dm);
      fprintf(f, "    out << \")\";\n  }\n  else ");
      break;
    case select_:
      fprintf(f, "  {\n    out << tmp.chars() << \"(\";\n");
      fprintf(f, "    _%s.STEPwrite_verbose (out, currSch);\n", dm);
      fprintf(f, "    out << \")\";\n  }\n  else ");
      break;
    default:
      fprintf(f, "  _%s -> STEPwrite (out); \n  else ");
      break;
    }
  LISTod;

  fprintf(f, "  {\n");
  PRINT_BUG_REPORT
  fprintf(f,"  }\n");
  fprintf(f,"  return;\n}\n");
      
  
  /*  Read part 21   */
  fprintf (f, "\nSeverity\n%s::STEPread_content "
	   "(istream& in, InstMgr * instances,\n\t\t\tconst char *utype, "
	   "int addFileId, const char *currSch)\n{\n", n);

/*  go through the items  */
  LISTdo( SEL_TYPEget_items(type), t, Type )
    
    fprintf( f, "  if (CurrentUnderlyingType () == %s) ", 
	     TYPEtd_name(t));

    dm = SEL_ITEMget_dmname (t);

    switch (TYPEget_body(t)->type) {
      /*  if it\'s a number, just read it  */
    case real_:
    case number_:
      /*  since REAL and NUMBER are handled the same they both need 
	  to be included in the case stmt  */
      fprintf( f, "  {\n\tReadReal (_%s, in, &_error, \"),\");\n"
	       "\treturn severity ();\n  }\n",
	       dm);
      break;

    case integer_:
      fprintf( f, "  {\n\tReadInteger (_%s, in, &_error, \"),\");\n"
	       "\treturn severity ();\n  }\n",
	       dm);
      break;

    case entity_:
      /*  if it\'s an entity, use Assign - done in Select class  */
      fprintf (f,
	       "  {\n\t// set Underlying Type in Select class\n"
	       "\t_%s = ReadEntityRef "
	       "(in, &_error, \",)\", instances, addFileId);\n", dm);
      fprintf (f, "\tif (_%s && (_%s != S_ENTITY_NULL)\n "
	       "\t  && (CurrentUnderlyingType () -> CanBe (_%s -> eDesc )) )\n"
	       "\t  return severity ();\n",
	       dm, dm, dm);
      fprintf (f, "\telse {\n "
	       "\t  Error (\"Reference to instance that is not indicated type\\n\");\n"
	       "\t  _%s = 0;\n"
	       "\t  nullify ();\n"
	       "\t  return severity (SEVERITY_USERMSG);\n\t}\n  }\n",
	       dm
	       );
      break;

    case string_:
    case enumeration_:
    case logical_:
    case boolean_:
    case binary_:
      fprintf( f,
	       "  {\n\t_%s.STEPread (in, &_error);\n"
	       "\treturn severity ();\n  }\n",
	       dm);
      break;
    case select_:
      fprintf( f,
	       "  {\n\t_%s.STEPread (in, &_error, instances, utype, "
	       "addFileId, currSch);\n\treturn severity ();\n  }\n",
	       dm);
      break;
    case aggregate_:
    case array_:
    case bag_:
    case set_:
    case list_:
      fprintf( f,
	       "  {\n\t_%s.STEPread (in, &_error,\n"
	       "\t\t\t     %s -> AggrElemTypeDescriptor (),\n"
	       "\t\t\t     instances, addFileId, currSch);\n"
	       "\treturn severity ();\n  }\n",
	       dm, TYPEtd_name(t));
      break;

    default:
      fprintf( f,
	       "  {\n\t_%s -> STEPread (in, &_error, instances, addFileId);\n"
	       "\treturn severity ();\n  }\n",
	       dm);
      break;
    }

  LISTod;

  PRINT_SELECTBUG_WARNING (f) ;
  fprintf (f, "#ifdef __SUNCPLUSPLUS__\n"
	   "std::cerr << instances << \"  \" << addFileId << std::endl;\n"
	   "#endif\n");

  LISTfree (data_members);
  fprintf (f,
/*	   "#ifdef __GNUG__\n"*/
/*	   "\n  return SEVERITY_NULL;\n"*/
/*	   "#endif"*/
	   "\n  return severity ();"
	   "\n}\n");
}


void
TYPEselect_lib_StrToVal (const Type type, FILE* f, Schema schema)
{
  char n[BUFSIZ], *z; 	/*  pointers to class name  */
  Linked_List data_members = SELgetnew_dmlist (type);
  int enum_cnt =0;

  strncpy( n, SelectName(TYPEget_name(type)), BUFSIZ );

  /*  read StrToVal_content   */
  fprintf (f, "\nSeverity\n%s::StrToVal_content "
	   "(const char * str, InstMgr * instances)"
	   "\n{\n", n);

  fprintf (f, "  switch (base_type)  {\n");
  LISTdo( data_members, t, Type )
/*	fprintf (f, "  case %s :  \n", 	FundamentalType (t, 0));*/

	switch (TYPEget_body(t)->type) {

	case real_:
	case integer_:
	case number_:
	case select_:
	  /*  if it\'s a number, use STEPread_content - done in Select class  */
	  /*  if it\'s a select, use STEPread_content - done in Select class  */
/*	  fprintf (f, "\t// done in Select class\n\treturn SEVERITY_NULL;\n");*/
	  break;

	case entity_:
	  /*  if it\'s an entity, use AssignEntity - done in Select class  */
/*	  fprintf (f, "\t// done in Select class\n\treturn SEVERITY_NULL;\n");*/

	  break;
	case binary_:
	case logical_:
	case boolean_:
	case enumeration_:
	  if (!enum_cnt) {
	    /*  if there\'s more than one enumeration they are done in Select class  */
	    fprintf (f, "  case %s :  \n", 	FundamentalType (t, 0));
	    fprintf( f,
		     "\treturn _%s.StrToVal (str, &_error);\n",
		     SEL_ITEMget_dmname (t));
	  }
	  else {
	    fprintf (f, "  //  case %s :  done in Select class\n", FundamentalType (t, 0));
	  }
	  ++enum_cnt;
	  break;

	case string_:
	  fprintf (f, "  case %s :  \n", 	FundamentalType (t, 0));
	  fprintf( f,
		   "\treturn _%s.StrToVal (str);\n",
		   SEL_ITEMget_dmname (t));
	  break;

	case aggregate_:
	case array_:
	case bag_:
	case set_:
	case list_:
	  fprintf (f, "  case %s :  \n", 	FundamentalType (t, 0));
	  fprintf( f,
		   "\treturn _%s.StrToVal (str, &_error, "
		   "%s -> AggrElemTypeDescriptor ());\n",
/*		   "instances, addFileId);  "*/
		   SEL_ITEMget_dmname (t),
		   TYPEtd_name(t));
	  break;

	default:	  
	  /*  otherwise use StrToVal on the contents to check the format  */
	  fprintf (f, "  case %s :  \n", 	FundamentalType (t, 0));
	  fprintf( f,
		   "\treturn _%s -> StrToVal (str, instances);\n",
		   SEL_ITEMget_dmname (t));
	}
  LISTod;

  fprintf (f, "  default:  // should never be here - done in Select class\n");
  PRINT_SELECTBUG_WARNING (f) ;
  fprintf (f, "#ifdef __SUNCPLUSPLUS__\n"
	   "std::cerr << str << \"  \" << instances << std::endl;\n"
	   "#endif\n");
  fprintf (f, "\treturn SEVERITY_WARNING;\n  }\n");

  LISTfree (data_members);
  fprintf (f,
	   "#ifdef __GNUG__\n"
	   "\n  return SEVERITY_NULL;\n"
	   "#endif"
	   "\n}\n");
}

void
TYPEselect_lib_virtual (const Type type, FILE* f, Schema schema)
{
  TYPEselect_lib_part21 ( type, f,  schema);
  TYPEselect_lib_StrToVal ( type, f,  schema);
}

void
SELlib_print_protected (const Type type,  FILE* f, const Schema schema)  
{
  const char * snm;

  /*  SELECT::AssignEntity  */
  fprintf (f, "\nconst TypeDescriptor * \n%s::AssignEntity (SCLP23(Application_instance) * se)\n"
	 "{\n", 
	   SelectName (TYPEget_name (type))
	   );


/* loop through the items in the SELECT */
  LISTdo( SEL_TYPEget_items(type), t, Type )
    if (TYPEis_entity (t))  {
      fprintf (f,
	   "  //  %s\n"  /*  item name  */
	   "  if (se -> IsA (%s))\n" /*  td */
	   "  {  \n"
	   "\t_%s = (%s_ptr) se;\n"  /* underlying data member */
	       /*  underlying data member type */
	   "\treturn SetUnderlyingType (%s);\n"  /* td */
	   "  }\n",
	       StrToUpper (TYPEget_name (t)),
	       TYPEtd_name(t), 
	       SEL_ITEMget_dmname (t),
	       ClassName (TYPEget_name (t)),
	       TYPEtd_name(t)
    );
    }
  if  (TYPEis_select (t)) {
      fprintf (f,
	   "  //  %s\n"  /*  item name  */
	   "  if (%s -> CanBe (se -> eDesc))\n" 
	   "  {  \n"
	   "\t_%s.AssignEntity (se);\n"  /* underlying data member */
	   "\treturn SetUnderlyingType (%s);\n"  /* td */
	   "  }\n",
	       StrToUpper (TYPEget_name (t)),
	       TYPEtd_name(t), 
	       SEL_ITEMget_dmname (t),
	       TYPEtd_name(t)
    );
    }

  LISTod;
  fprintf (f, "  // should never be here - done in Select class\n");
  PRINT_SELECTBUG_WARNING (f) ;
  fprintf (f,
	   "#ifdef __SUNCPLUSPLUS__\n"
	   "  std::cerr << se -> EntityName () << std::endl;\n"
	   "#endif\n"
	   "  return 0;\n}\n");

  /*  SELECT::NewSelect  */
  snm  = SelectName (TYPEget_name (type));
  fprintf (f, "\nSCLP23(Select) * \n%s::NewSelect ()\n{\n", snm);

  fprintf (f, "#ifdef __OSTORE__\n");
  fprintf (f, "    %s * tmp = \n\t\tnew( os_segment::of(this),\n\t\t     "
	      "%s::get_os_typespec() ) \n\t\t\t%s();\n", snm, snm, snm);
  fprintf (f, "#else\n");
  fprintf (f, "    %s * tmp = new %s();\n", snm, snm);
  fprintf (f, "#endif\n");
  fprintf (f, "    return tmp;\n}\n");
}


/*******************
TYPEselect_lib_print prints the member functions (definitions) of a select
class.
*******************/
void
TYPEselect_lib_print (const Type type, FILE* f, Schema schema)
{
	char n[BUFSIZ], m[BUFSIZ];
	const char *z;	/*  pointers to class name(s)  */
	Linked_List dups;
	int dup_result;

	dup_result = find_duplicate_list( type, &dups );
	strncpy( n, SelectName(TYPEget_name(type)), BUFSIZ );
	fprintf (f, "\n//////////  SELECT TYPE %s\n", TYPEget_name (type));

	SELlib_print_protected (type,  f,  schema)  ;
	TYPEselect_lib_virtual (type, f, schema);
	TYPEselect_lib_print_part_one( type, f, schema, dups, n );

	fprintf( f, "\n\t//  part 2\n" );

	LISTdo( SEL_TYPEget_items(type), t, Type )
	  if  (TYPEis_entity (t)) 
	    {  /*  if an entity  */
	      fprintf( f, "%s::operator %s_ptr()\n{\n", n,
		       ClassName(TYPEget_name(t)) );
	      fprintf( f, "   if( CurrentUnderlyingType () == %s )\n",
		       TYPEtd_name(t));
	      fprintf( f, "      return ((%s_ptr) _%s);\n", 
		       ClassName(TYPEget_name(t)),
		       SEL_ITEMget_dmname (t) );
	      PRINT_SELECTBUG_WARNING (f);
	      fprintf( f, "   return NULL;\n}\n\n" );
	    }
	  else if  ( !utype_member(dups, t, 1) )
	    {  /**  if not in the dup list  **/
	      fprintf( f, "%s::operator %s()\n{\n", n, AccessType (t));
	      fprintf( f, "   if( CurrentUnderlyingType () == %s )\n",
		       TYPEtd_name(t));
	      fprintf( f, "      return %s _%s;\n", 
		       ((TYPEis_aggregate (t) || TYPEis_select (t)) ? "&" : ""),
		       SEL_ITEMget_dmname (t) );
	      fprintf( f, "\n   severity( SEVERITY_WARNING );\n" ); 
	      fprintf( f, "   Error( \"Underlying type is not %s\" );\n",
		       AccessType (t) );
	      PRINT_SELECTBUG_WARNING (f) ;
	      if (TYPEis_boolean (t) || TYPEis_logical (t))
	          fprintf( f, "   return (%s)0;\n}\n\n", TYPEget_utype(t) );
	      else
	          fprintf( f, "   return 0;\n}\n\n" );
	    }
	LISTod;
	LISTdo( dups, t, Type )
	  if (! TYPEis_entity (t))  /*  entities were done already */
	    {
	      fprintf( f, "%s::operator %s()\n{\n", n, 
		       (TYPEis_aggregate(t) || TYPEis_select(t)) ?
		       AccessType (t) : TYPEget_utype(t) );
	      strncpy( m, TYPEget_utype(t), BUFSIZ );

			/**** MUST CHANGE FOR multiple big types ****/
	      LISTdo( SEL_TYPEget_items(type), x, Type )
		if(    ( strcmp( m, TYPEget_utype(x)) == 0 )
		    || ( compareOrigTypes( t, x ) ) )
		  {  /* If this is one of the dups.  compareOrigTypes checks
		        if x\'s type is a rename of t\'s (see comments there).
		      */
		    fprintf( f, "   if( CurrentUnderlyingType () == %s )\n",
			     TYPEtd_name (x));
		    fprintf( f, "      return %s _%s;\n", 
			     ( ( TYPEis_aggregate (x) || TYPEis_select(x) ) ?
			       "&" : ""),
			     SEL_ITEMget_dmname (x) );
		  }
	      LISTod;
	      fprintf( f, "\n   severity( SEVERITY_WARNING );\n" ); 
	      fprintf( f, "   Error( \"Underlying type is not %s\" );\n", 
		     TYPEis_aggregate (t) ? 
		     AccessType (t) : TYPEget_utype(t) );
	      PRINT_SELECTBUG_WARNING (f) ;
	      fprintf( f, "   return (%s)0;\n}\n\n",
		     TYPEis_aggregate (t) ? 
		     AccessType (t) : TYPEget_utype(t) );
	 /*     fprintf( f, "   return NULL;\n}\n\n" ); */
	    }
	LISTod;

   TYPEselect_lib_print_part_three( type, f, schema, n );
   TYPEselect_lib_print_part_four( type, f, schema, dups, n );

   fprintf( f, "\n\t//  part 5\n" );
   LISTdo( SEL_TYPEget_items(type), t, Type )
      z = FirstToUpper( TYPEget_name(t) );
      fprintf( f, "Logical %s::Is%s() const\n{\n", n, z );
      fprintf( f, "   if( !exists() )\n", n );
      fprintf( f, "      return LUnknown;\n" );
      fprintf( f, "   if( CurrentUnderlyingType () == %s )\n", TYPEtd_name(t));
      fprintf( f, "      return LTrue;\n" );
      fprintf( f, "   return LFalse;\n}\n\n" );
   LISTod;

#ifdef UNDERLYINGTYPE
fprintf( f, "\n\t//  part 6\n" );
   fprintf( f, "SCLP23(String) %s::UnderlyingTypeName() const\n{\n", n );
   fprintf( f, "   if( exists() )\n" );
   fprintf( f, "   {\n" );
   LISTdo( SEL_TYPEget_items(type), t, Type )
      fprintf( f, "   if( CurrentUnderlyingType () == %s )\n", TYPEtd_name(t));
      if( TYPEis_entity(t) )
         fprintf( f, "         return( _%s->Name() );\n"
		  StrToLower(TYPEget_name(t)) );
      else
         fprintf( f, "         return( \"%s\" );\n", TYPEget_utype(t) );
   LISTod;
   fprintf( f, "   }\n   return NULL;\n}\n\n" );

   fprintf( f, "const EntityDescriptor * %s::CurrentUnderlyingType()\n{\n", n );
   fprintf( f, "   if( exists() )\n" );
   fprintf( f, "   {\n" );
   LISTdo( SEL_TYPEget_items(type), t, Type )
      if( TYPEis_entity(t) )
      { 
	fprintf( f, "   if( discriminator == %s )\n", SEL_ITEMget_enumtype (t));
         fprintf( f, "         return( _%s->eDesc );\n", 
		  SEL_ITEMget_dmname (t) );
      } 
   LISTod;
   fprintf( f, "   }\n   return NULL;\n}\n\n" );

#endif
      if( dup_result )  {
	fprintf( f, "\n\t//  part 7\n" );
	fprintf( f, 
		 "const TypeDescriptor * \n"
		 "%s::SetUnderlyingType (const TypeDescriptor * td)\n{\n"
		 "  return " BASE_SELECT "::SetUnderlyingType (td);\n}\n", 
		 n);
      }

#ifdef PART8
	fprintf( f, "\n\t//  part 8\n" );
	fprintf( f, "%s* %s::operator->()\n", n, n );
	fprintf( f, "{\n   return this;\n}\n" );
#endif
   LISTfree( dups );

	fprintf (f, "//////////  END SELECT TYPE %s\n\n", n);

}

void
TYPEselect_print (Type t, FILES* files, Schema schema)  
{
  SelectTag tag, tmp;
  Type i, bt;  /*  type of elements in an aggregate  */
  char nm[BUFSIZ], tdnm[BUFSIZ];
  FILE *inc = files->inc;

  /*  if type is already marked, return  */
  if ((SelectTag) (tmp = TYPEget_clientData (t)))  {
    if ((tmp ->started) && (! tmp ->complete))
      fprintf (stderr, "WARNING:  SELECT type %s causes circular references\n",
	       TYPEget_name (t));
    return;
  }

  /*  mark the type as being processed  */
  tag = (SelectTag ) malloc (sizeof (struct SelectTag_));
  tag -> started =1;
  tag -> complete =0;
  TYPEput_clientData (t, tag);


  /* Check if we're a renamed type, e.g., TYPE B (sel) = A.  If so, if A has
  // been defined, we process B differently (with a couple of typedef's -
  // some are printed in files->classes rather than here).  If A has not been
  // defined, we must recurse. */
  if ( (i = TYPEget_ancestor(t)) != NULL ) {
      if ( !TYPEget_clientData(i) ) {
	  TYPEselect_print( i, files, schema );
      }
      strncpy (nm, SelectName( TYPEget_name(t) ), BUFSIZ);
      strncpy (tdnm, TYPEtd_name (t), BUFSIZ);
      fprintf (inc, "typedef %s *    \t%sH;\n", nm, nm);
      fprintf (inc, "typedef %s_ptr *\t%s_var;\n", nm, nm);
      /* Below are specialized create functions for the renamed sel type (both
      // single and aggregate).  The functions call the original sel's con-
      // structor, passing the new sel's typedescriptor to create a hybrid
      // entity - the original select pointing to a new typedesc.   These fns
      // give the user an easy way to create the renamed type properly. */
      fprintf (inc, "\n#ifdef __OSTORE__\n");
      fprintf (inc, "void %s_access_hook_in(void *object, \n\tenum"
	            " os_access_reason reason, void *user_data, \n"
	            "\tvoid *start_range, void *end_range);\n\n", nm);
      fprintf (inc, "SCLP23(Select) * create_%s (os_database *db);\n", nm, nm);
      fprintf (inc, "#else\n");
      fprintf (inc, "inline SCLP23(Select) *\ncreate_%s ()", nm);
      fprintf (inc, " { return new %s( %s ); }\n\n", nm, tdnm);
      fprintf (inc, "#endif\n");
      fprintf (inc, "\n#ifdef __OSTORE__\n");
      fprintf (inc, "STEPaggregate * create_%ss (os_database *db);\n", nm, nm);
      fprintf (inc, "#else\n");
      fprintf (inc, "inline STEPaggregate *\ncreate_%ss ()", nm);
      fprintf (inc, " { return new %ss( %s ); }\n\n", nm, tdnm);
      fprintf (inc, "#endif\n\n");
      return;
  }

  LISTdo ( SEL_TYPEget_items(t), i, Type )

  /*  check the items for select types  */
  /*  and do the referenced select types first  */

    /* check aggregates too  */
    /* set i to the bt and  catch in next ifs  */
    if (isAggregateType (i))  {
      bt = TYPEget_base_type (i);
      /* DAR - corrected - prev'ly above line retrieved non-aggr base type.
      // But unnec - we only need the item defined if it's a select or a 1D
      // aggregate.  If bt is also an aggr, we go on. */
      if (TYPEis_select (bt))  i = bt;
      else if (TYPEis_entity (bt))  i = bt; 
    }

    if (TYPEis_select (i) && !TYPEget_clientData (i))  
      TYPEselect_print (i, files, schema);
    /* NOTE - there was a bug here because above if did not take into account
       that i came from a different schema (and above loop would have printed
       it here!).  Taken care of by code in multpass.c which would not allow
       us to get this far (wouldn't have called the type_print() fn's) if a
       select has members in other schemas which haven't been processed.  So,
       now by definition i must have been processed already if it's in another
       schema.  So the above if will only reorder the printing of the sel's in
       this schema, which is the intent.  DAR */
       
#ifdef OBSOLETE
  /*  check the attributes to see if a select is referenced  */
    if (TYPEis_entity (i)) {
      LISTdo (ENTITYget_all_attributes (ENT_TYPEget_entity (i)), a, Variable)
	if (TYPEis_select (VARget_type (a)))
	  TYPEselect_print (VARget_type (a), files, schema);
      LISTod
      }
#endif
  LISTod

  TYPEselect_inc_print (t, files -> inc);
  TYPEselect_lib_print (t, files -> lib, schema);
  /* TYPEselect_init_print (t, files -> init, schema);
     DAR - moved to TYPEprint_init() - to keep init info together. */
  tag -> complete =1;
}  
#undef BASE_SELECT


/**************************************************************************
********		END  of SELECT TYPE
**************************************************************************/
