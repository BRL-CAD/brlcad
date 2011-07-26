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

/******************************************************************
***  The functions in this file generate the C++ code for ENTITY **
***  classes, TYPEs, and TypeDescriptors.                       ***
 **								**/

static char rcsid[] ="$Id: classes.c,v 3.0.1.11 1997/09/18 21:14:46 sauderd Exp sauderd";

/* this is used to add new dictionary calls */
/* #define NEWDICT */

#include <stdlib.h>
#include "classes.h"

char *FundamentalType(const Type t,int report_reftypes);

int multiple_inheritance = 1;
int print_logging = 0;
int corba_binding = 0;
int old_accessors = 0;

 /* several classes use attr_count for naming attr dictionary entry 
    variables.  All but the last function generating code for a particular 
    entity increment a copy of it for naming each attr in the entity. 
    Here are the functions:
    ENTITYhead_print (Entity entity, FILE* file,Schema schema)
    LIBdescribe_entity (Entity entity, FILE* file, Schema schema)  
    LIBcopy_constructor (Entity ent, FILE* file)
    LIBstructor_print (Entity entity, FILE* file, Schema schema)
    LIBstructor_print_w_args (Entity entity, FILE* file, Schema schema)
    ENTITYincode_print (Entity entity, FILE* file,Schema schema)  
    DAS
  */
static attr_count;	/* number each attr to avoid inter-entity clashes */
static type_count;	/* number each temporary type for same reason above */

char *FundamentalType(const Type,int);
extern int any_duplicates_in_select( const Linked_List list );
extern int unique_types ( const Linked_List list );
extern char* non_unique_types_string ( const Type type );
static void printEnumCreateHdr ( FILE *, const Type );
static void printEnumCreateBody( FILE *, const Type );
static void printEnumAggrCrHdr ( FILE *, const Type );
static void printEnumAggrCrBody( FILE *, const Type );

/*
Turn the string into a new string that will be printed the same as the 
original string. That is, turn backslash into a quoted backslash and 
turn \n into "\n" (i.e. 2 chars).
*/

char * format_for_stringout(char *orig_buf, char* return_buf)
{
    char * optr  = orig_buf;
    char * rptr  = return_buf;
    while(*optr) {
	if(*optr == '\n') {
	    *rptr = '\\'; rptr++;
	    *rptr = 'n';
	} else if(*optr == '\\') {
	    *rptr = '\\'; rptr++;
	    *rptr = '\\';
	} else {
	    *rptr = *optr;
	}
	rptr++;
	optr++;
    }
    *rptr = '\0';
    return return_buf;
}

void
USEREFout(Schema schema, Dictionary refdict,Linked_List reflist,char *type,FILE* file)
{
	Dictionary dict;
	DictionaryEntry de;
	struct Rename *r;
	Linked_List list;
	int level = 6;
	char td_name[BUFSIZ];
	char sch_name[BUFSIZ];

	strncpy(sch_name,PrettyTmpName(SCHEMAget_name(schema)),BUFSIZ);

	LISTdo(reflist,s,Schema)
	{
	    fprintf(file,"\t// %s FROM %s; (all objects)\n",type,s->symbol.name);
	    fprintf(file,"\tis = new Interface_spec(\"%s\",\"%s\");\n", sch_name,PrettyTmpName(s->symbol.name));
	    fprintf(file,"\tis->all_objects_(1);\n");
	    if(!strcmp(type, "USE"))
	    {
		fprintf(file,"\t%s%s->use_interface_list_()->Append(is);\n",SCHEMA_PREFIX, SCHEMAget_name(schema) );
	    } else {
		fprintf(file,"\t%s%s->ref_interface_list_()->Append(is);\n",SCHEMA_PREFIX, SCHEMAget_name(schema) );
	    }
	}
	LISTod

	if (!refdict) return;
	dict = DICTcreate(10);

	/* sort each list by schema */

	/* step 1: for each entry, store it in a schema-specific list */
	DICTdo_init(refdict,&de);
	while (0 != (r = (struct Rename *)DICTdo(&de))) {
		Linked_List list;

		list = (Linked_List)DICTlookup(dict,r->schema->symbol.name);
		if (!list) {
			list = LISTcreate();
			DICTdefine(dict,r->schema->symbol.name,list,
				(Symbol *)0,OBJ_UNKNOWN);
		}
		LISTadd(list,r);
	}

	/* step 2: for each list, print out the renames */
	DICTdo_init(dict,&de);
	while (0 != (list = (Linked_List)DICTdo(&de))) {
		int first_time = True;
		LISTdo(list,r,struct Rename *)

/*
   Interface_spec_ptr is;
   Used_item_ptr ui;
   is = new Interface_spec(const char * cur_sch_id);
   schemadescriptor->use_interface_list_()->Append(is);
   ui = new Used_item(TypeDescriptor *ld, const char *oi, const char *ni) ;
   is->_explicit_items->Append(ui);
*/

		  /* note: SCHEMAget_name(r->schema) equals r->schema->symbol.name) */
			if (first_time) {
			    fprintf(file,"\t// %s FROM %s (selected objects)\n",type,r->schema->symbol.name);
			    fprintf(file,"\tis = new Interface_spec(\"%s\",\"%s\");\n", sch_name,PrettyTmpName(r->schema->symbol.name) );
			    if(!strcmp(type, "USE"))
			    {
				fprintf(file,"\t%s%s->use_interface_list_()->Append(is);\n",SCHEMA_PREFIX, SCHEMAget_name(schema) );
			    } else {
				fprintf(file,"\t%s%s->ref_interface_list_()->Append(is);\n",SCHEMA_PREFIX, SCHEMAget_name(schema) );
			    }
			}

			if (first_time) {
				first_time = False;
			}
			if ( r->type == OBJ_TYPE ) 
			{
			    sprintf( td_name, "%s", TYPEtd_name((Type)r->object ) );
			}
			else if ( r->type == OBJ_FUNCTION ) 
			{
			    sprintf( td_name, "/* Function not implemented */ 0");
			}
			else if ( r->type == OBJ_PROCEDURE ) 
			{
			    sprintf( td_name, "/* Procedure not implemented */ 0");
			}
			else if ( r->type == OBJ_RULE ) 
			{
			    sprintf( td_name, "/* Rule not implemented */ 0");
			}
			else if ( r->type == OBJ_ENTITY ) 
			{
			    sprintf( td_name, "%s%s%s",
				    SCOPEget_name(((Entity)r->object)->superscope),
				    ENT_PREFIX, ENTITYget_name((Entity)r->object) );
			}
			else
			{
			    sprintf( td_name, "/* %c from OBJ_? in expbasic.h not implemented */ 0", r->type);
			}
			if (r->old != r->nnew) {
			    fprintf(file,"\t// object %s AS %s\n",r->old->name,
				    r->nnew->name);
			    if(!strcmp(type, "USE"))
			    {
				fprintf(file,"\tui = new Used_item(\"%s\", %s, \"%s\", \"%s\");\n", r->schema->symbol.name, td_name, r->old->name, r->nnew->name);
				fprintf(file,"\tis->explicit_items_()->Append(ui);\n");
				fprintf(file,"\t%s%s->interface_().explicit_items_()->Append(ui);\n",SCHEMA_PREFIX, SCHEMAget_name(schema) );
			    } else {
				fprintf(file,"\tri = new Referenced_item(\"%s\", %s, \"%s\", \"%s\");\n", r->schema->symbol.name, td_name, r->old->name, r->nnew->name);
				fprintf(file,"\tis->explicit_items_()->Append(ri);\n");
				fprintf(file,"\t%s%s->interface_().explicit_items_()->Append(ri);\n",SCHEMA_PREFIX, SCHEMAget_name(schema) );
			    }
			} 
			else 
			{
			    fprintf(file,"\t// object %s\n",r->old->name);
			    if(!strcmp(type, "USE"))
			    {
				fprintf(file,"\tui = new Used_item(\"%s\", %s, \"\", \"%s\");\n", r->schema->symbol.name, td_name, r->nnew->name);
				fprintf(file,"\tis->explicit_items_()->Append(ui);\n");
				fprintf(file,"\t%s%s->interface_().explicit_items_()->Append(ui);\n",SCHEMA_PREFIX, SCHEMAget_name(schema) );
			    } else {
				fprintf(file,"\tri = new Referenced_item(\"%s\", %s, \"\", \"%s\");\n", r->schema->symbol.name, td_name, r->nnew->name);
				fprintf(file,"\tis->explicit_items_()->Append(ri);\n");
				fprintf(file,"\t%s%s->interface_().explicit_items_()->Append(ri);\n",SCHEMA_PREFIX, SCHEMAget_name(schema) );
			    }
			}
		LISTod
	}
	HASHdestroy(dict);
}

const char *
IdlEntityTypeName (Type t)  
{
  static char name [BUFSIZ];
  strcpy (name, TYPE_PREFIX);
  if (TYPEget_name (t)) 
    strcpy (name, FirstToUpper (TYPEget_name (t)));
  else return TYPEget_ctype (t);
  return name; 
}

const char * 
GetAggrElemType(const Type type)
{
    Class_Of_Type class;
    Type bt;
    static char retval [BUFSIZ];

    if (isAggregateType (type)) 
    {
	bt = TYPEget_nonaggregate_base_type (type);
/*
	bt = TYPEget_body(t)->base;
*/
        if (isAggregateType(bt)) {
	    strcpy(retval,"ERROR_aggr_of_aggr");
	}

	class = TYPEget_type (bt);

	/*      case TYPE_INTEGER:	*/
	if (class == Class_Integer_Type) /* return ( "IntAggregate" );*/
	    strcpy(retval,"long");
	
	/*      case TYPE_REAL:
		case TYPE_NUMBER:	*/
	if ((class == Class_Number_Type) || ( class == Class_Real_Type))
/*	    return ( "RealAggregate" );*/
	    strcpy(retval,"double");

	/*      case TYPE_ENTITY:	*/
	if (class == Class_Entity_Type) 
	{
	    strcpy(retval,IdlEntityTypeName (bt));
	}

	
	/*      case TYPE_ENUM:		*/
        /*	case TYPE_SELECT:	*/
	if ((class == Class_Enumeration_Type) 
	    || (class == Class_Select_Type) )  {
/*
	    strcpy (retval, ClassName (TYPEget_name (bt)));
*/
	    strcpy(retval,TYPEget_ctype(bt));
	}

        /*	case TYPE_LOGICAL:	*/
	if (class == Class_Logical_Type)
	    strcpy(retval,"Logical");

        /*	case TYPE_BOOLEAN:	*/
	if (class == Class_Boolean_Type)
	    strcpy(retval,"Bool");

        /*	case TYPE_STRING:	*/
	if (class == Class_String_Type) /* return("StringAggregate");*/
	    strcpy(retval,"string");

        /*	case TYPE_BINARY:	*/
	if (class == Class_Binary_Type) /* return("BinaryAggregate");*/
	    strcpy(retval,"binary");
    }
    return retval;
}

const char *
TYPEget_idl_type (const Type t)
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

    if (isAggregateType (t))
    {
	strcpy(retval,GetAggrElemType(t));

/* ///////////////////////// */
	/*	case TYPE_ARRAY:	*/
	if (TYPEget_type (t) == Class_Array_Type) {
	    strcat (retval, "__array");
	}
	/*	case TYPE_LIST:	*/
	if (TYPEget_type (t) == Class_List_Type) {
	    strcat (retval, "__list");
	}
	/*  case TYPE_SET:	*/
	if (TYPEget_type (t) == Class_Set_Type) {
	    strcat (retval, "__set");
	}
	/*  case TYPE_BAG:	*/
	if (TYPEget_type (t) == Class_Bag_Type) {
	    strcat (retval, "__bag");
	}
	return retval;
	
/* //////////////////////// */
    }

    /*  the rest is for things that are not aggregates	*/
    
    class = TYPEget_type(t);

    /*    case TYPE_LOGICAL:	*/
    if ( class == Class_Logical_Type)
        return ("Logical"); 

    /*    case TYPE_BOOLEAN:	*/
    if (class == Class_Boolean_Type)
        return ("Boolean"); 

    /*      case TYPE_INTEGER:	*/
    if ( class == Class_Integer_Type)
	return ("SCLP23(Integer)"); 
/*	return ("CORBA::Long");*/

    /*      case TYPE_REAL:
	    case TYPE_NUMBER:	*/
    if ((class == Class_Number_Type) || ( class == Class_Real_Type))
	return ("SCLP23(Real)"); 
/*	return ("CORBA::Double"); */

    /*	    case TYPE_STRING:	*/
    if (class == Class_String_Type)
	return ("char *");

    /*	    case TYPE_BINARY:	*/
    if ( class == Class_Binary_Type)
	return (AccessType(t));

    /*      case TYPE_ENTITY:	*/
    if ( class == Class_Entity_Type)
      {
	  /* better do this because the return type might go away */
      strcpy (retval, IdlEntityTypeName(t));
      strcat (retval, "_ptr");
      return retval;
/*	return ("SCLP23(Application_instance_ptr)");    */
    }
    /*    case TYPE_ENUM:	*/
    /*    case TYPE_SELECT:	*/
    if (class == Class_Enumeration_Type)
    {
      strncpy (retval, EnumName(TYPEget_name(t)), BUFSIZ-2);

      strcat (retval, " /*");
      strcat (retval, IdlEntityTypeName(t) );
      strcat (retval, "*/ ");
/*      strcat (retval, "_var");*/
      return retval;
    }
    if (class == Class_Select_Type)  {
      return (IdlEntityTypeName (t));
    }

    /*  default returns undefined   */
    return ("SCLundefined");
}    

int Handle_FedPlus_Args(int i, char *arg)
{
    if( ((char)i == 's') || ((char)i == 'S'))
	multiple_inheritance = 0;
    if( ((char)i == 'a') || ((char)i == 'A'))
      old_accessors = 1;
    if( ((char)i == 'l') || ((char)i == 'L'))
      print_logging = 1;
    if( ((char)i == 'c') || ((char)i == 'C'))
      corba_binding = 1;
    return 0;
}

/******************************************************************
 ** Procedure:  generate_attribute_name
 ** Parameters:  Variable a, an Express attribute; char *out, the C++ name
 ** Description:  converts an Express name into the corresponding C++ name
 **		  see relation to generate_dict_attr_name() DAS
 ** Side Effects:  
 ** Status:  complete 8/5/93
 ******************************************************************/
char * 
generate_attribute_name( Variable a, char *out )
{
   char *temp, *p, *q;
   int j;

   temp = EXPRto_string( VARget_name(a) );
   p = temp;
   if (! strncmp (StrToLower (p), "self\\", 5)) 
       p = p +5;
   /*  copy p to out  */
   /* DAR - fixed so that '\n's removed */
   for ( j=0, q=out; p, j<BUFSIZ; p++ ) {
       /* copy p to out, 1 char at time.  Skip \n's, convert
       /*  '.' to '_', and convert to lowercase. */
       if ( *p != '\n' ) {
	   if ( *p == '.' ) {
	       *q = '_';
	   } else {
	       *q = tolower(*p);
	   }
	   j++;
	   q++;
       }
   }
   free( temp );
   return out;
}

char * 
generate_attribute_func_name( Variable a, char *out )
{
    generate_attribute_name( a, out  );
    strncpy (out, CheckWord (StrToLower (out)), BUFSIZ);
    if(old_accessors)
    {
	out[0] = toupper(out[0]);
    }
    else
    {
	out[strlen(out)] = '_';
    }
   return out;
}

/******************************************************************
 ** Procedure:  generate_dict_attr_name
 ** Parameters:  Variable a, an Express attribute; char *out, the C++ name
 ** Description:  converts an Express name into the corresponding SCL 
 **		  dictionary name.  The difference between this and the 
 ** 		  generate_attribute_name() function is that for derived 
 **		  attributes the name will have the form <parent>.<attr_name>
 **		  where <parent> is the name of the parent containing the 
 **		  attribute being derived and <attr_name> is the name of the
 **		  derived attribute. Both <parent> and <attr_name> may 
 **		  contain underscores but <parent> and <attr_name> will be 
 **		  separated by a period.  generate_attribute_name() generates
 **		  the same name except <parent> and <attr_name> will be 
 **		  separated by an underscore since it is illegal to have a
 **		  period in a variable name.  This function is used for the
 **		  dictionary name (a string) and generate_attribute_name()
 **		  will be used for variable and access function names.
 ** Side Effects:  
 ** Status:  complete 8/5/93
 ******************************************************************/
char * 
generate_dict_attr_name( Variable a, char *out )
{
   char *temp, *p, *q;
   int j;

   temp = EXPRto_string( VARget_name(a) );
   p = temp;
   if (! strncmp (StrToLower (p), "self\\", 5)) 
       p = p +5;
   /*  copy p to out  */
   strncpy( out, StrToLower (p), BUFSIZ );
   /* DAR - fixed so that '\n's removed */
   for ( j=0, q=out; p, j<BUFSIZ; p++ ) {
       /* copy p to out, 1 char at time.  Skip \n's, and convert to lc. */
       if ( *p != '\n' ) {
         *q = tolower(*p);
         j++;
         q++;
       }
   }

   free( temp );
   return out;
}

/******************************************************************
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

String
TYPEget_express_type (const Type t)
{      
    Class_Of_Type class;
    Type bt;
    char retval [BUFSIZ];
    char *n, * permval, * aggr_type;
    

    /*  1.  "DEFINED" types	*/
    /*    case TYPE_ENUM:	*/
    /*    case TYPE_ENTITY:	*/
    /*	case TYPE_SELECT:       */

    if (n = TYPEget_name (t)) {
      PrettyTmpName (n);
    }

    /*   2.   "BASE" types 	*/
    class = TYPEget_type (t);

    /*    case TYPE_LOGICAL:	*/
    if ((class == Class_Boolean_Type) || ( class == Class_Logical_Type))
        return ("Logical"); 

    /*      case TYPE_INTEGER:	*/
    if ( class == Class_Integer_Type)
	return ("Integer ");

    /*      case TYPE_REAL:
	    case TYPE_NUMBER:	*/
    if ((class == Class_Number_Type) || ( class == Class_Real_Type))
	return ("Real "); 

    /*	    case TYPE_STRING:	*/
    if (class == Class_String_Type)
	return ("String ")      ;

    /*	    case TYPE_BINARY:	*/
    if  ( class == Class_Binary_Type)
	return ("Binary ")      ;

    /*  AGGREGATES
    case TYPE_ARRAY:
    case TYPE_BAG:
    case TYPE_LIST:
    case TYPE_SET:
    */
    if (isAggregateType (t)) {
	bt = TYPEget_nonaggregate_base_type (t);
	class = TYPEget_type (bt);
	    
	/*	case TYPE_ARRAY:	*/
	if (TYPEget_type (t) == Class_Array_Type) {
	    aggr_type = "Array";
	}
	/*	case TYPE_LIST:	*/
	if (TYPEget_type (t) == Class_List_Type) {
	    aggr_type = "List";
	}
	/*  case TYPE_SET:	*/
	if (TYPEget_type (t) == Class_Set_Type) {
	    aggr_type = "Set";
	}
	/*  case TYPE_BAG:	*/
	if (TYPEget_type (t) == Class_Bag_Type) {
	    aggr_type = "Bag";
	}
	
	sprintf (retval, "%s of %s", 
		 aggr_type, TYPEget_express_type (bt));

	/*  this will declare extra memory when aggregate is > 1D  */
	
	permval = (char*)malloc(strlen (retval) * sizeof (char) +1);
	strcpy (permval, retval);
	return permval;
	
    }
 
    /*  default returns undefined   */

    printf ("WARNING2:  type  %s  is undefined\n", TYPEget_name (t));
    return ("SCLundefined");

}

/******************************************************************
 ** Procedure:  ATTRsign_access_method
 ** Parameters:  const Variable a --  attribute to print
                                      access method signature for 
 **	FILE* file  --  file being written to
 ** Returns:  nothing
 ** Description:  prints the signature for an access method 
 **               based on the attribute type  
 **		  DAS i.e. prints the header for the attr. access functions
 **		  (get and put attr value) in the entity class def in .h file
 ** Side Effects:  
 ** Status:  complete 17-Feb-1992
 ******************************************************************/

