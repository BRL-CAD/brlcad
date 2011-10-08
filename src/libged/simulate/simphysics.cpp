/*                    S I M P H Y S I C S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/*
 * Routines related to performing physics on the passed regions
 */

#include "common.h"

#ifdef HAVE_BULLET

/* system headers */
#include <iostream>
#include <btBulletDynamicsCommon.h>

/* public headers */
#include "db.h"
#include "vmath.h"

/* private headers */
#include "./simulate.h"
#include "./simcollisionalgo.h"


/**
 * Prints the 16 by 16 transform matrices for debugging
 *
 */
void
print_matrices(char *rb_namep, mat_t t, btScalar *m)
{
    int i, j;
    char buffer[800];

    sprintf(buffer, "------------Phy : Transformation matrices(%s)--------------\n",
	    rb_namep);

    for (i=0 ; i<4 ; i++) {
	for (j=0 ; j<4 ; j++) {
	    sprintf(buffer, "%st[%d]: %f\t", buffer, (j*4 + i), t[j*4 + i]);
	}
	sprintf(buffer, "%s\n", buffer);
    }

    sprintf(buffer, "%s\n", buffer);

    for (i=0 ; i<4 ; i++) {
	for (j=0 ; j<4 ; j++) {
	    sprintf(buffer, "%sm[%d]: %f\t", buffer, (j*4 + i), m[j*4 + i]);
	}
	sprintf(buffer, "%s\n", buffer);
    }

    sprintf(buffer, "%s-------------------------------------------------------\n", buffer);
    bu_log(buffer);

}


/**
 * Adds rigid bodies to the dynamics world from the BRL-CAD geometry,
 * will add a ground plane if a region by the name of sim_gp.r is detected
 * The plane will be static and have the same dimensions as the region
 *
 */
int
add_rigid_bodies(btDiscreteDynamicsWorld* dynamicsWorld,
		 struct simulation_params *sim_params,
		 btAlignedObjectArray<btCollisionShape*> collision_shapes)
{
    struct rigid_body *current_node;
    fastf_t volume;
    btScalar mass;
    btScalar m[16];
    btVector3 v;


    for (current_node = sim_params->head_node; current_node != NULL; current_node = current_node->next) {

	// Check if we should add a ground plane
	if (strcmp(current_node->rb_namep, sim_params->ground_plane_name) == 0) {
	    // Add a static ground plane : should be controlled by an option : TODO
	    btCollisionShape* groundShape = new btBoxShape(btVector3(current_node->bb_dims[0]/2,
								     current_node->bb_dims[1]/2,
								     current_node->bb_dims[2]/2));
	    /*btDefaultMotionState* groundMotionState = new btDefaultMotionState(
	      btTransform(btQuaternion(0, 0, 0, 1),
	      btVector3(current_node->bb_center[0],
	      current_node->bb_center[1],
	      current_node->bb_center[2])));*/

	    btDefaultMotionState* groundMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1),
											   btVector3(0, 0, 0)));

	    //Copy the transform matrix
	    MAT_COPY(m, current_node->m);
	    groundMotionState->m_graphicsWorldTrans.setFromOpenGLMatrix(m);

	    /* btCollisionShape* groundShape = new btStaticPlaneShape(btVector3(0, 0, 1), 1);
	       btDefaultMotionState* groundMotionState =
	       new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, -1)));*/

	    btRigidBody::btRigidBodyConstructionInfo
		groundRigidBodyCI(0, groundMotionState, groundShape, btVector3(0, 0, 0));
	    btRigidBody* groundRigidBody = new btRigidBody(groundRigidBodyCI);
	    groundRigidBody->setUserPointer((void *)current_node);

	    dynamicsWorld->addRigidBody(groundRigidBody);
	    collision_shapes.push_back(groundShape);

	    bu_vls_printf(sim_params->result_str, "Added static ground plane : %s to simulation with mass %f Kg\n",
			  current_node->rb_namep, 0.f);

	} else{
	    //Nope, its a rigid body
	    btCollisionShape* bb_Shape = new btBoxShape(btVector3(current_node->bb_dims[0]/2,
								  current_node->bb_dims[1]/2,
								  current_node->bb_dims[2]/2));
	    collision_shapes.push_back(bb_Shape);

	    volume = current_node->bb_dims[0] * current_node->bb_dims[1] * current_node->bb_dims[2];
	    mass = volume; // density is 1

	    btVector3 bb_Inertia(0, 0, 0);
	    bb_Shape->calculateLocalInertia(mass, bb_Inertia);

	    /* btDefaultMotionState* bb_MotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1),
	       btVector3(current_node->bb_center[0],
	       current_node->bb_center[1],
	       current_node->bb_center[2])));*/
	    btDefaultMotionState* bb_MotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1),
											btVector3(0, 0, 0)));

	    //Copy the transform matrix
	    MAT_COPY(m, current_node->m);
	    bb_MotionState->m_graphicsWorldTrans.setFromOpenGLMatrix(m);

	    btRigidBody::btRigidBodyConstructionInfo
		bb_RigidBodyCI(mass, bb_MotionState, bb_Shape, bb_Inertia);
	    btRigidBody* bb_RigidBody = new btRigidBody(bb_RigidBodyCI);
	    bb_RigidBody->setUserPointer((void *)current_node);


	    VMOVE(v, current_node->linear_velocity);
	    bb_RigidBody->setLinearVelocity(v);

	    VMOVE(v, current_node->angular_velocity);
	    bb_RigidBody->setAngularVelocity(v);

	    dynamicsWorld->addRigidBody(bb_RigidBody);

	    bu_vls_printf(sim_params->result_str, "Added new rigid body : %s to simulation with mass %f Kg\n",
			  current_node->rb_namep, mass);

	}

    }

    return 0;
}


