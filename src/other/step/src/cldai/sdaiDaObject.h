#ifndef SDAIDAOBJECT_H
#define SDAIDAOBJECT_H 1

/*
#include <sclprefixes.h>

#ifdef PART26
#include <CORBA.h>
#include <corbaIncludes.h>
#else
typedef char* DAObjectID;
#endif

#include <Object.h>
*/

#ifndef PART26
typedef char* SCLP23_NAME(DAObjectID);
#endif

     // interface PID (ISO/DIS 10303-23:1996(E) 5.3.10.1)
     // Also, CORBA POS specification, Section 5.4
     //
     //    The PID class maintains the persistent object identifier for every
     // persistent object, objects of class DAObject, and objects of any class
     // derived directly or indirectly from DAObject.
     //
     // POS: The PID identifies one or more locations within a Datastore that
     // represent the persistent data of an object and generates a string
     // identifier for that data. An object must have a PID in order to store
     // its data persistently.
     //
/*
   The EXPRESS ENTITY application_instance from the SDAI_data_type_schema from 
   clause 10 in ISO 10303-22, shall be mapped to the C++ class 
   Application_instance. The class Application_instance shall be a subtype of 
   the C++ class DAObject and of the C++ class Entity_instance. 
   The class DAObject is supported by the classes PID, PID_SDAI and the type 
   SCLP23(DAObjectID) as follows:
 */

class SCLP23_NAME(PID) : public SCLP23_NAME(sdaiObject)
{
  public:

// These are in the IDL generated code for Part 26. I will have to think about 
// why they should be here (for Part 23). DAS
//    static PID_ptr _duplicate(PID_ptr);
//    static PID_ptr _narrow(SCLP23_NAME(sdaiObject_ptr));
//    static PID_ptr _nil();

    SCLP23_NAME(String) _datastore_type ;
    SCLP23_NAME(String) _pidstring ;

    // constructor/destructor
    SCLP23_NAME(PID)();
    virtual ~SCLP23_NAME(PID)();

#ifndef PART26
    /*
       The Datestore_type attribute shall identify the type of the underlying 
       datastore.
     */
    char* Datastore_type() const  { return const_cast<char *>(_datastore_type.c_str()); }
    void Datastore_type(char *x) { _datastore_type = x; }

    /*
       This function shall return a string version of the receiver.
     */
    char* get_PIDString();
#else
    virtual void datastore_type (const char * x, CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()) throw (CORBA::SystemException)
	{ _datastore_type = x; }

    virtual char * datastore_type (CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()) throw (CORBA::SystemException)
	{ return CORBA::string_dupl(_datastore_type); }

        virtual char * get_PIDString (CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()) throw (CORBA::SystemException)
	{ return CORBA::string_dupl(_pidstring); }
#endif

};

#ifdef PART26
//DEF_TIE_PID( SCLP23_NAME(PID) )
#endif

typedef SCLP23_NAME(PID)* SCLP23_NAME(PID_ptr);
typedef SCLP23_NAME(PID_ptr) SCLP23_NAME(PID_var);


     // interface PID_DA (ISO/DIS 10303-23:1996(E) 5.3.10.3)
     // Also, CORBA POS specification, Direct Access Protocol, Section 5.10.1
     //
     // The Direct Access Protocol supports direct access to persistent data
     // through typed attributes organized in data objects that are defined
     // in a Data Definition Language (DDL). An object using this protocol
     // whould represent its persistent data as one or more interconnected
     // data objects, For uniformity, the persistent data of an object is
     // described as a single data object; however, that data object might be
     // the root of a graph of data objects interconnected by stored data
     // object references. If an object uses multiple data objects, the
     // object traverses the graph by following stored data object references.
     //
     //    The PID_DA class maintains the persistent object identifier for an
     // application DAObject.
     //
     // POS: The Persistent Identifiers (PIDs) used by the PDS_DA contain
     // an object identifier that is local to the particular PDS. This value
     // may be accessed through this extension to the CosPersistencePID 
     // interface.
     //

class SCLP23_NAME(PID_DA): public SCLP23_NAME(PID) 
{
  public:

    SCLP23_NAME(String) _oid ;
        // oid (ISO/DIS 10303-23:1996(E) 5.3.10.3)
        //
        //   This attribute shall set and return the string representation of the
        // persistent identifier, local to the Model_contents, of the DAObject
        // referred to by this object.
        //
        // POS: This returns the data object identifier used by this PDS for
        // the data object specified by the PID. The SCLP23(DAObjectID) type is
        // defined as an unbounded sequence of bytes that may be 
        // vendor-dependent.
        //

    // constructor/destructor
    SCLP23_NAME(PID_DA)();
    virtual ~SCLP23_NAME(PID_DA)();

#ifndef PART26
    virtual void oid (const SCLP23_NAME(DAObjectID) x) { _oid = x; }
    virtual SCLP23_NAME(DAObjectID) oid () const 
					    { return const_cast<char *>(_oid.c_str()); }
#else
//    virtual void oid (const SCLP26(DAObjectID) x, CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()) throw (CORBA::SystemException)
    virtual void oid (const DAObjectID x, CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()) throw (CORBA::SystemException)
	{ _oid = x; }
//    virtual SCLP26(DAObjectID) oid (CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()) throw (CORBA::SystemException)
    virtual DAObjectID oid (CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()) throw (CORBA::SystemException)
	{ return CORBA::string_dupl(_oid); }
#endif

};

#ifdef PART26
//DEF_TIE_PID_DA( SCLP23(PID_DA) )
#endif

typedef SCLP23_NAME(PID_DA)* SCLP23_NAME(PID_DA_ptr);
typedef SCLP23_NAME(PID_DA_ptr) SCLP23_NAME(PID_DA_var);

     // interface PID_SDAI (ISO/DIS 10303-23:1996(E) 5.3.10.2)
     //
     //    The PID_SDAI class maintains the persistent object identifier for
     // a Model_contents object.
     //
class SCLP23_NAME(PID_SDAI) : public SCLP23_NAME(PID)
{
  public:
    SCLP23_NAME(String) _modelid ;

//    static PID_SDAI_ptr _duplicate(PID_SDAI_ptr);
//    static PID_SDAI_ptr _narrow(SCLP23_NAME(sdaiObject_ptr));
//    static PID_SDAI_ptr _nil();

    // constructor/destructor
    SCLP23_NAME(PID_SDAI)();
    virtual ~SCLP23_NAME(PID_SDAI)();

        // Modelid (ISO/DIS 10303-23:1996(E) 5.3.10.3)
        //
        //    This attribute shall set and return the string representation of
        // the persistent identifier of the cluster of data for the 
        // Model_contents referred to by this PID.
        //
#ifndef PART26
    virtual void Modelid (const char * x) { _modelid = x; }
    virtual char * Modelid () const	  { return const_cast<char *>(_modelid.c_str()); }
#else
    virtual void Modelid (const char * x, CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()) throw (CORBA::SystemException)
	{ _modelid = x; }

    virtual char * Modelid (CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()) throw (CORBA::SystemException)
	{ return CORBA::string_dupl(_modelid); }
#endif

};

#ifdef PART26
//DEF_TIE_PID_SDAI( SCLP23(PID_SDAI) )
#endif

typedef SCLP23_NAME(PID_SDAI)* SCLP23_NAME(PID_SDAI_ptr);
typedef SCLP23_NAME(PID_SDAI_ptr) SCLP23_NAME(PID_SDAI_var);

     // interface DAObject (ISO/DIS 10303-23:1996(E) 5.3.10.5)
     // Also, CORBA POS Section 5.10.2, Direct Access Protocol.
     //
     //  From POS: The DAObject interface provides operations that many data
     // object clients need. A Datastore implementation may provide support
     // for these operations automatically for its data objects. A data object
     // is not required to support this interface. A client can obtain access
     // to these operations by narrowing a data object reference to the
     // DAObject interface.
     //

	// predefine these _ptr since they are used inside this class
class SCLP23_NAME(DAObject); 
typedef SCLP23_NAME(DAObject)* SCLP23_NAME(DAObject_ptr);
typedef SCLP23_NAME(DAObject_ptr) SCLP23_NAME(DAObject_var);