void
ATTRsign_access_methods (Variable a, FILE* file) 
{
    
    Type t = VARget_type (a);
    Class_Of_Type class;
    char ctype [BUFSIZ];
    char attrnm [BUFSIZ];

    generate_attribute_func_name(a, attrnm);

/*
    generate_attribute_name( a, attrnm  );
    strncpy (attrnm, CheckWord (StrToLower (attrnm)), BUFSIZ);
    if(old_accessors)
    {
	attrnm[0] = toupper(attrnm[0]);
    }
    else
    {
	attrnm[strlen(attrnm)] = '_';
    }
*/

    class = TYPEget_type(t);

    if(corba_binding)
      strncpy (ctype, TYPEget_idl_type(t), BUFSIZ);
    else
      strncpy (ctype, AccessType (t), BUFSIZ);
    if(corba_binding)
    {
/* string, entity, and aggregate = no const */
	if( isAggregateType(t) )
	{
	    fprintf (file, "\t%s * %s(", ctype, attrnm);
	}
	else
	{
	    fprintf (file, "\t%s %s(", ctype, attrnm);
	}
	fprintf (file, 
		 "CORBA::Environment &IT_env=CORBA::default_environment) ");
	fprintf (file, 
		 " /* const */ throw (CORBA::SystemException);\n");
	if( (class == Class_Enumeration_Type) ||
	   (class == Class_Entity_Type) ||
	   (class == Class_Boolean_Type) ||
	   (class == Class_Logical_Type) )
	{
	    fprintf (file, "\tvoid %s (%s x", attrnm, ctype );
	}
	else if( isAggregateType(t) )
	{
	    fprintf (file, "\tvoid %s (const %s& x", attrnm, ctype );
	}
	else
	{
	    fprintf (file, "\tvoid %s (const %s x", attrnm, ctype );
	}
	fprintf (file, 
		 ", CORBA::Environment &IT_env=CORBA::default_environment)");
	fprintf (file, 
		 " throw (CORBA::SystemException);\n\n");
    }
    else
    {
	fprintf (file, "\tconst %s %s() const;\n", ctype, attrnm);
	fprintf (file, "\tvoid %s (const %s x);\n\n", attrnm, ctype);
    }
    return;

#if 0
    /*  LOGICAL and ENUM use an enumerated value in 
	the put and the get function  */
    /*    case TYPE_LOGICAL:	*/
    if (class == Class_Logical_Type)  {
	if(corba_binding)
	{
	    fprintf (file, "\tconst Logical %s(", attrnm);
	    fprintf (file, 
		    "CORBA::Environment &IT_env=CORBA::default_environment) ");
	    fprintf (file, 
		     " const throw (CORBA::SystemException);\n");
	    fprintf (file, "\tvoid %s (Logical x", attrnm  );
	    fprintf (file, 
		   ", CORBA::Environment &IT_env=CORBA::default_environment)");
	    fprintf (file, 
		     " throw (CORBA::SystemException);\n\n");
	}
	else
	{
	    fprintf (file, "\tconst Logical %s() const;\n", attrnm);
	    fprintf (file, "\tvoid %s (Logical x);\n\n", attrnm);
	}
	return;
    }    
    /*    case TYPE_BOOLEAN:	*/
    if (class == Class_Boolean_Type)  {
	if(corba_binding)
	{
	    fprintf (file, "\tconst Boolean %s(", attrnm);
	    fprintf (file, 
		    "CORBA::Environment &IT_env=CORBA::default_environment) ");
	    fprintf (file, 
		     "const throw (CORBA::SystemException);\n");
	    fprintf (file, "\tvoid %s (Boolean x", attrnm  );
	    fprintf (file, 
		   ", CORBA::Environment &IT_env=CORBA::default_environment)");
	    fprintf (file, 
		     " throw (CORBA::SystemException);\n\n");
	}
	else
	{
	    fprintf (file, "\tconst Boolean %s() const;\n", attrnm);
	    fprintf (file, "\tvoid %s (Boolean x);\n\n", attrnm  );
	}
	return;
    }    
    /*    case TYPE_ENUM:	*/
    if ( class == Class_Enumeration_Type)  {
	fprintf (file, "\tconst %s %s(", 
		 EnumName (TYPEget_name (t)), attrnm);
	if(corba_binding)
	{
	    fprintf (file, 
		    "CORBA::Environment &IT_env=CORBA::default_environment) ");
	    fprintf (file, 
		     " const throw (CORBA::SystemException);\n");
	}
	else
	  fprintf (file, ") const;\n");
	fprintf (file, "\tvoid %s (%s x", 
		 attrnm, EnumName (TYPEget_name (t)) );
	if(corba_binding)
	{
	    fprintf (file, 
		   ", CORBA::Environment &IT_env=CORBA::default_environment)");
	    fprintf (file, 
		     " throw (CORBA::SystemException");
	}
	fprintf (file, ");\n\n");
	return;
    }

    /*  case STRING  */
    if ( class == Class_String_Type) {
	if(corba_binding)
	{
	    fprintf (file, "\tchar * %s(", attrnm);
	    fprintf (file, 
		    "CORBA::Environment &IT_env=CORBA::default_environment) ");
	    fprintf (file, 
		     " const throw (CORBA::SystemException);\n");
	}
	else
	{
	    fprintf (file, "\tconst %s %s() const;\n", ctype, attrnm);
	}
	fprintf (file, "\tvoid %s (const char * x", attrnm );
	if(corba_binding)
	{
	    fprintf (file, ", CORBA::Environment &IT_env=CORBA::default_environment) throw (CORBA::SystemException");
	}
	fprintf (file, ");\n\n");

	return;
    }    
    /*	    case TYPE_BINARY:	*/
    if ( class == Class_Binary_Type) {
	fprintf (file, "\tconst %s %s(", ctype, attrnm);
	if(corba_binding)
	{
	    fprintf (file, 
		     "CORBA::Environment &IT_env=CORBA::default_environment)");
	    fprintf (file, 
		     " const throw (CORBA::SystemException);\n");
        }
	else
	  fprintf (file, ") const;\n");
/*DASSTR      fprintf (file, "\tconst %s& %s() const;\n", ctype, attrnm); */
	fprintf (file, "\tvoid %s (const %s& x", attrnm, ctype );
	if(corba_binding)
	  fprintf (file, ", CORBA::Environment &IT_env=CORBA::default_environment) throw (CORBA::SystemException");
	fprintf (file, ");\n\n");
      return;
    }    

    /*      case TYPE_ENTITY:	*/
    if (class == Class_Entity_Type)  {
      /*  get method doesn\'t return const  */
	if(corba_binding)
	{
	    fprintf (file, "\t%s_ptr %s(", IdlEntityTypeName(t), attrnm);
	    fprintf (file, "CORBA::Environment &IT_env=CORBA::default_environment) const throw (CORBA::SystemException);\n");
	}
	else
	{
	    fprintf (file, "\t%s %s(", ctype, attrnm);
	    fprintf (file, ") const;\n");
	}
	if(corba_binding)
	{
	    fprintf (file, "\tvoid %s (%s_ptr x", attrnm, IdlEntityTypeName(t) );
	    fprintf (file, ", CORBA::Environment &IT_env=CORBA::default_environment) throw (CORBA::SystemException");
	}
	else
	{
	    fprintf (file, "\tvoid %s (%s x", attrnm, ctype );
	}
	fprintf (file, ");\n\n");
      return;
    }

    /*      case TYPE_AGGREGATE:	*/
    if (isAggregateType (t)) {
	/*    if (class == Class_Array_Type) */
      /*  get method doesn\'t return const  */
	if(corba_binding)
	{
	    fprintf (file, "\t%s* %s(", TYPEget_idl_type(t), attrnm);
	    fprintf (file, "CORBA::Environment &IT_env=CORBA::default_environment) throw (CORBA::SystemException);\n");
	}
	else
	{
	    fprintf (file, "\tconst %s %s(", ctype, attrnm);
	    fprintf (file, ") const;\n");
	}
	if(corba_binding)
	{
	    fprintf (file, "\tvoid %s(const %s& x", attrnm, 
		     TYPEget_idl_type(t) );
	    fprintf (file, ", CORBA::Environment &IT_env=CORBA::default_environment) throw (CORBA::SystemException");
	}
	else
	{
	    fprintf (file, "\tvoid %s (%s x", attrnm, ctype );
	}
	fprintf (file, ");\n\n");
      return;
    }

    if (class == Class_Integer_Type)  {
      /*  get method doesn\'t return const  */
	if(corba_binding)
	{
	    fprintf (file, "\tconst %s %s(", "CORBA::Long", attrnm);
	    fprintf (file, "CORBA::Environment &IT_env=CORBA::default_environment) throw (CORBA::SystemException);\n");
	}
	else
	{
	    fprintf (file, "\tconst %s %s(", ctype, attrnm);
	    fprintf (file, ") const;\n");
	}
	if(corba_binding)
	{
	    fprintf (file, "\tvoid %s (const %s x", attrnm, "CORBA::Long" );
	    fprintf (file, ", CORBA::Environment &IT_env=CORBA::default_environment) throw (CORBA::SystemException");
	}
	else
	{
	    fprintf (file, "\tvoid %s (%s x", attrnm, ctype );
	}
	fprintf (file, ");\n\n");
      return;
    }
    /*  default:  INTEGER, and NUMBER	*/
    /*    case TYPE_SELECT:	*/
    /*    case TYPE_AGGRETATES:	*/
    /*      case TYPE_ENTITY:	*/
      /*  is the same type as the data member  */
      fprintf (file, "\tconst %s %s(", ctype, attrnm);
	if(corba_binding)
	  fprintf (file, "CORBA::Environment &IT_env=CORBA::default_environment) const throw (CORBA::SystemException);\n");
	else
	  fprintf (file, ") const;\n");
	fprintf (file, "\tvoid %s (%s x", attrnm, ctype );
	if(corba_binding)
	  fprintf (file, ", CORBA::Environment &IT_env=CORBA::default_environment) throw (CORBA::SystemException");
	fprintf (file, ");\n\n");

#endif

}    

/******************************************************************
 ** Procedure:  ATTRprint_access_methods_get_head
 ** Parameters:  const Variable a --  attribute to find the type for
 **	FILE* file  --  file being written 
 **	Type t - type of the attribute
 **	Class_Of_Type class -- type name of the class
 **	const char *attrnm -- name of the attribute
 **	char *ctype -- (possibly returned) name of the attribute c++ type
 ** Returns:  name to be used for the type of the c++ access functions
 ** Description:  prints the access method get head based on the attribute type
 **		DAS which being translated is it prints the function header 
 **		for the get attr value access function defined for an 
 **		entity class. This is the .cc file version.
 ** Side Effects:  
 ** Status:  complete 7/15/93		by DDH
 ******************************************************************/
void
ATTRprint_access_methods_get_head (const char * classnm, Variable a,
				   FILE* file) 
{
    Type t = VARget_type (a);
    Class_Of_Type class = TYPEget_type(t);
    char ctype [BUFSIZ];   /*  return type of the get function  */
    char funcnm [BUFSIZ];  /*  name of member function  */

    generate_attribute_func_name(a, funcnm);
/*
    generate_attribute_name( a, funcnm  );
    funcnm[0] = toupper(funcnm[0]);
*/

/* ///////////////////////////////////////////////// */

    if(corba_binding)
      strncpy (ctype, TYPEget_idl_type(t), BUFSIZ);
    else
      strncpy (ctype, AccessType (t), BUFSIZ);
    if(corba_binding)
    {
/* string, entity, and aggregate = no const */
	if( isAggregateType(t) )
	{
	    fprintf (file, "\n%s * \n%s::%s(", ctype, classnm, funcnm);
	}
	else
	{
	    fprintf (file, "\n%s \n%s::%s(", ctype, classnm, funcnm);
	}
	fprintf (file, 
		 "CORBA::Environment &IT_env) ");
	fprintf (file, 
		 " /* const */ throw (CORBA::SystemException)\n");
    }
    else
    {
	fprintf (file, "\nconst %s \n%s::%s() const\n", ctype, classnm, funcnm);
    }
    return;
/* ///////////////////////////////////////////////// */
#if 0
    /*  case STRING:	*/
    /*	    case TYPE_BINARY:	*/
    /*  string can\'t be const because it causes problems with SELECTs  */
    if (( class == Class_String_Type) ||   ( class == Class_Binary_Type)) {
	if(corba_binding)
	{
	    fprintf (file, "\nchar * \n%s::%s(", classnm, funcnm);
	    fprintf (file, 
		    "CORBA::Environment &IT_env) const ");
	    fprintf (file, 
		     "throw (CORBA::SystemException)\n");
	}
	else
	{
	    fprintf (file, 
		     "\nconst %s\n%s::%s() const\n", ctype, classnm, funcnm);
	}
	return;
    }
    /*  case ENTITY:  */
    /*  return value isn\'t const  */
    if ( class == Class_Entity_Type) {
	if(corba_binding)
	{
	    fprintf (file, "\n%s\n%s::%s(", ctype, classnm, funcnm);
	    fprintf (file, 
		     "CORBA::Environment &IT_env) /* const */ ");
	    fprintf (file, 
		     "throw (CORBA::SystemException)\n");
	}
	else
	  fprintf (file, "\n%s\n%s::%s() const \n", ctype, classnm, funcnm);
	return;
    }

    /*    case TYPE_LOGICAL:	*/
    if (class == Class_Logical_Type)  {
      if(corba_binding)
      {
	  fprintf (file, "\nconst Logical\n%s::%s(", classnm, funcnm);
	  fprintf (file, 
	      "CORBA::Environment &IT_env) const throw (CORBA::SystemException)\n");
      }
      else
	fprintf (file, 
		 "\nconst Logical \n%s::%s() const\n", classnm, funcnm);
	return;
    }    

    /*    case TYPE_BOOLEAN:	*/
    if (class == Class_Boolean_Type)  {
      if(corba_binding)
      {
	  fprintf (file, "\nconst Boolean\n%s::%s(", classnm, funcnm);
	  fprintf (file, 
	 "CORBA::Environment &IT_env) const throw (CORBA::SystemException)\n");
      }
      else
	fprintf (file, 
		 "\nconst Boolean\n%s::%s() const\n", classnm, funcnm);
	return;
    }    

    /*    case TYPE_ENUM:	*/
    if ( class == Class_Enumeration_Type) 
      sprintf  (ctype, "%s ", EnumName (TYPEget_name (t)));


    /*  default:  INTEGER and NUMBER	*/
    /*    case TYPE_AGGRETATES:	*/
    /*    case TYPE_SELECT:	*/
    /*      case TYPE_ENTITY:	*/
      /*  is the same type as the data member  */
    fprintf (file, "\nconst %s\n%s::%s(", ctype, classnm, funcnm);
    if(corba_binding)
	fprintf (file, 
	 "CORBA::Environment &IT_env) const throw (CORBA::SystemException)\n");
    else
      fprintf (file, ") const\n");
#endif
}

/******************************************************************
 ** Procedure:  ATTRprint_access_methods_put_head
 ** Parameters:  const Variable a --  attribute to find the type for
 **	FILE* file  --  file being written to
 **	Type t - type of the attribute
 **	Class_Of_Type class -- type name of the class
 **	const char *attrnm -- name of the attribute
 **	char *ctype -- name of the attribute c++ type
 ** Returns:  name to be used for the type of the c++ access functions
 ** Description:  prints the access method put head based on the attribute type
 **		DAS which being translated is it prints the function header 
 **		for the put attr value access function defined for an 
 **		entity class. This is the .cc file version.
 ** Side Effects:  
 ** Status:  complete 7/15/93		by DDH
 ******************************************************************/
void
ATTRprint_access_methods_put_head  (CONST char * entnm, Variable a, FILE* file)
{

  Type t = VARget_type (a);
  char ctype [BUFSIZ];
  char funcnm [BUFSIZ];
  Class_Of_Type class = TYPEget_type (t);

    generate_attribute_func_name(a, funcnm);
/*
    generate_attribute_name( a, funcnm  );
    funcnm[0] = toupper(funcnm[0]);
*/

/* ///////////////////////////////////////////////// */

    if(corba_binding)
      strncpy (ctype, TYPEget_idl_type(t), BUFSIZ);
    else
      strncpy (ctype, AccessType (t), BUFSIZ);
    if(corba_binding)
    {
/* string, entity, and aggregate = no const */
	if( (class == Class_Enumeration_Type) ||
	   (class == Class_Entity_Type) ||
	   (class == Class_Boolean_Type) ||
	   (class == Class_Logical_Type) )
	{
	    fprintf (file, "\nvoid \n%s::%s (%s x", entnm, funcnm, ctype );
	}
	else if( isAggregateType(t) )
	{
	    fprintf (file, "\nvoid \n%s::%s (const %s& x", entnm, funcnm, ctype );
	}
	else
	{
	    fprintf (file, "\nvoid \n%s::%s (const %s x", entnm, funcnm, ctype );
	}
	fprintf (file, 
		 ", CORBA::Environment &IT_env)");
	fprintf (file, 
		 " throw (CORBA::SystemException)\n\n");
    }
    else
    {
	fprintf (file, "\nvoid \n%s::%s (const %s x)\n\n", entnm, funcnm, ctype);
    }
    return;
/* ///////////////////////////////////////////////// */
#if 0

  /*    case TYPE_LOGICAL:	*/
  if (class == Class_Logical_Type)
    strcpy (ctype, "Logical");

  /*    case TYPE_BOOLEAN:	*/
  if (class == Class_Boolean_Type)
    strcpy (ctype, "Boolean");

    /*    case TYPE_ENUM:	*/
  if ( class == Class_Enumeration_Type)  
    strncpy (ctype, EnumName (TYPEget_name (t)), BUFSIZ);


  /*  case STRING:*/
  if ( class == Class_String_Type)  
    strcpy (ctype, "const char *");

  /*	    case TYPE_BINARY:	*/
  if ( class == Class_Binary_Type)
    sprintf (ctype, "const %s&", AccessType (t));

  /*  default:  INTEGER and NUMBER	*/
  /*    case TYPE_AGGRETATES:	*/
  /*      case TYPE_ENTITY:	*/
  /*    case TYPE_SELECT:	*/
  /*  is the same type as the data member  */
  fprintf (file, "\nvoid \n%s::%s (%s x", entnm, funcnm, ctype );
  if(corba_binding)
	fprintf (file, ", CORBA::Environment &IT_env) \nthrow (CORBA::SystemException");
      fprintf (file, ")\n");

#endif

}

