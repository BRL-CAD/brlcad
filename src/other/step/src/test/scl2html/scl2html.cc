/*
 * scl2html.cc
 *
 * Ian Soboroff, NIST
 * June, 1994
 *
 * scl2html is a program to lay out a given schema in HTML, suitable for
 * hypertext browsing using Mosaic or any other HTML viewer/WWW surfboard.
 * While this particular code as designed is linked to the Example schema
 * (via tests.h), it can be "fed" any fedex_plus-compiled schema and give
 * appropriate output.
 *
 * The Makefile defaults to building the app to use the example schema.  To
 * link to another schema, build the appropriate libraries for it and
 * 'make SCHEMA_NAME=my-schema'.  See the Makefile for more details.
 *
 * The executable is named after the schema you use, e.g., scl2html.example.
 * This can be renamed to whatever you want.
 *
 * This program works by using the registry to walk through each entity type.
 * At each entity, the supertypes, subtypes, and attributes are probed out
 * and mapped into a relatively nifty-looking HTML format.
 *
 * This code was heavily inspired by the original scltest program,
 * distributed with the DataProbe, v2.0.x.
 */

/* Since scl2html doesn't use any schema-specific things except the init
   function, we don't need to include the schema's header file inside
   tests.h  */
#define DONT_NEED_HEADER
#include <tests.h>

// PrintAttrTypeWithAnchor()
// Given an atribute, print out its immediate type (not fundamental).
// Ah, but if life were so simple.  If this attribute is _not_ of a
// fundamental type, put in an anchor to somewhere with info on the type.
// This could be either the index page Type list, or an entity's page if
// the attribute is an entity.
// BUGS: (sort of) IMHO, the handling of aggregates here is a kludge, but
// it's a kludge without a solution I can see.  While the dictionary has
// ways to show you what kinds of things an aggregate holds, it won't tell
// you what kind of _aggragation_ (no pun intended) you have, as in giving
// you a code or "Bag" or something by itself.  It probably doesn't work
// aesthetically for compound aggregates (i.e., a list of lists).
void PrintAttrTypeWithAnchor(const TypeDescriptor *typeDesc, ofstream &outhtml)
{
  std::string buf;

    // The type.  See src/clstepcore/baseType.h for info
    PrimitiveType base = typeDesc->Type();

    // the type descriptor for the "referent type," if any.
    // This is NULL if the attribute is a fundamental type.
    // All we use it for now is checking for NULL.
    const TypeDescriptor *reference = typeDesc->ReferentType();

    // Do we need an anchor?
    int anchor = 0;

    // First, figure out if we need an anchor...
    if (reference)  // if this has a referent type (i.e., is a non-base type)
    {
        if (base != sdaiAGGR)   // anchor all non-bases except for aggregates
  	    anchor = 1;  // which we'll take care of recursively
    }
    else {
        // entities and enumerations show up as base, but we want to
        // anchor them anyway
	if (base == sdaiENUMERATION || base == sdaiINSTANCE) anchor = 1;
    }
    
    // Now, print type, with anchor if necessary
    if (anchor && base != sdaiINSTANCE) {
        // for regular TYPEs, anchor to the index at that definition
        outhtml << "<A HREF=\"" << "index.html#";
	outhtml << typeDesc->Name() << "\">";
    }
    else if (anchor && base == sdaiINSTANCE)
    {
        // for entities, anchor to that entity's page
	outhtml << "<A HREF=\"" << typeDesc->Name();
	outhtml << ".html\">";
    }

    outhtml << typeDesc->AttrTypeName(buf);

    if (base == sdaiAGGR)
    {
	outhtml << " (contains elements of type ";
	PrintAttrTypeWithAnchor(typeDesc->AggrElemTypeDescriptor(), outhtml);
	outhtml << ")" << endl;
    }

    if (anchor)
        outhtml << "</A>";
}