class SCLP23_NAME(DAObject) : public SCLP23_NAME(sdaiObject)
//class DAObject : public SCLP23_NAME(sdaiObject)
{
  public:

    SCLP23_NAME(String) _dado_oid;
//    static DAObject_ptr _duplicate(DAObject_ptr);
//    static DAObject_ptr _narrow(SCLP23_NAME(sdaiObject_ptr));
//    static DAObject_ptr _nil();

        // dado_same (ISO/DIS 10303-23:1996(E) 5.3.10.5)
        //
        //   Returns TRUE if obj points to the same object as self
        //
        //    POS specification description: This returns true if the target
        // data object and the parameter data object are the same data object.
        // This operation can be used to test data object references for
        // identity.
        //

    // constructor/destructor
    SCLP23_NAME(DAObject)();
    virtual ~SCLP23_NAME(DAObject)();

#ifdef PART26
//    virtual SCLP26(Application_instance_ptr) create_TIE();
    virtual IDL_Application_instance_ptr create_TIE();
#endif

#ifndef PART26
    Boolean dado_same(SCLP23_NAME(DAObject_ptr) obj) 
	{ 
	    // this will make the compiler happy that we used obj
	    strcmp(obj->dado_oid(),dado_oid()); 
	    return BUnset;
	}
#else
    virtual Boolean dado_same (SCLP23_NAME(DAObject_ptr) obj, CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()) throw (CORBA::SystemException) { return BUnset; }
#endif

        // Get persistent label (ISO/DIS 10303-22:1996(E) 10.11.6)
        //
        //    This operation returns a persistent label for the specified
        // application instance. The label shall be unique within the
        // repository containing the SDAI-model containing the application
        // instance. Any subsequent request for a persistent label for the
        // same application instance shall return the same persistent label
        // in the current or nay subsequent SDAI session.
        //
        //   Possible Error indicators:
        //
        //  sdaiTR_NEXS  The transaction has not been started
        //  sdaiTR_NAVL  The transaction is currently not available
        //  sdaiTR_EAB   The transaction ended abnormally
        //  sdaiRP_NOPN  The repository is not open
        //  sdaiEI_NEXS  The entity instance does not exist 
        //  sdaiFN_NAVL  This function is not supported by the implementation
        //  sdaiSY_ERR   An underlying system error occurred
        //
        //    POS specification description: This returns the object identifier
        // for the data object. The scope of data object identifiers is
        // implementation-specific, but is not guaranteed to be global.
        // 
        /* jg == The return value of this function in Part 23 is PID_DA which
                 seems wrong. We follow the CORBAservices specification here.
                 (Persistent Object Service Specification, Section 5.10). Also
                 note that the return value as described in the text above
                 should be a string type.
         */
#ifndef PART26
    SCLP23_NAME(DAObjectID) dado_oid()
	{ return const_cast<char *>(_dado_oid.c_str()); }
#else
//    virtual SCLP26(DAObjectID) dado_oid (CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()) throw (CORBA::SystemException)
    virtual DAObjectID dado_oid (CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()) throw (CORBA::SystemException)
	{ return CORBA::string_dupl(_dado_oid); }
#endif

        // dado_pid
        //
        //    POS specification description: This returns a PID_DA for the 
        // data object.
        //
        /* jg == This function is not included in Part 23 and 26. However, it is 
                 part of interface DAObject in the specification of the
                 Persistent Object Service. 
         */
#ifndef PART26
    SCLP23_NAME(PID_DA_ptr) dado_pid()
	{ return 0; }
#else
    virtual SCLP23_NAME(PID_DA_ptr) dado_pid (CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()) throw (CORBA::SystemException)
	{ return 0; }
#endif

        // dado_remove (ISO/DIS 10303-23:1996(E) 5.3.10.5)
        //
        //    This function shall delete the object from persistent store and
        // releases its resources. The function need not delete the aggregate-
        // valued attributes of an object.
        //
        //    POS specification description: This deletes the object from the
        // persistent store and deletes the in-memory data object.
        //
#ifndef PART26
    void dado_remove()
	{ }
#else
    virtual void dado_remove (CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()) throw (CORBA::SystemException)
	{ }
#endif

        // dado_free (ISO/DIS 10303-23:1996(E) 5.3.10.5)
        // 
        //    This function shall inform the implementation that it is free
        // to move the object back to the persistent store.
        // 
        //    POS specification description: This informs the PDS (Persistent 
        // Data Service) that the data object is not required for the time
        // being, and the PDS may move it back to persistent store. The data
        // object must be preserved and must be brought back the next time it is 
        // referenced. This operation is only a hint and is provided to improve
        // performance and resource usage. 
        //
#ifndef PART26
    void dado_free()
	{ }
#else
        virtual void dado_free (CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()) throw (CORBA::SystemException)
	{ }
#endif

};