/**
 * Steps the dynamics world according to the simulation parameters
 *
 */
int
step_physics(btDiscreteDynamicsWorld* dynamicsWorld, struct simulation_params *sim_params)
{
    bu_vls_printf(sim_params->result_str, "Simulation will run for %d steps.\n", sim_params->duration);
    bu_vls_printf(sim_params->result_str, "----- Starting simulation -----\n");

    //time step of 1/60th of a second(same as internal fixedTimeStep, maxSubSteps=10 to cover 1/60th sec.)
    dynamicsWorld->stepSimulation(1/60.f, 10);

    bu_vls_printf(sim_params->result_str, "----- Simulation Complete -----\n");
    return 0;
}


/**
 * Get the final transforms and pack off back to libged
 *
 */
int
get_transforms(btDiscreteDynamicsWorld* dynamicsWorld, struct simulation_params *sim_params)
{
    int i;
    btScalar m[16];
    btVector3 aabbMin, aabbMax, v;
    btTransform identity;

    identity.setIdentity();
    const int num_bodies = dynamicsWorld->getNumCollisionObjects();


    for (i=0; i < num_bodies; i++) {

	//Common properties among all rigid bodies
	btCollisionObject* bb_ColObj = dynamicsWorld->getCollisionObjectArray()[i];
	btRigidBody* bb_RigidBody   = btRigidBody::upcast(bb_ColObj);
	const btCollisionShape* bb_Shape = bb_ColObj->getCollisionShape(); //may be used later

	if (bb_RigidBody && bb_RigidBody->getMotionState()) {

	    //Get the motion state and the world transform from it
	    btDefaultMotionState* bb_MotionState = (btDefaultMotionState*)bb_RigidBody->getMotionState();
	    bb_MotionState->m_graphicsWorldTrans.getOpenGLMatrix(m);
	    //bu_vls_printf(sim_params->result_str, "Position : %f, %f, %f\n", m[12], m[13], m[14]);

	    struct rigid_body *current_node = (struct rigid_body *)bb_RigidBody->getUserPointer();

	    if (current_node == NULL) {
		bu_vls_printf(sim_params->result_str, "get_transforms : Could not get the user pointer \
			(ground plane perhaps)\n");
		continue;

	    }

	    //Copy the transform matrix
	    MAT_COPY(current_node->m, m);

	    //print_matrices(current_node->rb_namep, current_node->m, m);

	    //Get the state of the body
	    current_node->state = bb_RigidBody->getActivationState();

	    //Get the AABB
	    bb_Shape->getAabb(bb_MotionState->m_graphicsWorldTrans, aabbMin, aabbMax);

	    VMOVE(current_node->btbb_min, aabbMin);
	    VMOVE(current_node->btbb_max, aabbMax);

	    // Get BB length, width, height
	    VSUB2(current_node->btbb_dims, current_node->btbb_max, current_node->btbb_min);

	    bu_vls_printf(sim_params->result_str, "get_transforms: Dimensions of this BB : %f %f %f\n",
			  current_node->btbb_dims[0], current_node->btbb_dims[1], current_node->btbb_dims[2]);

	    //Get BB position in 3D space
	    VCOMB2(current_node->btbb_center, 1, current_node->btbb_min, 0.5, current_node->btbb_dims)

		v = bb_RigidBody->getLinearVelocity();
	    VMOVE(current_node->linear_velocity, v);

	    v = bb_RigidBody->getAngularVelocity();
	    VMOVE(current_node->angular_velocity, v);

	}
    }

    return 0;
}


