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

#ifndef BT_INTERNAL_EDGE_UTILITY_H
#define BT_INTERNAL_EDGE_UTILITY_H

#include "LinearMath/btHashMap.h"
#include "LinearMath/btVector3.h"

#include "BulletCollision/CollisionShapes/btTriangleInfoMap.h"

///The btInternalEdgeUtility helps to avoid or reduce artifacts due to wrong collision normals caused by internal edges.
///See also http://code.google.com/p/bullet/issues/detail?id=27

class btBvhTriangleMeshShape;
class btCollisionObject;
struct btCollisionObjectWrapper;
class btManifoldPoint;
class btIDebugDraw;



enum btInternalEdgeAdjustFlags
{
	BT_TRIANGLE_CONVEX_BACKFACE_MODE = 1,
	BT_TRIANGLE_CONCAVE_DOUBLE_SIDED = 2, //double sided options are experimental, single sided is recommended
	BT_TRIANGLE_CONVEX_DOUBLE_SIDED = 4
};


///Call btGenerateInternalEdgeInfo to create triangle info, store in the shape 'userInfo'
void	btGenerateInternalEdgeInfo (btBvhTriangleMeshShape*trimeshShape, btTriangleInfoMap* triangleInfoMap);


///Call the btFixMeshNormal to adjust the collision normal, using the triangle info map (generated using btGenerateInternalEdgeInfo)
///If this info map is missing, or the triangle is not store in this map, nothing will be done
void	btAdjustInternalEdgeContacts(btManifoldPoint& cp, const btCollisionObjectWrapper* trimeshColObj0Wrap,const btCollisionObjectWrapper* otherColObj1Wrap, int partId0, int index0, int normalAdjustFlags = 0);

///Enable the BT_INTERNAL_EDGE_DEBUG_DRAW define and call btSetDebugDrawer, to get visual info to see if the internal edge utility works properly.
///If the utility doesn't work properly, you might have to adjust the threshold values in btTriangleInfoMap
//#define BT_INTERNAL_EDGE_DEBUG_DRAW

#ifdef BT_INTERNAL_EDGE_DEBUG_DRAW
void	btSetDebugDrawer(btIDebugDraw* debugDrawer);
#endif //BT_INTERNAL_EDGE_DEBUG_DRAW


#endif //BT_INTERNAL_EDGE_UTILITY_H

