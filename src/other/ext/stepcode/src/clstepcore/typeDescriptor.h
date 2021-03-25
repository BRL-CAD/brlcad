#ifndef TYPEDESCRIPTOR_H
#define TYPEDESCRIPTOR_H

#include "schRename.h"
#include "whereRule.h"
#include "dictSchema.h"

#include "sc_export.h"

/**
 * TypeDescriptor
 * This class and the classes inherited from this class are used to describe
 * all types (base types and created types).  There will be an instance of this
 * class generated for each type found in the schema.
 * A TypeDescriptor will be generated in three contexts:
 * 1) to describe a base type - e.g. INTEGER, REAL, STRING.  There is only one
 *        TypeDescriptor created for each Express base type. Each of these will
 *        be pointed to by several other AttrDescriptors and TypeDescriptors)
 * 2) to describe a type created by an Express TYPE statement.
 *        e.g. TYPE label = STRING END_TYPE;
 *        These TypeDescriptors will be pointed to by other AttrDescriptors (and
 *        TypeDescriptors) representing attributes (and Express TYPEs) that are
 *        of the type created by this Express TYPE.
 * 3) to describe a type created in an attribute definition
 *        e.g. part_label_grouping : ARRAY [1.10] label;
 *        or part_codes : ARRAY [1.10] INTEGER;
 *        In this #3 context there will not be a name associated with the type.
 *        The TypeDescriptor created in this case will only be pointed to by the
 *        single AttrDescriptor associated with the attribute it was created for.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * \var _name
 * \brief the name of the type.
 * In the case of the TypeDescriptors representing the Express base
 * types this will be the name of the base type.
 * In the case where this TypeDescriptor is representing an Express
 * TYPE it is the LEFT side of an Express TYPE statement (i.e. label
 * as in TYPE label = STRING END_TYPE;) This name would in turn be
 * found on the RIGHT side of an Express attribute definition (e.g.
 * attr defined as part_label : label; )
 * In the case where this TypeDescriptor was generated to describe a
 * type created in an attr definition, it will be a null pointer (e.g
 * attr defined as part_label_grouping : ARRAY [1..10] label)
 * \var _fundamentalType
 *  the 'type' of the type being represented by
 *  the TypeDescriptor . i.e. the following 2 stmts
 *  would cause 2 TypeDescriptors to be generated - the 1st having
 *  _fundamentalType set to STRING_TYPE and for the 2nd to
 *  REFERENCE_TYPE.
 * TYPE label = STRING END_TYPE;
 * TYPE part_label = label END_TYPE;
 * part_label and label would be the value of the respective
 * _name member variables for the 2 TypeDescriptors.
 * \var _referentType
 * will point at another TypeDescriptor further specifying
 *  the type in all cases except when the type is directly
 *  an enum or select.  i.e. in the following... _referentType for
 *  the 1st type does not point at anything and for the 2nd it does:
 * TYPE color = ENUMERATION OF (red, blue); END_TYPE;
 * TYPE color_ref = color; END_TYPE;
 ** var _fundamentalType
 * being REFERENCE_TYPE (as would be the case for
 * part_label and color_ref above) means that the _referentType
 * member variable points at a TypeDescriptor representing a type
 * that has been defined in an Express TYPE stmt.
 *  Otherwise _fundamental type reflects
 *  the type directly as in the type label above.  type label above
 *  has a _referentType that points at a TypeDescriptor for STRING
 *  described in the next sentence (also see #1 above).
 * A TypeDescriptor would be generated for each of the EXPRESS base
 * types (int, string, real, etc) having _fundamentalType member
 * variables set to match the EXPRESS base type being represented.
 ** var _referentType
 * For the TypeDescriptors describing the EXPRESS base types this will
 * be a null pointer.  For all other TypeDescriptors this will point
 * to another TypeDescriptor which further describes the type. e.g.
 * TYPE part_label = label END_TYPE; TYPE label = STRING END_TYPE;
 * part_label's _referentType will point to the TypeDescriptor for
 * label.  label's _referentType will point to the TypeDescriptor
 * for STRING. The _fundamentalType for part_label will be
 * REFERENCE_TYPE and for label will be STRING_TYPE.
 * The _fundamentalType for the EXPRESS base type STRING's
 * TypeDescriptor will be STRING_TYPE.
 * The _referentType member variable will in most cases point to
 * a subtype of TypeDescriptor.
 * \var _description
 * This is the string description of the type as found in the
 * EXPRESS file. e.g. aggr of [aggr of ...] [list of ...] someType
 * It is the RIGHT side of an Express TYPE statement
 * (i.e. LIST OF STRING as in
 * TYPE label_group = LIST OF STRING END_TYPE;)
 * It is the same as _name for EXPRESS base types TypeDescriptors (with
 * the possible exception of upper or lower case differences).
 */
class SC_CORE_EXPORT TypeDescriptor
{

    protected:

        /// the name of the type (see above)
        ///
        /// NOTE - memory is not allocated for this, or for _description
        /// below.  It is assumed that at creation, _name will be made
        /// to point to a static location in memory.  The exp2cxx
        /// generated code, for example, places a literal string in its
        /// TypeDesc constructor calls.  This creates a location in me-
        /// mory static throughout the lifetime of the calling program.
        const char   *_name ;

        /// an alternate name of type - such as one given by a different
        /// schema which USEs/ REFERENCEs this.  (A complete list of
        /// alternate names is stored in altNames below.  _altname pro-
        /// vides storage space for the currently used one.)
        char _altname[BUFSIZ];

        /// contains list of renamings of type - used by other schemas
        /// which USE/ REFERENCE this
        const SchRename *altNames;

        /// the type of the type (see above).
        /// it is an enum see file clstepcore/baseType.h
        PrimitiveType _fundamentalType;

