#ifndef BT_DEFAULT_MOTION_STATE_H
#define BT_DEFAULT_MOTION_STATE_H

#include "btMotionState.h"

#ifndef LINEARMATH_EXPORT
#  if defined(LINEARMATH_DLL_EXPORTS) && defined(LINEARMATH_DLL_IMPORTS)
#    error "Only LINEARMATH_DLL_EXPORTS or LINEARMATH_DLL_IMPORTS can be defined, not both."
#  elif defined(LINEARMATH_DLL_EXPORTS)
#    define LINEARMATH_EXPORT __declspec(dllexport)
#  elif defined(LINEARMATH_DLL_IMPORTS)
#    define LINEARMATH_EXPORT __declspec(dllimport)
#  else
#    define LINEARMATH_EXPORT
#  endif
#endif


///The btDefaultMotionState provides a common implementation to synchronize world transforms with offsets.
ATTRIBUTE_ALIGNED16(struct)	btDefaultMotionState : public btMotionState
{
	btTransform m_graphicsWorldTrans;
	btTransform	m_centerOfMassOffset;
	btTransform m_startWorldTrans;
	void*		m_userPointer;

	BT_DECLARE_ALIGNED_ALLOCATOR();

	btDefaultMotionState(const btTransform& startTrans = btTransform::getIdentity(),const btTransform& centerOfMassOffset = btTransform::getIdentity())
		: m_graphicsWorldTrans(startTrans),
		m_centerOfMassOffset(centerOfMassOffset),
		m_startWorldTrans(startTrans),
		m_userPointer(0)

	{
	}

	///synchronizes world transform from user to physics
	virtual void	getWorldTransform(btTransform& centerOfMassWorldTrans ) const 
	{
			centerOfMassWorldTrans = m_graphicsWorldTrans * m_centerOfMassOffset.inverse() ;
	}

	///synchronizes world transform from physics to user
	///Bullet only calls the update of worldtransform for active objects
	virtual void	setWorldTransform(const btTransform& centerOfMassWorldTrans)
	{
			m_graphicsWorldTrans = centerOfMassWorldTrans * m_centerOfMassOffset;
	}

	

};

#endif //BT_DEFAULT_MOTION_STATE_H
