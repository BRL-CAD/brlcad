#ifndef _STEPSELECT_H
#define _STEPSELECT_H

/*
* NIST STEP Core Class Library
* clstepcore/sdaiSelect.h
* April 1997
* Dave Helfrick
* KC Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: sdaiSelect.h,v 1.5 1997/11/05 21:59:15 sauderd DP3.1 $ */

/**********
	class definition for the select superclass SCLP23_NAME(Select).
**********/
class SCLP23_NAME(Select) {
  protected:
        const SelectTypeDescriptor *_type;
        const TypeDescriptor *      underlying_type;
	BASE_TYPE 		    base_type; // used by the subtypes

	// it looks like this member, val, is not used anywhere 9/27/96 - DAS
	SCLP23_NAME(String) val;
	ErrorDescriptor _error;

        const TypeDescriptor * SetUnderlyingType (const TypeDescriptor *);

        const TypeDescriptor * CanBe (const char *) const;
        const TypeDescriptor * CanBe (BASE_TYPE) const;
	const TypeDescriptor * CanBe (const TypeDescriptor * td) const;
	const TypeDescriptor * CanBeSet (const char *, const char *) const;

        const int IsUnique(const BASE_TYPE bt) const;
    
	virtual const TypeDescriptor * AssignEntity (SCLP23_NAME(Application_instance) * se) =0;
	virtual SCLP23_NAME(Select) * NewSelect () =0;
  public:
	Severity severity() const;
	Severity severity( Severity );
	const char *Error();
	void Error( char * );
		// clears select's error  
	void ClearError();
		// clears error

        virtual BASE_TYPE ValueType() const =0;

  // constructors
        SCLP23_NAME(Select) (const SelectTypeDescriptor * s =0, 
		     const TypeDescriptor * td =0);
	virtual ~SCLP23_NAME(Select) ();

  // from SDAI binding
        SCLP23_NAME(String) UnderlyingTypeName () const;
	const TypeDescriptor * CurrentUnderlyingType() const;
	bool exists() const;
	void nullify();

	Severity SelectValidLevel(const char *attrValue, ErrorDescriptor *err, 
				  InstMgr *im, int clearError);

  // reading and writing
        const char * STEPwrite (std::string& s, const char *currSch =0) const;
        void STEPwrite (ostream& out =cout, const char *currSch =0) const;

  // IMS 8/2/95: added as part of new select implementation
        virtual void STEPwrite_verbose (ostream& out =cout, const char * =0)
	               const;
    
        virtual void STEPwrite_content (ostream& out, const char * =0) const =0;
//	char * asStr() const;


	Severity StrToVal(const char *val, const char *selectType, 
			  ErrorDescriptor *err, InstMgr * instances =0);
        virtual Severity StrToVal_content (const char *, 
					   InstMgr * instances =0) =0;

	Severity STEPread(istream& in, ErrorDescriptor *err, 
			  InstMgr * instances =0, const char* utype =0, 
			  int addFileId =0, const char * =NULL);

		// abstract function
        virtual Severity STEPread_content (istream& in =cin,
					   InstMgr * instances =0,
					   const char *utype =0,
					   int addFileId =0,
					   const char *currSch =0) =0;

	virtual SCLP23_NAME(Select)& operator =( const SCLP23_NAME(Select)& ) =0; 

	int set_null();
	int is_null();

};	/** end class  **/

typedef SCLP23_NAME(Select) * SCLP23_NAME(Select_ptr) ;
typedef SCLP23_NAME(Select_ptr) SCLP23_NAME(Select_var) ;

#endif