void 
AGGRprint_access_methods(CONST char *entnm, Variable a, FILE* file, Type t, 
			 char *ctype, char *attrnm)
{
    char aggrnode_name [BUFSIZ];
    Type bt;
    Class_Of_Type class = TYPEget_type (t);
    char nm [BUFSIZ];

    ATTRprint_access_methods_get_head( entnm, a, file);
    fprintf (file, "{\n");
    if(!corba_binding)
    {
	fprintf (file, "    return (%s) &_%s; \n}\n", ctype, attrnm);
	ATTRprint_access_methods_put_head( entnm, a, file);
	fprintf (file, "\t{ _%s.ShallowCopy (*x); }\n", attrnm  );
	return;
    }
    else
    {
	bt = TYPEget_nonaggregate_base_type (t);
	if (isAggregateType(bt)) {
	    strcpy(aggrnode_name,"/* ERROR aggr_of_aggr */");
	}

	fprintf (file, "    %s * seq = new %s;\n\n", ctype, ctype);
/*           Part_version__set * seq = new Part_version__set;*/

	fprintf (file, "    int count = _%s.EntryCount();\n", attrnm);
	fprintf (file, "    seq->length(count);\n\n");

	fprintf (file, "    int i = 0;\n");
	fprintf (file, "    SingleLinkNode *n = _%s.GetHead();\n\n", attrnm);


	class = TYPEget_type (bt);

	if (class == Class_Integer_Type)
	{
/*	  strcpy(aggrnode_name,"IntNode"); */
	    fprintf (file, "    while(n)\n");
	    fprintf (file, "    {\n");

	    fprintf (file, "\t(*seq)[i] = ((IntNode*)n)->value;\n");
	    fprintf (file, "\tstd::cout << \"returning entity %s, attr _%s: aggr integer element: \" << ((IntNode*)n)->value << std::endl;\n", entnm, attrnm);
/* /////////////////////////////////////////// */
	    if(print_logging)
	    {
		fprintf (file, "#ifdef SCL_LOGGING\n");
		fprintf (file, "\tif(*logStream)\n\t{\n");
		fprintf (file, 
			 "\t    logStream->open(SCLLOGFILE,ios::app);\n");
		fprintf (file, "\t    *logStream << time(NULL) << \" SDAI returning entity: %s, attr: _%s, aggr integer element: \" << ((IntNode*)n)->value << std::endl;\n", entnm, attrnm);
		fprintf (file, "\t    logStream->close();\n");
		fprintf (file, "\t}\n");
		fprintf (file, "#endif\n");
	    }
/* /////////////////////////////////////////// */

	    fprintf (file, "\ti++;\n");
	    fprintf (file, "\tn = n->NextNode();\n");
	    fprintf (file, "    }\n");
	    fprintf (file, "    return seq;\n");
	    fprintf (file, "}\n");
	}

	if ((class == Class_Number_Type) || ( class == Class_Real_Type))
	{
/*	  strcpy(aggrnode_name,"RealNode"); */
	    fprintf (file, "    while(n)\n");
	    fprintf (file, "    {\n");

	    fprintf (file, "\t(*seq)[i] = ((RealNode*)n)->value;\n");
	    fprintf (file, "\tstd::cout << \"returning entity %s, attr _%s: aggr real element: \" << ((RealNode*)n)->value << std::endl;\n", entnm, attrnm);
/* /////////////////////////////////////////// */
	    if(print_logging)
	    {
		fprintf (file, "#ifdef SCL_LOGGING\n");
		fprintf (file, "\tif(*logStream)\n\t{\n");
		fprintf (file, 
			 "\t    logStream->open(SCLLOGFILE,ios::app);\n");

		fprintf (file, "\t    *logStream << time(NULL) << \" SDAI returning entity: %s, attr: _%s, aggr real element: \" << ((RealNode*)n)->value << std::endl;\n", entnm, attrnm);
		fprintf (file, "\t    logStream->close();\n");
		fprintf (file, "\t}\n");
		fprintf (file, "#endif\n");
	    }
/* /////////////////////////////////////////// */

	    fprintf (file, "\ti++;\n");
	    fprintf (file, "\tn = n->NextNode();\n");
	    fprintf (file, "    }\n");
	    fprintf (file, "    return seq;\n");
	    fprintf (file, "}\n");
	}

	if (class == Class_Entity_Type) 
	{
/*	    strcpy(aggrnode_name,"EntityNode");*/
	    fprintf (file, "    int file_id = 0;\n");
	    fprintf (file, "    char markerServer[BUFSIZ];\n");
	    fprintf (file, "    while(n)\n");
	    fprintf (file, "    {\n");
	    fprintf (file, "\tfile_id = ((EntityNode*)n)->node->STEPfile_id;\n");
	    fprintf (file, "\tstd::cout << \"StepFileId: \" << file_id;\n");

	    fprintf (file, "\t// the marker:server is used\n");
	    fprintf (file, "\tsprintf(markerServer, \"%%d:%%s\", file_id, serverName);\n");
	    fprintf (file, "\tstd::cout << \" markerServer: \" << markerServer << std::endl;\n");
/* /////////////////////////////////////////// */
	    if(print_logging)
	    {
		fprintf (file, "#ifdef SCL_LOGGING\n");
		fprintf (file, "\tif(*logStream)\n\t{\n");

		fprintf (file, 
			 "\t    logStream->open(SCLLOGFILE,ios::app);\n");
		fprintf (file, "\t    *logStream << time(NULL) << \" SDAI returning entity: %s, attr: _%s, aggr entity element w/file_id: \" << file_id << \" markerServer: \" << markerServer << std::endl;\n", entnm, attrnm);
		fprintf (file, "\t    logStream->close();\n");
		fprintf (file, "\t}\n");
		fprintf (file, "#endif\n");
	    }
/* /////////////////////////////////////////// */

	    if (TYPEget_name (bt)) 
	      strncpy (nm, FirstToUpper (TYPEget_name (bt)), BUFSIZ-1);
	    fprintf (file, "\t(*seq)[i] = %s::_bind(markerServer, sclHostName);\n", nm);
	    fprintf (file, 
		     "/*\n\t%s_var x = %s::_bind((const char *)markerServer,"
		     "sclHostName);\n", nm, nm);
	    fprintf (file, 
		     "\t%s::_duplicate(x);\n\n", nm);
	    fprintf (file, "\t(*seq)[i] = x;\n*/\n");

	    fprintf (file, "\ti++;\n");
	    fprintf (file, "\tn = n->NextNode();\n");
	    fprintf (file, "    }\n");
	    fprintf (file, "    return seq;\n");
	    fprintf (file, "}\n");
	    /* ////////////////////////////////////////////////// */
	}

	if( (class == Class_Enumeration_Type) || 
	   (class == Class_Logical_Type) || 
	   (class == Class_Boolean_Type) )
	{
/*	    strcpy(aggrnode_name,"EnumNode"); */
	    fprintf (file, "    while(n)\n");
	    fprintf (file, "    {\n");

	    fprintf (file, "\t(*seq)[i] = ((EnumNode*)n)->node->asInt();\n");
	    fprintf (file, "\tstd::cout << \"returning entity %s, attr _%s: aggr enumeration/Boolean/Logical element: \" << ((EnumNode*)n)->node->element_at( ((EnumNode*)n)->node->asInt() ) << std::endl;\n", entnm, attrnm);
/* /////////////////////////////////////////// */
	    if(print_logging)
	    {
		fprintf (file, "#ifdef SCL_LOGGING\n");
		fprintf (file, "\tif(*logStream)\n\t{\n");
		fprintf (file, 
			 "\t    logStream->open(SCLLOGFILE,ios::app);\n");
		fprintf (file, "\t    *logStream << time(NULL) << \" SDAI returning entity: %s, attr: _%s, aggr enumeration/Boolean/Logical element: \" << ((EnumNode*)n)->node->element_at( ((EnumNode*)n)->node->asInt() ) << std::endl;\n", entnm, attrnm);
		fprintf (file, "\t    logStream->close();\n");
		fprintf (file, "\t}\n");
		fprintf (file, "#endif\n");
	    }
/* /////////////////////////////////////////// */

	    fprintf (file, "\ti++;\n");
	    fprintf (file, "\tn = n->NextNode();\n");
	    fprintf (file, "    }\n");
	    fprintf (file, "    return seq;\n");
	    fprintf (file, "}\n");
	}

	if (class == Class_Select_Type)
	{
	    strcpy(aggrnode_name,"SelectNode");
	    fprintf (file, "    std::cout << \"ERROR function not implemented: entity %s, attr _%s: aggr select element: \" << std::endl;\n", entnm, attrnm);
	    fprintf (file, "    return 0;\n");
	    fprintf (file, "}\n");
	}

	if (class == Class_String_Type) /* return("StringAggregate");*/
	{
/*	    strcpy(aggrnode_name,"StringNode");*/
	    fprintf (file, "    while(n)\n");
	    fprintf (file, "    {\n");

	    fprintf (file, "\t(*seq)[i] = CORBA::string_dupl( ((StringNode*)n)->value.chars() );\n");
	    fprintf (file, "\tstd::cout << \"returning entity %s, attr _%s: aggr string element: \" << ((StringNode*)n)->value.chars() << std::endl;\n", entnm, attrnm);
/* /////////////////////////////////////////// */
	    if(print_logging)
	    {
		fprintf (file, "#ifdef SCL_LOGGING\n");
		fprintf (file, "\tif(*logStream)\n\t{\n");
		fprintf (file, 
			 "\t    logStream->open(SCLLOGFILE,ios::app);\n");
		fprintf (file, "\t    *logStream << time(NULL) << \" SDAI returning entity: %s, attr: _%s, aggr string element: \" << ((StringNode*)n)->value.chars() << std::endl;\n", entnm, attrnm);
		fprintf (file, "\t    logStream->close();\n");
		fprintf (file, "\t}\n");
		fprintf (file, "#endif\n");
	    }
/* /////////////////////////////////////////// */

	    fprintf (file, "\ti++;\n");
	    fprintf (file, "\tn = n->NextNode();\n");
	    fprintf (file, "    }\n");
	    fprintf (file, "    return seq;\n");
	    fprintf (file, "}\n");
	}
	if (class == Class_Binary_Type) /* return("BinaryAggregate");*/
	{
	    strcpy(aggrnode_name,"BinaryNode");
	    fprintf (file, "    std::cout << \"ERROR function not implemented: entity %s, attr _%s: aggr binary element: \" << std::endl;\n", entnm, attrnm);
	    fprintf (file, "    return 0;\n");
	    fprintf (file, "}\n");
	}
/* ////////////////////////////////////// */
	ATTRprint_access_methods_put_head( entnm, a, file);

	bt = TYPEget_nonaggregate_base_type (t);
	if (isAggregateType(bt)) {
	    strcpy(aggrnode_name,"/* ERROR aggr_of_aggr */");
	}

	class = TYPEget_type (bt);

	if (class == Class_Integer_Type)
	{
/* ************************************** */
	    if (TYPEget_name (bt)) 
	    {
		strcpy (nm, "Sdai");
		strcat (nm, FirstToUpper (TYPEget_name (bt)));
	    }

	    fprintf (file, "\t/* { _%s.ShallowCopy (*x); } */\n", attrnm  );
	    fprintf (file, "{\n");
	    fprintf (file, "    int countx = x.length();\n");
	    fprintf (file, "    SingleLinkNode *trailn = 0;\n");
	    fprintf (file, "    SingleLinkNode *n = _%s.GetHead();\n", attrnm);
	    fprintf (file, "    if( countx == 0 )\n");
	    fprintf (file, "    {\n");
	    fprintf (file, "\t_%s.Empty();\n", attrnm);
	    fprintf (file, "\treturn;\n");
	    fprintf (file, "    }\n\n");
	    fprintf (file, "    int i = 0;\n");
	    fprintf (file, "    while(i < countx)\n");
	    fprintf (file, "    {\n");
	    fprintf (file, "\tif(n == 0)\n");
	    fprintf (file, "\t{\n");
	    fprintf (file, "\t    n = _%s.NewNode();\n", attrnm);
	    fprintf (file, "\t    _%s.AppendNode( (IntNode*)n );\n", attrnm);
	    fprintf (file, "\t}\n");
	    fprintf (file, "\t((IntNode*)n)->value = x[i];\n", nm);
	    fprintf (file, "\tstd::cout << \"Assigning aggr int element: \" << ((IntNode*)n)->value;\n");
/* /////////////////////////////////////////// */
	    if(print_logging)
	    {
		fprintf (file, "#ifdef SCL_LOGGING\n");
		fprintf (file, "\tif(*logStream)\n\t{\n");
		fprintf (file, 
			 "\t    logStream->open(SCLLOGFILE,ios::app);\n");
		fprintf (file, "\t    *logStream << time(NULL) << \" SDAI assigning entity: %s, attr: _%s, aggr integer element: \" << ((IntNode*)n)->value << std::endl;\n", entnm, attrnm);

		fprintf (file, "\t    logStream->close();\n");
		fprintf (file, "\t}\n");
		fprintf (file, "#endif\n");
	    }
/* /////////////////////////////////////////// */

	    fprintf (file, "\ti++;\n");
	    fprintf (file, "\ttrailn = n;\n");
	    fprintf (file, "\tn = n->NextNode();\n");
	    fprintf (file, "    }\n");
	    fprintf (file, "/*\n");
	    fprintf (file, "    if(n)\n");
	    fprintf (file, "\t_%s.DeleteFollowingNodes((IntNode*)trailn);\n", attrnm);
	    fprintf (file, "*/\n");
	    fprintf (file, "}\n");
/* ************************************** */
	}

	if ((class == Class_Number_Type) || ( class == Class_Real_Type))
	{
/* ************************************** */
	    if (TYPEget_name (bt)) 
	    {
		strcpy (nm, "Sdai");
		strcat (nm, FirstToUpper (TYPEget_name (bt)));
	    }

	    fprintf (file, "\t/* { _%s.ShallowCopy (*x); } */\n", attrnm  );
	    fprintf (file, "{\n");
	    fprintf (file, "    int countx = x.length();\n");
	    fprintf (file, "    SingleLinkNode *trailn = 0;\n");
	    fprintf (file, "    SingleLinkNode *n = _%s.GetHead();\n", attrnm);
	    fprintf (file, "    if( countx == 0 )\n");
	    fprintf (file, "    {\n");
	    fprintf (file, "\t_%s.Empty();\n", attrnm);
	    fprintf (file, "\treturn;\n");
	    fprintf (file, "    }\n\n");
	    fprintf (file, "    int i = 0;\n");
	    fprintf (file, "    while(i < countx)\n");
	    fprintf (file, "    {\n");
	    fprintf (file, "\tif(n == 0)\n");
	    fprintf (file, "\t{\n");
	    fprintf (file, "\t    n = _%s.NewNode();\n", attrnm);
	    fprintf (file, "\t    _%s.AppendNode( (RealNode*)n );\n", attrnm);
	    fprintf (file, "\t}\n");
	    fprintf (file, "\t((RealNode*)n)->value = x[i];\n", nm);
	    fprintf (file, "\tstd::cout << \"Assigning aggr real element: \" << ((RealNode*)n)->value;\n");
/* /////////////////////////////////////////// */
	    if(print_logging)
	    {
		fprintf (file, "#ifdef SCL_LOGGING\n");
		fprintf (file, "\tif(*logStream)\n\t{\n");

		fprintf (file, 
			 "\t    logStream->open(SCLLOGFILE,ios::app);\n");
		fprintf (file, "\t    *logStream << time(NULL) << \" SDAI assigning entity: %s, attr: _%s, aggr real element: \" << ((RealNode*)n)->value << std::endl;\n", entnm, attrnm);
		fprintf (file, "\t    logStream->close();\n");
		fprintf (file, "\t}\n");
		fprintf (file, "#endif\n");
	    }
/* /////////////////////////////////////////// */
	    fprintf (file, "\ti++;\n");
	    fprintf (file, "\ttrailn = n;\n");
	    fprintf (file, "\tn = n->NextNode();\n");
	    fprintf (file, "    }\n");
	    fprintf (file, "/*\n");
	    fprintf (file, "    if(n)\n");
	    fprintf (file, "\t_%s.DeleteFollowingNodes((RealNode*)trailn);\n", attrnm);
	    fprintf (file, "*/\n");
	    fprintf (file, "}\n");
/* ************************************** */
	}

	if (class == Class_Entity_Type) 
	{
/*	    strcpy(nm, TYPEget_name(bt));
	    strcpy(nm, TYPEget_name
		    (TYPEget_nonaggregate_base_type (VARget_type (a))));
*/
/* ************************************** */
	    if (TYPEget_name (bt)) 
	    {
		strcpy (nm, "Sdai");
		strcat (nm, FirstToUpper (TYPEget_name (bt)));
	    }

	    fprintf (file, "\t/* { _%s.ShallowCopy (*x); } */\n", attrnm  );
	    fprintf (file, "{\n");
	    fprintf (file, "    int countx = x.length();\n");
	    fprintf (file, "    SingleLinkNode *trailn = 0;\n");
	    fprintf (file, "    SingleLinkNode *n = _%s.GetHead();\n", attrnm);
	    fprintf (file, "    if( countx > 0 )\n");
	    fprintf (file, "    {\n");
	    fprintf (file, "\tif( n == 0 )\n");
	    fprintf (file, "\t{\n");
	    fprintf (file, "\t    n = _%s.NewNode();\n", attrnm);
	    fprintf (file, "\t    _%s.AppendNode( (EntityNode*)n );\n", attrnm);
	    fprintf (file, "\t}\n");
	    fprintf (file, "    }\n");
	    fprintf (file, "    else\n");
	    fprintf (file, "    {\n");
	    fprintf (file, "\t_%s.Empty();\n", attrnm);
	    fprintf (file, "\treturn;\n");
	    fprintf (file, "    }\n\n");
	    fprintf (file, "    int i = 0;\n");
	    fprintf (file, "    while(i < countx)\n");
	    fprintf (file, "    {\n");
	    fprintf (file, "\tif(n == 0)\n");
	    fprintf (file, "\t{\n");
	    fprintf (file, "\t    n = _%s.NewNode();\n", attrnm);
	    fprintf (file, "\t    _%s.AppendNode( (EntityNode*)n );\n", attrnm);
	    fprintf (file, "\t}\n");
	    fprintf (file, "\t((EntityNode*)n)->node = (%s*)DEREF( x[i] );\n", nm);
	    fprintf (file, "\tstd::cout << \"Assigning entity w/StepFileId: \" << ((EntityNode*)n)->node->STEPfile_id;\n");
/* /////////////////////////////////////////// */
	    if(print_logging)
	    {
		fprintf (file, "#ifdef SCL_LOGGING\n");
		fprintf (file, "\tif(*logStream)\n\t{\n");

		fprintf (file, 
			 "\t    logStream->open(SCLLOGFILE,ios::app);\n");
		fprintf (file, "\t    *logStream << time(NULL) << \" SDAI assigning entity: %s, attr: _%s, aggr entity element w/file_id: \" << ((EntityNode*)n)->node->STEPfile_id << std::endl;\n", entnm, attrnm);
		fprintf (file, "\t    logStream->close();\n");
		fprintf (file, "\t}\n");
		fprintf (file, "#endif\n");
	    }
/* /////////////////////////////////////////// */

	    fprintf (file, "\ti++;\n");
	    fprintf (file, "\ttrailn = n;\n");
	    fprintf (file, "\tn = n->NextNode();\n");
	    fprintf (file, "    }\n");
	    fprintf (file, "/*\n");
	    fprintf (file, "    if(n)\n");
	    fprintf (file, "\t_%s.DeleteFollowingNodes((EntityNode*)trailn);\n", attrnm);
	    fprintf (file, "*/\n");
	    fprintf (file, "}\n");
/* ************************************** */

	}

	if( (class == Class_Enumeration_Type) || 
	   (class == Class_Logical_Type) || 
	   (class == Class_Boolean_Type) )
	{
/* ************************************** */
	    if (TYPEget_name (bt)) 
	    {
		strcpy (nm, "Sdai");
		strcat (nm, FirstToUpper (TYPEget_name (bt)));
	    }

	    fprintf (file, "\t/* { _%s.ShallowCopy (*x); } */\n", attrnm  );
	    fprintf (file, "{\n");
	    fprintf (file, "    int countx = x.length();\n");
	    fprintf (file, "    SingleLinkNode *trailn = 0;\n");
	    fprintf (file, "    SingleLinkNode *n = _%s.GetHead();\n", attrnm);
	    fprintf (file, "    if( countx == 0 )\n");
	    fprintf (file, "    {\n");
	    fprintf (file, "\t_%s.Empty();\n", attrnm);
	    fprintf (file, "\treturn;\n");
	    fprintf (file, "    }\n\n");
	    fprintf (file, "    int i = 0;\n");
	    fprintf (file, "    while(i < countx)\n");
	    fprintf (file, "    {\n");
	    fprintf (file, "\tif(n == 0)\n");
	    fprintf (file, "\t{\n");
	    fprintf (file, "\t    n = _%s.NewNode();\n", attrnm);
	    fprintf (file, "\t    _%s.AppendNode( (EnumNode*)n );\n", attrnm);
	    fprintf (file, "\t}\n");
	    fprintf (file, "\t((EnumNode*)n)->node->put( (int)x[i] );\n", nm);
	    fprintf (file, "\tstd::cout << \"Assigning aggr enum element: \" << ((EnumNode*)n)->node->element_at( ((EnumNode*)n)->node->asInt() );\n");
/* /////////////////////////////////////////// */
	    if(print_logging)
	    {
		fprintf (file, "#ifdef SCL_LOGGING\n");
		fprintf (file, "\tif(*logStream)\n\t{\n");
		fprintf (file, 
			 "\t    logStream->open(SCLLOGFILE,ios::app);\n");
		fprintf (file, "\t    *logStream << time(NULL) << \" SDAI assigning entity: %s, attr: _%s, aggr enumeration/Boolean/Logical element: \" << ((EnumNode*)n)->node->element_at( ((EnumNode*)n)->node->asInt() ) << std::endl;\n", entnm, attrnm);
		fprintf (file, "\t    logStream->close();\n");
		fprintf (file, "\t}\n");
		fprintf (file, "#endif\n");
	    }
/* /////////////////////////////////////////// */
	    fprintf (file, "\ti++;\n");
	    fprintf (file, "\ttrailn = n;\n");
	    fprintf (file, "\tn = n->NextNode();\n");
	    fprintf (file, "    }\n");
	    fprintf (file, "/*\n");
	    fprintf (file, "    if(n)\n");
	    fprintf (file, "\t_%s.DeleteFollowingNodes((EnumNode*)trailn);\n", attrnm);
	    fprintf (file, "*/\n");
	    fprintf (file, "}\n");
/* ************************************** */
	}

	if (class == Class_Select_Type)
	{
	    fprintf (file, "\t{ /*_%s.ShallowCopy (*x); */ }\n", attrnm  );
	    strcpy(aggrnode_name,"SelectNode");
	}

	if (class == Class_String_Type) /* return("StringAggregate");*/
	{
/* ************************************** */
	    if (TYPEget_name (bt)) 
	    {
		strcpy (nm, "Sdai");
		strcat (nm, FirstToUpper (TYPEget_name (bt)));
	    }

	    fprintf (file, "\t/* { _%s.ShallowCopy (*x); } */\n", attrnm  );
	    fprintf (file, "{\n");
	    fprintf (file, "    int countx = x.length();\n");
	    fprintf (file, "    SingleLinkNode *trailn = 0;\n");
	    fprintf (file, "    SingleLinkNode *n = _%s.GetHead();\n", attrnm);
	    fprintf (file, "    if( countx == 0 )\n");
	    fprintf (file, "    {\n");
	    fprintf (file, "\t_%s.Empty();\n", attrnm);
	    fprintf (file, "\treturn;\n");
	    fprintf (file, "    }\n\n");
	    fprintf (file, "    int i = 0;\n");
	    fprintf (file, "    while(i < countx)\n");
	    fprintf (file, "    {\n");
	    fprintf (file, "\tif(n == 0)\n");
	    fprintf (file, "\t{\n");
	    fprintf (file, "\t    n = _%s.NewNode();\n", attrnm);
	    fprintf (file, "\t    _%s.AppendNode( (StringNode*)n );\n", attrnm);
	    fprintf (file, "\t}\n");
	    fprintf (file, "\t((StringNode*)n)->value = x[i];\n", nm);
	    fprintf (file, "\tstd::cout << \"Assigning aggr string element: \" << ((StringNode*)n)->value.chars();\n");
/* /////////////////////////////////////////// */
	    if(print_logging)
	    {
		fprintf (file, "#ifdef SCL_LOGGING\n");
		fprintf (file, "\tif(*logStream)\n\t{\n");
		fprintf (file, 
			 "\t    logStream->open(SCLLOGFILE,ios::app);\n");
		fprintf (file, "\t    *logStream << time(NULL) << \" SDAI assigning entity: %s, attr: _%s, aggr string element: \" << ((StringNode*)n)->value.chars() << std::endl;\n", entnm, attrnm);
		fprintf (file, "\t    logStream->close();\n");
		fprintf (file, "\t}\n");
		fprintf (file, "#endif\n");
	    }
/* /////////////////////////////////////////// */
	    fprintf (file, "\ti++;\n");
	    fprintf (file, "\ttrailn = n;\n");
	    fprintf (file, "\tn = n->NextNode();\n");
	    fprintf (file, "    }\n");
	    fprintf (file, "/*\n");
	    fprintf (file, "    if(n)\n");
	    fprintf (file, "\t_%s.DeleteFollowingNodes((EnumNode*)trailn);\n", attrnm);
	    fprintf (file, "*/\n");
	    fprintf (file, "}\n");
/* ************************************** */
	}
	if (class == Class_Binary_Type) /* return("BinaryAggregate");*/
	{
	    fprintf (file, "\t{ /*_%s.ShallowCopy (*x); */ }\n", attrnm  );
	    strcpy(aggrnode_name,"BinaryNode");
	}
/* x-> x-> */
/* ////////////////////////// */
    }
    return;
}

/******************************************************************
 ** Procedure:  ATTRprint_access_method
 ** Parameters:  const Variable a --  attribute to find the type for
 **	FILE* file  --  file being written to
 ** Returns:  name to be used for the type of the c++ access functions
 ** Description:  prints the access method based on the attribute type  
 **		  i.e. get and put value access functions defined in a class
 **		  generated for an entity.
 ** Side Effects:  
 ** Status:  complete 1/15/91
 **  	updated 17-Feb-1992 to print to library file instead of header
 **	updated 15-July-1993 to call the get/put head functions	by DDH
 ******************************************************************/
void
ATTRprint_access_methods  (CONST char * entnm, Variable a, FILE* file)
{
    Type t = VARget_type (a);
    Class_Of_Type class;
    char ctype [BUFSIZ];  /*  type of data member  */
    char return_type [BUFSIZ];
    char attrnm [BUFSIZ];
    char membernm[BUFSIZ];
    char funcnm [BUFSIZ];  /*  name of member function  */

    char nm [BUFSIZ];
    /* I believe nm has the name of the underlying type without Sdai in 
       front of it */
    if (TYPEget_name (t)) 
      strncpy (nm, FirstToUpper (TYPEget_name (t)), BUFSIZ-1);

    generate_attribute_func_name(a, funcnm);
/*
    generate_attribute_name( a, funcnm  );
    funcnm[0] = toupper(funcnm[0]);
*/

    generate_attribute_name( a, attrnm );
    strcpy( membernm, attrnm );
    membernm[0] = toupper(membernm[0]);	
    class = TYPEget_type (t);
/*    strncpy (ctype, TYPEget_ctype (t), BUFSIZ);*/
    if(corba_binding)
      strncpy (ctype, TYPEget_idl_type(t), BUFSIZ);
    else
      strncpy (ctype, AccessType (t), BUFSIZ);

    if ( isAggregate (a))
    {
	AGGRprint_access_methods(entnm, a, file, t, ctype, attrnm);
	return;
    }
    ATTRprint_access_methods_get_head( entnm, a, file);

    /*      case TYPE_ENTITY:	*/
    if ( class == Class_Entity_Type)  {

	fprintf (file, "{\n");
	if(print_logging)
	{
	    fprintf (file, "#ifdef SCL_LOGGING\n");
	    fprintf (file, "    if(*logStream)\n    {\n");
	    fprintf (file, "\tlogStream->open(SCLLOGFILE,ios::app);\n");
	    fprintf (file, "\tif(! (_%s == S_ENTITY_NULL) )\n\t{\n", attrnm);
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file,
		     "\t    *logStream << \"reference to Sdai%s entity #\" << _%s->STEPfile_id << std::endl;\n",
		     nm, attrnm);
/*		     funcnm, attrnm);*/
	    fprintf (file, "\t}\n\telse\n\t{\n");
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file, 
		     "\t    *logStream << \"null entity\" << std::endl;\n\t}\n");
	    fprintf (file, "\tlogStream->close();\n");
	    fprintf (file, "    }\n");
	    fprintf (file, "#endif\n");
/*	    fprintf (file, "    return (%s) _%s;\n}\n", ctype, attrnm); */
	}
	  if(corba_binding)
	  {
	    if (TYPEget_name (t)) 
	      strncpy (nm, FirstToUpper (TYPEget_name (t)), BUFSIZ-1);

/*	    fprintf (file, "{\n    if(_%s != 0)\n    {\n", attrnm); */
	    fprintf (file, "    if(_%s != 0)\n    {\n", attrnm);
	    fprintf (file, "\ttry\n\t{\n");
	    fprintf (file, 
		     "\t    const char *hostName = CORBA::Orbix.myHost();\n");
	    fprintf (file, 
		     "\t    char markerServer[64];\n");
	    fprintf (file, 
		     "\t    sprintf(markerServer, \"%%d:%%s\", _%s->"
		     "STEPfile_id, serverName);\n\n", attrnm);
	    fprintf (file, 
		     "\t    std::cout << \"*****\" << markerServer << std::endl;\n\n");
	    fprintf (file, 
		     "\t    %s_var x = %s::_bind((const char *)markerServer,"
		     "hostName);\n", nm, nm);
	    fprintf (file, 
		     "\t    %s::_duplicate(x);\n\n", nm);
	    fprintf (file, 
		     "\t    std::cout << std::endl << \"x->_refCount(): \" << x->"
		     "_refCount();\n");
	    fprintf (file, 
		     "\t    std::cout << std::endl << \"STEPfile id inside _%s's get "
		     "function is: \" \n", attrnm);
	    fprintf (file, 
		     "\t\t << _%s->STEPfile_id << std::endl;\n", attrnm);
	    fprintf (file, 
		     "\t    std::cout << \"x's marker name in server's "
		     "implementation object's attr _%s's get function is: "
		     "'\" \n", attrnm);
	    fprintf (file, 
		     "\t\t << x->_marker() << \"'\" << std::endl << std::endl;\n");
	    fprintf (file, "\t    return x;\n\t}\n");
	    fprintf (file, 
		     "\tcatch (CORBA::SystemException &se) {\n");
	    fprintf (file, 
		     "\t    std::cerr << \"Unexpected system exception in _%s's "
		     "get funct: \" << &se;\n", attrnm);
	    fprintf (file, 
		     "\t    throw;\n");
	    fprintf (file, 
		     "\t}\n\tcatch(...) {\n");
	    fprintf (file, 
		     "\t    std::cerr << \"Caught Unknown Exception in _%s's get "
		     "funct!\" << std::endl;\n", attrnm);
	    fprintf (file, 
		     "\t    throw;\n\t}\n");

/*
	    fprintf (file, "\t%s_ptr x = new TIE_%s(Sdai%s) ((Sdai%s*)_%s);\n",
		     nm, nm, nm, nm, attrnm);
	    fprintf (file, "\t%s::_duplicate(x);\n", nm);
	    fprintf (file, "\tstd::cout << \"STEPfile id is: \" << _%s->STEPfile_id << std::endl;\n", attrnm);
*/
	    fprintf (file, "    }\n");
	    fprintf (file, "    else\n");
	    fprintf (file, "\tstd::cout << \"nil object ref in attr _%s's put "
		     "funct\" << std::endl;\n", attrnm);
	    fprintf (file, "    return %s::_nil();\n}\n", nm);
	  }
	  else
/*	    fprintf (file, "\t{ return (%s) _%s; }\n", ctype, attrnm);*/
	    fprintf (file, "    return (%s) _%s; \n}\n", ctype, attrnm);

	ATTRprint_access_methods_put_head( entnm, a, file);
	fprintf (file, "{\n");
	if(print_logging)
	{
	    fprintf (file, "\n");
/*	    fprintf (file, "{\n"); */
	    fprintf (file, "#ifdef SCL_LOGGING\n");
	    fprintf (file, "    if(*logStream)\n    {\n");
	    fprintf (file, "\tlogStream->open(SCLLOGFILE,ios::app);\n");

	if(corba_binding)
	    fprintf (file, "\tif(x && !((Sdai%s*)(DEREF(x)) == S_ENTITY_NULL) )\n\t{\n", nm);
	else
	    fprintf (file, "\tif(! (x == S_ENTITY_NULL) )\n\t{\n");

	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() assigned: \";\n", 
		     entnm, funcnm);

	if(corba_binding)
	    fprintf (file,
		     "\t    *logStream << \"reference to Sdai%s entity #\" << ((Sdai%s*)(DEREF(x)))->STEPfile_id << std::endl;\n",
		     nm, nm);
	else
	    fprintf (file,
		     "\t    *logStream << \"reference to Sdai%s entity #\" << x->STEPfile_id << std::endl;\n",
		     nm);

	    fprintf (file, "\t}\n\telse\n\t{\n");
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() assigned: \";\n", 
		     entnm, funcnm);
	    fprintf (file, 
		     "\t    *logStream << \"null entity\" << std::endl;\n\t}\n");
	    fprintf (file, "\tlogStream->close();\n");
	    fprintf (file, "    }\n");
	    fprintf (file, "#endif\n");