/**
 * Cleanup the physics collision shapes, rigid bodies etc
 *
 */
int
cleanup(btDiscreteDynamicsWorld* dynamicsWorld,
	btAlignedObjectArray<btCollisionShape*> collision_shapes)
{
    //remove the rigid bodies from the dynamics world and delete them
    int i;

    for (i=dynamicsWorld->getNumCollisionObjects()-1; i>=0; i--) {

	btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
	btRigidBody* body = btRigidBody::upcast(obj);
	if (body && body->getMotionState()) {
	    delete body->getMotionState();
	}
	dynamicsWorld->removeCollisionObject(obj);
	delete obj;
    }

    //delete collision shapes
    for (i=0; i<collision_shapes.size(); i++) {
	btCollisionShape* shape = collision_shapes[i];
	delete shape;
    }
    collision_shapes.clear();

    //delete dynamics world
    delete dynamicsWorld;

    return 0;
}


//--------------------- Collision specific code ------------------------

/**
 * Broadphase filter callback struct : used to show the detected AABB overlaps
 *
 */
struct broadphase_callback : public btOverlapFilterCallback
{
    //Return true when pairs need collision
    virtual bool
    needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const {
	bool collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
	collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask);

	//This would prevent collision between proxy0 and proxy1 inspite of
	//AABB overlap being detected
	//collides = false;
	btRigidBody* box0 = (btRigidBody*)proxy0->m_clientObject;
	btRigidBody* box1 = (btRigidBody*)proxy1->m_clientObject;
	if (box0 != NULL && box0 != NULL) {
	    struct rigid_body *upA = (struct rigid_body *)box0->getUserPointer();
	    struct rigid_body *upB = (struct rigid_body *)box1->getUserPointer();

	    if (upA != NULL && upB != NULL)
		bu_log("broadphase_callback: %s & %s has overlapping AABBs",
		       upA->rb_namep, upB->rb_namep);
	}

	//add some additional logic here that modifies 'collides'
	return collides;
    }
};


/**
 * Narrowphase filter callback : used to show the generated collision points
 *
 */
