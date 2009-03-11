#include <stdlib.h>
#include <stdio.h>
#include "../express/express.h"
#include "../express/resolve.h"
#include "classes.h"

#define SCHEMA_FILE_PREFIX	"Sdai"

int multiple_inheritance = 0;

char ** aggrNameList = 0;
int aggrNameList_size = 0;
int aggrNameList_maxsize = 200;

void printAggrNameList(FILE* idl_file) 
{
    int i = 0;
    fprintf(idl_file, "/*\nThe AggrNameList list:\n");
    while(i < aggrNameList_size)
    {
	fprintf(idl_file, "%d: %s\n", i, aggrNameList[i]);
	i++;
    }
    fprintf(idl_file, "*/\n\n");
}

int AddToAggrNameList(const char *aggr, FILE* idl_file) 
{
    char ** tmpList = 0;
    int found = 0;
    int i = 0;

/*    fprintf(idl_file, " CALLED addtoagrr w/%s \n", aggr);*/

    if(!aggrNameList)
      aggrNameList = (char**)malloc( sizeof(char*)*aggrNameList_maxsize );
/*
    while( (i < aggrNameList_maxsize) && 
	    (strcmp(aggr, aggrNameList[i]) < 0) )
*/
    while( (i < aggrNameList_size) && 
	   strcmp(aggr, aggrNameList[i]) )
    {
	i++;
    }

    /* found entry already */
    if(i < aggrNameList_size)
    {
	return 0;
    }
    else if(i < aggrNameList_maxsize)
    {
	aggrNameList[i] = (char*)malloc( sizeof(char)*(strlen(aggr)+1) );
	strcpy(aggrNameList[i],aggr);
	aggrNameList_size++;
    }
    else
    {
	tmpList = (char**)malloc( sizeof(char*)*(aggrNameList_maxsize*2) );
	aggrNameList_maxsize = aggrNameList_maxsize*2;
	i = 0;
	while(i < aggrNameList_size)
	{
	    tmpList[i] = aggrNameList[i];
	    i++;
	}
	free(aggrNameList);
	aggrNameList = tmpList;
	aggrNameList[i] = (char*)malloc( sizeof(char)*(strlen(aggr)+1) );
	strcpy(aggrNameList[i],aggr);
	aggrNameList_size++;
    }
    return 1;
}


const char * 
EnumCElementName (Type type, Expression expr)  {

  static char buf [BUFSIZ];
  sprintf (buf,"%s__", 
	   EnumName (TYPEget_name (type)));
  strcat(buf, StrToLower (EXPget_name(expr)));

  return buf;
}

