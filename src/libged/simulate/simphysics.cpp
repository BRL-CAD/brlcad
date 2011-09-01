/*                          S I M P H Y S I C S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/simulate/simphysics.cpp
 *
 *
 * Routines related to performing physics on the passed regions
 *
 * 
 * 
 */

#include "common.h"

#ifdef HAVE_BULLET

#include <iostream>

#include "db.h"
#include "vmath.h"
#include "simulate.h"

#include <btBulletDynamicsCommon.h>

/**
 * Prints the 16 by 16 transform matrices for debugging
 *
 */
void print_matrices(struct bu_vls *result_str, mat_t t, btScalar *m)
{
	int i, j;

	bu_vls_printf(result_str,"------------Transformation matrices(Debug)--------------\n");

	for (i=0 ; i<4 ; i++) {
		for (j=0 ; j<4 ; j++) {
			bu_vls_printf(result_str,"t[%d]: %f\t", (i*4 + j), t[i*4 + j] );
		}
		bu_vls_printf(result_str,"\n");
	}

	bu_vls_printf(result_str,"\n");

	for (i=0 ; i<4 ; i++) {
		for (j=0 ; j<4 ; j++) {
			bu_vls_printf(result_str,"m[%d]: %f\t", (j*4 + i), m[j*4 + i] );
		}
		bu_vls_printf(result_str,"\n");
	}

	bu_vls_printf(result_str,"-------------------------------------------------------\n");

}

/**
 * C++ wrapper for doing physics using bullet
 * 
 */
extern "C" int
run_simulation(struct bu_vls *result_str, struct simulation_params *sim_params)
{
	int i;
	btScalar m[16];
	int num_steps = sim_params->duration;
	struct rigid_body *current_node;

	/* Show list of objects to be added to the sim */
	bu_log("The following %d rigid bodies will participate in the simulation : \n", sim_params->num_bodies);
	for (current_node = sim_params->head_node; current_node != NULL; current_node = current_node->next) {
		bu_vls_printf(result_str, "Rigid Body : %s\n", current_node->rb_namep);
	}

	bu_vls_printf(result_str, "Simulation will run for %d steps.\n",num_steps);
	bu_vls_printf(result_str, "A 1 kg sphere will be dropped from a height of 50 m.\n");
	bu_vls_printf(result_str, "Every time step is 1/60th of a second long(can be altered).\n");

	btBroadphaseInterface* broadphase = new btDbvtBroadphase();

	btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();
	btCollisionDispatcher* dispatcher = new btCollisionDispatcher(collisionConfiguration);

	btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver;

	btDiscreteDynamicsWorld* dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher,broadphase,solver,collisionConfiguration);

	dynamicsWorld->setGravity(btVector3(0,-10,0));


	btCollisionShape* groundShape = new btStaticPlaneShape(btVector3(0,1,0),1);

	btCollisionShape* fallShape = new btSphereShape(1);


	btDefaultMotionState* groundMotionState = new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1),btVector3(0,-1,0)));
	btRigidBody::btRigidBodyConstructionInfo
			groundRigidBodyCI(0,groundMotionState,groundShape,btVector3(0,0,0));
	btRigidBody* groundRigidBody = new btRigidBody(groundRigidBodyCI);
	dynamicsWorld->addRigidBody(groundRigidBody);


	btDefaultMotionState* fallMotionState =
			new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1),btVector3(0,50,0)));
	btScalar mass = 1;
	btVector3 fallInertia(0,0,0);
	fallShape->calculateLocalInertia(mass,fallInertia);
	btRigidBody::btRigidBodyConstructionInfo fallRigidBodyCI(mass,fallMotionState,fallShape,fallInertia);
	btRigidBody* fallRigidBody = new btRigidBody(fallRigidBodyCI);
	dynamicsWorld->addRigidBody(fallRigidBody);

	bu_vls_printf(result_str, "----- Starting simulation -----\n");
	btTransform trans;
	for (i=0 ; i<num_steps ; i++) {
		//time step of 1/60th of a second(same as internal fixedTimeStep, maxSubSteps=10 to cover 1/60th sec.)
		dynamicsWorld->stepSimulation(1/60.f,10);
		fallRigidBody->getMotionState()->getWorldTransform(trans);
		bu_vls_printf(result_str, "Step %3d : Sphere height: %f\n", i, trans.getOrigin().getY());

	}

	bu_vls_printf(result_str, "----- Simulation Complete -----\n");
	trans.getOpenGLMatrix(m);

	//Copy the transform matrix : transpose to convert from column major to row major, y to z and vice versa
	//Use transform matrix to convert openGL Y axis up to BRL-CAD Z axis up, both are right handed:TODO
/*	t[0]  = m[0];  t[1]  = m[4];  t[2]  = m[8];  t[3]  = m[12];
	t[4]  = m[1];  t[5]  = m[5];  t[6]  = m[9];  t[7]  = m[13];
	t[8]  = m[2];  t[9]  = m[6];  t[10] = m[10]; t[11] = m[14];
	t[12] = m[3]; t[13]  = m[7];  t[14] = m[11]; t[15] = m[15];

	print_matrices(result_str, t, m);*/


	//Cleanup in order of creation : better ways for cleanup is possible
	dynamicsWorld->removeRigidBody(fallRigidBody);
	delete fallRigidBody->getMotionState();
	delete fallRigidBody;

	dynamicsWorld->removeRigidBody(groundRigidBody);
	delete groundRigidBody->getMotionState();
	delete groundRigidBody;
	delete fallShape;
	delete groundShape;
	delete dynamicsWorld;
	delete solver;
	delete collisionConfiguration;
	delete dispatcher;
	delete broadphase;

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