/*
   5.3.10.1  DAObject_SDAI
*/

class SCLP23_NAME(DAObject_SDAI) : public SCLP23_NAME(DAObject) { 

  public:    
    SCLP23_NAME(DAObject_SDAI)();
//    SCLP23_NAME(DAObject_SDAI)(const SCLP23_NAME(DAObject_SDAI)&);

    virtual ~SCLP23_NAME(DAObject_SDAI)();

#ifdef PART26
//    virtual SCLP26(Application_instance_ptr) create_TIE();
    virtual IDL_Application_instance_ptr create_TIE();
#endif

    /*
       5.3.10.1.1  Find entity instance SDAI-model
     */

//    Model_ptr Owning_model() const;

    /*
       The Owning_model function shall implement the SDAI operation, Find 
       entity instance SDAI-model.

       The following specifies the coorespondence between each SDAI 
       input/output and its equivalent specified in this Part.

       SDAI             C++
       ------		-------
       Object           receiver
       Model            return value

       5.3.10.1.2  Find entity instance users
     */

//    Object__list_var FindObjectUsers(const
//				     Schema_instance__list_var& aggr) const;

    /*
       The FindObjectUsers function shall implement the SDAI operation, 
       Find entity instance users.

       The following specifies the coorespondence between each SDAI 
       input/output and its equivalent specified in this Part.

       SDAI             C++
       --------		--------
       Instance		receiver
       Domain		aggr
       Result		return value

       5.3.10.1.3  Is same
     */

    Logical IsSame(const SCLP23_NAME(sdaiObject_ptr)& otherEntity) const;

    /*
       Function:
       The IsSame function shall determine whether the parameter otherEntity 
       refers to the same instance as the receiver. This function shall 
       implement the behaviour of the EXPRESS operator (:=:).

       Output:
       This function shall return TRUE if the two instances are identical.  
       This function shall return FALSE if the two instances are not identical.
       This function shall return UNKNOWN if either instance is unset. 

       Possible error indicators:
       sdaiMX_NDEF           // SDAI-model access not defined
       sdaiEI_NEXS           // Entity instance does not exist
       sdaiTY_NDEF           // Type not defined 
       sdaiSY_ERR            // Underlying system error 

       Origin: Convenience function

       5.3.10.1.4  Get attribute
     */

#ifdef SDAI_CPP_LATE_BINDING
    Any_var GetAttr(const Attribute_ptr& attDef);
    Any_var GetAttr(const char* attName);  
#endif

    /*
       The GetAttr functions shall implement the SDAI operation, Get attribute.

       The following specifies the coorespondence between each SDAI 
       input/output and its equivalent specified in this Part.

       SDAI             C++
       ------		---------
       Object		receiver
       Attribute	attDef, attName
       Value            return value

       5.3.10.1.5  Get instance type
     */

#ifdef SDAI_CPP_LATE_BINDING
    const Entity_ptr GetInstanceType() const;
#endif

    /*
       Function:

       The GetInstanceType functions shall implement the SDAI operation, 
       Get instance type.

       The following specifies the coorespondence between each SDAI 
       input/output and its equivalent specified in this Part.

       SDAI             C++
       ------		--------
       Object		receiver
       Type		return value

       5.3.10.1.6  Is instance of
     */