/* 	    fprintf (file, "    _%s = x; \n}\n", attrnm  ); */
	}
	if(corba_binding)
	{
	  fprintf (file, "\n");
	  fprintf (file, "    _%s = (Sdai%s*)(DEREF(x)); \n", attrnm, nm  );
	  fprintf (file, "    if(_%s)\n    {\n", attrnm  );
	  fprintf (file, "\tstd::cout << \"STEPfile id inside _%s's put function is: \"\n", attrnm);
	  fprintf (file, "\t     << _%s->STEPfile_id << std::endl;\n", attrnm);
	  fprintf (file, "    }\n    else\n");
	  fprintf (file, "\tstd::cout << \"nil object ref in _%s's put funct\" << std::endl;\n", attrnm);
	  fprintf (file, "}\n");
	}
	else
	  fprintf (file, "    _%s = x; \n}\n", attrnm  );
/*	  fprintf (file, "\t{ _%s = x; }\n", attrnm  ); */

	return;
    }    
    /*    case TYPE_LOGICAL:	*/
    if ((class == Class_Boolean_Type) || ( class == Class_Logical_Type))  {
	
	fprintf (file, "{\n");
	if(print_logging)
	{
	    fprintf (file, "#ifdef SCL_LOGGING\n");
	    fprintf (file, "    if(*logStream)\n    {\n");
	    fprintf (file, "\tlogStream->open(SCLLOGFILE,ios::app);\n");
	    fprintf (file, "\tif(!_%s.is_null())\n\t{\n", attrnm);
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file,
		     "\t    *logStream << _%s.element_at(_%s.asInt()) << std::endl;\n",
		     attrnm, attrnm);
	    fprintf (file, "\t}\n\telse\n\t{\n");
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file, 
		     "\t    *logStream << \"unset\" << std::endl;\n\t}\n");
	    fprintf (file, "\t    logStream->close();\n");
	    fprintf (file, "    }\n");
	    fprintf (file, "#endif\n");
/*	fprintf (file, "\t{ return (const PSDAI::LOGICAL&) _%s; }\n", attrnm);*/
	}
	if(corba_binding)
	{
	    if (class == Class_Boolean_Type) 
	      fprintf (file, "    return (Boolean) _%s;\n}\n", attrnm);
	    else if ( class == Class_Logical_Type)
	      fprintf (file, "    return (Logical) _%s;\n}\n", attrnm);
	}
	else
	  fprintf (file, "    return (%s) _%s;\n}\n", ctype, attrnm);

	ATTRprint_access_methods_put_head( entnm, a, file);
	fprintf (file, "{\n");
	if(print_logging)
	{
	    fprintf (file, "#ifdef SCL_LOGGING\n");
	    fprintf (file, "    if(*logStream)\n    {\n");
	    fprintf (file, "\t*logStream << time(NULL) << \" SDAI %s::%s() assigned: \";\n", 
		     entnm, funcnm);
	    fprintf (file,
		     "\t*logStream << _%s.element_at(x) << std::endl;\n",attrnm);
	    fprintf (file, "    }\n");
	    fprintf (file, "#endif\n");

/*	fprintf (file, "\t{ return (const PSDAI::LOGICAL&) _%s; }\n", attrnm);*/
/*	    fprintf (file, "    _%s.put (x); \n}\n", attrnm  ); */
	}
	fprintf (file, "    _%s.put (x); \n}\n", attrnm  );
	return;
    }    
    /*    case TYPE_ENUM:	*/
    if ( class == Class_Enumeration_Type)  {
	fprintf (file, "{\n");
	if(print_logging)
	{
	    fprintf (file, "#ifdef SCL_LOGGING\n");
	    fprintf (file, "    if(*logStream)\n    {\n");
	    fprintf (file, "\tif(!_%s.is_null())\n\t{\n", attrnm);
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file,
		     "\t    *logStream << _%s.element_at(_%s.asInt()) << std::endl;\n",
		     attrnm, attrnm);
	    fprintf (file, "\t}\n\telse\n\t{\n");
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file, 
		     "\t    *logStream << \"unset\" << std::endl;\n\t}\n    }\n");
	    fprintf (file, "#endif\n");
/*	fprintf (file, "\t{ return (const %s&) _%s; }\n", ctype, attrnm);*/
/*	    fprintf (file, "    return (%s) _%s; \n}\n",  */
/*		     EnumName (TYPEget_name (t)), attrnm); */
	}
	fprintf (file, "    return (%s) _%s; \n}\n", 
		 EnumName (TYPEget_name (t)), attrnm);

	ATTRprint_access_methods_put_head( entnm, a, file);
	fprintf (file, "{\n");
	if(print_logging)
	{
	    fprintf (file, "#ifdef SCL_LOGGING\n");
	    fprintf (file, "    if(*logStream)\n    {\n");
	    fprintf (file, "\t*logStream << time(NULL) << \" SDAI %s::%s() assigned: \";\n", 
		     entnm, funcnm);
	    fprintf (file,
		     "\t*logStream << _%s.element_at(x) << std::endl;\n",attrnm);
	    fprintf (file, "    }\n");
	    fprintf (file, "#endif\n");

/*	fprintf (file, "\t{ return (const PSDAI::LOGICAL&) _%s; }\n", attrnm);*/
/*	    fprintf (file, "    _%s.put (x); \n}\n", attrnm  ); */
	}
	fprintf (file, "    _%s.put (x); \n}\n", attrnm  );
	return;
    }
    /*    case TYPE_SELECT:	*/
    if ( class == Class_Select_Type)  {
	fprintf (file, "\t{ return (const %s) &_%s; }\n",  ctype, attrnm);
	ATTRprint_access_methods_put_head( entnm, a, file);
	fprintf (file, "\t{ _%s = x; }\n", attrnm  );
	return;
    }
    /*    case TYPE_AGGRETATES:	*/
    /* handled in AGGRprint_access_methods(entnm, a, file, t, ctype, attrnm) */


    /*  case STRING:*/
    /*	    case TYPE_BINARY:	*/
    if (( class == Class_String_Type) || ( class == Class_Binary_Type))  {
	fprintf (file, "{\n");
	if(print_logging)
	{
	    fprintf (file, "#ifdef SCL_LOGGING\n");
	    fprintf (file, "    if(*logStream)\n    {\n");
	    fprintf (file, "\tif(!_%s.is_null())\n\t{\n", attrnm);
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file,
		     "\t    *logStream << _%s.chars() << std::endl;\n", attrnm);
	    fprintf (file, "\t}\n\telse\n\t{\n");
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file, 
		     "\t    *logStream << \"unset\" << std::endl;\n\t}\n    }\n");
	    fprintf (file, "#endif\n");

/*	    fprintf (file, "\t{ return (%s) _%s; }\n", ctype, attrnm);*/
/*	    fprintf (file, "    return (const %s) _%s; \n}\n", ctype, attrnm); */
/*DASSTR	fprintf (file, "\t{ return (const %s&) _%s; }\n", ctype, attrnm);*/
	}
	  if(corba_binding)
	  {
	     fprintf (file, "    return CORBA::string_dupl(_%s); \n}\n", attrnm);
	  }
	  else
	  {
	      fprintf (file, "    return (const %s) _%s; \n}\n", ctype, attrnm);
	  }
	ATTRprint_access_methods_put_head( entnm, a, file);
	fprintf (file, "{\n");
	if(print_logging)
	{
	    fprintf (file, "#ifdef SCL_LOGGING\n");
	    fprintf (file, "    if(*logStream)\n    {\n");
	    fprintf (file, "\tif(!x)\n\t{\n");
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file,
		     "\t    *logStream << x << std::endl;\n");
	    fprintf (file, "\t}\n\telse\n\t{\n");
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file, 
		     "\t    *logStream << \"unset\" << std::endl;\n\t}\n    }\n");
	    fprintf (file, "#endif\n");

/*	    fprintf (file, "    _%s = x; \n}\n", attrnm  ); */
	}
	fprintf (file, "    _%s = x; \n}\n", attrnm  );
	return;
    }
    /*      case TYPE_INTEGER:	*/
    if ( class == Class_Integer_Type)
    {
	fprintf (file, "{\n");
	if(print_logging)
	{
	    fprintf (file, "#ifdef SCL_LOGGING\n");
	    fprintf (file, "    if(*logStream)\n    {\n");
	    fprintf (file, "\tif(!(_%s == S_INT_NULL) )\n\t{\n", attrnm);
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file,
		     "\t    *logStream << _%s << std::endl;\n", attrnm);
	    fprintf (file, "\t}\n\telse\n\t{\n");
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file, 
		     "\t    *logStream << \"unset\" << std::endl;\n\t}\n    }\n");
	    fprintf (file, "#endif\n");
	}
    /*  default:  INTEGER	*/
      /*  is the same type as the data member  */
	fprintf (file, "    return (const %s) _%s; \n}\n", ctype, attrnm);
	ATTRprint_access_methods_put_head( entnm, a, file);
	fprintf (file, "{\n");
	if(print_logging)
	{
	    fprintf (file, "#ifdef SCL_LOGGING\n");
	    fprintf (file, "    if(*logStream)\n    {\n");
	    fprintf (file, "\tif(!(x == S_INT_NULL) )\n\t{\n");
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file,
		     "\t    *logStream << x << std::endl;\n");
	    fprintf (file, "\t}\n\telse\n\t{\n");
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file, 
		     "\t    *logStream << \"unset\" << std::endl;\n\t}\n    }\n");
	    fprintf (file, "#endif\n");
    /*  default:  INTEGER	*/
      /*  is the same type as the data member  */
	}
	fprintf (file, "    _%s = x; \n}\n", attrnm  );
    }

    /*      case TYPE_REAL:
	    case TYPE_NUMBER:	*/
    if ((class == Class_Number_Type) || ( class == Class_Real_Type))
    {
	fprintf (file, "{\n");
	if(print_logging)
	{
	    fprintf (file, "#ifdef SCL_LOGGING\n");
	    fprintf (file, "    if(*logStream)\n    {\n");
	    fprintf (file, "\tif(!(_%s == S_REAL_NULL) )\n\t{\n", attrnm);
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file,
		     "\t    *logStream << _%s << std::endl;\n", attrnm);
	    fprintf (file, "\t}\n\telse\n\t{\n");
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file, 
		     "\t    *logStream << \"unset\" << std::endl;\n\t}\n    }\n");
	    fprintf (file, "#endif\n");
	}
	fprintf (file, "    return (const %s) _%s; \n}\n", ctype, attrnm);
	ATTRprint_access_methods_put_head( entnm, a, file);
	fprintf (file, "{\n");
	if(print_logging)
	{
	    fprintf (file, "#ifdef SCL_LOGGING\n");
	    fprintf (file, "    if(*logStream)\n    {\n");
	    fprintf (file, "\tif(!(_%s == S_REAL_NULL) )\n\t{\n", attrnm);
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file,
		     "\t    *logStream << _%s << std::endl;\n", attrnm);
	    fprintf (file, "\t}\n\telse\n\t{\n");
	    fprintf (file, "\t    *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n", 
		     entnm, funcnm);
	    fprintf (file, 
		     "\t    *logStream << \"unset\" << std::endl;\n\t}\n    }\n");
	    fprintf (file, "#endif\n");
	}
	fprintf (file, "    _%s = x; \n}\n", attrnm  );
    }
}

/******************************************************************
**		Entity Generation				 */

/******************************************************************
 ** Procedure:  ENTITYhead_print
 ** Parameters:  const Entity entity
 **   FILE* file  --  file being written to
 ** Returns:  
 ** Description:  prints the beginning of the entity class definition for the 
 **               c++ code and the declaration of extern attr descriptors for 
 **		  the registry.  In the .h file
 ** Side Effects:  generates c++ code
 ** Status:  good 1/15/91 
 **          added registry things 12-Apr-1993
 ******************************************************************/

void
ENTITYhead_print (Entity entity, FILE* file,Schema schema)
{
    char buffer [BUFSIZ];
    char entnm [BUFSIZ];
    char attrnm [BUFSIZ];
    char *buf = buffer;
    char *tmp;
    Linked_List list;
    int attr_count_tmp = attr_count;
    int super_cnt =0;
    Entity super =0;

    strncpy (entnm, ENTITYget_classname (entity), BUFSIZ);

    /* DAS print all the attr descriptors and inverse attr descriptors for an 
       entity as extern defs in the .h file. */
    LISTdo(ENTITYget_attributes(entity),v,Variable)
      generate_attribute_name (v, attrnm);
      fprintf(file,"extern %s *%s%d%s%s;\n",
	      (VARget_inverse (v) ? "Inverse_attribute" : (VARis_derived (v) ? "Derived_attribute" : "AttrDescriptor")),
	      ATTR_PREFIX,attr_count_tmp++, 
	      (VARis_derived (v) ? "D" : (VARis_type_shifter (v) ? "R" : (VARget_inverse (v) ? "I" : ""))),
	      attrnm);

/* **** testing the functions **** */
/*
    if( !(VARis_derived(v) &&
	  VARget_initializer(v) && 
	  VARis_type_shifter(v) && 
	  VARis_overrider(entity, v)) )
      fprintf(file,"// %s Attr is not derived, a type shifter, overrider, no initializer.\n",attrnm);

    if(VARis_derived (v))
      fprintf(file,"// %s Attr is derived\n",attrnm);
    if (VARget_initializer (v)) 
      fprintf(file,"// %s Attr has an initializer\n",attrnm);
    if(VARis_type_shifter (v))
      fprintf(file,"// %s Attr is a type shifter\n",attrnm);
    if(VARis_overrider (entity, v))
      fprintf(file,"// %s Attr is an overrider\n",attrnm);
*/
/* ****** */

    LISTod			

    fprintf (file, "\nclass %s  :  ", entnm);

    /* inherit from either supertype entity class or root class of 
       all - i.e. SCLP23(Application_instance) */

    if(multiple_inheritance)
    {
	list = ENTITYget_supertypes (entity);
	if (! LISTempty (list)) {
	    super = (Entity)LISTpeek_first(list);
	}
    }
    else /* the old way */
	super = ENTITYput_superclass (entity);

    if (super)
	fprintf( file, "  public %s  {\n ", ENTITYget_classname(super) );
    else
	fprintf (file, "  public SCLP23(Application_instance) {\n");

#if 0
/* this code is old non-working multiple inheritance code */

    list = ENTITYget_supertypes (entity);
    if (! LISTempty (list)) {
	LISTdo (list, e, Entity)
	  /*  if there\'s no super class yet, 
	      or the super class doesn\'t have any attributes
	      */
	  if ( (! super) || (! ENTITYhas_explicit_attributes (super) )) {
	    super = e;
	    ++ super_cnt;
	  }  else {
	    printf ("WARNING:  multiple inheritance not implemented.\n");
	    printf ("\tin ENTITY %s\n\tSUPERTYPE %s IGNORED.\n\n", 
		    ENTITYget_name (entity), ENTITYget_name (e));
	  }
	LISTod;
	fprintf (file, "  public %s  {\n ", ENTITYget_classname (super));

/*  for multiple inheritance
	LISTdo (list, e, Entity)
	    sprintf (buf, "  public %s, ", ENTITYget_classname (e));
	    move (buf);
	LISTod;
	sprintf (buf - 2, " {\n");
	move (buf);
	fprintf(file,buffer);
*/
    }  else {	/*  if entity has no supertypes, it's at top of hierarchy  */ 
	fprintf (file, "  public SCLP23(Application_instance) {\n");
    }
#endif
    
}

/******************************************************************
 ** Procedure:  DataMemberPrint
 ** Parameters:  const Entity entity  --  entity being processed
 **   FILE* file  --  file being written to
 ** Returns:  
 ** Description:  prints out the data members for an entity's c++ class 
 **               definition
 ** Side Effects:  generates c++ code
 ** Status:  ok 1/15/91
 ******************************************************************/

void
DataMemberPrint(Entity entity, FILE* file,Schema schema)
{
    Linked_List attr_list;
    char entnm [BUFSIZ];
    char attrnm [BUFSIZ];    

    const char * ctype, * etype;
    
    strncpy (entnm, ENTITYget_classname (entity), BUFSIZ);  /*  assign entnm  */

    /*	print list of attributes in the protected access area 	*/

    fprintf (file, "  protected:\n");

    attr_list = ENTITYget_attributes (entity);
    LISTdo (attr_list, a, Variable)
	if (VARget_initializer (a) == EXPRESSION_NULL) {
	    ctype = TYPEget_ctype (VARget_type (a));
	    generate_attribute_name( a, attrnm );
	    if (!strcmp (ctype, "SCLundefined") ) {
		printf ("WARNING:  in entity %s:\n", ENTITYget_name (entity)); 
		printf ("\tthe type for attribute  %s is not fully ",
			"implemented\n", attrnm);
	    }
	    if (TYPEis_entity (VARget_type (a)))
	      fprintf (file, "\tSCLP23(Application_instance_ptr) _%s ;", attrnm);
	    else
	      fprintf (file, "\t%s _%s ;", ctype, attrnm);
	    if (VARget_optional (a)) fprintf (file, "    //  OPTIONAL");
	    if (isAggregate (a)) 		{
		/*  if it's a named type, comment the type  */
		if (etype = TYPEget_name
		            (TYPEget_nonaggregate_base_type (VARget_type (a))))
		  fprintf (file, "\t  //  of  %s\n", etype);
	    }

	    fprintf (file, "\n");
	}	
    
    LISTod;
}

/******************************************************************
 ** Procedure:  MemberFunctionSign
 ** Parameters:  Entity *entity --  entity being processed
 **     FILE* file  --  file being written to
 ** Returns:  
 ** Description:  prints the signature for member functions 
                  of an entity's class definition
 **		  DAS prints the end of the entity class def and the creation
 **		  function that the EntityTypeDescriptor uses.
 ** Side Effects:  prints c++ code to a file
 ** Status:  ok 1/1/5/91
 **  updated 17-Feb-1992 to print only the signature 
             and not the function definitions
 ******************************************************************/

void 
MemberFunctionSign (Entity entity, FILE* file)
{

    Linked_List attr_list;
    static int entcode = 0;    
    char entnm [BUFSIZ];

    /* added for calling multiple_inheritance */
    Linked_List parent_attr_list;
    Linked_List parent_list;
    Entity super =0;
    int super_cnt =0;

    strncpy (entnm, ENTITYget_classname (entity), BUFSIZ);  /*  assign entnm  */
    
    fprintf (file, "  public:  \n");

    /*	put in member functions which belong to all entities	*/
    /*	constructor:	*/
    fprintf (file, "\n	%s ( ); \n", entnm);

    fprintf (file, "\t%s (SCLP23(Application_instance) *se, int *addAttrs = 0); \n", entnm);
    /*  copy constructor*/
    fprintf (file, "	%s (%s& e ); \n",entnm,entnm);
    /*	destructor:	*/
    fprintf (file, "	~%s ();\n", entnm);

    /*  Open OODB reconstructor  */
    fprintf(file,"\n#ifdef __O3DB__\n");
    fprintf (file, "\tvoid oodb_reInit();\n");
    fprintf(file,"#endif\n\n");

/*    fprintf (file, "	char *Name () { return \"%s\"; }\n", */
/*	     PrettyTmpName (ENTITYget_name (entity)));*/
    fprintf (file, "	int opcode ()  { return %d ; } \n", 
	     entcode++);

    /*	print signature of access functions for attributes  	*/
    attr_list = ENTITYget_attributes (entity);
    LISTdo (attr_list, a, Variable)
	if (VARget_initializer (a) == EXPRESSION_NULL) {

    /*  retrieval  and  assignment	*/
	    ATTRsign_access_methods (a, file);
	}
    
    LISTod;

/* //////////////// */
    if(multiple_inheritance)
    {
	/* could print out access functions for parent attributes not 
	   inherited through C++ inheritance. */
	parent_list = ENTITYget_supertypes (entity);
	if (! LISTempty (parent_list)) {

	LISTdo (parent_list, e, Entity)
	  /*  if there\'s no super class yet, 
	      or the super class doesn\'t have any attributes
	  */

	    super = e;
	    super_cnt++;
	    if (super_cnt == 1)
	    {
		/* ignore the 1st parent */
		fprintf(file, 
		       "\t/* The first parent's access functions are */\n%s\n",
			"\t/* above or covered by inherited functions. */");
	    } 
	    else 
	    {
		fprintf (file, "\n#if 0\n");
/*		printf("\tin ENTITY %s\n\tadding SUPERTYPE %s\'s member functions. 1\n\n", 
		       ENTITYget_name (entity), ENTITYget_name (e));*/

    parent_attr_list = ENTITYget_attributes (e);
    LISTdo (parent_attr_list, a2, Variable)
    /*  do for EXPLICIT, REDEFINED, and INVERSE attributes - but not DERIVED */
     if  ( ! VARis_derived (a2)  )  {

       /*  retrieval  and  assignment	*/
	ATTRsign_access_methods (a2, file);
     }
    LISTod;
		fprintf (file, "\n#endif\n");
	    }
	LISTod;
	}
    }
/* //////////////// */
    if(corba_binding)
    {
	fprintf(file, "\n//\t%s_ptr create_TIE();\n\tIDL_Application_instance_ptr create_TIE();\n", 
		ENTITYget_CORBAname(entity));
/*
	fprintf(file, "\n//\t%s_ptr create_TIE();\n\tP26::Application_instance_ptr create_TIE();\n", 
		ENTITYget_CORBAname(entity));
*/
    }
    fprintf (file, "};\n");
    if(corba_binding)
    {
	fprintf (file, "\n// Associate IDL interface generated code with implementation object\nDEF_TIE_%s(%s)\n", ENTITYget_CORBAname(entity), entnm);
    }

    /*  print creation function for class	*/
    fprintf(file,"\n#if defined(__O3DB__)\n");
    fprintf (file, "inline SCLP23(Application_instance_ptr) \ncreate_%s () {  return (SCLP23(Application_instance_ptr)) new %s ;  }\n",
	 entnm, entnm);
    fprintf (file, "#else\n");
    fprintf (file, "inline %s *\ncreate_%s () {  return  new %s ;  }\n",
	 entnm, entnm, entnm);
    fprintf (file, "#endif\n");
    
}

/******************************************************************
 ** Procedure:    LIBdescribe_entity (entity, file, schema)
 ** Parameters:  Entity entity --  entity being processed
 **     FILE* file  --  file being written to
 **     Schema schema -- schema being processed
 ** Returns:  
 ** Description:  declares the global pointer to the EntityDescriptor
                  representing a particular entity
 **		  DAS also prints the attr descs and inverse attr descs 
 **		  This function creates the storage space for the externs defs 
 **		  that were defined in the .h file. These global vars go in 
 **		  the .cc file.
 ** Side Effects:  prints c++ code to a file
 ** Status:  ok 12-Apr-1993
 ******************************************************************/
void
LIBdescribe_entity (Entity entity, FILE* file, Schema schema)  
{
  Linked_List list;
  int attr_count_tmp = attr_count;
  char attrnm [BUFSIZ];
  char * tmp;

  fprintf(file,"EntityDescriptor *%s%s%s =0;\n",
	  SCHEMAget_name(schema),ENT_PREFIX,ENTITYget_name(entity));
    
  LISTdo(ENTITYget_attributes(entity),v,Variable)
    generate_attribute_name (v, attrnm);
    fprintf(file,"%s *%s%d%s%s =0;\n",
	    (VARget_inverse (v) ? "Inverse_attribute" : (VARis_derived (v) ? "Derived_attribute" : "AttrDescriptor")),
	    ATTR_PREFIX, attr_count_tmp++, 
	    (VARis_derived (v) ? "D" : (VARis_type_shifter (v) ? "R" : (VARget_inverse (v) ? "I" : ""))),
	    attrnm);
  LISTod			

}

/******************************************************************
 ** Procedure:  LIBmemberFunctionPrint 
 ** Parameters:  Entity *entity --  entity being processed
 **     FILE* file  --  file being written to
 ** Returns:  
 ** Description:  prints the member functions for the class
                  representing an entity.  These go in the .cc file
 ** Side Effects:  prints c++ code to a file
 ** Status:  ok 17-Feb-1992
 ******************************************************************/
