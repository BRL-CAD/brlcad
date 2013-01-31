/**
 * @memo	Macros for examining and changing the path to a vdsNode.
 * @name 	Manipulating vdsNodePath structures.
 *
 * 		These preprocessor macros manipulate vdsNodePath, the bit 
 *		vector that encapsulates the path from the root of a vertex 
 *		tree to a node.<p><dl>
 *
 * <dd>Details:	For a binary tree, each bit represents a branch.  For an 8-way
 *		tree, 3 bits represent a branch.  And so on.  The least
 *		significant bit(s) represent the branch from the root node to
 *		the level-1 node, the next least significant bit(s) represent
 *		the next branch, and so on; the most significant meaningful
 *		bit(s) represent the final branch to the leaf node.  The root
 *		node has a depth of 0 and none of the bits mean anything (but
 *		all must be set to zero, see below).<p>
 * <dd>Note 1:	Defining VDS_64_BITS enforces 64-bit paths.  This enables much
 *		larger vertex trees but may be slower on some architectures.<p>
 * <dd>Note 2: 	For convenience, the depth of the node (and thus the node path
 *		length) is currently stored separately, and all unused bits
 *		must be set to zero (this eases path comparisons).</dl>
 *
 * @see		path.h
 */
/*@{*/

/** Copy one vdsNodePath structure into another.
 * 		Copies the vdsNodePath <b>src</b> into <b>dst</b>.
 */
#define PATH_COPY(dst,src) ((dst) = (src))

/** Look up a particular branch within a vdsNodePath.
 * 		Given a vdsNodePath P and a depth D, returns a branch number
 *		indicating which child of the node at depth D is an ancestor
 *		of the node represented by P.
 */
#define PATH_BRANCH(P,D) (((P)>>((D)*VDS_NUMBITS))&VDS_BITMASK)

/** Set a particular branch within a vdsNodePath.
 * 		Takes a vdsNodePath P representing node N, a depth D,
 *		and a branch number B.  If node A is the ancestor of N at
 *		depth D, sets P to indicate that A->children[B] is the 
 *		ancestor of N at depth D+1.<p>
 * Note:	B is assumed to be <= VDS_MAXDEGREE; no checking is done
 */
#define PATH_SET_BRANCH(P,D,B) (P |= ((vdsNodePath)(B)<<((D)*VDS_NUMBITS)))

/*@}*/