void
nearphase_callback(btBroadphasePair& collisionPair,
		   btCollisionDispatcher& dispatcher,
		   btDispatcherInfo& dispatchInfo)
{

    int i, j, numContacts;
    int numManifolds = dispatcher.getNumManifolds();

    /* First iterate through the number of manifolds for the whole dynamics world
     * A manifold is a set of contact points containing upto 4 points
     * There may be multiple manifolds where objects touch
     */
    for (i=0; i<numManifolds; i++) {

	//Get the manifold and the objects which created it
	btPersistentManifold* contactManifold =
	    dispatcher.getManifoldByIndexInternal(i);
	btCollisionObject* obA = static_cast<btCollisionObject*>(contactManifold->getBody0());
	btCollisionObject* obB = static_cast<btCollisionObject*>(contactManifold->getBody1());

	//Get the user pointers to struct rigid_body, for printing the body name
	btRigidBody* rbA = btRigidBody::upcast(obA);
	btRigidBody* rbB = btRigidBody::upcast(obB);
	struct rigid_body *upA = (struct rigid_body *)rbA->getUserPointer();
	struct rigid_body *upB = (struct rigid_body *)rbB->getUserPointer();

	//Get the number of points in this manifold
	numContacts = contactManifold->getNumContacts();

	bu_log("nearphase_callback : Manifold %d of %d, contacts : %d\n",
	       i+1,
	       numManifolds,
	       numContacts);

	//Iterate over the points for this manifold
	for (j=0;j<numContacts;j++) {
	    btManifoldPoint& pt = contactManifold->getContactPoint(j);

	    btVector3 ptA = pt.getPositionWorldOnA();
	    btVector3 ptB = pt.getPositionWorldOnB();

	    if (upA == NULL || upB == NULL) {
		bu_log("nearphase_callback : contact %d of %d, could not get user pointers\n",
		       j+1, numContacts);

	    } else{
		bu_log("nearphase_callback: contact %d of %d, %s(%f, %f, %f) , %s(%f, %f, %f)\n",
		       j+1, numContacts,
		       upA->rb_namep, ptA[0], ptA[1], ptA[2],
		       upB->rb_namep, ptB[0], ptB[1], ptB[2]);
	    }
	}

	//Can un-comment this line, and then all points are removed
	//contactManifold->clearManifold();
    }

    // Only dispatch the Bullet collision information if physics should continue
    dispatcher.defaultNearCallback(collisionPair, dispatcher, dispatchInfo);
}


/**
 * C++ wrapper for doing physics using bullet
 *
 */
extern "C" int
run_simulation(struct simulation_params *sim_params)
{
    //int i;

    //for (i=0 ; i < sim_params->duration ; i++) {

    // Initialize the physics world
    btDiscreteDynamicsWorld* dynamicsWorld;

    // Keep the collision shapes, for deletion/cleanup
    btAlignedObjectArray<btCollisionShape*> collision_shapes;

    btBroadphaseInterface* broadphase = new btDbvtBroadphase();
    btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();
    btCollisionDispatcher* dispatcher = new btCollisionDispatcher(collisionConfiguration);

    //Register custom rt based nearphase algo for box-box collision,
    //arbitrary shapes from brlcad are all represented by the box collision shape
    //in bullet, the movement will not be like a box however, but according to
    //the collisions detected by rt and therefore should follow any arbitrary shape correctly
    dispatcher->registerCollisionCreateFunc(BOX_SHAPE_PROXYTYPE,
					    BOX_SHAPE_PROXYTYPE,
					    new btRTCollisionAlgorithm::CreateFunc);

    btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver;

    dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);

    //Set the gravity direction along -ve Z axis
    dynamicsWorld->setGravity(btVector3(0, 0, -10));

    //Add the rigid bodies to the world, including the ground plane
    add_rigid_bodies(dynamicsWorld, sim_params, collision_shapes);

    //Add a broadphase callback to hook to the AABB detection algos
    btOverlapFilterCallback * filterCallback = new broadphase_callback();
    dynamicsWorld->getPairCache()->setOverlapFilterCallback(filterCallback);

    //Add a nearphase callback to hook to the contact points generation algos
    dispatcher->setNearCallback((btNearCallback)nearphase_callback);

    //Step the physics the required number of times
    step_physics(dynamicsWorld, sim_params);

    //Get the world transforms back into the simulation params struct
    get_transforms(dynamicsWorld, sim_params);

    //Clean and free memory used by physics objects
    cleanup(dynamicsWorld, collision_shapes);

    //Clean up stuff in here
    delete filterCallback;
    delete solver;
    delete dispatcher;
    delete collisionConfiguration;
    delete broadphase;
    //}


    return 0;
}


#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