void
LIBmemberFunctionPrint (Entity entity, FILE* file)
{

    Linked_List attr_list;
    char entnm [BUFSIZ];

    /* added for calling multiple_inheritance */
    Linked_List parent_attr_list;
    Linked_List parent_list;
    Entity super =0;
    int super_cnt =0;

    strncpy (entnm, ENTITYget_classname (entity), BUFSIZ);  /*  assign entnm */
    
    /*	1. put in member functions which belong to all entities	*/
    /*  the common function are still in the class definition 17-Feb-1992 */
    /* fprintf (file, "	char *Name () { return \"%s\"; }\n", 
	     PrettyTmpName (ENTITYget_name (entity)));
    fprintf (file, "	int opcode () { return %d ; }\n", 
	     entcode++);
    */

    /*	2. print access functions for attributes  	*/
    attr_list = ENTITYget_attributes (entity);
    LISTdo (attr_list, a, Variable)
    /*  do for EXPLICIT, REDEFINED, and INVERSE attributes - but not DERIVED */
     if  ( ! VARis_derived (a)  )  {

       /*  retrieval  and  assignment	*/
       ATTRprint_access_methods (entnm, a, file);
     }
    LISTod;
/* //////////////// */
    if(multiple_inheritance)
    {
	/* could print out access functions for parent attributes not 
	   inherited through C++ inheritance. */
	parent_list = ENTITYget_supertypes (entity);
	if (! LISTempty (parent_list)) {

	LISTdo (parent_list, e, Entity)
	  /*  if there\'s no super class yet, 
	      or the super class doesn\'t have any attributes
	  */

	    super = e;
	    super_cnt++;
	    if (super_cnt == 1)
	    {
		/* ignore the 1st parent */
		fprintf(file, 
		       "\t/* The first parent's access functions are */\n%s\n",
			"\t/* above or covered by inherited functions. */");
	    } 
	    else 
	    {
		fprintf (file, "\n#if 0\n");
/*		printf("\tin ENTITY %s\n\tadding SUPERTYPE %s\'s member functions. 1\n\n", 
		       ENTITYget_name (entity), ENTITYget_name (e)); */

    parent_attr_list = ENTITYget_attributes (e);
    LISTdo (parent_attr_list, a2, Variable)
    /*  do for EXPLICIT, REDEFINED, and INVERSE attributes - but not DERIVED */
     if  ( ! VARis_derived (a2)  )  {

       /*  retrieval  and  assignment	*/
       ATTRprint_access_methods (entnm, a2, file);
     }
    LISTod;
		fprintf (file, "\n#endif\n");
	    }
	LISTod;
	}
    }
/* //////////////// */

}

/******************************************************************
 ** Procedure:  ENTITYinc_print
 ** Parameters:  Entity *entity --  entity being processed
 **     FILE* file  --  file being written to
 ** Returns:  
 ** Description:  drives the generation of the c++ class definition code
 ** Side Effects:  prints segment of the c++ .h file
 ** Status:  ok 1/15/91
 ******************************************************************/

void 
ENTITYinc_print (Entity entity, FILE* file,Schema schema)
{
    ENTITYhead_print ( entity, file,schema);
    DataMemberPrint (entity, file, schema);
    MemberFunctionSign (entity, file);
}

/******************************************************************
 ** Procedure:  LIBcopy_constructor 
 ** Parameters:  
 ** Returns:  
 ** Description:  
 ** Side Effects:  
 ** Status:  not used 17-Feb-1992
 ******************************************************************/
void
LIBcopy_constructor (Entity ent, FILE* file)
{
    Linked_List attr_list;
    Class_Of_Type class;
    Type t;
    char buffer [BUFSIZ],
             attrnm[BUFSIZ],
      *b = buffer, *n = '\0';
    int count = attr_count;
    char * tmp;    
    
    String entnm = ENTITYget_classname (ent);
    Boolean opt;    
    String StrToLower (String word);

    /*mjm7/10/91 copy constructor definition  */
    fprintf (file, "\t%s::%s(%s& e ) \n", entnm, entnm,entnm); 
    fprintf (file, "  {");

    /*  attributes	*/
    attr_list = ENTITYget_attributes (ent);
    LISTdo (attr_list, a, Variable)
	if (VARget_initializer (a) == EXPRESSION_NULL) {
	    /*  include attribute if it is not derived	*/
	    generate_attribute_name( a, attrnm );
	    t = VARget_type (a);
	    class = TYPEget_type (t);
	    opt = VARget_optional (a);
	    
	    /*  1. initialize everything to NULL (even if not optional)  */

	    /*	  default:  to intialize attribute to NULL	*/
	    sprintf (b, "\t_%s = e.%s();\n", attrnm,attrnm);

	    /*mjm7/11/91  case TYPE_STRING */
	    if ((class == Class_String_Type) || (class == Class_Binary_Type))
	    sprintf (b, "\t_%s = strdup(e.%s());\n", attrnm,attrnm);


	    /*      case TYPE_ENTITY:	*/
	    if ( class == Class_Entity_Type)
		sprintf (b, "\t_%s = e.%s();\n", attrnm,attrnm);
	    /* previous line modified to conform with SDAI C++ Binding for PDES, Inc. Prototyping 5/22/91 CD */

	    /*    case TYPE_ENUM:	*/
	    if (class == Class_Enumeration_Type) 
	      sprintf (b, "\t_%s.put(e.%s().asInt());\n", attrnm,attrnm);
	    /*    case TYPE_SELECT:	*/
	    if (class == Class_Select_Type) 
	      sprintf (b, "DDDDDDD\t_%s.put(e.%s().asInt());\n", attrnm,attrnm);
	    /*   case TYPE_BOOLEAN    */
	    if ( class == Class_Boolean_Type)
	      sprintf (b, "\t_%s.put(e.%s().asInt());\n", attrnm,attrnm);
	    /* previous line modified to conform with SDAI C++ Binding for PDES, Inc. Prototyping 5/22/91 CD */

	    /*   case TYPE_LOGICAL    */
	    if ( class == Class_Logical_Type)
	      sprintf (b, "\t_%s.put(e.%s().asInt());\n", attrnm,attrnm);
	    /* previous line modified to conform with SDAI C++ Binding for PDES, Inc. Prototyping 5/22/91 CD */
    
            /*	case TYPE_ARRAY:
		case TYPE_LIST:
		case TYPE_SET:
		case TYPE_BAG:	*/
            if (isAggregateType (t)) 
		*b = '\0';
    
            fprintf (file, "%s", b)	      ;

	    fprintf (file, "\t attributes.push ");

	    /*  2.  put attribute on attributes list	*/

	    /*  default:	*/

	    fprintf (file,"\n\t(new STEPattribute(*%s%d%s, %s &_%s));\n", 
		     ATTR_PREFIX,count, 
/*		     (VARis_derived (t) ? "D" : (VARis_type_shifter (t) ? "R" : (VARget_inverse (t) ? "I" : ""))),*/
/*		     (VARis_derived (t) ? "D" : (VARget_inverse (t) ? "I" : "")),*/
		     attrnm, 
		     (TYPEis_entity (t) ? "(SCLP23(Application_instance_ptr) *)" : ""),
		     attrnm);
	    ++count;
	    
	}    
    LISTod;
    fprintf (file, " }\n");


}

int
get_attribute_number (Entity entity)
{
    int i = 0;
    int found =0;
    Linked_List local, complete;
    complete = ENTITYget_all_attributes (entity);
    local = ENTITYget_attributes (entity);

    LISTdo (local, a, Variable)
	/*  go to the child's first explicit attribute */
	if  (( ! VARget_inverse (a)) && ( ! VARis_derived (a)) )  {
	    LISTdo (complete, p, Variable)
	    /*  cycle through all the explicit attributes until the
		child's attribute is found  */
	      if  (!found && ( ! VARget_inverse (p)) && ( ! VARis_derived (p)) )
		{
		if (p !=a ) ++i;
		else found =1;
		}
	    LISTod;
	    if (found)  return i;
	    else printf ( "Internal error:  %s:%d\n"
			"Attribute %s not found. \n"
			,__FILE__,__LINE__, VARget_name (a));
	}

    LISTod;
    return -1;
}

/******************************************************************
 ** Procedure:  LIBstructor_print
 ** Parameters:  Entity *entity --  entity being processed
 **     FILE* file  --  file being written to
 ** Returns:  
 ** Description:  prints the c++ code for entity class's 
 **     constructor and destructor.  goes to .cc file
 ** Side Effects:  generates codes segment in c++ .cc file
 ** Status:  ok 1/15/91
 ** Changes: Modified generator to initialize attributes to NULL based
 **          on the NULL symbols defined in "SDAI C++ Binding for PDES,
 **          Inc. Prototyping" by Stephen Clark.
 ** Change Date: 5/22/91 CD
 ** Changes: Modified STEPattribute constructors to take fewer arguments
 ** 	21-Dec-1992 -kcm
 ******************************************************************/
void
LIBstructor_print (Entity entity, FILE* file, Schema schema)
{
    Linked_List attr_list;
    Type t;
    char attrnm [BUFSIZ];

    Linked_List list;
    Entity super =0;
    int super_cnt =0;
    Entity principalSuper =0;

    const char * entnm = ENTITYget_classname (entity);
    int count = attr_count;
    int index =0;
    int first =1;

    /*  constructor definition  */
    fprintf (file, "%s::%s( ) \n", entnm, entnm); 
    fprintf (file, "{\n");

/*    super = ENTITYput_superclass (entity); */

/* ////MULTIPLE INHERITANCE//////// */

    if(multiple_inheritance)
    {
    fprintf (file, "\n");
    list = ENTITYget_supertypes (entity);
    if (! LISTempty (list)) 
    {
	LISTdo (list, e, Entity)
	  /*  if there\'s no super class yet, 
	      or the super class doesn\'t have any attributes
	  */
	    fprintf (file, "\t/*  parent: %s  */\n", ENTITYget_classname (e));

	    super = e;
	    super_cnt++;
	    if (super_cnt == 1)
	    {
		/* ignore the 1st parent */
		fprintf (file, 
			 "\t/* Ignore the first parent since it is */\n %s\n",
			 "\t/* part of the main inheritance hierarchy */"); 
		principalSuper = e; /* principal SUPERTYPE */
	    } 
	    else 
	    {
		fprintf (file, "    HeadEntity(this); \n");
		fprintf (file, "#if 0 \n");
		fprintf (file, 
       "\t/* Optionally use the following to replace the line following \n");
		fprintf (file, 
       "\t   the endif. Use this to turn off adding attributes in \n");
		fprintf (file, 
       "\t   diamond shaped hierarchies for each additional parent at this\n");
		fprintf (file, 
       "\t   level. You currently must hand edit this for it to work. */\n");
		fprintf (file, "    int attrFlags[3]; // e.g. \n");
		fprintf (file, "    attrFlags[0] = 1; // add parents attrs\n");
		fprintf (file, 
		     "    attrFlags[1] = 1; // add parent of parents attrs\n");
		fprintf (file, 
    "    attrFlags[2] = 0; // do not add parent of parent of parents attrs\n");
		fprintf (file, 
    "      // In *imaginary* hierarchy turn off attrFlags[2] since it \n"); 
		fprintf (file, 
    "      // would be the parent that has more than one path to it.\n");
		fprintf (file, 
			 "    AppendMultInstance(new %s(this, attrFlags)); \n",
			 ENTITYget_classname (e));
		fprintf (file, "#endif \n");

		fprintf (file, "    AppendMultInstance(new %s(this)); \n", 
			 ENTITYget_classname (e));
/*	      fprintf (file, "new %s(this);  \n", ENTITYget_classname (e));*/
	    
		if(super_cnt == 2)
		{
		   printf("\nMULTIPLE INHERITANCE for entity: %s\n", 
			   ENTITYget_name (entity));
		    printf("\tSUPERTYPE 1: %s (principal supertype)\n", 
			   ENTITYget_name (principalSuper));
		}
		printf("\tSUPERTYPE %d: %s\n", super_cnt, ENTITYget_name (e));
/*		printf("\tin ENTITY %s\n\tadding SUPERTYPE %s. cp\n\n", 
		       ENTITYget_name (entity), ENTITYget_name (e));*/
	    }
	LISTod;

    } else {	/*  if entity has no supertypes, it's at top of hierarchy  */ 
	fprintf (file, "\t/*  no SuperTypes */\n");
    }
    }
/* ////MULTIPLE INHERITANCE//////// */

    /* Next lines added for independent field - DAR */
/*  if ( ENTITYget_supertypes(entity) || ENTITYget_abstract(entity) ) {
	/* If entity has supertypes or is abstract it's not independent.
	fprintf (file, "\n    _independent = 0;\n");
	fprintf (file, "    // entity either has supertypes or is abstract\n");
	/* Otherwise, keep the default value of 1.
    }
*/
    /*  attributes	*/
/*    fprintf (file, "\n\tSTEPattribute * a;\n");*/

    /* what if entity comes from other schema? */
    fprintf(file,"\n    eDesc = %s%s%s;\n",
	    SCHEMAget_name(schema),ENT_PREFIX,ENTITYget_name(entity));

    attr_list = ENTITYget_attributes (entity);

    LISTdo (attr_list, a, Variable)
	if (VARget_initializer (a) == EXPRESSION_NULL) {
	    /*  include attribute if it is not derived	*/
	    generate_attribute_name( a, attrnm );
	    t = VARget_type (a);

	    /*  1.  declare the AttrDescriptor	*/
/*  this is now in the header  */
/*	    fprintf(file,"extern AttrDescriptor *%s%d%s;\n",*/
/*		    ATTR_PREFIX,count,VARget_name(a));*/

	    /*  if the attribute is Explicit, make a STEPattribute  */
/*	    if (VARis_simple_explicit (a))  {*/
	    if  (( ! VARget_inverse (a)) && ( ! VARis_derived (a)) )  {
	      /*  1. create a new STEPattribute	*/

	      fprintf (file,"    "
		       "%sa = new STEPattribute(*%s%d%s%s, %s &_%s);\n", 
		       (first ? "STEPattribute *" : ""),
		       /*  first time through declare a */
		       ATTR_PREFIX,count, 
		       (VARis_type_shifter(a) ? "R" : ""),
		       attrnm, 
		       (TYPEis_entity (t) ? "(SCLP23(Application_instance_ptr) *)" : ""),
		       attrnm);
	      if (first)  first = 0 ;
	      /*  2. initialize everything to NULL (even if not optional)  */

	      fprintf (file, "    a -> set_null ();\n");
	    
	      /*  3.  put attribute on attributes list	*/
	      fprintf (file, "    attributes.push (a);\n");

	      /* if it is redefining another attribute make connection of
	         redefined attribute to redefining attribute */
	      if( VARis_type_shifter(a) ) {
		  fprintf (file, "    MakeRedefined(a, \"%s\");\n", 
			   VARget_simple_name (a));
	      }
	    }
	    count++;
	  }
    
    LISTod;

    attr_list = ENTITYget_all_attributes (entity);

    LISTdo (attr_list, a, Variable)
/*      if (VARis_overrider (entity, a)) { */
      if(VARis_derived (a)) {
	fprintf (file, "    MakeDerived (\"%s\");\n", 
		 VARget_simple_name (a));
      }
    LISTod;
    fprintf (file, "}\n");

    /*  copy constructor  */
    /*  LIBcopy_constructor (entity, file);	*/
    entnm = ENTITYget_classname (entity);
    fprintf (file, "%s::%s (%s& e ) \n",entnm,entnm,entnm);
    fprintf (file, "\t{  CopyAs((SCLP23(Application_instance_ptr)) &e);\t}\n");

    /*  print destructor  */
    /*  currently empty, but should check to see if any attributes need
	to be deleted -- attributes will need reference count  */

    entnm = ENTITYget_classname (entity);
    fprintf (file, "%s::~%s () {  }\n", entnm, entnm);

    /*  Open OODB reInit function  */
    fprintf(file,"\n#ifdef __O3DB__\n");
    fprintf (file, "void \n%s::oodb_reInit ()\n{", entnm);
    fprintf(file,"\teDesc = %s%s%s;\n",
	    SCHEMAget_name(schema),ENT_PREFIX,ENTITYget_name(entity));

    count = attr_count;
    attr_list = ENTITYget_attributes (entity);
    index = get_attribute_number (entity);

    LISTdo (attr_list, a, Variable)
      /*  if the attribute is Explicit, assign the Descriptor  */
      if  (( ! VARget_inverse (a)) && ( ! VARis_derived (a)) )  {
	generate_attribute_name( a, attrnm );
	/*  1. assign the Descriptor for the STEPattributes	*/
	fprintf (file,"\tattributes [%d].aDesc = %s%d%s%s;\n",
		 index,
		 ATTR_PREFIX,count, 
		 (VARis_type_shifter(a) ? "R" : ""),
		 attrnm);
      }
      index++,
      count++;
    LISTod;
    fprintf(file,"}\n"
	    "#endif\n\n");

    
}

/********************/
/* print the constructor that accepts a SCLP23(Application_instance) as an argument used 
   when building multiply inherited entities.
*/

void
LIBstructor_print_w_args (Entity entity, FILE* file, Schema schema)
{
    Linked_List attr_list;
    Type t;
    char attrnm [BUFSIZ];
    
    Linked_List list;
    Entity super =0;
    int super_cnt =0;

    /* added for calling parents constructor if there is one */
    char parentnm [BUFSIZ];
    char *parent = 0;
    Link parentLink = 0;
    Entity parentEntity = 0;

    const char * entnm;
    int count = attr_count;
    int first =1;

    if(multiple_inheritance)
    {

/* //////////// */
    list = ENTITYget_supertypes (entity);
    if (! LISTempty (list)) {
/*
      parentLink = list->mark->next; 
      parentEntity = (Entity) parentLink->data;
*/
      parentEntity = (Entity)LISTpeek_first(list);
      if(parentEntity)
      {
	strcpy(parentnm, ENTITYget_classname (parentEntity));
	parent = parentnm;
      }
      else
	parent = 0; /* no parent */
    }
    else
      parent = 0; /* no parent */

    /* ENTITYget_classname returns a static buffer so don't call it twice
       before it gets used - (I didn't write it) - I had to move it below
        the above use. DAS */
    entnm = ENTITYget_classname (entity); 
	/*  constructor definition  */
    if(parent)
      fprintf (file, "%s::%s (SCLP23(Application_instance) *se, int *addAttrs) : %s(se, (addAttrs ? &addAttrs[1] : 0)) \n", entnm, entnm, 
	       parentnm);
    else
      fprintf (file, "%s::%s( SCLP23(Application_instance) *se, int *addAttrs)\n", entnm, entnm);

    fprintf (file, "{\n");

/* ////MULTIPLE INHERITANCE//////// */
/*    super = ENTITYput_superclass (entity); */

    fprintf (file, "\t/* Set this to point to the head entity. */\n");
    fprintf (file, "    HeadEntity(se); \n");
    
    fprintf (file, "\n");
    list = ENTITYget_supertypes (entity);
    if (! LISTempty (list)) {
	LISTdo (list, e, Entity)
	  /*  if there\'s no super class yet, 
	      or the super class doesn\'t have any attributes
	      */
	  fprintf (file, "\t/*  parent: %s  */\n", ENTITYget_classname (e));

	  super = e;
	  super_cnt++;
	  if (super_cnt == 1) {
	    /* ignore the 1st parent */
	    fprintf (file, 
		     "\t/* Ignore the first parent since it is */\n %s\n",
		     "\t/* part of the main inheritance hierarchy */"); 
	  }  else {
		fprintf (file, "#if 0 \n");
		fprintf (file, 
       "\t/* Optionally use the following to replace the line following \n");
		fprintf (file, 
       "\t   the endif. Use this to turn off adding attributes in \n");
		fprintf (file, 
       "\t   diamond shaped hierarchies for each additional parent at this\n");
		fprintf (file, 
       "\t   level. You currently must hand edit this for it to work. */\n");
		fprintf (file, "    int attrFlags[3]; // e.g. \n");
		fprintf (file, "    attrFlags[0] = 1; // add parents attrs\n");
		fprintf (file, 
		     "    attrFlags[1] = 1; // add parent of parents attrs\n");
		fprintf (file, 
    "    attrFlags[2] = 0; // do not add parent of parent of parents attrs\n");
		fprintf (file, 
    "      // In *imaginary* hierarchy turn off attrFlags[2] since it \n"); 
		fprintf (file, 
    "      // would be the parent that has more than one path to it.\n");
		fprintf (file, 
		       "    se->AppendMultInstance(new %s(se, attrFlags)); \n",
			 ENTITYget_classname (e));
		fprintf (file, "#endif \n");
	    fprintf (file, "    se->AppendMultInstance(new %s(se, 0)); \n",
		     ENTITYget_classname (e));
/*	    printf("\tin ENTITY %s\n\thandling SUPERTYPE %s cp wArgs\n\n", 
		    ENTITYget_name (entity), ENTITYget_name (e));*/
	  }
	LISTod;

    }  else {	/*  if entity has no supertypes, it's at top of hierarchy  */ 
	fprintf (file, "\t/*  no SuperTypes */\n");
    }

/* ////MULTIPLE INHERITANCE//////// */

    /*  attributes	*/
/*    fprintf (file, "\n    STEPattribute * a;\n");*/

    /* what if entity comes from other schema? */
    fprintf(file,"\n    eDesc = %s%s%s;\n",
	    SCHEMAget_name(schema),ENT_PREFIX,ENTITYget_name(entity));

    attr_list = ENTITYget_attributes (entity);

    LISTdo (attr_list, a, Variable)
	if (VARget_initializer (a) == EXPRESSION_NULL) {
	    /*  include attribute if it is not derived	*/
	    generate_attribute_name( a, attrnm );
	    t = VARget_type (a);

	    /*  1.  declare the AttrDescriptor	*/
/*  this is now in the header  */
/*	    fprintf(file,"extern AttrDescriptor *%s%d%s;\n",*/
/*		    ATTR_PREFIX,count,VARget_name(a));*/

	    /*  if the attribute is Explicit, make a STEPattribute  */
/*	    if (VARis_simple_explicit (a))  {*/
	    if  (( ! VARget_inverse (a)) && ( ! VARis_derived (a)) )  {
	      /*  1. create a new STEPattribute	*/

	      fprintf (file,"    "
		       "%sa = new STEPattribute(*%s%d%s%s, %s &_%s);\n", 
		       (first ? "STEPattribute *" : ""),
		       /*  first time through declare a */
		       ATTR_PREFIX,count, 
		       (VARis_type_shifter(a) ? "R" : ""),
		       attrnm, 
		       (TYPEis_entity (t) ? "(SCLP23(Application_instance_ptr) *)" : ""),
		       attrnm);

	      if (first)  first = 0 ;
	      /*  2. initialize everything to NULL (even if not optional)  */

	      fprintf (file, "    a -> set_null ();\n");
	    
	      fprintf (file, 
		       "\t/* Put attribute on this class' %s\n", 
		       "attributes list so the */\n\t/*access functions still work. */");
	      /*  3.  put attribute on this class' attributes list so the
		  access functions still work */
	      fprintf (file, "    attributes.push (a);\n");
	      fprintf (file, 
		       "\t/* Put attribute on the attributes list %s\n", 
		       "for the */\n\t/* main inheritance heirarchy. */" );
/* ////MULTIPLE INHERITANCE//////// */
	      /*  4.  put attribute on attributes list for the main 
		  inheritance heirarchy */
	      fprintf (file, "    if(!addAttrs || addAttrs[0])\n");
	      fprintf (file, "        se->attributes.push (a);\n");

	      /* if it is redefining another attribute make connection of
	         redefined attribute to redefining attribute */
	      if( VARis_type_shifter(a) ) {
		  fprintf (file, "    MakeRedefined(a, \"%s\");\n", 
			   VARget_simple_name (a));
	      }
/* ////MULTIPLE INHERITANCE//////// */
	    }
	    count++;
	  }
    
    LISTod;

    attr_list = ENTITYget_all_attributes (entity);

    LISTdo (attr_list, a, Variable)
/*      if (VARis_overrider (entity, a)) { */
      if(VARis_derived (a)) {
	fprintf (file, "    MakeDerived (\"%s\");\n", 
		 VARget_simple_name (a));
      }
    LISTod;
    fprintf (file, "}\n");
    } /* end if(multiple_inheritance) */

}
/********************/

/******************************************************************
 ** Procedure:  ENTITYlib_print
 ** Parameters:  Entity *entity --  entity being processed
 **     FILE* file  --  file being written to
 ** Returns:  
 ** Description:  drives the printing of the code for the class library
 **     additional member functions can be generated by writing a routine 
 **     to generate the code and calling that routine from this procedure
 ** Side Effects:  generates code segment for c++ library file
 ** Status:  ok 1/15/91
 ******************************************************************/

void
ENTITYlib_print (Entity entity, FILE* file,Schema schema)
{
  LIBdescribe_entity (entity, file, schema);
  LIBstructor_print (entity, file,schema);
  if(multiple_inheritance)
  {
  LIBstructor_print_w_args (entity, file,schema);
  }
  LIBmemberFunctionPrint (entity, file);
}