        const Schema *_originatingSchema;

        /// further describes the type (see above)
        /// most often (or always) points at a subtype.
        const TypeDescriptor *_referentType;

        /// Express file description (see above)
        /// e.g. the right side of an Express TYPE stmt
        /// (See note above by _name regarding memory allocation.)
        const char   *_description;

    public:
        /// a Where_rule may contain only a comment
        Where_rule__list_var _where_rules; // initially a null pointer

        Where_rule__list_var &where_rules_()
        {
            return _where_rules;
        }

        void where_rules_(Where_rule__list *wrl)
        {
            _where_rules = wrl;
        }

    protected:
        /// Functions used to check the current name of the type (may
        /// != _name if altNames has diff name for current schema).
        bool PossName(const char *) const;
        bool OurName(const char *) const;
        bool AltName(const char *) const;

    public:

        TypeDescriptor(const char *nm, PrimitiveType ft, const char *d);
        TypeDescriptor(const char *nm, PrimitiveType ft,
                       Schema *origSchema, const char *d);
        TypeDescriptor();
        virtual ~TypeDescriptor();

        virtual const char *GenerateExpress(std::string &buf) const;

        /// The name of this type.  If schnm != NULL, the name we're
        /// referred to by schema schnm (may be diff name in our alt-
        /// names list (based on schnm's USE/REF list)).
        const char *Name(const char *schnm = NULL) const;

        /// The name that would be found on the right side of an
        /// attribute definition. In the case of a type defined like
        /// TYPE name = STRING END_TYPE;
        /// with attribute definition   employee_name : name;
        /// it would be the _name member variable. If it was a type
        /// defined in an attribute it will be the _description
        /// member variable since _name will be null. e.g. attr. def.
        /// project_names : ARRAY [1..10] name;
        void AttrTypeName(std::string &buf, const char *schnm = NULL) const;

        /// Linked link of alternate names for the type:
        const SchRename *AltNameList() const
        {
            return altNames;
        }

        /// This is a fully expanded description of the type.
        /// This returns a string like the _description member variable
        /// except it is more thorough of a description where possible
        /// e.g. if the description contains a TYPE name it will also
        /// be explained.
        const char *TypeString(std::string &s) const;

        /// This TypeDescriptor's type
        PrimitiveType Type() const
        {
            return _fundamentalType;
        }
        void Type(const PrimitiveType type)
        {
            _fundamentalType = type;
        }

        /// This is the underlying Express base type of this type. It will
        /// be the type of the last TypeDescriptor following the
        /// _referentType member variable pointers. e.g.
        /// TYPE count = INTEGER;
        /// TYPE ref_count = count;
        /// TYPE count_set = SET OF ref_count;
        ///  each of the above will generate a TypeDescriptor and for
        ///  each one, PrimitiveType BaseType() will return INTEGER_TYPE.
        ///  TypeDescriptor *BaseTypeDescriptor() returns the TypeDescriptor
        ///  for Integer.
        PrimitiveType       BaseType() const;
        const TypeDescriptor *BaseTypeDescriptor() const;
        const char *BaseTypeName() const;

        /// the first PrimitiveType that is not REFERENCE_TYPE (the first
        /// TypeDescriptor *_referentType that does not have REFERENCE_TYPE
        /// for it's fundamentalType variable).  This would return the same
        /// as BaseType() for fundamental types.  An aggregate type
        /// would return AGGREGATE_TYPE then you could find out the type of
        /// an element by calling AggrElemType().  Select types
        /// would work the same?

        PrimitiveType   NonRefType() const;
        const TypeDescriptor *NonRefTypeDescriptor() const;

        int   IsAggrType() const;
        PrimitiveType   AggrElemType() const;
        const TypeDescriptor *AggrElemTypeDescriptor() const;

        PrimitiveType FundamentalType() const
        {
            return _fundamentalType;
        }
        void FundamentalType(PrimitiveType ftype)
        {
            _fundamentalType = ftype;
        }

        /// The TypeDescriptor for the type this type is based on
        const TypeDescriptor *ReferentType() const
        {
            return _referentType;
        }
        void ReferentType(const TypeDescriptor *rtype)
        {
            _referentType = rtype;
        }


        const Schema *OriginatingSchema()  const
        {
            return _originatingSchema;
        }
        void OriginatingSchema(const Schema *os)
        {
            _originatingSchema = os;
        }
        const char *schemaName() const
        {
            if(_originatingSchema) {
                return _originatingSchema->Name();
            } else {
                return "";
            }
        }

        /// A description of this type's type. Basically you
        /// get the right side of a TYPE statement minus END_TYPE.
        /// For base type TypeDescriptors it is the same as _name.
        const char *Description() const
        {
            return _description;
        }
        void Description(const char *desc)
        {
            _description = desc;
        }

        virtual const TypeDescriptor *IsA(const TypeDescriptor *) const;
        virtual const TypeDescriptor *BaseTypeIsA(const TypeDescriptor *)
        const;
        virtual const TypeDescriptor *IsA(const char *) const;
        virtual const TypeDescriptor *CanBe(const TypeDescriptor *n) const
        {
            return IsA(n);
        }
        virtual const TypeDescriptor *CanBe(const char *n) const
        {
            return IsA(n);
        }
        virtual const TypeDescriptor *CanBeSet(const char *n,
                                               const char *schNm = 0) const
        {
            return (CurrName(n, schNm) ? this : 0);
        }
        bool CurrName(const char *, const char * = 0) const;
        /// Adds an additional name, newnm, to be use when schema schnm is USE/REFERENCE'ing us (added to altNames).
        void addAltName(const char *schnm, const char *newnm);
};

#endif //TYPEDESCRIPTOR_H
