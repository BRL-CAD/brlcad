/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#ifndef BT_COLLISION_OBJECT_WRAPPER_H
#define BT_COLLISION_OBJECT_WRAPPER_H

///btCollisionObjectWrapperis an internal data structure. 
///Most users can ignore this and use btCollisionObject and btCollisionShape instead
class btCollisionShape;
class btCollisionObject;
class btTransform;
#include "LinearMath/btScalar.h" // for SIMD_FORCE_INLINE definition

#define BT_DECLARE_STACK_ONLY_OBJECT \
	private: \
		void* operator new(size_t size); \
		void operator delete(void*);

struct btCollisionObjectWrapper;
struct btCollisionObjectWrapper
{
BT_DECLARE_STACK_ONLY_OBJECT

private:
	btCollisionObjectWrapper(const btCollisionObjectWrapper&); // not implemented. Not allowed.
	btCollisionObjectWrapper* operator=(const btCollisionObjectWrapper&);

public:
	const btCollisionObjectWrapper* m_parent;
	const btCollisionShape* m_shape;
	const btCollisionObject* m_collisionObject;
	const btTransform& m_worldTransform;
	int		m_partId;
	int		m_index;

	btCollisionObjectWrapper(const btCollisionObjectWrapper* parent, const btCollisionShape* shape, const btCollisionObject* collisionObject, const btTransform& worldTransform, int partId, int index)
	: m_parent(parent), m_shape(shape), m_collisionObject(collisionObject), m_worldTransform(worldTransform),
	m_partId(partId), m_index(index)
	{}

	SIMD_FORCE_INLINE const btTransform& getWorldTransform() const { return m_worldTransform; }
	SIMD_FORCE_INLINE const btCollisionObject* getCollisionObject() const { return m_collisionObject; }
	SIMD_FORCE_INLINE const btCollisionShape* getCollisionShape() const { return m_shape; }
};

#endif //BT_COLLISION_OBJECT_WRAPPER_H