/* return 1 if types are predefined by us */
int
TYPEis_builtin(const Type t) {
	switch( TYPEget_body(t)->type) {/* dunno if correct*/
	case integer_:
	case real_:
	case string_:
	case binary_:
	case boolean_:
	case number_:
	case logical_:
		return 1;
	}
	return 0;
}

/* go down through a type'sbase type chain, 
   Make and print new TypeDescriptors for each type with no name.  

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
void
print_typechain(FILE *f,const Type t,char *buf,Schema schema)
{
  /* if we've been called, current type has no name */
  /* nor is it a built-in type */
  /* the type_count variable is there for debugging purposes  */

    const char * ctype = TYPEget_ctype (t);
    Type base;
    int count = type_count++;
    char typename_buf[MAX_LEN];

    switch (TYPEget_body(t)->type)
    {
      case aggregate_:
      case array_:
      case bag_:
      case set_:
      case list_:
      /* create a new TypeDescriptor variable, e.g. t1, and new space for it */
	fprintf(f,"\t%s * %s%d = new %s;\n",
		GetTypeDescriptorName(t), TD_PREFIX, count, 
		GetTypeDescriptorName(t) );

	fprintf(f,
		"\t%s%d->AssignAggrCreator((AggregateCreator) create_%s);%s", 
		TD_PREFIX, count, ctype, "\t// Creator function \n");
	if(!TYPEget_head(t))
	{
	  if(TYPEget_body(t)->lower)
	    fprintf(f, "\t%s%d->Bound1(%d);\n", TD_PREFIX, count, 
		    TYPEget_body(t)->lower->u.integer);
	  if(TYPEget_body(t)->upper)
	    fprintf(f, "\t%s%d->Bound2(%d);\n", TD_PREFIX, count, 
		    TYPEget_body(t)->upper->u.integer);
	  if(TYPEget_body(t)->flags.unique)
	    fprintf(f, "\t%s%d->UniqueElements(LTrue);\n",
		    TD_PREFIX, count);
	  if(TYPEget_body(t)->flags.optional)
	    fprintf(f, "\t%s%d->OptionalElements(LTrue);\n",
		    TD_PREFIX, count);
	}
	break;

      default: /* this should not happen since only aggregates are allowed to
		  not have a name. This funct should only be called for aggrs 
		  without names. */
	fprintf(f,"\tTypeDescriptor * %s%d = new TypeDescriptor;\n",
		TD_PREFIX,count);
    }

    /* there is no name so name doesn't need to be initialized */

    fprintf(f,"\t%s%d->FundamentalType(%s);\n",TD_PREFIX,count, 
	    FundamentalType(t,1));
    fprintf(f,"\t%s%d->Description(\"%s\");\n",TD_PREFIX,count,
	    TypeDescription(t));

/* DAS ORIG SCHEMA FIX */
    fprintf(f,"\t%s%d->OriginatingSchema(%s%s);\n",TD_PREFIX,count,
	    SCHEMA_PREFIX,SCHEMAget_name(schema) );

    if(TYPEget_RefTypeVarNm(t, typename_buf, schema)) {
      fprintf(f, "\t%s%d->ReferentType(%s);\n", TD_PREFIX,count, typename_buf);
    } else {
	/* no name, recurse */
	char callee_buffer[MAX_LEN];
	if (TYPEget_body(t)) base = TYPEget_body(t)->base;
	print_typechain(f,base,callee_buffer,schema);
	fprintf(f,"\t%s%d->ReferentType(%s);\n",TD_PREFIX,count,callee_buffer);
    }
    sprintf(buf,"%s%d",TD_PREFIX,count);

#if 0
/* the old way */
    if (TYPEget_body(t)) base = TYPEget_body(t)->base;
    if (TYPEget_head(t)) {
	fprintf( f, "\t%s%d->ReferentType(%s%s%s);\n",
		 TD_PREFIX,count,
		 SCHEMAget_name( TYPEget_head(t)->superscope ),
					       schema) ),
		 TYPEprefix(t), TYPEget_name(TYPEget_head(t)) );

    } else if( TYPEis_builtin(base) ) {/* dunno if correct*/
	fprintf(f,"\t%s%d->ReferentType(%s%s);\n",
		TD_PREFIX,count,
		TD_PREFIX,FundamentalType(base,0));

    } else if (TYPEget_body(base)->type == entity_) {
	fprintf( f,"\t%s%d->ReferentType(%s%s%s);\n",
		 TYPEprefix (t) ,count,
		 /* following assumes we are not in a nested entity */
		 /* otherwise we should search upward for schema */
		 TYPEget_name(TYPEget_body(base)->entity->superscope),
		 ENT_PREFIX,TYPEget_name(base) );
    } else if(TYPEget_name(base)) { /* aggr elements that are enums go here */
	/* type ref with name */
	fprintf( f,"\t%s%d->ReferentType(%s%s%s);\n",TD_PREFIX,count,
 		 SCHEMAget_name( base->superscope ), TD_PREFIX,
 		 TYPEget_name(base) );
    } else {
	/* no name, recurse */
	char callee_buffer[MAX_LEN];
	print_typechain(f,base,callee_buffer,schema);
	fprintf(f,"\t%s%d->ReferentType(%s);\n",TD_PREFIX,count,callee_buffer);
    }
    sprintf(buf,"%s%d",TD_PREFIX,count);
#endif
}

/******************************************************************
 ** Procedure:  ENTITYincode_print
 ** Parameters:  Entity *entity --  entity being processed
 **     FILE* file  --  file being written to
 ** Returns:  
 ** Description:  generates code to enter entity in STEP registry
 **		 This goes to the .init.cc file
 ** Side Effects:  
 ** Status:  ok 1/15/91
 ******************************************************************/
void
ENTITYincode_print (Entity entity, FILE* file,Schema schema)  /*  ,FILES *files) */
{
#define entity_name	ENTITYget_name(entity)
#define schema_name	SCHEMAget_name(schema)
	const char *cn = ENTITYget_classname(entity);
	char attrnm [BUFSIZ];
	char dict_attrnm [BUFSIZ];
	const char * super_schema;
	char * tmp, *tmp2;
#if 0
	String cn_unsafe = ENTITYget_classname (entity);
	char cn[MAX_LEN];

	/* cn_unsafe points to a static buffer, grr..., so save it */
	strcpy(cn,cn_unsafe);
#endif


#ifdef NEWDICT
/* DAS New SDAI Dictionary 5/95 */
    /* insert the entity into the schema descriptor */
	fprintf(file,
		"\t((SDAIAGGRH(Set,EntityH))%s%s->Entities())->Add(%s%s%s);\n",
		SCHEMA_PREFIX,schema_name,schema_name,ENT_PREFIX,entity_name);
#endif

	if (ENTITYget_abstract(entity)) {
	    fprintf(file,"\t%s%s%s->AddSupertype_Stmt(\"",
		    schema_name,ENT_PREFIX,entity_name);
	    if (entity->u.entity->subtype_expression) {
		fprintf(file,"ABSTRACT SUPERTYPE OF (");
		tmp = SUBTYPEto_string(entity->u.entity->subtype_expression);
		tmp2 = (char*)malloc( sizeof(char) * (strlen(tmp)+BUFSIZ) );
		fprintf(file,"%s)\");\n",format_for_stringout(tmp,tmp2));
		free(tmp);
		free(tmp2);
	    } else {
		fprintf(file,"ABSTRACT SUPERTYPE\");\n");
	    }
	} else {
		if (entity->u.entity->subtype_expression) {
		    fprintf(file,"\t%s%s%s->AddSupertype_Stmt(\"",
			    schema_name,ENT_PREFIX,entity_name);
		    fprintf(file,"SUPERTYPE OF (");
		    tmp = SUBTYPEto_string(entity->u.entity->subtype_expression);
		    tmp2 = (char*)malloc( sizeof(char) * (strlen(tmp)+BUFSIZ) );
		    fprintf(file,"%s)\");\n",format_for_stringout(tmp,tmp2));
		    free(tmp);
		    free(tmp2);
		}
	}
/*
	if (entity->u.entity->subtype_expression) {
	    tmp = SUBTYPEto_string(entity->u.entity->subtype_expression);
	    tmp2 = (char*)malloc( sizeof(char) * (strlen(tmp)+BUFSIZ) );
	    fprintf(file,"\t%s%s%s->AddSupertype_Stmt(\"(%s)\");\n",
		    schema_name,ENT_PREFIX,entity_name,format_for_stringout(tmp,tmp2));
	    free(tmp);
	    free(tmp2);
	}
*/
/*
	LISTdo(ENTITYget_subtypes(entity),sub,Entity)
		fprintf(file,"	%s%s%s->AddSubtype(%s%s%s);\n",
			schema_name,ENT_PREFIX,entity_name,
			schema_name,ENT_PREFIX,ENTITYget_name(sub));
	LISTod
*/
	LISTdo(ENTITYget_supertypes(entity),sup,Entity)
          /*  set the owning schema of the supertype  */
	  super_schema = SCHEMAget_name(ENTITYget_schema(sup));
	    /* print the supertype list for this entity	*/
		fprintf(file,"	%s%s%s->AddSupertype(%s%s%s);\n",
			schema_name,ENT_PREFIX,entity_name,
			super_schema,
			ENT_PREFIX,ENTITYget_name(sup));

	    /* add this entity to the subtype list of it's supertype	*/
		fprintf(file,"	%s%s%s->AddSubtype(%s%s%s);\n",
			super_schema,
			ENT_PREFIX,ENTITYget_name(sup),
			schema_name,ENT_PREFIX, entity_name);
	LISTod

	LISTdo(ENTITYget_attributes(entity),v,Variable)
	  generate_attribute_name( v, attrnm );
	  /*  do EXPLICIT and DERIVED attributes first  */
/*	  if  ( ! VARget_inverse (v))  {*/
		/* first make sure that type descriptor exists */
		if (TYPEget_name(v->type)) {
		    if ((!TYPEget_head(v->type)) && 
			(TYPEget_body(v->type)->type == entity_))
		    {
/*			fprintf(file, "\t%s%d%s%s = new %sAttrDescriptor(\"%s\",%s%s%s,%s,%s,%s,*%s%s%s);\n", */
			fprintf(file, "\t%s%d%s%s =\n\t  new %s"
                              "(\"%s\",%s%s%s,\n\t  %s,%s%s,\n\t  *%s%s%s);\n",
				ATTR_PREFIX,attr_count, 
				(VARis_derived (v) ? "D" : 
				 (VARis_type_shifter (v) ? "R" : 
				  (VARget_inverse (v) ? "I" : ""))),
				attrnm,

				(VARget_inverse (v) ? "Inverse_attribute" : (VARis_derived (v) ? "Derived_attribute" : "AttrDescriptor")),

					/* attribute name param */
				generate_dict_attr_name(v, dict_attrnm),

				 /* following assumes we are not in a nested */
				 /* entity otherwise we should search upward */
				 /* for schema */
				 /* attribute's type  */
				TYPEget_name(
				    TYPEget_body(v->type)->entity->superscope),
				ENT_PREFIX,TYPEget_name(v->type), 

				(VARget_optional(v)?"LTrue":"LFalse"),

				(VARget_unique(v)?"LTrue":"LFalse"),

/* Support REDEFINED */
				(VARget_inverse (v) ? "" : 
				 (VARis_derived(v) ? ", AttrType_Deriving" : 
				  (VARis_type_shifter(v) ? ", AttrType_Redefining" : ", AttrType_Explicit"))),
 
				schema_name,ENT_PREFIX,TYPEget_name(entity) 
				);
		    } else {
			/* type reference */
/*			fprintf(file,"	%s%d%s%s = new %sAttrDescriptor(\"%s\",%s%s%s,%s,%s,%s,*%s%s%s);\n",*/
			fprintf(file,"  %s%d%s%s =\n\t  new %s"
                               "(\"%s\",%s%s%s,\n\t  %s,%s%s,\n\t  *%s%s%s);\n",
				ATTR_PREFIX,attr_count, 
				(VARis_derived (v) ? "D" : 
				 (VARis_type_shifter (v) ? "R" : 
				  (VARget_inverse (v) ? "I" : ""))),
				attrnm,

				(VARget_inverse (v) ? "Inverse_attribute" : (VARis_derived (v) ? "Derived_attribute" : "AttrDescriptor")),

					/* attribute name param */
				generate_dict_attr_name(v, dict_attrnm),

 				SCHEMAget_name(v->type->superscope),
 				TD_PREFIX,TYPEget_name(v->type),

				(VARget_optional(v)?"LTrue":"LFalse"),

				(VARget_unique(v)?"LTrue":"LFalse"),

				(VARget_inverse (v) ? "" : 
				 (VARis_derived(v) ? ", AttrType_Deriving" : 
				  (VARis_type_shifter(v) ? ", AttrType_Redefining" : ", AttrType_Explicit"))),

				schema_name,ENT_PREFIX,TYPEget_name(entity)
			      );
		    }
		} else if (TYPEis_builtin(v->type)) {
		  /*  the type wasn\'t named -- it must be built in or aggregate  */

/*			fprintf(file,"	%s%d%s%s = new %sAttrDescriptor(\"%s\",%s%s,%s,%s,%s,*%s%s%s);\n",*/
		    fprintf(file,"  %s%d%s%s =\n\t  new %s"
			    "(\"%s\",%s%s,\n\t  %s,%s%s,\n\t  *%s%s%s);\n",
				ATTR_PREFIX,attr_count, 
				(VARis_derived (v) ? "D" : 
				 (VARis_type_shifter (v) ? "R" : 
				  (VARget_inverse (v) ? "I" : ""))),
				attrnm,
				(VARget_inverse (v) ? "Inverse_attribute" : (VARis_derived (v) ? "Derived_attribute" : "AttrDescriptor")),
					/* attribute name param */
				generate_dict_attr_name(v, dict_attrnm),
/* not sure about 0 here */	TD_PREFIX,FundamentalType(v->type,0),
				(VARget_optional(v)?"LTrue":
				                    "LFalse"),
				(VARget_unique(v)?"LTrue":
				                  "LFalse"),
				(VARget_inverse (v) ? "" : 
				 (VARis_derived(v) ? ", AttrType_Deriving" : 
				  (VARis_type_shifter(v) ?
				     ", AttrType_Redefining" :
				     ", AttrType_Explicit"))),
				schema_name,ENT_PREFIX,TYPEget_name(entity)
			       );
		} else {
		    /* manufacture new one(s) on the spot */
		    char typename_buf[MAX_LEN];
		    print_typechain(file, v->type, typename_buf, schema);
		    /*			fprintf(file,"	%s%d%s%s = new %sAttrDescriptor(\"%s\",%s,%s,%s,%s,*%s%s%s);\n",*/
		    fprintf(file,"  %s%d%s%s =\n\t  new %s"
			    "(\"%s\",%s,%s,%s%s,\n\t  *%s%s%s);\n",
			    ATTR_PREFIX,attr_count,
			    (VARis_derived (v) ? "D" : 
			     (VARis_type_shifter (v) ? "R" : 
			      (VARget_inverse (v) ? "I" : ""))),
			    attrnm,
			    (VARget_inverse (v) ? "Inverse_attribute" : (VARis_derived (v) ? "Derived_attribute" : "AttrDescriptor")),
			    /* attribute name param */
			    generate_dict_attr_name(v, dict_attrnm),
			    typename_buf,
			    (VARget_optional(v)?"LTrue":
			     "LFalse"),
			    (VARget_unique(v)?"LTrue":
			     "LFalse"),
			    (VARget_inverse (v) ? "" :
			     (VARis_derived(v) ? ", AttrType_Deriving" :
			      (VARis_type_shifter(v) ?
			       ", AttrType_Redefining" :
			       ", AttrType_Explicit"))),
			    schema_name,ENT_PREFIX,TYPEget_name(entity)
			    );
		}

	fprintf(file,"	%s%s%s->Add%sAttr (%s%d%s%s);\n",
		schema_name,ENT_PREFIX,TYPEget_name(entity),
		(VARget_inverse (v) ? "Inverse" : "Explicit"),
		ATTR_PREFIX,attr_count, 
		(VARis_derived (v) ? "D" : 
		 (VARis_type_shifter (v) ? "R" : 
		  (VARget_inverse (v) ? "I" : ""))),
		attrnm);

	    if(VARis_derived(v) && v->initializer)
	    {
		tmp = EXPRto_string(v->initializer);
		tmp2 = (char*)malloc( sizeof(char) * (strlen(tmp)+BUFSIZ) );
		fprintf(file, "\t%s%d%s%s->initializer_(\"%s\");\n",
			ATTR_PREFIX,attr_count, 
			(VARis_derived (v) ? "D" : 
			 (VARis_type_shifter (v) ? "R" : 
			  (VARget_inverse (v) ? "I" : ""))),
			attrnm,format_for_stringout(tmp,tmp2));
		free(tmp);
		free(tmp2);
	    }
		if(VARget_inverse(v))
		{
		    fprintf(file,"\t%s%d%s%s->inverted_attr_id_(\"%s\");\n",
			  ATTR_PREFIX,attr_count, 
			  (VARis_derived (v) ? "D" : 
			   (VARis_type_shifter (v) ? "R" : 
			    (VARget_inverse (v) ? "I" : ""))),
			  attrnm,v->inverse_attribute->name->symbol.name);
		    if (v->type->symbol.name) {
			fprintf(file,
				"\t%s%d%s%s->inverted_entity_id_(\"%s\");\n",
				ATTR_PREFIX,attr_count, 
				(VARis_derived (v) ? "D" : 
				 (VARis_type_shifter (v) ? "R" : 
				  (VARget_inverse (v) ? "I" : ""))), attrnm,
				v->type->symbol.name);
			fprintf(file,"// inverse entity 1 %s\n",v->type->symbol.name);
		    } else {
/*			fprintf(file,"// inverse entity %s",TYPE_body_out(v->type));*/
			switch(TYPEget_body(v->type)->type) {
			  case entity_:	
			fprintf(file,
				"\t%s%d%s%s->inverted_entity_id_(\"%s\");\n",
				ATTR_PREFIX,attr_count, 
				(VARis_derived (v) ? "D" : 
				 (VARis_type_shifter (v) ? "R" : 
				  (VARget_inverse (v) ? "I" : ""))), attrnm,
				TYPEget_body(v->type)->entity->symbol.name);
			    fprintf(file,"// inverse entity 2 %s\n",TYPEget_body(v->type)->entity->symbol.name);
				break;
			  case aggregate_:
			  case array_:
			  case bag_:
			  case set_:
			  case list_:
			fprintf(file,
				"\t%s%d%s%s->inverted_entity_id_(\"%s\");\n",
				ATTR_PREFIX,attr_count, 
				(VARis_derived (v) ? "D" : 
				 (VARis_type_shifter (v) ? "R" : 
				  (VARget_inverse (v) ? "I" : ""))), attrnm,
				TYPEget_body(v->type)->base->symbol.name);
			    fprintf(file,"// inverse entity 3 %s\n",TYPEget_body(v->type)->base->symbol.name);
			    break;
			}
		    }
		}
#if 0
	      } else   if  (VARget_inverse (v)) {
		/*  do INVERSE attributes too  */
/* 
TODO   -- write algorithm for determining descriptor of this attribute\'s inverse
*/
/*		if ((!TYPEget_head(v->type)) && (TYPEget_body(v->type)->type == entity_)) {  */
                  /* this should always be true for INVERSE attributes  */
                  fprintf(file,"\t%s%dI%s =\n\t  new Inverse_attribute"
			  "(\"%s\",%s%s%s,\n\t  %s,%s,\n\t  *%s%s%s);\n",
			  ATTR_PREFIX,attr_count,attrnm, /*  descriptor name */
/*			  ATTR_PREFIX,attr_count,TYPEget_name(v->name), */
			  StrToLower (attrnm), /*  attribute name  */
/*			  StrToLower (TYPEget_name(v->name)), */  
			  /* following assumes we are not in a nested entity */
			  /* otherwise we should search upward for schema */
			  /* attribute's type  */
			  TYPEget_name(TYPEget_body(v->type)->entity->superscope),ENT_PREFIX,attrnm,
/*TYPEget_name(v->type), */
			  (VARget_optional(v)?"LTrue":"LFalse"),
			  (VARget_unique(v)?"LTrue":"LFalse"),
			  schema_name,ENT_PREFIX, TYPEget_name(entity));

		  fprintf(file,"\t%s%s%s->AddInverseAttr (%s%dI%s);\n",
			  schema_name,ENT_PREFIX,TYPEget_name(entity),
			  ATTR_PREFIX,attr_count,TYPEget_name(v->name));
/*		}*/
	      }
#endif
	attr_count++;

	LISTod			

 	fprintf(file,"\treg.AddEntity (*%s%s%s);\n",
 		schema_name,ENT_PREFIX,entity_name);

#undef schema_name
}

#if 0
void
/* create calls to connect sub/supertype relationships */
ENTITYstype_connect(Entity entity, FILE* file,Schema schema)
{
	LISTdo(ENTITYget_subtypes(entity),sub,Entity)
		fprintf(file,"	%s->Subtypes().AddNode(%s);\n",
			ENTITYget_name(sub));
	LISTod

	LISTdo(ENTITYget_supertypes(entity),sup,Entity)
		fprintf(file,"	%s->Supertypes().AddNode(%s);\n",
			ENTITYget_name(sup));
	LISTod
	}
#endif

/******************************************************************
 ** Procedure:  ENTITYPrint
 ** Parameters:  Entity *entity --  entity being processed
 **     FILE* file  --  file being written to
 ** Returns:  
 ** Description:  drives the functions for printing out code in lib,
 **     include, and initialization files for a specific entity class
 ** Side Effects:  generates code in 3 files
 ** Status:  complete 1/15/91
 ******************************************************************/


void
ENTITYPrint(Entity entity, FILES* files,Schema schema)
{

  char * n = ENTITYget_name (entity);
  DEBUG ("Entering ENTITYPrint for %s\n", n);

  fprintf (files->inc, "\n/////////\t ENTITY %s\n\n", n);
  ENTITYinc_print (entity, files -> inc,schema);
  fprintf (files->inc, "\n/////////\t END_ENTITY %s\n\n", n);
  
  fprintf (files->lib, "\n/////////\t ENTITY %s\n\n", n);
  ENTITYlib_print (entity, files -> lib,schema);
  fprintf (files->lib, "\n/////////\t END_ENTITY %s\n\n", n);

  fprintf (files->init, "\n/////////\t ENTITY %s\n\n", n);
  ENTITYincode_print (entity, files -> init, schema);
  fprintf (files->init, "/////////\t END_ENTITY %s\n", n);
   
  DEBUG ("DONE ENTITYPrint\n")    ;
}

void
MODELPrintConstructorBody(Entity entity, FILES* files,Schema schema
                          /*, int index*/)
{
  const char * n;
  DEBUG ("Entering MODELPrintConstructorBody for %s\n", n);

  n = ENTITYget_classname (entity);

  fprintf (files->lib, "        eep = new SCLP23(Entity_extent);\n");


  fprintf (files->lib, "    eep->definition_(%s%s%s);\n", 
	   SCHEMAget_name(schema), ENT_PREFIX, ENTITYget_name(entity) );
  fprintf (files->lib, "    _folders.Append(eep);\n\n"); 

/*
  fprintf (files->lib, "    %s__set_var SdaiModel_contents_%s::%s_get_extents()\n", 
	   n, SCHEMAget_name(schema), n); 

	   fprintf(files->create,"	%s%s%s = new EntityDescriptor(\"%s\",%s%s,%s, (Creator) create_%s);\n",

		PrettyTmpName (ENTITYget_name(entity)),
		SCHEMA_PREFIX,SCHEMAget_name(schema),
		(ENTITYget_abstract(entity)?"LTrue":"LFalse"),
		ENTITYget_classname (entity)  
		);


  fprintf (files->lib, 
	   "{\n    return (%s__set_var)((_folders.retrieve(%d))->instances_());\n}\n", 
	   n, index); 
*/
}

