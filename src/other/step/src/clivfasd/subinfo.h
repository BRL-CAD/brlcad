
/*
 * SubordinateInfo - used for supervisor/subordinate communication.
 *
 */

#ifndef subinfo_h
#define subinfo_h

#include <IV-2_6/InterViews/interactor.h>

#include <IV-2_6/_enter.h>

class SubordinateInfo {
public:
    SubordinateInfo(int val, Interactor* sub);
    SubordinateInfo(void* val, Interactor* sub);

    void operator=(SubordinateInfo& s);
    void SetValue(int val);
    void SetValue(void* val);
    void GetValue(int& v);
    void GetValue(void*& v);
//    int  GetValue();
    void* GetValue();
    void GetSubordinate(Interactor*& s);
    Interactor* GetSubordinate();
private:
    Interactor *subordinate;
    void* value;
};

inline SubordinateInfo::SubordinateInfo(int val, Interactor* sub)
				{ subordinate = sub; value = (void*)val; };

inline SubordinateInfo::SubordinateInfo(void* val, Interactor* sub)
				{ subordinate = sub; value = val; };

inline void SubordinateInfo::SetValue(int val)	{ value = (void*)val; };

inline void SubordinateInfo::SetValue(void* val){ value = val; };

inline void SubordinateInfo::GetValue (int& v)	{ v = (int)value; };

inline void SubordinateInfo::GetValue (void*& v){ v = value; };

//inline int SubordinateInfo::GetValue () 	{ return (int)value; };

inline void* SubordinateInfo::GetValue ()	{ return value; };

inline void SubordinateInfo::GetSubordinate (Interactor*& s) 
						{ s = subordinate; };

inline Interactor* SubordinateInfo::GetSubordinate ()
						{ return subordinate; };

inline void SubordinateInfo::operator=(SubordinateInfo& s)
		{  s.GetValue(value); subordinate = s.GetSubordinate(); };

#include <IV-2_6/_leave.h>

#endif