// PrintAttrsHTML()
// Given an entity, print out to the HTML file an unordered list of
// the entity's attributes and their types.  It returns the number of
// explicit attributes that the entity has.
int PrintAttrsHTML(const EntityDescriptor *ent, ofstream &outhtml) 
{
    int attrCount = 0;

    // To traverse the attributes of the entity, we're going to use
    // an iterator native to the class library.  This could also have
    // been done using GetHead() and NextNode() of the entity's attribute
    // list (nearly identical to how the entity lists are traversed), see
    // PrintParentAttrsHTML() and also main() below.
    AttrDescItr aditr(ent->ExplicitAttr());
    const AttrDescriptor *attrDesc = aditr.NextAttrDesc();
    if (attrDesc != 0) 
    {
        outhtml << "\n<! -- These are the attributes for entity ";
        outhtml << ent->Name() << " >\n\n";
	outhtml << "<UL>" << endl;
	while (attrDesc != 0) 
	{
	    attrCount++;
	    outhtml << "<LI>" << attrDesc->Name() << " : ";
	    if ((SCLP23(Logical))(attrDesc->Optional()) == SCLLOG(LTrue))
      	        outhtml << "optional ";
	    PrintAttrTypeWithAnchor(attrDesc->ReferentType(), outhtml);
	    outhtml << endl;
	    attrDesc = aditr.NextAttrDesc();
	}
	outhtml << "</UL>" << endl;
    }
    return attrCount;
}


// PrintParentAttrsHTML()
// This function, given an entity and its parent, recursively travels up
// the inheritance tree of the entity, printing to the HTML file a
// description-list structre showing all ancestors.  For each ancestor,
// the attributes are printed using the PrintAttrsHTML() function above.
void PrintParentAttrsHTML (const EntityDescriptor* ent, 
    const EntityDescriptor *parent, ofstream &outhtml) 
{
    // Passing this function the pointer to ent is really cosmetic, so
    // we can easily print out this 'header' information.
    outhtml << "\n<! -- This is a parent of " << ent->Name() << ">\n\n";
    outhtml << "<DL>" << endl;
    outhtml << "<DT>" << ent->Name() << " inherits from ";
    outhtml << "<A HREF=\"" << parent->Name() << ".html\">";
    outhtml << parent->Name() << "</A>" << endl;
    
    // Here we're going to traverse the list of supertypes of the parent
    // using the EntityDescriptorList
    const EntityDescriptorList *grandpaList = &(parent->Supertypes());
    EntityDescLinkNode *grandpaNode = 
	(EntityDescLinkNode*)grandpaList->GetHead();
    while (grandpaNode) 
    {
        // for each "grandparent" of ent, inside a descriptor body (<DD>)
        // recursively call this function... the 'while' takes care of
        // multiple inheritance: for each grandparent, trace back.
	outhtml << "<DD>" << endl;
	const EntityDescriptor *grandpa = grandpaNode->EntityDesc();
	PrintParentAttrsHTML(parent, grandpa, outhtml);
	grandpaNode = (EntityDescLinkNode*)grandpaNode->NextNode();
    }
    
    // Now print the parent's atributes.  This calls PrintAttrsHTML() to
    // actually print any existing attributes, but to see if there are
    // any, we'll check to see if the head of the atribute descriptor list
    // exists.  Conversely, once grabbing the head we could print out
    // the attributes by following the list (attrNode->NextNode()).
    const AttrDescriptorList *attrList = &(parent->ExplicitAttr());
    AttrDescLinkNode *attrNode = (AttrDescLinkNode*)attrList->GetHead();
    if (attrNode) 
    {
	outhtml << "<DT>" << parent->Name();
	outhtml << " has the following attributes" << endl;
	outhtml << "<DD>" << endl;
	if (PrintAttrsHTML(parent, outhtml) == 0)
	    outhtml << "<EM>none</EM>" << endl;
    }
    
    outhtml << "</DL>" << endl;
}


/********************  main()  ****************************/