void
MODELPrint(Entity entity, FILES* files,Schema schema, int index)
{

  const char * n;
  DEBUG ("Entering MODELPrint for %s\n", n);

  n = ENTITYget_classname (entity);
  fprintf (files->lib, "\n%s__set_var SdaiModel_contents_%s::%s_get_extents()\n", 
	   n, SCHEMAget_name(schema), n); 
  fprintf (files->lib, 
	   "{\n    return (%s__set_var)((_folders.retrieve(%d))->instances_());\n}\n", 
	   n, index); 
/*
  fprintf (files->lib, 
	   "{\n    return (%s__set_var)((_folders[%d])->instances_());\n}\n", 
	   n, index); 
*/

/* //////////////// */
/*
  fprintf (files->inc, "\n/////////\t ENTITY %s\n\n", n);
  ENTITYinc_print (entity, files -> inc,schema);
  fprintf (files->inc, "\n/////////\t END_ENTITY %s\n\n", n);
  
  fprintf (files->lib, "\n/////////\t ENTITY %s\n\n", n);
  ENTITYlib_print (entity, files -> lib,schema);
  fprintf (files->lib, "\n/////////\t END_ENTITY %s\n\n", n);

  fprintf (files->init, "\n/////////\t ENTITY %s\n\n", n);
  ENTITYincode_print (entity, files -> init, schema);
  fprintf (files->init, "/////////\t END_ENTITY %s\n", n);
*/   
  DEBUG ("DONE MODELPrint\n")    ;
}

/*
getEntityDescVarName(Entity entity)
{
    SCHEMAget_name(schema),ENT_PREFIX,ENTITYget_name(entity),
}
*/

/* print in include file: class forward prototype, class typedefs, and 
   extern EntityDescriptor.  `externMap' = 1 if entity must be instantiated
   with external mapping (see Part 21, sect 11.2.5.1).  */
void
ENTITYprint_new(Entity entity,FILES *files, Schema schema, int externMap)
{
	const char * n;
	Linked_List wheres;
/*	char buf[BUFSIZ],buf2[BUFSIZ]; */
	char *whereRule, *whereRule_formatted;
	int whereRule_formatted_size = 0;    
	char *ptr,*ptr2;
	char *uniqRule, *uniqRule_formatted;
	Linked_List uniqs;
	int i;

	fprintf(files->create, "\t%s%s%s = new EntityDescriptor(\n\t\t",
		SCHEMAget_name(schema),ENT_PREFIX,ENTITYget_name(entity));
	fprintf(files->create, "  \"%s\", %s%s, %s, ",
		PrettyTmpName (ENTITYget_name(entity)),
		SCHEMA_PREFIX,SCHEMAget_name(schema),
		(ENTITYget_abstract(entity) ? "LTrue" : 
		 "LFalse"));
	fprintf(files->create, "%s,\n\t\t", externMap ? "LTrue" :
                "LFalse");

	fprintf(files->create, "  (Creator) create_%s );\n",
		ENTITYget_classname (entity));
	/* add the entity to the Schema dictionary entry */
	fprintf(files->create,"\t%s%s->AddEntity(%s%s%s);\n",SCHEMA_PREFIX, SCHEMAget_name(schema), SCHEMAget_name(schema),ENT_PREFIX,ENTITYget_name(entity));

	wheres = TYPEget_where(entity);

	if (wheres) {
	    fprintf(files->create, 
		    "\t%s%s%s->_where_rules = new Where_rule__list;\n",
		    SCHEMAget_name(schema),ENT_PREFIX,ENTITYget_name(entity));

	    LISTdo(wheres,w,Where)
	      whereRule = EXPRto_string(w->expr);
	      ptr2 = whereRule;
	      
	      if(whereRule_formatted_size == 0) 
	      {
		  whereRule_formatted_size = 3*BUFSIZ;
		  whereRule_formatted = (char *)malloc(sizeof(char)*whereRule_formatted_size);
	      } 
	      else if( (strlen(whereRule) + 300) > whereRule_formatted_size )
	      {
		  free(whereRule_formatted);
		  whereRule_formatted_size = strlen(whereRule) + BUFSIZ;
		  whereRule_formatted = (char *)malloc(sizeof(char)*whereRule_formatted_size);
	      }
	      whereRule_formatted[0] = '\0';
/*
	    printf("whereRule length: %d\n",strlen(whereRule));
	    printf("whereRule_formatted size: %d\n",whereRule_formatted_size);
*/
	      if (w->label) {
		  strcpy(whereRule_formatted,w->label->name);
		  strcat(whereRule_formatted,": (");
		  ptr = whereRule_formatted + strlen(whereRule_formatted);
		  while(*ptr2) {
		      if(*ptr2 == '\n')
			;
		      else if(*ptr2 == '\\') {
			  *ptr = '\\'; ptr++;
			  *ptr = '\\'; ptr++;

		      } else if(*ptr2 == '(') {
			  *ptr = '\\'; ptr++;
			  *ptr = 'n';  ptr++;
			  *ptr = '\\'; ptr++;
			  *ptr = 't';  ptr++;
			  *ptr = *ptr2; ptr++;
		      } else {
			  *ptr = *ptr2;
			  ptr++;
		      }
		      ptr2++;
		  }
		  *ptr = '\0';

		  strcat(ptr,");\\n");
	      } else {
		  /* no label */
		  strcpy(whereRule_formatted,"(");
		  ptr = whereRule_formatted + strlen(whereRule_formatted);

		  while(*ptr2) {
		      if(*ptr2 == '\n')
			;
		      else if(*ptr2 == '\\') {
			  *ptr = '\\'; ptr++;
			  *ptr = '\\'; ptr++;

		      } else if(*ptr2 == '(') {
			  *ptr = '\\'; ptr++;
			  *ptr = 'n';  ptr++;
			  *ptr = '\\'; ptr++;
			  *ptr = 't';  ptr++;
			  *ptr = *ptr2; ptr++;
		      } else {
			  *ptr = *ptr2;
			  ptr++;
		      }
		      ptr2++;
		  }
		  *ptr = '\0';
		  strcat(ptr,");\\n");
	      }
	      fprintf(files->create, "\twr = new Where_rule(\"%s\");\n",whereRule_formatted);
	    fprintf(files->create, "\t%s%s%s->_where_rules->Append(wr);\n",
		SCHEMAget_name(schema),ENT_PREFIX,ENTITYget_name(entity));

	      free(whereRule);
	      ptr2 = whereRule = 0;
	    LISTod
	}

	uniqs = entity->u.entity->unique;

	if(uniqs)
	{
	    fprintf(files->create, 
		   "\t%s%s%s->_uniqueness_rules = new Uniqueness_rule__set;\n",
		    SCHEMAget_name(schema),ENT_PREFIX,ENTITYget_name(entity));

	    if(whereRule_formatted_size == 0)
	    {
		uniqRule_formatted = (char *)malloc(sizeof(char)*2*BUFSIZ);
		whereRule_formatted = uniqRule_formatted;
	    }
	    else
	      uniqRule_formatted = whereRule_formatted;

/*******/
/*
DASBUG
 * take care of qualified attribute names like SELF\entity.attrname 
 * add parent entity to the uniqueness rule
 * change EntityDescriptor::generate_express() to generate the UNIQUE clause
*/
	    LISTdo(uniqs,list,Linked_List)
		i = 0;
		fprintf(files->create, "\tur = new Uniqueness_rule(\"");
		LISTdo(list,v,Variable)
			i++;
			if (i == 1) {
				/* print label if present */
				if (v) {
				    fprintf(files->create, "%s : ",StrToUpper(((Symbol *)v)->name));
				}
			} else {
				if (i > 2) fprintf(files->create,", ");
				uniqRule = EXPRto_string(v->name);
				fprintf(files->create, uniqRule);
			}
		LISTod
		fprintf(files->create,";\\n\");\n");
		fprintf(files->create, "\t%s%s%s->_uniqueness_rules->Append(ur);\n",
		SCHEMAget_name(schema),ENT_PREFIX,ENTITYget_name(entity));
	LISTod
/********/
	    
	}

	if(whereRule_formatted_size > 0)
	  free(whereRule_formatted);

	n = ENTITYget_classname (entity);
	fprintf (files->classes, "\nclass %s;\n", n); 
	fprintf (files->classes, "typedef %s *  \t%sH;\n", n, n);
	fprintf (files->classes, "typedef %s *  \t%s_ptr;\n", n, n);
	fprintf (files->classes, "typedef %s_ptr\t%s_var;\n", n, n);

	fprintf (files->classes,
		 "#define %s__set \tSCLP23(DAObject__set)\n", n);

	fprintf (files->classes,
		 "#define %s__set_var \tSCLP23(DAObject__set_var)\n", n);

	fprintf(files ->classes,"extern EntityDescriptor \t*%s%s%s;\n",
		SCHEMAget_name(schema),ENT_PREFIX,ENTITYget_name(entity));

}

void
MODELprint_new(Entity entity,FILES *files, Schema schema)
{
	const char * n;

	n = ENTITYget_classname (entity);
	fprintf (files->inc, "\n    %s__set_var %s_get_extents();\n", n, n); 
/*
	fprintf(files->create," %s%s%s = new EntityDescriptor(\"%s\",%s%s,%s, (Creator) create_%s);\n",
		SCHEMAget_name(schema),ENT_PREFIX,ENTITYget_name(entity),
		PrettyTmpName (ENTITYget_name(entity)),
		SCHEMA_PREFIX,SCHEMAget_name(schema),
		(ENTITYget_abstract(entity)?"LTrue":"LFalse"),
		ENTITYget_classname (entity)  
		);
*/
/*
	fprintf(files ->inc,"extern EntityDescriptor \t*%s%s%s;\n",
		SCHEMAget_name(schema),ENT_PREFIX,ENTITYget_name(entity));
*/

}

/******************************************************************
 **			TYPE GENERATION				**/


/******************************************************************
 ** Procedure:	TYPEprint_enum
 ** Parameters:	const Type type	- type to print
 **		FILE*      f	- file on which to print
 ** Returns:	
 ** Requires:	TYPEget_class(type) == TYPE_ENUM
 ** Description:  prints code to represent an enumerated type in c++ 
 ** Side Effects:  prints to header file
 ** Status:  ok 1/15/91
 ** Changes: Modified to check for appropiate key words as described
 **          in "SDAI C++ Binding for PDES, Inc. Prototyping" by
 **          Stephen Clark.
 ** - Changed to match CD2 Part 23, 1/14/97 DAS
 ** Change Date: 5/22/91  CD
 ******************************************************************/
const char * 
EnumCElementName (Type type, Expression expr)  {

  static char buf [BUFSIZ];
  sprintf (buf,"%s__", 
	   EnumName (TYPEget_name (type)));
  strcat(buf, StrToLower (EXPget_name(expr)));

  return buf;
}

char *
CheckEnumSymbol (char * s)
{

    static char b [BUFSIZ];
    if (strcmp (s, "sdaiTRUE") 
	&& strcmp (s, "sdaiFALSE") 
	&& strcmp (s, "sdaiUNKNOWN")) {
	/*  if the symbol is not a reserved one	*/
	return (s);
	
    } else {
	strcpy (b, s);
	strcat (b, "_");
	printf ("** warning:  the enumerated value %s is already being used ", s);
	printf (" and has been changed to %s **\n", b);
	return (b);
    }
}

/*********************************************************************
 ** Procedure:	 TYPEenum_inc_print
 ** Description: Writes enum type descriptors and classes.
 ** Change Date:
 ********************************************************************/
void
TYPEenum_inc_print (const Type type, FILE* inc)
{
    DictionaryEntry de;
    Expression expr;

    char tdnm[BUFSIZ],
	 enumAggrNm[BUFSIZ];
    const char * n;  /*  pointer to class name  */
    int cnt =0;

	fprintf (inc, "\n//////////  ENUMERATION TYPE %s\n",
		 TYPEget_name (type));

	/*  print c++ enumerated values for class	*/
	if(corba_binding)
	{
	    fprintf (inc, "#ifndef PART26\n");
	}
	fprintf (inc, "enum %s {\n", EnumName (TYPEget_name (type)));
	
	LISTdo_links( TYPEget_body(type)->list, link )
	    /*  print the elements of the c++ enum type  */
	    expr = (Expression)link->data;
	    if(cnt != 0) fprintf(inc, ",\n");
	    ++cnt;
	    fprintf (inc,"\t%s", EnumCElementName (type, expr));

/*	      printf( "WARNING: truncating values %s%s in the .h file.\n\n",
		      "that will be used to print the enumeration ", 
		      TYPEget_name (type));*/
        LISTod
	
	fprintf (inc, ",\n\t%s_unset\n};\n", EnumName(TYPEget_name(type)));
	if(corba_binding)
	{
	    fprintf (inc, "#endif\n");
	}

    /*  print class for enumeration	*/
	n = TYPEget_ctype (type);
	fprintf (inc, "\nclass %s  :  public SCLP23(Enum)  {\n", n);

	fprintf (inc, "  protected:\n\tEnumTypeDescriptor *type;\n\n");

	/*	constructors	*/
        strncpy (tdnm, TYPEtd_name(type), BUFSIZ);
	fprintf (inc, "  public:\n\t%s (const char * n =0, Enum"
                      "TypeDescriptor *et =%s);\n", n, tdnm);
/*	fprintf (inc, "\t%s (%s e) { Init ();  set_value (e);  }\n", */
	fprintf (inc, "\t%s (%s e, EnumTypeDescriptor *et =%s)\n"
	              "\t\t: type(et) {  set_value (e);  }\n",
		 n, EnumName (TYPEget_name (type)), tdnm);

	/*	destructor	*/
	fprintf (inc, "\t~%s () { }\n", n);

	/*      operator =      */
	fprintf (inc, "\t%s& operator= (const %s& e)\n", 
		 n, TYPEget_ctype (type));
/*	fprintf (inc, "\t%s& operator= (const %s& e)\n", n, n);*/
	fprintf (inc, "\t\t{  set_value (e);  return *this;  }\n");

	/*      operator to cast to an enumerated type  */
	fprintf (inc, "\toperator %s () const;\n", 
		 EnumName(TYPEget_name (type) ));

	/*      others          */
	fprintf (inc, "\n\tinline virtual const char * Name () const\n");
	fprintf (inc, "\t\t{  return type->Name();  }\n");
	fprintf (inc, "\tinline virtual int no_elements () const"
		      "  {  return %d;  }\n", cnt);
/*	fprintf (inc, "  private:\n\tvoid Init ();\n");*/
	fprintf (inc, "\tvirtual const char * element_at (int n) const;\n"); 

	/*  end class definition  */
	fprintf (inc, "};\n");

	fprintf (inc, "\ntypedef %s * %s_ptr;\n", n, n);
/*	fprintf (inc, "typedef %s_ptr %s_var;\n", n, n);*/


    /*  Print ObjectStore Access Hook function  */
	printEnumCreateHdr( inc, type );

	/* DAS brandnew above */

    /*  print things for aggregate class  */
	sprintf (enumAggrNm,"%ss", n);
	
	fprintf (inc, "\nclass %ss  :  public EnumAggregate  {\n", n);

/*      fprintf (inc, "  public:\n\tvirtual EnumNode * NewNode ()  {\n");*/
	fprintf (inc, "  protected:\n    EnumTypeDescriptor *enum_type;\n\n");
	fprintf (inc, "  public:\n");
	fprintf (inc, "    %ss( EnumTypeDescriptor * =%s);\n", n, tdnm);
	fprintf (inc, "    virtual ~%ss();\n", n);
	fprintf (inc, "    virtual SingleLinkNode * NewNode()\n");
	fprintf (inc, "\t{ return new EnumNode (new %s( \"\", enum_type )); }"
		      "\n", n);

	fprintf (inc, "};\n");

	fprintf (inc, "\ntypedef %ss * %ss_ptr;\n", n, n);
/*
	fprintf (inc, "typedef %ss_ptr %ss_var;\n", n, n);
	fprintf (inc, "typedef %ss * %ssH;\n", n, n);
*/

	/* DAS brandnew below */

	/* DAS creation function for enum aggregate class */
	printEnumAggrCrHdr( inc, type );

	/* DAS brandnew above */

    fprintf (inc, "\n//////////  END ENUMERATION %s\n\n", TYPEget_name (type));
}

void
TYPEenum_lib_print (const Type type, FILE* f)
{
  DictionaryEntry de;
  Expression expr;
  const char *n;	/*  pointer to class name  */
  char buf [BUFSIZ];
  char c_enum_ele [BUFSIZ];
    
  fprintf (f, "//////////  ENUMERATION TYPE %s\n", TYPEget_ctype (type));
  n = TYPEget_ctype(type);
    
  /*  set up the dictionary info  */
/*  fprintf (f, "void \t%s::Init ()  {\n", n);*/
/*  fprintf (f, "  static const char * const l [] = {\n");*/
  
  fprintf (f, "const char * \n%s::element_at (int n) const  {\n", n);
  fprintf (f, "  switch (n)  {\n");
  DICTdo_type_init(ENUM_TYPEget_items(type),&de,OBJ_ENUM);
  while (0 != (expr = (Expression)DICTdo(&de))) {
    strncpy (c_enum_ele, EnumCElementName (type, expr), BUFSIZ);
    fprintf(f,"  case %s\t:  return \"%s\";\n",
	    c_enum_ele,
	    StrToUpper (EXPget_name(expr)));
  }
  fprintf (f, "  case %s_unset\t:\n", EnumName(TYPEget_name(type)));
  fprintf (f, "  default\t\t:  return \"UNSET\";\n  }\n}\n");
/*  fprintf (f, "\t 0\n  };\n");*/
/*  fprintf (f, "  set_elements (l);\n  v = ENUM_NULL;\n}\n");*/

  /*	constructors	*/
  /*    construct with character string  */
  fprintf (f, "\n%s::%s (const char * n, EnumTypeDescriptor *et)\n"
	      "  : type(et)\n{\n", n, n);
/*  fprintf (f, "  Init ();\n  set_value (n);\n}\n");*/
  fprintf (f, "  set_value (n);\n}\n");
  
  /*    copy constructor  */
/*  fprintf (f, "%s::%s (%s& n )  {\n", n, n, n);*/
/*  fprintf (f, "   (l);\n");*/

  /*      operator =      */
/*  fprintf (f, "%s& \t%s::operator= (%s& x)", n, n, n);*/
/*  fprintf (f, "\n\t{  put (x.asInt ()); return *this;   }\n");*/

	/*      cast operator to an enumerated type  */
  fprintf (f, "\n%s::operator %s () const {\n", n, 
	   EnumName(TYPEget_name (type)));
  fprintf (f, "  switch (v) {\n");
        //buf [0] = '\0';
	DICTdo_type_init(ENUM_TYPEget_items(type),&de,OBJ_ENUM);
	while (0 != (expr = (Expression)DICTdo(&de))) {
	  strncpy (c_enum_ele, EnumCElementName (type, expr), BUFSIZ);
	  fprintf (f, "\tcase %s\t:  ", c_enum_ele);
	  fprintf (f, "return %s;\n", c_enum_ele);

	  //fprintf (f, "%s", buf);
	  //strncpy (c_enum_ele, EnumCElementName (type, expr), BUFSIZ);
	  //fprintf (f, "\tcase %s\t:  ", c_enum_ele);
	  //sprintf (buf, "return %s;\n", c_enum_ele);

	}
  /*  print the last case with the default so sun c++ doesn\'t complain */
  fprintf (f, "\tcase %s_unset\t:\n", EnumName(TYPEget_name(type)));
  fprintf (f, "\tdefault\t\t:  return %s_unset;\n  }\n}\n", EnumName(TYPEget_name(type)));

	//fprintf (f, "\n\tdefault\t\t:  %s  }\n}\n", buf);

/*	fprintf (f, "\n\tdefault :  %s;\n  }\n}\n", buf);*/
/*	fprintf (f, "\t\tdefault:  return 0;\n  }\n}\n");*/

  printEnumCreateBody( f, type );
		 
/* print the enum aggregate functions */

  fprintf (f, "\n%ss::%ss( EnumTypeDescriptor *et )\n", n, n);
  fprintf (f, "    : enum_type(et)\n{\n}\n\n");
  fprintf (f, "%ss::~%ss()\n{\n}\n", n, n);

  printEnumAggrCrBody( f, type );

  fprintf (f, "\n//////////  END ENUMERATION %s\n\n", TYPEget_name (type));
}


void Type_Description(const Type,char *);

/* return printable version of entire type definition */
/* return it in static buffer */
char *
TypeDescription(const Type t)
{
	static char buf[4000];

	buf[0] = '\0';

	if (TYPEget_head(t)) Type_Description(TYPEget_head(t),buf);
	else TypeBody_Description(TYPEget_body(t),buf);

	/* should also print out where clause here */

	return buf+1;
}

strcat_expr(Expression e,char *buf)
{
	if (e == LITERAL_INFINITY) {
		strcat(buf,"?");
	} else if (e == LITERAL_PI) {
		strcat(buf,"PI");
	} else if (e == LITERAL_E) {
		strcat(buf,"E");
	} else if (e == LITERAL_ZERO) {
		strcat(buf,"0");
	} else if (e == LITERAL_ONE) {
		strcat(buf,"1");
	} else if (TYPEget_name(e)) {
		strcat(buf,TYPEget_name(e));
	} else if (TYPEget_body(e->type)->type == integer_) {
		char tmpbuf[30];
		sprintf(tmpbuf,"%d",e->u.integer);
		strcat(buf,tmpbuf);
	} else {
		strcat(buf,"??");
	}
}

/* print t's bounds to end of buf */
void
strcat_bounds(TypeBody b,char *buf)
{
	if (!b->upper) return;

	strcat(buf," [");
	strcat_expr(b->lower,buf);
	strcat(buf,":");
	strcat_expr(b->upper,buf);
	strcat(buf,"]");
}

TypeBody_Description(TypeBody body, char *buf)
{
	Expression expr;
	DictionaryEntry de;
	char *s;
/* // I believe it should never go here? DAS
	if(body->type != array_ && body->type != list_)
	{
	    if (body->flags.unique)	strcat(buf," UNIQUE");
	    if (body->flags.optional)	strcat(buf," OPTIONAL");
	}
*/
	switch (body->type) {
	case integer_:		strcat(buf," INTEGER");	break;
	case real_:		strcat(buf," REAL");	break;
	case string_:		strcat(buf," STRING");	break;
	case binary_:		strcat(buf," BINARY");	break;
	case boolean_:		strcat(buf," BOOLEAN");	break;
	case logical_:		strcat(buf," LOGICAL");	break;
	case number_:		strcat(buf," NUMBER");	break;
	case entity_:		strcat(buf," ");
				strcat(buf,PrettyTmpName (TYPEget_name(body->entity)));
				break;
	case aggregate_:
	case array_:
	case bag_:
	case set_:
	case list_:
		switch (body->type) {
		/* ignore the aggregate bounds for now */
		case aggregate_:	strcat(buf," AGGREGATE OF"); 	break;
		case array_:		strcat(buf," ARRAY");
					strcat_bounds(body,buf);
					strcat(buf," OF");
		  if (body->flags.optional)	strcat(buf," OPTIONAL");
		  if (body->flags.unique)	strcat(buf," UNIQUE");
		  break;
		case bag_:		strcat(buf," BAG");
					strcat_bounds(body,buf);
					strcat(buf," OF");		break;
		case set_:		strcat(buf," SET");
					strcat_bounds(body,buf);
					strcat(buf," OF");		break;
		case list_:		strcat(buf," LIST");
					strcat_bounds(body,buf);
					strcat(buf," OF");
		  if (body->flags.unique)	strcat(buf," UNIQUE");
		  break;
		}

		Type_Description(body->base,buf);
		break;
	case enumeration_:
		strcat(buf," ENUMERATION of (");
		LISTdo(body->list,e,Expression)
			strcat(buf,ENUMget_name(e));
			strcat(buf,", ");
		LISTod
		/* find last comma and replace with ')' */
		s = strrchr(buf,',');
		if (s) strcpy(s,")");
		break;

	case select_:
		strcat(buf," SELECT (");
		LISTdo (body->list, t, Type)  
		  strcat (buf, PrettyTmpName (TYPEget_name(t)) );
		  strcat(buf,", ");
	        LISTod
		/* find last comma and replace with ')' */
		s = strrchr(buf,',');
		if (s) strcpy(s,")");
		break;
	default:		strcat(buf," UNKNOWN");
	}

	if (body->precision) {
		strcat(buf," (");
		strcat_expr(body->precision,buf);
		strcat(buf,")");
	}
	if (body->flags.fixed)	strcat(buf," FIXED");
}