    Boolean IsInstanceOf(const char* typeName) const; 
#ifdef SDAI_CPP_LATE_BINDING
    Boolean IsInstanceOf(const Entity_ptr& otherEntity) const; 
#endif

    /*
       Function:
       The IsInstanceOf functions shall implement the SDAI operation Is 
       instance of.

       The following specifies the coorespondence between each SDAI 
       input/output and its equivalent specified in this Part.

       SDAI             C++
       ------		--------
       Object		receiver
       Type		typeName, otherEntity
       Result           return value

       5.3.10.1.7  Is kind of
     */

    Boolean IsKindOf(const char* typeName) const;
#ifdef SDAI_CPP_LATE_BINDING
    Boolean IsKindOf(const Entity_ptr& theType) const;
#endif

    /*
       The IsKindOf functions shall implement the SDAI operation, Is kind of.

       The following specifies the coorespondence between each SDAI 
       input/output and its equivalent specified in this Part.

       SDAI             C++
       ------		--------
       Object		receiver
       Type		typeName, theType
       Result           return value

       5.3.10.1.8  Is SDAI kind of
       */

    Boolean IsSDAIKindOf(const char* typeName) const;

#ifdef SDAI_CPP_LATE_BINDING
    Boolean IsSDAIKindOf(const Entity_ptr& theType) const;
#endif

    /*
       The IsSDAIKindOf functions shall implement the SDAI operation, 
       Is SDAI Kind Of.

       The following specifies the coorespondence between each SDAI 
       input/output and its equivalent specified in this Part.

       SDAI             C++
       ------		--------
       Object		receiver
       Type		typeName, theType
       Result           return value

       5.3.10.1.9  Test attribute
     */

#ifdef SDAI_CPP_LATE_BINDING
    Boolean TestAttr(const Attribute_ptr& attDef);
    Boolean TestAttr(const char* attName) const;
#endif

    /*
       The TestAttr functions shall implement the SDAI operation, 
       Test attribute.

       The following specifies the coorespondence between each SDAI 
       input/output and its equivalent specified in this Part.

       SDAI		C++
       ------		---------
       Object		receiver
       Attribute	attDef, attName
       Result		return value
       
       5.3.10.1.10  Get instance type name
     */

#ifndef SDAI_CPP_LATE_BINDING
    char * GetInstanceTypeName() const;
#endif
    /*
       Function:
       The GetInstanceType function shall retrieve the name of the type of 
       the receiver.

       Output:
       This function shall return a String which contains the name of the 
       type of the receiver. The returned name shall be the name as it 
       appears in the Dictionary Model.

       Possible error indicators:
       sdaiMX_NDEF           // SDAI-model access not defined
       sdaiEI_NEXS           // Entity instance does not exist
       sdaiSY_ERR            // Underlying system error 

       Origin: Convenience function
     */

};

typedef SCLP23_NAME(DAObject_SDAI)* SCLP23_NAME(DAObject_SDAI_ptr);
typedef SCLP23_NAME(DAObject_SDAI_ptr) SCLP23_NAME(DAObject_SDAI_var);

class SCLP23_NAME(DAObject__set) {
public:
    SCLP23_NAME(DAObject__set)(int = 16);
    ~SCLP23_NAME(DAObject__set)();

    SCLP23_NAME(DAObject_ptr) retrieve(int index);
    int is_empty();

    SCLP23_NAME(DAObject_ptr)& operator[](int index);

    void Insert(SCLP23_NAME(DAObject_ptr), int index);
    void Append(SCLP23_NAME(DAObject_ptr));
    void Remove(int index);

    int Index(SCLP23_NAME(DAObject_ptr));

    void Clear();
    int Count();

private:
    void Check(int index);
private:
    SCLP23_NAME(DAObject_ptr)* _buf;
    int _bufsize;
    int _count;

  public:

};

typedef SCLP23_NAME(DAObject__set)* SCLP23_NAME(DAObject__set_ptr);
typedef SCLP23_NAME(DAObject__set_ptr) SCLP23_NAME(DAObject__set_var);

#endif