const char *
IDLClassName (const char * oldname)
{
    int i= 0, j = 0;
    static char newname [BUFSIZ];
    if (!oldname)  return ("");
    
    
/*
    strcpy (newname, ENTITYCLASS_PREFIX)    ;
    j = strlen (ENTITYCLASS_PREFIX)    ;
*/
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
IDLENTITYget_classname (Entity ent)
{
    const char * oldname = ENTITYget_name (ent);
    return (IDLClassName (oldname));
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
	    strcpy(retval,"ERROR aggr_of_aggr");
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

/***********************************************************************
 I changed this function from FILEcreate() in class_misc.c in this way. 
 The message that gets printed to the file says fedex_os instead 
 of fedex_plus - DAS
***********************************************************************/
FILE*
myFILEcreate (const char * filename)
{
    FILE* file;
    const char * fn;
    
    if ((file = fopen (filename, "w")) == NULL) {
	printf ("**Error in myFILEcreate:  unable to create file %s ** \n", filename);
	return (NULL);
    }

    fprintf (file, "#ifndef  %s\n", fn = StrToConstant (filename));
    fprintf (file, "#define  %s\n", fn );

	fprintf(file,"// This file was generated by fedex_os.  You probably don't want to edit\n");
	fprintf(file,"// it since your modifications will be lost if fedex_os is used to\n");
	fprintf(file,"// regenerate it.\n");
    return (file);
    
}

void
SCOPEPrintHooks(Scope scope, FILE *os_hooks_file,Schema schema,Express model)
{
    const char * entnm;
    Linked_List list;
    char nm[BUFSIZ];
    char classNm [BUFSIZ];

    DictionaryEntry de;
    int index = 0;

    /* do \'new\'s for types descriptors  */
/*
    SCOPEdo_types(scope,t,de)
      TYPEprint_new (t, files,schema);
    SCOPEod;
*/
    /* do \'new\'s for entity descriptors  */
    list = SCOPEget_entities_superclass_order (scope);
    fprintf (os_hooks_file, "\n#if 0\n"); 
    fprintf (os_hooks_file, "// comment all of these out since they don't work with ObjectStore (an ObjectStore bug).\n\n"); 

    fprintf (os_hooks_file, "// ***********  Make the access hook call for each entity ********** \n"); 
    LISTdo (list, e, Entity);
	entnm = IDLENTITYget_classname (e);

	fprintf (os_hooks_file, "    db->set_access_hooks(\"%s\", %s_access_hook_in, 0,0,0,0);\n", entnm, entnm); 
    LISTod;

    fprintf (os_hooks_file, "// ***********  Make the access hook call for each type as needed ********** \n"); 
    SCOPEdo_types(scope,t,de)
      if( TYPEis_select(t) )
      {
	  strncpy (classNm, SelectName (TYPEget_name (t)), BUFSIZ);
	  fprintf (os_hooks_file, "    db->set_access_hooks(\"%s\", %s_access_hook_in, 0,0,0,0);\n", classNm, classNm); 
      }
      else if ( TYPEis_enumeration(t) ) {
	  strncpy (classNm, TYPEget_ctype (t), BUFSIZ);
	  fprintf (os_hooks_file, "    db->set_access_hooks(\"%s\", %s_access_hook_in, 0,0,0,0);\n", classNm, classNm); 
	  fprintf (os_hooks_file, "    db->set_access_hooks(\"%ss\", %ss_access_hook_in, 0,0,0,0);\n", classNm, classNm); 
      }
      else if ( TYPEis_aggregate(t) ) {
	  strncpy (classNm, SelectName (TYPEget_name (t)), BUFSIZ);
	  fprintf (os_hooks_file, "    aggr db->set_access_hooks(\"%s\", %s_access_hook_in, 0,0,0,0);\n", classNm, classNm); 
      }
    SCOPEod;
    fprintf (os_hooks_file, "\n#endif\n"); 

    LISTfree(list);
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

    if (isAggregateType (t)) {
	strcpy(retval,GetAggrElemType(t));
#if 0
	bt = TYPEget_nonaggregate_base_type (t);
/*
    if (isAggregateType (t)) {
	bt = TYPEget_body(t)->base;
*/
        if (isAggregateType(bt)) {
		return("aggr_of_aggr");
	}

	class = TYPEget_type (bt);

	/*      case TYPE_INTEGER:	*/
	if (class == Class_Integer_Type) /* return ( "IntAggregate" );*/
	    strcpy (retval, "Long");
	
	/*      case TYPE_REAL:
		case TYPE_NUMBER:	*/
	if ((class == Class_Number_Type) || ( class == Class_Real_Type))
	    strcpy (retval, "Double");

	if (class == Class_Entity_Type) 
	{
	    strcpy (retval, IdlEntityTypeName (bt));
	}

	
	/*      case TYPE_ENUM:		*/
        /*	case TYPE_SELECT:	*/
	if ((class == Class_Enumeration_Type) 
	    || (class == Class_Select_Type) )  {
/*
	    strcpy (retval, ClassName (TYPEget_name (bt)));
*/
	    strcpy (retval, TYPEget_ctype(bt));
	}

        /*	case TYPE_LOGICAL:	*/
	if (class == Class_Logical_Type)
	    strcpy (retval, "Logical"); 

        /*	case TYPE_BOOLEAN:	*/
	if (class == Class_Boolean_Type)
	    strcpy (retval, "Bool");

        /*	case TYPE_STRING:	*/
	if (class == Class_String_Type) /* return("StringAggregate");*/
	    strcpy (retval, "String");

        /*	case TYPE_BINARY:	*/
	if (class == Class_Binary_Type) /* return("BinaryAggregate");*/
	    strcpy (retval, "Binary");
#endif
/*************************/
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
/*************************/
    }

    /*  the rest is for things that are not aggregates	*/
    
    class = TYPEget_type(t);

    /*    case TYPE_LOGICAL:	*/
    if ( class == Class_Logical_Type)
        return ("Logical"); 
/*        return ("P26::Logical"); */

    /*    case TYPE_BOOLEAN:	*/
    if (class == Class_Boolean_Type)
        return ("Bool"); 
/*        return ("P26::Bool"); */

    /*      case TYPE_INTEGER:	*/
    if ( class == Class_Integer_Type)
	return ("long");

    /*      case TYPE_REAL:
	    case TYPE_NUMBER:	*/
    if ((class == Class_Number_Type) || ( class == Class_Real_Type))
	return ("double")      ; 

    /*	    case TYPE_STRING:	*/
    if (class == Class_String_Type)
	return ("string");

    /*	    case TYPE_BINARY:	*/
    if ( class == Class_Binary_Type)
	return ("Binary");

    /*      case TYPE_ENTITY:	*/
    if ( class == Class_Entity_Type)
      {
	  /* better do this because the return type might go away */
      strcpy (retval, IdlEntityTypeName (t));
      return retval;
    }
    /*    case TYPE_ENUM:	*/
    /*    case TYPE_SELECT:	*/
    if (class == Class_Enumeration_Type)
    {
      strncpy (retval, IdlEntityTypeName (t), BUFSIZ-2);
/*      strcat (retval, "_var");*/
      return retval;
    }
    if (class == Class_Select_Type)  {
      return (IdlEntityTypeName (t));
    }

    /*  default returns undefined   */
    return ("SCLundefined");
}    

char * 
generate_attribute_name( Variable a, char *out )
{
   char *temp, *p;

   temp = EXPRto_string( VARget_name(a) );
   p = temp;
   if (! strncmp (StrToLower (p), "self\\", 5)) 
       p = p +5;
   /*  copy p to out  */
   strncpy( out, StrToLower (p), BUFSIZ );
/* This is an error in Part26
   strcat( out, "_");
*/
   out[0] = toupper(p[0]);
   /*  if there\'s an illegal character in out,
       change it to a _	 */
   while (p = strrchr(out,'.'))
     *p = '_';

   free( temp );
   return out;
}

IDL_Print_Enum(FILE *idl_file, const Type type)
{
    Expression expr;
    DictionaryEntry de;
    int first_time = 1;

    fprintf (idl_file, "enum %s {\n", EnumName (TYPEget_name (type)));

    DICTdo_type_init(ENUM_TYPEget_items(type),&de,OBJ_ENUM);
    while (0 != (expr = (Expression)DICTdo(&de))) {
	/*  print the elements of the c++ enum type  */
	if(!first_time) 
	  fprintf (idl_file,",\n");
	else
	  first_time = 0;

	fprintf (idl_file,"\t%s", EnumCElementName (type, expr));
    }

    fprintf (idl_file, ",\n\t%s_unset\n};\n", EnumName(TYPEget_name(type)));
    fprintf (idl_file,"\n");
}

void
SCOPEPrint(Scope scope, FILE *idl_file,Schema schema,Express model)
{
    char entnm [BUFSIZ];
    char attrnm [BUFSIZ];

    Linked_List list;
    char nm[BUFSIZ];

    Linked_List attr_list;

    const char * idl_type, * etype;
    Entity super =0;

    DictionaryEntry de;
    int index = 0;

    Type bt;
    Class_Of_Type class;

    /* do \'new\'s for types descriptors  */
/*
    SCOPEdo_types(scope,t,de)
      TYPEprint_new (t, files,schema);
    SCOPEod;
*/
/*************************/

    list = SCOPEget_entities_superclass_order (scope);

    fprintf (idl_file, "/************  Specify forward idl interface declarations for the entities for the %s schema ***********/ \n\n", SCHEMAget_name(schema)); 

    LISTdo (list, e, Entity);

/*  ENTITYhead_print ( entity, idl_file,schema);*/
    {
	strncpy (entnm, IDLENTITYget_classname (e), BUFSIZ);

	fprintf (idl_file, "interface %s;\n", entnm);
    }
    LISTod;

    /*  fill in the values for the type descriptors */
    /*  and print the enumerations  */
    fprintf (idl_file, "\n/***********  Types from the EXPRESS schema **********/ \n"); 

/*
    SCOPEdo_types(scope,t,de)
        // Do the non-redefined enumerations:
        if (    ( t->search_id == CANPROCESS )
	     && ! ( TYPEis_enumeration(t) && TYPEget_head(t) ) ) {
	    TYPEprint_descriptions (t, files, schema);
	    if ( !TYPEis_select(t) ) {
		// Selects have a lot more processing and are done below.
		t->search_id = PROCESSED;
	    }
	}
    SCOPEod;
*/
/*//////////
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
/////////////////*/

    SCOPEdo_types(scope,type,de)
      switch(TYPEget_body(type)->type)
      {
	case enumeration_:
	{
	    IDL_Print_Enum(idl_file, type);
	    break;
	}
	case select_:
	{
	    /*   do the select aggregates here  */
	    strncpy (nm, SelectName (TYPEget_name (type)), BUFSIZ);
	    fprintf(idl_file, "/* SELECT %s not supported */\n", nm);
	    break;
	}
	case reference_: 
	{
	    strncpy (nm, TYPEget_name (type), BUFSIZ);
	    fprintf(idl_file, "/* REFERENCE %s not supported */\n", nm);
	    break;
	}
	case integer_: 
	{
	    strncpy (nm, FirstToUpper(TYPEget_name (type)), BUFSIZ);
	    fprintf(idl_file, "typedef long %s;\n", nm);
	    break;
	}
	case real_: 
	{
	    strncpy (nm, FirstToUpper(TYPEget_name (type)), BUFSIZ);
	    fprintf(idl_file, "typedef double %s;\n", nm);
	    break;
	}
	case string_: 
	{
	    strncpy (nm, FirstToUpper(TYPEget_name (type)), BUFSIZ);
	    fprintf(idl_file, "typedef string %s;\n", nm);
	    break;
	}
	case binary_: 
	{
	    strncpy (nm, FirstToUpper(TYPEget_name (type)), BUFSIZ);
	    fprintf(idl_file, "/* BINARY %s not supported */\n", nm);
	    break;
	}
	case boolean_: 
	{
	    strncpy (nm, FirstToUpper(TYPEget_name (type)), BUFSIZ);
	    fprintf(idl_file, "typedef Bool %s;\n", nm);
/*	    fprintf(idl_file, "typedef P26::Bool %s;\n", nm);*/
	    break;
	}
	case logical_: 
	{
	    strncpy (nm, FirstToUpper(TYPEget_name (type)), BUFSIZ);
	    fprintf(idl_file, "typedef Logical %s;\n", nm);
/*	    fprintf(idl_file, "typedef P26::Logical %s;\n", nm);*/
	    break;
	}
	case number_: 
	{
	    strncpy (nm, FirstToUpper(TYPEget_name (type)), BUFSIZ);
	    fprintf(idl_file, "/* NUMBER is not supported correctly */\ntypedef double %s;\n", nm);
	    break;
	}
	case aggregate_: /* aggregate_ should not happen? DAS */
	case array_:
	case bag_:
	case set_:
	case list_:
	{
	    fprintf(idl_file, "typedef sequence<");
	    fprintf(idl_file, GetAggrElemType(type));
	    fprintf(idl_file, "> %s;\n",
		    TYPEget_idl_type (type));

	    fprintf(idl_file, "typedef %s %s;\n", 
		    TYPEget_idl_type (type),
		    FirstToUpper(TYPEget_name (type)));
	    if(AddToAggrNameList(TYPEget_idl_type (type),idl_file))
	    {
		strncpy (nm, FirstToUpper(TYPEget_name (type)), BUFSIZ);
	    }
	    fprintf(idl_file, "/* AGGREGATE %s */\n", 
		    FirstToUpper(TYPEget_name (type)));
	    break;
	}
/*
	case aggregate_:
	{
	    strncpy (nm, FirstToUpper(TYPEget_name (type)), BUFSIZ);
	    fprintf(idl_file, "AGGREGATE %s\n", nm);
	    break;
	}
	case array_:
	{
	    strncpy (nm, FirstToUpper(TYPEget_name (type)), BUFSIZ);
	    fprintf(idl_file, "ARRAY %s\n", nm);
	    break;
	}
	case bag_:
	{
	    strncpy (nm, FirstToUpper(TYPEget_name (type)), BUFSIZ);
	    fprintf(idl_file, "BAG %s\n", nm);
	    break;
	}
	case set_:
	{
	    strncpy (nm, FirstToUpper(TYPEget_name (type)), BUFSIZ);
	    fprintf(idl_file, "SET %s\n", nm);
	    break;
	}
	case list_:
	{
	    strncpy (nm, FirstToUpper(TYPEget_name (type)), BUFSIZ);
	    fprintf(idl_file, "LIST %s\n", nm);
	    break;
	}
*/
	default:
	{
	    strncpy (nm, FirstToUpper(TYPEget_name (type)), BUFSIZ);
	    fprintf(idl_file, "OTHER %s\n", nm);
	    break;
	}
      }
    SCOPEod;
/*      TYPEprint_descriptions (t, files, schema);*/


    /*  build the typedefs  */
/*
    SCOPEdo_types(scope,t,de)
      if( ! (TYPEis_select(t) ))
	TYPEprint_typedefs (t, files ->inc, schema);
    SCOPEod;
*/
    /*  do the select definitions next, since they depend on the others  */
/*
    fprintf (files->inc, "\n//\t***** Build the SELECT Types  \t\n");
    fprintf (files->init, "\n//\t***** Add the TypeDescriptor's to the SELECT Types  \t\n");
    SCOPEdo_types(scope,t,de)
      if( TYPEis_select(t) )
	TYPEselect_print (t, files, schema);
    SCOPEod;
*/
/*************************/
    fprintf (idl_file, "/***********  Generate sequences for aggregates defined with attribute definitions **********/ \n\n"); 

/********************************************************/
    LISTdo (list, e, Entity);
    {
	attr_list = ENTITYget_attributes (e);
	LISTdo (attr_list, a, Variable)
	{
	    if (VARget_initializer (a) == EXPRESSION_NULL) 
	    {
		idl_type = TYPEget_idl_type (VARget_type (a));
		generate_attribute_name( a, attrnm );
		if (isAggregate (a)) 
		{
		    fprintf(idl_file, 
			    "/* AGGREGATE %s in entity %s, attr: %s */\n", 
			    idl_type, IDLENTITYget_classname(e), attrnm);
		    if(AddToAggrNameList(idl_type,idl_file))
		    {
			fprintf(idl_file, "typedef sequence<");
			fprintf(idl_file, GetAggrElemType(VARget_type(a)));
			fprintf(idl_file, "> %s;\n", idl_type);
		    }
		    fprintf (idl_file, "\n");
		}
	    }	
	}
	LISTod;
    }
    LISTod;
    printAggrNameList(idl_file);
/********************************************************/

    fprintf (idl_file, "/************  Specify the entities for the %s schema ***********/ \n", SCHEMAget_name(schema)); 

    LISTdo (list, e, Entity);

/*    ENTITYhead_print ( entity, idl_file,schema); */
    {
	strncpy (entnm, IDLENTITYget_classname (e), BUFSIZ);

	fprintf (idl_file, "\ninterface %s  :  ", entnm);

	/* inherit from either supertype entity class or root class of 
	   all - i.e. SCLP23(Application_instance) */

/*	if(multiple_inheritance)
	{*/
	    list = ENTITYget_supertypes (e);
	    if (! LISTempty (list)) {
		super = (Entity)LISTpeek_first(list);
	    }
	    else
	      super =0;

/*	}
	else /* the old way * /
	  super = ENTITYput_superclass (e);*/

	if (super)
	  fprintf( idl_file, " %s \n{\n", IDLENTITYget_classname(super) );
	else
	  fprintf (idl_file, " IDL_Application_instance \n{\n");
/*	  fprintf (idl_file, " P26::Application_instance \n{\n");*/

/*	DataMemberPrint(Entity entity, FILE* file,Schema schema)*/
    
	attr_list = ENTITYget_attributes (e);
	LISTdo (attr_list, a, Variable)
	{
	    if (VARget_initializer (a) == EXPRESSION_NULL) 
	    {
		idl_type = TYPEget_idl_type (VARget_type (a));
		generate_attribute_name( a, attrnm );
		if (!strcmp (idl_type, "SCLundefined") ) {
		    printf ("WARNING:  in entity %s:\n", ENTITYget_name (e)); 
		    printf ("\tthe type for attribute  %s is not fully ",
			    "implemented\n", attrnm);
		}
/*
		if (TYPEis_entity (VARget_type (a)))
		  fprintf (idl_file, "\tP26::Application_instance_ptr %s;", attrnm);
		else
*/
		  fprintf (idl_file, "    attribute %s %s;", idl_type, attrnm);
		if (VARget_optional (a)) fprintf (idl_file, "    //  OPTIONAL");
		if (isAggregate (a)) 		{
		    /*  if it's a named type, comment the type  */
		    if (etype = TYPEget_name
			(TYPEget_nonaggregate_base_type (VARget_type (a))))
		      fprintf (idl_file, "\t// of %s", etype);
		}

		fprintf (idl_file, "\n");
	    }	
	}
	LISTod;
	fprintf (idl_file, "\n}; /* end interface %s */\n", entnm);
    }
    LISTod;

/*    LISTfree(list);*/
    
    fprintf (idl_file, "\n /* end of interface definitions */\n");
}

/* This corresponds to fedex_plus's SCHEMAprint() function.
   It creates and closes the files that will be generated. */

void 
idlSCHEMAprint (Schema schema, FILES* files, Express model)
{
    char fnm [BUFSIZ], *np;
    char schnm[BUFSIZ];

    FILE *idl_file;
    FILE *os_hooks_file_h;
    FILE *os_hooks_file_cc;
    FILE* os_makefile = files -> make;
/*
    FILE* idl_file;

    FILE* incfile;
    FILE* schemafile = files -> incall;
    FILE* schemainit = files -> initall;
    FILE *initfile;    
*/    

    /**********  create files based on name of schema	***********/
    /*  return if failure			*/
    /*  1.  header file				*/
    sprintf(schnm,"%s%s", SCHEMA_FILE_PREFIX, 
	    StrToUpper (SCHEMAget_name(schema)));
    sprintf(fnm,"idl_%s.idl",schnm);

/* print a new make_schema called make_schema_os that includes the additional
   ObjectStore files generated */
    fprintf (os_makefile, "\\\n\t%s.o ",schnm);
    fprintf (os_makefile, "\\\n\t%s.init.o ",schnm);
    fprintf (os_makefile, "\\\n\tosdb_%s.o ",schnm);

    if (! (idl_file = myFILEcreate (fnm)))  return;
    fprintf (idl_file, "/* %cId$  */ \n",'\044');

    fprintf (idl_file, "\n// This file specifies built-in schema-independent SDAI IDL types \n");
/*    fprintf (idl_file, "#include <p26sdai.idl> \n");*/
    fprintf (idl_file, "#include <sdaitypes.idl> \n");
    fprintf (idl_file, "#include <appinstance.idl> \n");

	/* really, create calls for entity constructors */
    SCOPEPrint(schema, idl_file,schema,model);

    fprintf (idl_file, "\n /******** end of interface definitions for entities in  the %s schema ********/\n\n",schnm);

    /**********  close the files	***********/
    FILEclose (idl_file);

/****************
Generate access hooks for reinitializing EntityDescriptor* in each SCLP23(Application_instance)
*****************/
/*
    sprintf(fnm,"osdb_%s.cc",schnm);
    if (! (os_hooks_file_cc = myFILEcreate (fnm)))  return;

    sprintf(fnm,"osdb_%s.h",schnm);
    if (! (os_hooks_file_h = myFILEcreate (fnm)))  return;
    fprintf (os_hooks_file_cc,"#include <%s>\n", fnm);
*/
	/* really, create calls for entity constructors */
/*
    SCOPEPrintHooks(schema, os_hooks_file_cc,schema,model);
*/
    /**********  close the files	***********/
/*
    FILEclose (os_hooks_file_h);
    FILEclose (os_hooks_file_cc);
*/
}


void
os_print_schemas_separate (Express express, FILES* files)
{
    DictionaryEntry de;
    Schema schema;
    
    /**********  do all schemas	***********/
    DICTdo_init(express->symbol_table,&de);
    while (0 != (schema = (Scope)DICTdo(&de))) {
	    idlSCHEMAprint(schema, files, express);	
    }
    fprintf (files->make, "\n");
    fclose(files->make);
}

static void
fedex_os_usage()
{
        fprintf(stderr,"usage: %s [-v] [-d #] [-p <object_type>] {-w|-i <warning>} express_file\n",EXPRESSprogram_name);
	exit(2);
}

int Handle_FedPlus_Args(int i, char *arg)
{
    if( ((char)i == 's') || ((char)i == 'S'))
      printf("used -%c option\n", (char)i);
/*
	multiple_inheritance = 0;
*/
    return 0;
}

void 
print_file(Express express)
{
	extern void RESOLUTIONsucceed(void);
	File_holder  files;

	/* print a new make_schema called make_schema_os that includes the 
	   additional ObjectStore files generated */
	if (((files.make) = fopen ("make_schema_os", "w")) == NULL) {
	    printf ("**Error in print_file:  unable to open file make_schema_os ** \n");
	    return;
	}
/* 
May want to change the generator to generate a new file that defines some 
of these:
OS_SCL_SCHEMA_OBJ= osdb_SdaiEB203_PRODUCT_IDENTIFICATION.o

SCHEMA_SRC= idl_SdaiEB203_PRODUCT_IDENTIFICATION.cc
SCL_OS_OBJ= 
APP_SCHEMA_SRC= idl.cc
APP_SCHEMA_OBJ= idl.o
SCHEMA_SRC2= 
APP_SCHEMA_HDRS=
 #APP_SCHEMA_DB= $(OS_SCHEMA_DB_DIR)scl.adb
 #APP_SCHEMA_DB= /proj/niiip/src/ds/proto/ostr2/c3appldb.adb
APP_SCHEMA_DB= /proj/framedb/niiipdemo97/c3appl_db_niiip.adb
LIB_SCHEMA_DBS= $(OS_ROOTDIR)/lib/liboscol.ldb
#LIB_SCHEMA_DBS=
 #PIB_DB= /proj/framewrk/src/objectstore_persist_stuff/database_pib.db
PIB_DB= /proj/framedb/niiipdemo97/c3database_niiip.db
*/
	fprintf (files.make, "OFILES =  schema.o SdaiAll.o compstructs.o ");

	resolution_success();
	os_print_schemas_separate (express, &files);
}    

void resolution_success(void)
{
	printf("Resolution successful.  Writing IDL output...\n");
}

int success(Express model)
{
	printf("Finished writing files.\n");
	return(0);
}

/* This function is called from main() which is part of the NIST Express 
   Toolkit.  It assigns 2 pointers to functions which are called in main() */
void
EXPRESSinit_init() 
{
	EXPRESSbackend = print_file;
	EXPRESSsucceed = success;
	EXPRESSgetopt = Handle_FedPlus_Args;
  /* so the function getopt (see man 3 getopt) will not report an error */
/*	strcat(EXPRESSgetopt_options, "");*/
	ERRORusage_function = fedex_os_usage;
}