void
Type_Description(const Type t,char *buf)
{
	if (TYPEget_name(t)) {
		strcat(buf," ");	
		strcat(buf,TYPEget_name(t));
		/* strcat(buf,PrettyTmpName (TYPEget_name(t)));*/
	} else {
		TypeBody_Description(TYPEget_body(t),buf);
	}
}

/**************************************************************************
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
 **************************************************************************/
void
TYPEprint_typedefs (Type t, FILE *classes)  
{
    Class_Of_Type class;
    char nm [BUFSIZ];
    Type i;

    /* Print the typedef statement (poss also a forward class def: */
    if ( TYPEis_enumeration(t) ) {
	/* For enums and sels (else clause below), we need forward decl's so
	// that if we later come across a type which is an aggregate of one of
	// them, we'll be able to process it.  For selects, we also need a decl
	// of the class itself, while for enum's we don't.  Objects which con-
	// tain an enum can't be generated until the enum is generated.  (The
	// same is basically true for the select, but a sel containing an ent
	// containing a sel needs the forward decl (trust me ;-) ). */
	if ( !TYPEget_head(t) ) {
	    /* Only print this enum if it is an actual type and not a redefi-
	    // nition of another enum.  (Those are printed at the end of the
	    // classes file - after all the actual enum's.  They must be
	    // printed last since they depend on the others.) */
	    strncpy( nm, TYPEget_ctype(t), BUFSIZ );
	    fprintf (classes, "class %ss;\n", nm);
	}
    } else if ( TYPEis_select(t) ) {
	if ( !TYPEget_head(t) ) {
	    /* Same comment as above. */
	    strncpy (nm, SelectName (TYPEget_name (t)), BUFSIZ);
	    fprintf (classes, "class %s;\n", nm);
	    fprintf (classes, "typedef %s * %s_ptr;\n", nm, nm);
	    fprintf (classes, "class %ss;\n", nm);
	    fprintf (classes, "typedef %ss * %ss_ptr;\n", nm, nm);
	}
    } else {
	if ( TYPEis_aggregate(t) ) {
	    i = TYPEget_base_type(t);
	    if ( TYPEis_enumeration(i) || TYPEis_select(i) ) {
		/* One exceptional case - a 1d aggregate of an enum or select.
		// We must wait till the enum/sel itself has been processed.
		// To ensure this, we process all such 1d aggrs in a special
		// loop at the end (in multpass.c).  2d aggrs (or higher), how-
		// ever, can be processed now - they only require GenericAggr
		// for their definition here. */
		goto externln;
	    }
	}
	/* At this point, we'll print typedefs for types which are redefined
	// fundamental types and their aggregates, and for 2D aggregates(aggre-
	// gates of aggregates) of enum's and selects. */
	strncpy (nm, ClassName (TYPEget_name (t)), BUFSIZ);
	fprintf (classes, "typedef %s \t%s;\n", TYPEget_ctype(t), nm);
	if (TYPEis_aggregate(t) ) {
	    fprintf (classes, "typedef %s * \t%sH;\n", nm, nm);
	    fprintf (classes, "typedef %s * \t%s_ptr;\n", nm, nm);
	    fprintf (classes, "typedef %s_ptr \t%s_var;\n", nm, nm);
	}
    }

  externln:
    /* Print the extern statement: */
    strncpy (nm, TYPEtd_name(t), BUFSIZ);
    fprintf( classes, "extern %s \t*%s;\n", GetTypeDescriptorName(t), nm );
}

/* return 1 if it is a multidimensional aggregate at the level passed in
   otherwise return 0;  If it refers to a type that is a multidimensional
   aggregate 0 is still returned. */
int 
isMultiDimAggregateType (const Type t)
{
  if(TYPEget_body(t)->base)
    if(isAggregateType(TYPEget_body(t)->base))
      return 1;
  return 0;
}

/* Get the TypeDescriptor variable name that t's TypeDescriptor references (if
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
*/
int TYPEget_RefTypeVarNm(const Type t,char *buf,Schema schema)
{

	/* It looks like TYPEget_head(t) is true when processing a type 
	   that refers to another type. e.g. when processing "name" in:
	   TYPE name = label; ENDTYPE; TYPE label = STRING; ENDTYPE; DAS */
  if( TYPEget_head(t) )
  {		/* this means that it is defined in an Express TYPE stmt and
		   it refers to another Express TYPE stmt */
    /*  it would be a reference_ type */
    /*  a TypeDescriptor of the form <schema_name>t_<type_name_referred_to> */
    sprintf(buf, "%s%s%s", 
	    SCHEMAget_name( TYPEget_head(t)->superscope ),
	    TYPEprefix(t), TYPEget_name(TYPEget_head(t)) );
    return 1;
  }
  else 
  {
    switch( TYPEget_body(t)->type ) 
      {
      case integer_:
      case real_:
      case boolean_:
      case logical_:
      case string_:
      case binary_:
      case number_:
	/* one of the SCL builtin TypeDescriptors of the form 
	   t_STRING_TYPE, or t_REAL_TYPE */
        sprintf(buf,"%s%s", TD_PREFIX,FundamentalType(t,0));
	return 1;
	break;

      case enumeration_: /* enums don't have a referent type */
      case select_:	 /* selects are handled differently elsewhere, they
			    refer to several TypeDescriptors */
	return 0;
	break;

      case entity_:
	sprintf( buf,"%s", TYPEtd_name(t) );
		 /* following assumes we are not in a nested entity */
		 /* otherwise we should search upward for schema */
/*		 TYPEget_name(TYPEget_body(t)->entity->superscope),
		 ENT_PREFIX,TYPEget_name(t) ); */
	return 1;
	break;

      case aggregate_:
      case array_:
      case bag_:
      case set_:
      case list_:
	/* referent TypeDescriptor will be the one for the element unless it 
	   is a multidimensional aggregate then return 0 */

	if( isMultiDimAggregateType(t) )
	{
	    if(TYPEget_name(TYPEget_body(t)->base))
	    {
		sprintf(buf, "%s%s%s", 
			SCHEMAget_name( TYPEget_body(t)->base->superscope ),
			TYPEprefix(t), TYPEget_name(TYPEget_body(t)->base) );
		return 1;
	    }

	    /* if the multi aggr doesn't have a name then we are out of scope
	       of what this function can do */
	    return 0;
	}
	else {
		/* for a single dimensional aggregate return TypeDescriptor 
		   for element */
		/* being an aggregate implies that base below is not 0 */

	  if( TYPEget_body(TYPEget_body(t)->base)->type == enumeration_ ||
	      TYPEget_body(TYPEget_body(t)->base)->type == select_ )
	  {

	    sprintf( buf,"%s", TYPEtd_name( TYPEget_body(t)->base ) );
	    return 1;
	  }
	  else
	    if(TYPEget_name(TYPEget_body(t)->base))
	    {
		if( TYPEget_body(TYPEget_body(t)->base)->type == entity_)
		{
		    sprintf( buf,"%s", TYPEtd_name(TYPEget_body(t)->base) );
		    return 1;
		}
		sprintf(buf, "%s%s%s", 
			SCHEMAget_name( TYPEget_body(t)->base->superscope ),
			TYPEprefix(t), TYPEget_name(TYPEget_body(t)->base) );
		return 1;
	    }
	    return TYPEget_RefTypeVarNm(TYPEget_body(t)->base, buf, schema);
	}
	break;
      default:
	return 0;
    }
  }
/*  return 0; // this stmt will never be reached */
}


/***** 
   print stuff for types that are declared in Express TYPE statements... i.e. 
   extern descriptor declaration in .h file - MOVED BY DAR to TYPEprint_type-
       defs - in order to print all the Sdaiclasses.h stuff in fedex_plus's
       first pass through each schema.
   descriptor definition in the .cc file
   initialize it in the .init.cc file (DAR - all initialization done in fn
       TYPEprint_init() (below) which is done in fedex_plus's 1st pass only.)
*****/   

void
TYPEprint_descriptions (const Type type, FILES* files, Schema schema)
{
    char tdnm [BUFSIZ],
         typename_buf [MAX_LEN],
         base [BUFSIZ],
         nm [BUFSIZ];
    Type i;

    strncpy (tdnm, TYPEtd_name (type), BUFSIZ);

  /* define type descriptor pointer */
  /*  put extern def in header, put the real definition in .cc file  */

        /*  put extern def in header (DAR - moved to TYPEprint_typedef's -
         *  see fn header comments.)*/
/*
    fprintf(files->inc,"extern %s \t*%s;\n", 
	    GetTypeDescriptorName(type), tdnm);
*/

	/*  in source - declare the real definition of the pointer */
	/*  i.e. in the .cc file                                   */
    fprintf(files -> lib,"%s \t*%s;\n", GetTypeDescriptorName(type), tdnm);

    if(isAggregateType(type)) {
      const char * ctype = TYPEget_ctype (type);

      fprintf(files->inc, "STEPaggregate * create_%s ();\n\n", 
	      ClassName(TYPEget_name(type)));

      fprintf(files->lib, 
	      "STEPaggregate *\ncreate_%s () {  return create_%s();  }\n",
	      ClassName(TYPEget_name(type)), ctype);

      /* this function is assigned to the aggrCreator var in TYPEprint_new */
    }

    if ( TYPEis_enumeration(type) && (i = TYPEget_ancestor(type)) != NULL ) {
	/* If we're a renamed enum type, just print a few typedef's to the
	// original and some specialized create functions: */
	strncpy (base, EnumName( TYPEget_name(i) ), BUFSIZ);
	strncpy (nm, EnumName( TYPEget_name(type) ), BUFSIZ);
	fprintf (files->inc, "typedef %s %s;\n", base, nm);
	strncpy (base, TYPEget_ctype(i), BUFSIZ);
	strncpy (nm, TYPEget_ctype(type), BUFSIZ);
	fprintf (files->inc, "typedef %s %s;\n", base, nm);
	printEnumCreateHdr( files->inc, type );
	printEnumCreateBody( files->lib, type );
	fprintf (files->inc, "typedef %ss * %ss_ptr;\n", nm, nm);
	printEnumAggrCrHdr( files->inc, type );
	printEnumAggrCrBody( files->lib, type );
	return;
    }

    if(!TYPEget_RefTypeVarNm(type, typename_buf, schema)) {
      switch(TYPEget_body(type)->type)
	{
	case enumeration_:
	  TYPEenum_inc_print (type, files -> inc);
	  TYPEenum_lib_print (type, files -> lib);
	  break;

	case select_:
	  /*  the select definitions are done seperately, since they depend 
	      on the others  */
	  /*******
	    TYPEselect_inc_print (type, files -> inc);
	    TYPEselect_lib_print (type, files -> lib);
	    *******/
	  break;
	default:
	  break;
	}
    }
}


static void
printEnumCreateHdr( FILE *inc, const Type type )
    /*
     * Prints a bunch of lines for enumeration creation functions (i.e., "cre-
     * ate_SdaiEnum1()").  Since this is done both for an enum and for "copies"
     * of it (when "TYPE enum2 = enum1"), I placed this code in a separate fn.
     */
{
    const char *nm = TYPEget_ctype (type);

    fprintf (inc, "  SCLP23(Enum) * create_%s ();\n", nm);
}

static void
printEnumCreateBody( FILE *lib, const Type type )
    /*
     * See header comment above by printEnumCreateHdr.
     */
{
    const char *nm = TYPEget_ctype (type);
    char tdnm[BUFSIZ];

    strncpy (tdnm, TYPEtd_name(type), BUFSIZ);

    fprintf (lib, "\nSCLP23(Enum) * \ncreate_%s () \n{\n", nm );
    fprintf (lib, "    return new %s( \"\", %s );\n}\n\n", nm, tdnm );
}

static void
printEnumAggrCrHdr( FILE *inc, const Type type )
    /*
     * Similar to printEnumCreateHdr above for the enum aggregate.
     */
{
    const char *n = TYPEget_ctype (type);
/*    const char *n = ClassName( TYPEget_name(type) ));*/

    fprintf (inc, "  STEPaggregate * create_%ss ();\n", n);
}

static void
printEnumAggrCrBody( FILE *lib, const Type type )
{
    const char *n = TYPEget_ctype(type);
    char tdnm[BUFSIZ];

    strncpy (tdnm, TYPEtd_name(type), BUFSIZ);

    fprintf (lib, "\nSTEPaggregate * \ncreate_%ss ()\n{\n", n);
    fprintf (lib, "    return new %ss( %s );\n}\n", n, tdnm);
}

void
TYPEprint_init (const Type type, FILE *ifile, Schema schema)
{
    char tdnm [BUFSIZ];
    char typename_buf[MAX_LEN];

    strncpy (tdnm, TYPEtd_name (type), BUFSIZ);

    if(isAggregateType(type)) {
      if(!TYPEget_head(type))
      {
	if(TYPEget_body(type)->lower)
	  fprintf(ifile, "\t%s->Bound1(%d);\n", tdnm, 
		  TYPEget_body(type)->lower->u.integer);
	if(TYPEget_body(type)->upper)
	  fprintf(ifile, "\t%s->Bound2(%d);\n", tdnm, 
		  TYPEget_body(type)->upper->u.integer);
	if(TYPEget_body(type)->flags.unique)
	  fprintf(ifile, "\t%s->UniqueElements(\"LTrue\");\n", tdnm);
/*	  fprintf(ifile, "\t%s->UniqueElements(%d);\n", tdnm, 
		  TYPEget_body(type)->flags.unique); */
	if(TYPEget_body(type)->flags.optional)
	  fprintf(ifile, "\t%s->OptionalElements(\"LTrue\");\n", tdnm);
/*	  fprintf(ifile, "\t%s->OptionalElements(%d);\n", tdnm, 
		  TYPEget_body(type)->flags.optional);*/
      }
    }

    /* fill in the TD's values in the SchemaInit function (it is already 
	declared with basic values) */

    if(TYPEget_RefTypeVarNm(type, typename_buf, schema)) {
      fprintf(ifile, "\t%s->ReferentType(%s);\n", tdnm, typename_buf);
    } else {
      switch(TYPEget_body(type)->type)
	{
	case aggregate_: /* aggregate_ should not happen? DAS */
	case array_:
	case bag_:
	case set_:
	case list_:
	{
	  const char * ctype = TYPEget_ctype (type);

	  if( isMultiDimAggregateType(type) ) 
	  {
	      print_typechain(ifile, TYPEget_body(type)->base, 
			      typename_buf, schema);
	      fprintf(ifile,"	%s->ReferentType(%s);\n", tdnm, 
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
    if ( TYPEis_select(type) ) {
	TYPEselect_init_print( type, ifile, schema );
    }
#ifdef NEWDICT
    /* DAS New SDAI Dictionary 5/95 */
    /* insert the type into the schema descriptor */
	fprintf(ifile,
    "\t((SDAIAGGRH(Set,DefinedTypeH))%s%s->Types())->Add((DefinedTypeH)%s);\n",
		SCHEMA_PREFIX,SCHEMAget_name(schema),tdnm);
#endif
    /* insert into type dictionary */
    fprintf(ifile,"\treg.AddType (*%s);\n", tdnm);
}

/* print name, fundamental type, and description initialization function 
   calls */

void 
TYPEprint_nm_ft_desc (Schema schema, const Type type, FILE* f, char *endChars)
{

	fprintf(f,"\t\t  \"%s\",\t// Name\n",
		PrettyTmpName (TYPEget_name(type)));
	fprintf(f,"\t\t  %s,\t// FundamentalType\n",
		FundamentalType(type,1));
	fprintf(f,"\t\t  %s%s,\t// Originating Schema\n",
		SCHEMA_PREFIX,SCHEMAget_name(schema) );
	fprintf(f,"\t\t  \"%s\"%s\t// Description\n",
		TypeDescription(type), endChars);
}

#if 0
    if (TYPEget_head(type)) { 
	if (!streq("",TYPEget_name(type))) {
	    /* type ref with name */
	    fprintf(files->init,
		    "\t%s->ReferentType (%s%s%s);\n",
		    tdnm, 
 		    SCHEMAget_name( TYPEget_head(type)->superscope ),
		    TYPEprefix(type), 
		    TYPEget_name(TYPEget_head(type)) );
	    switch(TYPEget_body(type)->type)
	    {
		case aggregate_: /* aggregate_ should not happen? DAS */
		case array_:
		case bag_:
		case set_:
		case list_:
		    fprintf(files->inc, "STEPaggregate * create_%s ();\n\n", 
			    ClassName(TYPEget_name(type)));
		    fprintf(files->lib, 
		   "STEPaggregate *\ncreate_%s () {  return create_%s();  }\n",
			ClassName(TYPEget_name(type)), 
			TYPEget_ctype(TYPEget_head(type)));
		    break;
	    }
	} else if (streq("",TYPEget_name(type))) {
	    /* no name, recurse */
	    char callee_buffer[MAX_LEN];
	    print_typechain(files -> init, TYPEget_head(type), 
			    callee_buffer, schema);
	    fprintf(files->init,"	%s->ReferentType(%s);\n",
	      tdnm, callee_buffer);
	}
    } 
#endif
/* new space for a variable of type TypeDescriptor (or subtype).  This 
   function is called for Types that have an Express name. */

void 
TYPEprint_new (const Type type, FILE *create, Schema schema)
{
    Linked_List wheres;
    char *whereRule, *whereRule_formatted;
    int whereRule_formatted_size = 0;    
    char *ptr,*ptr2;

    Type tmpType = TYPEget_head(type);
    Type bodyType = tmpType;

    const char * ctype = TYPEget_ctype (type);

    /* define type definition */
    /*  in source - the real definition of the TypeDescriptor   */

    if( TYPEis_select(type) )
    {
	fprintf(create,
	  "\t%s = new SelectTypeDescriptor (\n\t\t  ~%s,\t//unique elements,\n",
		TYPEtd_name (type),
/*		! any_duplicates_in_select (SEL_TYPEget_items(type)) );*/
/*		unique_types (SEL_TYPEget_items (type) ) );*/
                non_unique_types_string(type));
/* DAS ORIG SCHEMA FIX */
	TYPEprint_nm_ft_desc (schema, type, create, ",");

	fprintf(create,
		"\t\t  (SelectCreator) create_%s);\t// Creator function\n",
		SelectName( TYPEget_name(type) ) );
    }
    else 
/*DASSSS	if (TYPEget_name(t)) {
		strcat(buf," ");	
		strcat(buf,PrettyTmpName (TYPEget_name(t)));
*/
	switch( TYPEget_body(type)->type) 
	{
	    

	  case boolean_:

	    fprintf(create,"\t%s = new EnumTypeDescriptor (\n",
		    TYPEtd_name (type));

	    /* fill in it's values	*/
	    TYPEprint_nm_ft_desc (schema, type, create, ",");
	    fprintf(create,
		"\t\t  (EnumCreator) create_BOOLEAN);\t// Creator function\n");
	    break;

	  case logical_:

	    fprintf(create,"\t%s = new EnumTypeDescriptor (\n",
		    TYPEtd_name (type));

	    /* fill in it's values	*/
	    TYPEprint_nm_ft_desc (schema, type, create, ",");
	    fprintf(create,
		"\t\t  (EnumCreator) create_LOGICAL);\t// Creator function\n");
	    break;

	  case enumeration_:

	    fprintf(create,"\t%s = new EnumTypeDescriptor (\n",
		    TYPEtd_name (type));

	    /* fill in it's values	*/
	    TYPEprint_nm_ft_desc (schema, type, create, ",");
/*
	    fprintf(create,
		    "\t\t  (EnumCreator) create_%s);\t// Creator function\n",
		    TYPEget_ctype(type) );
   */
	    /* DASCUR */

	    /* get the type name of the underlying type - it is the type that 
	       needs to get created */

	    tmpType = TYPEget_head(type);
	    if (tmpType) {

		bodyType = tmpType;

		while( tmpType )
		{
		    bodyType = tmpType;
		    tmpType = TYPEget_head(tmpType);
		}

		fprintf(create,
		      "\t\t  (EnumCreator) create_%s);\t// Creator function\n",
			TYPEget_ctype(bodyType) );
	    }
	    else
		fprintf(create,
		    "\t\t  (EnumCreator) create_%s);\t// Creator function\n",
		    TYPEget_ctype(type) );
	    break;

	  case aggregate_:
	  case array_:
	  case bag_:
	  case set_:
	  case list_:

	    fprintf(create,"\n\t%s = new %s (\n",
		    TYPEtd_name (type), GetTypeDescriptorName(type));
	    
	    /* fill in it's values	*/
	    TYPEprint_nm_ft_desc (schema, type, create, ",");

	    fprintf(create,
	       "\t\t  (AggregateCreator) create_%s);\t// Creator function\n\n",
		    ClassName( TYPEget_name(type) ) );
	    break;

	  default:
	    fprintf(create,"\t%s = new TypeDescriptor (\n",
		    TYPEtd_name (type));
	    
	    /* fill in it's values	*/
	    TYPEprint_nm_ft_desc (schema, type, create, ");");

	    break;
	}
	/* add the type to the Schema dictionary entry */
	fprintf(create,"\t%s%s->AddType(%s);\n",SCHEMA_PREFIX, SCHEMAget_name(schema), TYPEtd_name (type));


	wheres = type->where;

	if (wheres) {
	    fprintf(create, "\t%s->_where_rules = new Where_rule__list;\n",
		    TYPEtd_name (type));

	    LISTdo(wheres,w,Where)
	      whereRule = EXPRto_string(w->expr);
	      ptr2 = whereRule;
	      
	      if(whereRule_formatted_size == 0) 
	      {
		  whereRule_formatted_size = 3*BUFSIZ;
		  whereRule_formatted = (char *)malloc(sizeof(char)*whereRule_formatted_size);
	      } 
	      else if( (strlen(whereRule) + 300) > whereRule_formatted_size )
	      {
		  free(whereRule_formatted);
		  whereRule_formatted_size = strlen(whereRule) + BUFSIZ;
		  whereRule_formatted = (char *)malloc(sizeof(char)*whereRule_formatted_size);
	      }
	      whereRule_formatted[0] = '\0';
	      if (w->label) {
		  strcpy(whereRule_formatted,w->label->name);
		  strcat(whereRule_formatted,": (");
		  ptr = whereRule_formatted + strlen(whereRule_formatted);
		  while(*ptr2) {
		      if(*ptr2 == '\n')
			;
		      else if(*ptr2 == '\\') {
			  *ptr = '\\'; ptr++;
			  *ptr = '\\'; ptr++;

		      } else if(*ptr2 == '(') {
			/*  *ptr = '\\'; ptr++;
			  *ptr = 'n';  ptr++;
			  *ptr = '\\'; ptr++;
			  *ptr = 't';  ptr++; */
			  *ptr = *ptr2; ptr++;
		      } else {
			  *ptr = *ptr2;
			  ptr++;
		      }
		      ptr2++;
		  }
		  *ptr = '\0';

		  strcat(ptr,");\\n");
	      } else {
		  /* no label */
		  strcpy(whereRule_formatted,"(");
		  ptr = whereRule_formatted + strlen(whereRule_formatted);

		  while(*ptr2) {
		      if(*ptr2 == '\n')
			;
		      else if(*ptr2 == '\\') {
			  *ptr = '\\'; ptr++;
			  *ptr = '\\'; ptr++;

		      } else if(*ptr2 == '(') {
			/*  *ptr = '\\'; ptr++;
			  *ptr = 'n';  ptr++;
			  *ptr = '\\'; ptr++;
			  *ptr = 't';  ptr++; */
			  *ptr = *ptr2; ptr++;
		      } else {
			  *ptr = *ptr2;
			  ptr++;
		      }
		      ptr2++;
		  }
		  *ptr = '\0';
		  strcat(ptr,");\\n");
	      }
	      fprintf(create, "\twr = new Where_rule(\"%s\");\n",whereRule_formatted);
	    fprintf(create, "\t%s->_where_rules->Append(wr);\n",
		    TYPEtd_name (type));

	      free(whereRule);
	      ptr2 = whereRule = 0;
	    LISTod
	    free(whereRule_formatted);
	}
}

/*
      case aggregate_:
      case array_:
      case bag_:
      case set_:
      case list_:
      case select_:
      case enumeration_:
      case boolean_:
      case logical_:
      case integer_:
      case real_:
      case number_:
      case string_:
      case binary_:
      case number_:
      case generic_:
      case entity_:
*/
