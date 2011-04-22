/*               U V P O I N T S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2010-2011 United States Government as represented by
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

/* test routines related to quad trees and uv points for NURBS - this
 * is not the final form of this code, just testing.
 */

#include <iostream>
#include <string>
#include <fstream>
#include <iomanip>
#include <vector>
#include <set>
#include <list>
#include <math.h>
#include <time.h>

#define POOL_SIZE 1024

using namespace std;

/* Number of subdivisions to perform */
/*#define MAX_TREE_DEPTH 6*/
#define TREE_DEBUG 0
int rejected = 0;
int counting = 0;

/* For memory management stuff see the tutorial at
 * http://www.ibm.com/developerworks/aix/tutorials/au-memorymanager/section9.html */
class MemoryManager
{
       private:
               std::list<void*> QuadNodePtrList;
               std::vector<void*> MemoryPoolList;
       public:
               MemoryManager( ) {}
               ~MemoryManager( ) {}
               void* allocate(size_t);
               void  free(void*);
};

void* MemoryManager::allocate(size_t size)
{
       void* base = 0;
       if (QuadNodePtrList.empty()) {
               base = new char[size * POOL_SIZE];
               MemoryPoolList.push_back(base);
               for(int i = 0; i < POOL_SIZE; ++i) {
                       QuadNodePtrList.push_front(&(static_cast<char*>(base)[i*size]));
               }
       }
       void* blockPtr = QuadNodePtrList.front();
/*     *((static_cast<QuadNode*>(blockPtr)) + sizeof(QuadNode) - 2) = sizeof(QuadNode);
       *((static_cast<QuadNode*>(blockPtr)) + sizeof(QuadNode) - 1) = 0;*/
       QuadNodePtrList.pop_front();
       return blockPtr;
}

void MemoryManager::free(void* object)
{
       QuadNodePtrList.push_front(object);
}

MemoryManager QuadMemoryManager;

/**
 * UVKey Class - minimal class for holding UV space coordinate keys
 */

class UVKey {
	public:
		UVKey(string newkey);
		const string& getKey() const;
	private:
		string key;
};

UVKey::UVKey(string newkey)
{
	key.assign(newkey);
}

const string& UVKey::getKey() const
{
	const string& keyref = key;
	return keyref;
}

/**
 * UVKeyComp - Tell the C++ set how to compare keys for sorting
 */
class UVKeyComp {
	public:
		bool operator()(const UVKey& key1, const UVKey& key2)
		{
			if(key1.getKey().compare(key2.getKey()) < 0) {
				return true;
			} else {
				return false;
			}
		}
};



/**
 * QuadNode - Holds information about the UV coordinates associated with
 * a node in a subdivision quad tree.  Assumes the following relationship
 * between point indicies and positioning:
 *
 *
 *                   3-------------------2   
 *                   |                   |  
 *                   |    6         8    |  
 *                   |                   |  
 *                 V |         4         |  
 *                   |                   |  
 *                   |    5         7    |  
 *                   |                   |  
 *                   0-------------------1  
 *                             U            

 *
 */

class QuadNode {
	public:
		void SubDivide(int MAX_TREE_DEPTH);
		set<UVKey, UVKeyComp> *keys;
		void AppendKeys(set <UVKey, UVKeyComp> *keys, int MAX_TREE_DEPTH);
		size_t PU[9];
		size_t PV[9];
		int depth;
		QuadNode *Children[4];
                inline void* operator new(size_t size)
                {
                        return QuadMemoryManager.allocate(sizeof(QuadNode));
                }
                inline void operator delete(void* object)
                {
                        QuadMemoryManager.free(object);
                }
};	

int ints_to_key(string *cppstr, int left, int right, int MAX_TREE_DEPTH) 
{
	char formatstring[20];
	char maxkeystr[20];
	char finalstring[20];
	int max_key_length;
	int max_key = pow(2, MAX_TREE_DEPTH + 1);
	sprintf(maxkeystr, "%d", max_key);
	max_key_length = strlen(maxkeystr);
	sprintf(formatstring, "%%\'0%dd%%\'0%dd", max_key_length, max_key_length);
	sprintf(finalstring, formatstring, left, right);
	cppstr->assign(finalstring);
}


void QuadNode::AppendKeys(set <UVKey, UVKeyComp> *keys, int MAX_TREE_DEPTH)
{
	UVKey *point;
	set<UVKey, UVKeyComp>::iterator item;
	string keystring;
	int i;
	for( int i = 0; i < 9; i++ ) {
		counting++;
		ints_to_key(&keystring, PU[i], PV[i], MAX_TREE_DEPTH);
		item = keys->find(keystring);
		if(item == keys->end()) {
			point = new UVKey(keystring);
			keys->insert(*point);
		} else {
			rejected++;
		}
		keystring.erase();
	}
}