main()
{
    // This has to be done before anything else.  This initializes
    // all of the registry information for the schema you are using.
    // The SchemaInit() function is generated by fedex_plus... see
    // extern statement above.

    Registry *registry = new Registry(SchemaInit);

    // Rather than using the standard Registry class here, as in the treg
    // example, we are using MyRegistry, which is derived from the original.
    // Registry doesn`t contain a facility for browsing the types, as it
    // does schemas and entities... so I added it.  See the file
    // MyRegistry.h for details on how this was done. 
    
    // "Reset" has tables for browsing
    registry->ResetEntities();
    registry->ResetSchemas();
    registry->ResetTypes();

    const SchemaDescriptor *schema = registry->NextSchema();
    int num_ents = registry->GetEntityCnt();
    cout << "Processing schema " << schema->Name();
    cout << " with " << num_ents << " entities." << endl;
    
    // Set up root-level index of the schema, in a file called
    // "index.html".  In this document are links to all objects in
    // the schema.

    cout << "Creating 'index.html'" << endl;
    ofstream root("index.html");
    root << "<TITLE>" << schema->Name() << "</TITLE>" << endl;
    root << "<H1>Schema: " << schema->Name() << "</H1>" << endl;

    // Do Type-list
    cout << "Processing types ";
    root << "<HR>" << endl;
    root << "<H2>Types</H2>" << endl;
    root << "<UL>" << endl;
    
    const TypeDescriptor *type;
    std::string tmp;
    type = registry->NextType();
    root << "<! -- The following is a list of types, which are>\n";
    root << "<! --  cross-referenced from the entities.>\n";
    while (type != 0)
    {
        cout << ".";
	root << "<LI><A NAME=\"" << type->Name() << "\">";
	root << type->Name() << "</A>: ";
	root << type->Description() << endl;
	type = registry->NextType();
    }
    root << "</UL>" << endl;
    cout << endl;

    // Do entity root-section and pages
    root << "<HR>" << endl;
    root << "<H2>Entities</H2>" << endl;
    root << "<UL>" << endl;
    root << "<! -- These all lead to a page for each entity>\n";
    
    // These are all pointers we need to wander around the registry
    // information.  We'll want to not only look at the current entity
    // and its attributes, but also which entites are super- and subclasses
    // of the current entity.  Here, supers isn't really used beyond checking
    // for ancestors, but we'll use subs to list subclasses and make links
    // to them.

    const EntityDescriptor *ent;
    const EntityDescriptorList *supers;
    const EntityDescriptorList *subs;
    EntityDescLinkNode *entNode;

    for (int i=0; i<num_ents; i++) 
    {
	ent = registry->NextEntity();
	cout << "Processing " << ent->Name() << endl;

        // add anchor to root page
        root << "<LI><A HREF=\"" << ent->Name() << ".html\">"; 
	root << ent->Name() << "</A>" << endl;
	
        // construct page for entity
        char *tmpstr = new char[strlen(ent->Name()) + 6];
	ofstream entout(strcat(strcpy(tmpstr,ent->Name()),".html"));
	delete [] tmpstr;
	entout << "<TITLE>Entity " << ent->Name() << "</TITLE>" << endl;
	entout << "<H1>" << ent->Name() << "</H1>" << endl;

        // supertypes
        entout << "<HR>\n<H2>Inherited Attributes</H2>" << endl;
	entout << "<! -- Below is the direct ancestry (if any) of ";
	entout << ent->Name() << ">\n";
        supers = &(ent->Supertypes());
        entNode = (EntityDescLinkNode*)supers->GetHead();
        if (!entNode)
            entout << "<EM>none</EM>" << endl;
        while (entNode)
        {
            // call PrintParentAttrsHTML to explore the parents
            // of the entity, for each parent.
	    const EntityDescriptor *parent = entNode->EntityDesc();
            PrintParentAttrsHTML(ent, parent, entout);
            entNode = (EntityDescLinkNode*)entNode->NextNode();
        }

        // local attributes
        entout << "<HR>\n<H2>Local Attributes</H2>" << endl;
        if (PrintAttrsHTML(ent, entout) == 0)
            entout << "<EM>none</EM>" << endl;

        // subtypes
        // We're going to traverse the subtypes by using the subtype list
        // of the entity, a little more simply than in PrintParentAttrsHTML()
        entout << "<HR>\n<H2>Subtypes</H2>" << endl;
	entout << "<! -- The following entities inherit from this one>\n";
        subs = &(ent->Subtypes());
        entNode = (EntityDescLinkNode*)subs->GetHead();
        if (entNode) 
	{
	    entout << "<UL>" << endl;
	    EntityDescriptor *child;
	    while (entNode) 
	    {
		child = entNode->EntityDesc();
		entout << "<LI><A HREF=\"" << child->Name() << ".html\">";
		entout << child->Name() << "</A>" << endl;
		entNode = (EntityDescLinkNode*)entNode->NextNode();
	    }
	    entout << "</UL>" << endl;
        }
        else entout << "<EM>none</EM>" << endl;

	entout << "<HR>" << endl;
        entout << "Click <A HREF=\"index.html\">here</A> to ";
        entout << "return to the index." << endl;
    }
    root << "</UL>" << endl;
    cout << "Done!" << endl;
}