/*
 * When subdivision is made, parent coordinate values are used
 * to deduce those of the children.  Some are direct copies, as
 * follows (this relates quadrant conventions to the above diagram
 * identifying point positions by number)
 *
 *    3----------------     ----------------2
 *    |               |     |               |
 *    |               |     |               |
 *  V |       6       |     |       8       |
 *    |               |     |               |
 *    |               |     |               |
 *    ----------------4     4----------------
 *            U                     U
 *
 *        Quadrant 3            Quadrant 2
 *
 *    ----------------4     4----------------
 *    |               |     |               |
 *    |               |     |               |
 *  V |       5       |     |       7       |
 *    |               |     |               |
 *    |               |     |               |
 *    0----------------     ----------------1
 *             U                         U
 *
 *        Quadrant 0            Quadrant 1
 */

void QuadNode::SubDivide(int MAX_TREE_DEPTH)
{
	int i;
		/* Quadrant 0 */
		Children[0] = new QuadNode();
		Children[0]->depth = depth + 1;
		Children[0]->keys = keys;
		Children[0]->PU[0] = PU[0];
		Children[0]->PV[0] = PV[0];
		Children[0]->PU[1] = PU[4];
		Children[0]->PV[1] = PV[0];
		Children[0]->PU[2] = PU[4];
		Children[0]->PV[2] = PV[4];
		Children[0]->PU[3] = PU[0];
		Children[0]->PV[3] = PV[4];
		Children[0]->PU[4] = PU[5];
		Children[0]->PV[4] = PV[5];
		Children[0]->PU[5] = (Children[0]->PU[4] - Children[0]->PU[0])/2 + Children[0]->PU[0];
		Children[0]->PV[5] = (Children[0]->PV[4] - Children[0]->PV[0])/2 + Children[0]->PV[0];
		Children[0]->PU[6] = Children[0]->PU[5];
		Children[0]->PV[6] = 3*(Children[0]->PV[4] - Children[0]->PV[0])/2 + Children[0]->PV[0];
		Children[0]->PU[7] = 3*(Children[0]->PU[4] - Children[0]->PU[0])/2 + Children[0]->PU[0];
		Children[0]->PV[7] = Children[0]->PV[5];
		Children[0]->PU[8] = Children[0]->PU[7];
		Children[0]->PV[8] = Children[0]->PV[6];
		Children[0]->AppendKeys(keys, MAX_TREE_DEPTH);
#if TREE_DEBUG
		cout << "Q0 Depth: " << depth + 1 << "\n";
		cout << "PU: {";
		for( int i = 0; i < 9; i++ ) {
			cout << Children[0]->PU[i] << ",";
		}
		cout << "}\n";
		cout << "PV: {";
		for( int i = 0; i < 9; i++ ) {
			cout << Children[0]->PV[i] << ",";
		}
		cout << "}\n";
#endif
		if (Children[0]->depth < MAX_TREE_DEPTH)
			Children[0]->SubDivide(MAX_TREE_DEPTH);


		/* Quadrant 1 */
		Children[1] = new QuadNode();
		Children[1]->depth = depth + 1;
		Children[1]->keys = keys;
		Children[1]->PU[0] = PU[4];
		Children[1]->PV[0] = PV[1];
		Children[1]->PU[1] = PU[1];
		Children[1]->PV[1] = PV[1];
		Children[1]->PU[2] = PU[1];
		Children[1]->PV[2] = PV[4];
		Children[1]->PU[3] = PU[4];
		Children[1]->PV[3] = PV[4];
		Children[1]->PU[4] = PU[7];   
		Children[1]->PV[4] = PV[7];   
		Children[1]->PU[5] = (Children[1]->PU[4] - Children[1]->PU[0])/2 + Children[1]->PU[0];   
		Children[1]->PV[5] = (Children[1]->PV[4] - Children[1]->PV[0])/2 + Children[1]->PV[0];   
		Children[1]->PU[6] = Children[1]->PU[5];                            
		Children[1]->PV[6] = 3*(Children[1]->PV[4] - Children[1]->PV[0])/2 + Children[1]->PV[0]; 
		Children[1]->PU[7] = 3*(Children[1]->PU[4] - Children[1]->PU[0])/2 + Children[1]->PU[0]; 
		Children[1]->PV[7] = Children[1]->PV[5];                            
		Children[1]->PU[8] = Children[1]->PU[7];                            
		Children[1]->PV[8] = Children[1]->PV[6];                            
		Children[1]->AppendKeys(keys, MAX_TREE_DEPTH);
#if TREE_DEBUG
		cout << "Q1 Depth: " << depth + 1 << "\n";
		cout << "PU: {";
		for( int i = 0; i < 9; i++ ) {
			cout << Children[1]->PU[i] << ",";
		}
		cout << "}\n";
		cout << "PV: {";
		for( int i = 0; i < 9; i++ ) {
			cout << Children[1]->PV[i] << ",";
		}
		cout << "}\n";
#endif
		if (Children[1]->depth < MAX_TREE_DEPTH)
			Children[1]->SubDivide(MAX_TREE_DEPTH);
	
		/* Quadrant 2 */
		Children[2] = new QuadNode();
		Children[2]->depth = depth + 1;
		Children[2]->keys = keys;
		Children[2]->PU[0] = PU[4];
		Children[2]->PV[0] = PV[4];
		Children[2]->PU[1] = PU[2];
		Children[2]->PV[1] = PV[4];
		Children[2]->PU[2] = PU[2];
		Children[2]->PV[2] = PV[2];
		Children[2]->PU[3] = PU[4];
		Children[2]->PV[3] = PV[2];
		Children[2]->PU[4] = PU[8];
		Children[2]->PV[4] = PV[8];
		Children[2]->PU[5] = (Children[2]->PU[4] - Children[2]->PU[0])/2 + Children[2]->PU[0];
		Children[2]->PV[5] = (Children[2]->PV[4] - Children[2]->PV[0])/2 + Children[2]->PV[0];
		Children[2]->PU[6] = Children[2]->PU[5];
		Children[2]->PV[6] = 3*(Children[2]->PV[4] - Children[2]->PV[0])/2 + Children[2]->PV[0];
		Children[2]->PU[7] = 3*(Children[2]->PU[4] - Children[2]->PU[0])/2 + Children[2]->PU[0];
		Children[2]->PV[7] = Children[2]->PV[5];
		Children[2]->PU[8] = Children[2]->PU[7];
		Children[2]->PV[8] = Children[2]->PV[6];
		Children[2]->AppendKeys(keys, MAX_TREE_DEPTH);
#if TREE_DEBUG
		cout << "Q2 Depth: " << depth + 1 << "\n";
		cout << "PU: {";
		for( int i = 0; i < 9; i++ ) {
			cout << Children[2]->PU[i] << ",";
		}
		cout << "}\n";
		cout << "PV: {";
		for( int i = 0; i < 9; i++ ) {
			cout << Children[2]->PV[i] << ",";
		}
		cout << "}\n";
#endif

		if (Children[2]->depth < MAX_TREE_DEPTH)
		Children[2]->SubDivide(MAX_TREE_DEPTH);

		/* Quadrant 3 */
		Children[3] = new QuadNode();
		Children[3]->depth = depth + 1;
		Children[3]->keys = keys;
		Children[3]->PU[0] = PU[3];
		Children[3]->PV[0] = PV[4];
		Children[3]->PU[1] = PU[4];
		Children[3]->PV[1] = PV[4];
		Children[3]->PU[2] = PU[4];
		Children[3]->PV[2] = PV[3];
		Children[3]->PU[3] = PU[3];
		Children[3]->PV[3] = PV[3];
		Children[3]->PU[4] = PU[6];
		Children[3]->PV[4] = PV[6];
		Children[3]->PU[5] = (Children[3]->PU[4] - Children[3]->PU[0])/2 + Children[3]->PU[0];
		Children[3]->PV[5] = (Children[3]->PV[4] - Children[3]->PV[0])/2 + Children[3]->PV[0];
		Children[3]->PU[6] = Children[3]->PU[5];
		Children[3]->PV[6] = 3*(Children[3]->PV[4] - Children[3]->PV[0])/2 + Children[3]->PV[0];
		Children[3]->PU[7] = 3*(Children[3]->PU[4] - Children[3]->PU[0])/2 + Children[3]->PU[0];
		Children[3]->PV[7] = Children[3]->PV[5];
		Children[3]->PU[8] = Children[3]->PU[7];
		Children[3]->PV[8] = Children[3]->PV[6];
		Children[3]->AppendKeys(keys, MAX_TREE_DEPTH);
#if TREE_DEBUG
		cout << "Q3 Depth: " << depth + 1 << "\n";
		cout << "PU: {";
		for( int i = 0; i < 9; i++ ) {
			cout << Children[3]->PU[i] << ",";
		}
		cout << "}\n";
		cout << "PV: {";
		for( int i = 0; i < 9; i++ ) {
			cout << Children[3]->PV[i] << ",";
		}
		cout << "}\n";
#endif

		if (Children[3]->depth < MAX_TREE_DEPTH)
		Children[3]->SubDivide(MAX_TREE_DEPTH);
	
}

/**
 * UVKeyQuadTree - This holds a bounding box subdivision tree using
 * a quad tree on a generic UV space - this is used to identify keys that
 * will potentially need to have points evaluated for a real surface tree
 * build
 */
class UVKeyQuadTree {
	public:
		UVKeyQuadTree(set<UVKey, UVKeyComp> *keys, int MAX_TREE_DEPTH);
		QuadNode *root;
};

UVKeyQuadTree::UVKeyQuadTree(set<UVKey, UVKeyComp> *keys, int MAX_TREE_DEPTH)
{
	UVKey *point;
	set<UVKey, UVKeyComp>::iterator item;
	int Ucoord;
	int Vcoord;
	string keynum;
	int i;

	root = new QuadNode();
	root->depth = 0;
	root->keys = keys;
	root->PU[0] = 0;
	root->PV[0] = 0;
	root->PU[1] = pow(2, MAX_TREE_DEPTH + 1);
	root->PV[1] = 0;
	root->PU[2] = pow(2, MAX_TREE_DEPTH + 1);
	root->PV[2] = pow(2, MAX_TREE_DEPTH + 1);
	root->PU[3] = 0;
	root->PV[3] = pow(2, MAX_TREE_DEPTH + 1);
	root->PU[4] = pow(2, MAX_TREE_DEPTH + 1)/2;
	root->PV[4] = pow(2, MAX_TREE_DEPTH + 1)/2;
	root->PU[5] = pow(2, MAX_TREE_DEPTH + 1)/4;
	root->PV[5] = pow(2, MAX_TREE_DEPTH + 1)/4;
	root->PU[6] = pow(2, MAX_TREE_DEPTH + 1)/4;
	root->PV[6] = 3*pow(2, MAX_TREE_DEPTH + 1)/4;
	root->PU[7] = 3*pow(2, MAX_TREE_DEPTH + 1)/4;
	root->PV[7] = pow(2, MAX_TREE_DEPTH + 1)/4;
	root->PU[8] = 3*pow(2, MAX_TREE_DEPTH + 1)/4; 
	root->PV[8] = 3*pow(2, MAX_TREE_DEPTH + 1)/4;
	for( int i = 0; i < 9; i++ ) {
		counting++;
		cout << root->PU[i] << "," << root->PV[i] << "\n";
		ints_to_key(&keynum, root->PU[i], root->PV[i], MAX_TREE_DEPTH);
		item = keys->find(keynum);
		if(item == keys->end()) {
			point = new UVKey(keynum);
			keys->insert(*point);
		}
		keynum.erase();
	}	
}

int main(int argc, char **argv)
{
	int matsize;
	time_t t0, t1;
	int MAX_TREE_DEPTH, tdiff;
	if(argc == 1) {
		MAX_TREE_DEPTH = 2;
	} else {
		MAX_TREE_DEPTH = atoi(argv[1]);
	}
	t0 = time(NULL);
	t1 = time(NULL);
	matsize = pow(2, MAX_TREE_DEPTH + 1) + 1;
	vector<vector<int> > matitems ( matsize , vector<int> ( matsize ) );
	int k = 0;

	cout << "Max tree depth: " << MAX_TREE_DEPTH << "\n";
	if (MAX_TREE_DEPTH < 3) {
	cout << "Matrix: \n";
	for ( int i = 0; i < matsize; i++ ) {
		for ( int j = 0; j < matsize; j++ )
			matitems[i][j] = k++;
	}

	for ( int i = matsize - 1; i >= 0; i-- ) {
		for ( int j = 0; j < matsize; j++ )
			cout<< setw ( MAX_TREE_DEPTH ) << setfill( '0' ) << j << "," << setw ( MAX_TREE_DEPTH ) << setfill( '0' ) << i <<' ';
		cout<<'\n';
	}
	cout<<'\n';
	}


	set <UVKey, UVKeyComp> keys;
	set<UVKey, UVKeyComp>::iterator keyiterator;
	UVKeyQuadTree *testtree = new UVKeyQuadTree(&keys, MAX_TREE_DEPTH);
	testtree->root->SubDivide(MAX_TREE_DEPTH);
	t1 = time(NULL);
	tdiff = (int)difftime(t1,t0);
	printf("subdivide: %d sec\n", tdiff);
	t0 = time(NULL);

/*	for(keyiterator = keys.begin(); keyiterator != keys.end(); keyiterator++) {
		cout << "Key: " << keyiterator->getKey()  << "\n";
	}*/
	ofstream keyfile;
	keyfile.open("keys.txt");
	for(keyiterator = keys.begin(); keyiterator != keys.end(); keyiterator++) {
		keyfile << keyiterator->getKey()  << "\n";
	}
	keyfile.close();

	cout << "Total checked: " << counting << "\n";
	cout << "Rejected from Set: " << rejected << "\n";
	cout << "Set Contains " << keys.size() << " items\n";
}
