/*
Copyright (c) 2006, Michael Kazhdan and Matthew Bolitho
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of
conditions and the following disclaimer. Redistributions in binary form must reproduce
the above copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the distribution. 

Neither the name of the Johns Hopkins University nor the names of its contributors
may be used to endorse or promote products derived from this software without specific
prior written permission. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO THE IMPLIED WARRANTIES 
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#ifdef _WIN32
#include <Windows.h>
#include <Psapi.h>
#endif // _WIN32
#include "MyTime.h"
#include "MarchingCubes.h"
#include "Octree.h"
#include "SparseMatrix.h"
#include "CmdLineParser.h"
#include "PPolynomial.h"
#include "Ply.h"
#include "MemoryUsage.h"
#ifdef _OPENMP
#include "omp.h"
#endif // _OPENMP
void DumpOutput( const char* format , ... );
#include "MultiGridOctreeData.h"
void DumpOutput2( std::vector< char* >& comments , const char* format , ... );

#define DEFAULT_FULL_DEPTH 5

#define XSTR(x) STR(x)
#define STR(x) #x
#if DEFAULT_FULL_DEPTH
#pragma message ( "[WARNING] Setting default full depth to " XSTR(DEFAULT_FULL_DEPTH) )
#endif // DEFAULT_FULL_DEPTH

#include <stdarg.h>
char* outputFile=NULL;
int echoStdout=0;
void DumpOutput( const char* format , ... )
{
	if( outputFile )
	{
		FILE* fp = fopen( outputFile , "a" );
		va_list args;
		va_start( args , format );
		vfprintf( fp , format , args );
		fclose( fp );
		va_end( args );
	}
	if( echoStdout )
	{
		va_list args;
		va_start( args , format );
		vprintf( format , args );
		va_end( args );
	}
}
void DumpOutput2( std::vector< char* >& comments  , const char* format , ... )
{
	if( outputFile )
	{
		FILE* fp = fopen( outputFile , "a" );
		va_list args;
		va_start( args , format );
		vfprintf( fp , format , args );
		fclose( fp );
		va_end( args );
	}
	if( echoStdout )
	{
		va_list args;
		va_start( args , format );
		vprintf( format , args );
		va_end( args );
	}
	comments.push_back( new char[1024] );
	char* str = comments.back();
	va_list args;
	va_start( args , format );
	vsprintf( str , format , args );
	va_end( args );
	if( str[strlen(str)-1]=='\n' ) str[strlen(str)-1] = 0;
}


cmdLineString
	In( "in" ) ,
	Out( "out" ) ,
	VoxelGrid( "voxel" ) ,
	XForm( "xForm" );

cmdLineReadable
#ifdef _WIN32
	Performance( "performance" ) ,
#endif // _WIN32
	Complete( "complete" ) ,
	ShowResidual( "showResidual" ) ,
	NoComments( "noComments" ) ,
	PolygonMesh( "polygonMesh" ) ,
	Confidence( "confidence" ) ,
	NormalWeights( "nWeights" ) ,
	NonManifold( "nonManifold" ) ,
	ASCII( "ascii" ) ,
	Density( "density" ) ,
	Verbose( "verbose" ) ,
	Double( "double" );

cmdLineInt
	Depth( "depth" , 8 ) ,
	CGDepth( "cgDepth" , 0 ) ,
	KernelDepth( "kernelDepth" ) ,
	AdaptiveExponent( "adaptiveExp" , 1 ) ,
	Iters( "iters" , 8 ) ,
	VoxelDepth( "voxelDepth" , -1 ) ,
	FullDepth( "fullDepth" , DEFAULT_FULL_DEPTH ) ,
	MinDepth( "minDepth" , 0 ) ,
	MaxSolveDepth( "maxSolveDepth" ) ,
	BoundaryType( "boundary" , 1 ) ,
	Threads( "threads" , omp_get_num_procs() );

cmdLineFloat
	Color( "color" , 16.f ) ,
	SamplesPerNode( "samplesPerNode" , 1.5f ) ,
	Scale( "scale" , 1.1f ) ,
	CSSolverAccuracy( "cgAccuracy" , float(1e-3) ) ,
	PointWeight( "pointWeight" , 4.f );


cmdLineReadable* params[] =
{
	&In , &Depth , &Out , &XForm ,
	&Scale , &Verbose , &CSSolverAccuracy , &NoComments , &Double ,
	&KernelDepth , &SamplesPerNode , &Confidence , &NormalWeights , &NonManifold , &PolygonMesh , &ASCII , &ShowResidual , &VoxelDepth ,
	&PointWeight , &VoxelGrid , &Threads , &MaxSolveDepth ,
	&AdaptiveExponent , &BoundaryType ,
	&Density ,
	&FullDepth ,
	&MinDepth ,
	&CGDepth , &Iters ,
	&Complete ,
	&Color ,
#ifdef _WIN32
	&Performance ,
#endif // _WIN32
};


//DRH: Copied from SPR.h/cpp
struct spr_options {
    const char *xform;
    const char *voxelgrid;
    const char *confidence;
    const char *normalweights;
    /* boolean */
    int density;
    int confidenceset;
    int normalweightsset;
    int nonManifold;
    int polygon;
    int colorset;
    int complete;
    int showresidual;
    /* int */
    int depth;
    int cgdepth;
    int kerneldepth;
    int adaptiveexponent;
    int iters;
    int voxeldepth;
    int fulldepth;
    int mindepth;
    int maxsolvedepth;
    int boundarytype;
    int thread_cnt;
    
    double samples_per_node;
    double scale;
    double cssolveraccuracy;
    double pointweight;
    double color;

    /* comments */
    void* comments;  // needs to be of type std::vector< char* >
};

template< class Real, class Vertex > int
spr_surface_build_cpp_with_data(XForm4x4< Real>& xForm,
				OrientedPointStreamWithData< float, Point3D< unsigned char > >* pointStream,
				const struct spr_options &opts,
				CoredFileMeshData< Vertex >* mesh)
{
    double maxMemoryUsage;
    double t;
    double tt=Time();
    std::vector< char *>* comments = (std::vector< char *>*) opts.comments;
    
    // Probably unnecessary but here to be consistent with original code
    Reset< Real >();
    Octree< Real > tree;
    typename Octree< Real >::template SparseNodeData< typename Octree< Real >::PointData >* pointInfo = new typename Octree< Real >::template SparseNodeData< typename Octree< Real >::PointData >();    
    typename Octree< Real >::template SparseNodeData< Point3D< Real > >* normalInfo = new typename Octree< Real >::template SparseNodeData< Point3D< Real > >();
    std::vector< Real >* kernelDensityWeights = new std::vector< Real >();
    std::vector< Real >* centerWeights = new std::vector< Real >();

    t=Time() , tree.maxMemoryUsage=0;
    tree.threads = opts.thread_cnt;

    OctNode< TreeNodeData >::SetAllocator( MEMORY_ALLOCATOR_BLOCK_SIZE );
    
    typedef typename Octree< Real >::template ProjectiveData< Point3D< Real > > ProjectiveColor;
    typename Octree< Real >::template SparseNodeData< ProjectiveColor > colorData;
    int pointCount = tree.template SetTree< float >(pointStream , opts.mindepth, opts.depth, opts.fulldepth, opts.kerneldepth,
						    opts.samples_per_node, opts.scale, opts.confidenceset, opts.normalweightsset,
						    opts.pointweight, opts.adaptiveexponent, *kernelDensityWeights, *pointInfo,
						    *normalInfo , *centerWeights, colorData, xForm, opts.boundarytype, opts.complete);
    for( const OctNode< TreeNodeData >* n = tree.tree.nextNode() ; n!=NULL ; n=tree.tree.nextNode( n ) )
    {
	int idx = colorData.index( n );
	if( idx>=0 ) colorData.data[idx] *= (Real)pow( opts.color , n->depth() );
    }
    
    DumpOutput2( *comments , "#             Tree set in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    DumpOutput( "Input Points: %d\n" , pointCount );
    DumpOutput( "Leaves/Nodes: %d/%d\n" , tree.tree.leaves() , tree.tree.nodes() );
    DumpOutput( "Memory Usage: %.3f MB\n" , float( MemoryInfo::Usage() )/(1<<20) );
    
    maxMemoryUsage = tree.maxMemoryUsage;
    t=Time() , tree.maxMemoryUsage=0;
    Pointer( Real ) constraints = tree.SetLaplacianConstraints( *normalInfo );
    delete normalInfo;
    DumpOutput2( *comments , "#      Constraints set in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    DumpOutput( "Memory Usage: %.3f MB\n" , float( MemoryInfo::Usage())/(1<<20) );
    maxMemoryUsage = std::max< double >( maxMemoryUsage , tree.maxMemoryUsage );

    t=Time() , tree.maxMemoryUsage=0;
    Pointer( Real ) solution = tree.SolveSystem(*pointInfo, constraints, opts.showresidual, opts.iters, opts.maxsolvedepth,
						opts.cgdepth, float(opts.cssolveraccuracy));
    delete pointInfo;
    FreePointer(constraints);
    
    DumpOutput2( *comments , "# Linear system solved in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    DumpOutput( "Memory Usage: %.3f MB\n" , float( MemoryInfo::Usage() )/(1<<20) );
    maxMemoryUsage = std::max< double >( maxMemoryUsage , tree.maxMemoryUsage );
    
    t=Time();
    Real isoValue = tree.GetIsoValue( solution , *centerWeights );
    centerWeights->clear();
    delete centerWeights;
    centerWeights = NULL;
    
    DumpOutput( "Got average in: %f\n" , Time()-t );
    DumpOutput( "Iso-Value: %e\n" , isoValue );

    /* VoxelGrid.set ??? */
    
    t = Time() , tree.maxMemoryUsage = 0;
    tree.GetMCIsoSurface( opts.density ? GetPointer(*kernelDensityWeights) : NullPointer( Real ) ,
			  opts.colorset ? & colorData : NULL, solution , isoValue , *mesh , true ,
			  !opts.nonManifold , opts.polygon );

    if( opts.polygon )
	DumpOutput2( *comments , "#         Got polygons in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    else
	DumpOutput2( *comments , "#        Got triangles in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    maxMemoryUsage = std::max< double >( maxMemoryUsage , tree.maxMemoryUsage );
    DumpOutput2( *comments , "#             Total Solve: %9.1f (s), %9.1f (MB)\n" , Time()-tt , maxMemoryUsage );
    
    kernelDensityWeights->clear();
    delete kernelDensityWeights;
    kernelDensityWeights = NULL;

    // Cleanup
    FreePointer( solution );
    Reset< Real >();

    return 0;
}

template< class Real, class Vertex > int
spr_surface_build_cpp(XForm4x4< Real>& xForm,
		      OrientedPointStream< float >* pointStream,
		      const struct spr_options &opts,
		      CoredFileMeshData< Vertex >* mesh)
{
    double maxMemoryUsage;
    double t;
    double tt=Time();
    std::vector< char *>* comments = (std::vector< char *>*) opts.comments;
    
    // Probably unnecessary but here to be consistent with original code
    Reset< Real >();
    Octree< Real > tree;
    typename Octree< Real >::template SparseNodeData< typename Octree< Real >::PointData >* pointInfo = new typename Octree< Real >::template SparseNodeData< typename Octree< Real >::PointData >();    
    typename Octree< Real >::template SparseNodeData< Point3D< Real > >* normalInfo = new typename Octree< Real >::template SparseNodeData< Point3D< Real > >();
    std::vector< Real >* kernelDensityWeights = new std::vector< Real >();
    std::vector< Real >* centerWeights = new std::vector< Real >();

    t=Time() , tree.maxMemoryUsage=0;
    tree.threads = opts.thread_cnt;

    OctNode< TreeNodeData >::SetAllocator( MEMORY_ALLOCATOR_BLOCK_SIZE );
    
    typedef typename Octree< Real >::template ProjectiveData< Point3D< Real > > ProjectiveColor;
    typename Octree< Real >::template SparseNodeData< ProjectiveColor > colorData;    
    int pointCount = tree.template SetTree< float >(pointStream , opts.mindepth, opts.depth, opts.fulldepth, opts.kerneldepth,
						    opts.samples_per_node, opts.scale, opts.confidenceset, opts.normalweightsset,
						    opts.pointweight, opts.adaptiveexponent, *kernelDensityWeights, *pointInfo,
						    *normalInfo , *centerWeights, xForm, opts.boundarytype, opts.complete);
    
    DumpOutput2( *comments , "#             Tree set in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    DumpOutput( "Input Points: %d\n" , pointCount );
    DumpOutput( "Leaves/Nodes: %d/%d\n" , tree.tree.leaves() , tree.tree.nodes() );
    DumpOutput( "Memory Usage: %.3f MB\n" , float( MemoryInfo::Usage() )/(1<<20) );
    
    maxMemoryUsage = tree.maxMemoryUsage;
    t=Time() , tree.maxMemoryUsage=0;
    Pointer( Real ) constraints = tree.SetLaplacianConstraints( *normalInfo );
    delete normalInfo;
    DumpOutput2( *comments , "#      Constraints set in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    DumpOutput( "Memory Usage: %.3f MB\n" , float( MemoryInfo::Usage())/(1<<20) );
    maxMemoryUsage = std::max< double >( maxMemoryUsage , tree.maxMemoryUsage );

    t=Time() , tree.maxMemoryUsage=0;
    Pointer( Real ) solution = tree.SolveSystem(*pointInfo, constraints, opts.showresidual, opts.iters, opts.maxsolvedepth,
						opts.cgdepth, float(opts.cssolveraccuracy));
    delete pointInfo;
    FreePointer(constraints);
    
    DumpOutput2( *comments , "# Linear system solved in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    DumpOutput( "Memory Usage: %.3f MB\n" , float( MemoryInfo::Usage() )/(1<<20) );
    maxMemoryUsage = std::max< double >( maxMemoryUsage , tree.maxMemoryUsage );
    
    t=Time();
    Real isoValue = tree.GetIsoValue( solution , *centerWeights );
    centerWeights->clear();
    delete centerWeights;
    centerWeights = NULL;
    DumpOutput( "Got average in: %f\n" , Time()-t );
    DumpOutput( "Iso-Value: %e\n" , isoValue );

    /* VoxelGrid.set ??? */
    
    t = Time() , tree.maxMemoryUsage = 0;
    tree.GetMCIsoSurface( opts.density ? GetPointer(*kernelDensityWeights) : NullPointer( Real ) ,
			  opts.colorset ? & colorData : NULL, solution , isoValue , *mesh , true ,
			  !opts.nonManifold , opts.polygon );

    if( opts.polygon )
	DumpOutput2( *comments , "#         Got polygons in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    else
	DumpOutput2( *comments , "#        Got triangles in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    maxMemoryUsage = std::max< double >( maxMemoryUsage , tree.maxMemoryUsage );
    DumpOutput2( *comments , "#             Total Solve: %9.1f (s), %9.1f (MB)\n" , Time()-tt , maxMemoryUsage );
    
    kernelDensityWeights->clear();
    delete kernelDensityWeights;
    kernelDensityWeights = NULL;

    // Cleanup
    FreePointer( solution );
    Reset< Real >();

    return 0;
}
//DRH

void ShowUsage(char* ex)
{
	printf( "Usage: %s\n" , ex );
	printf( "\t --%s  <input points>\n" , In.name );

	printf( "\t[--%s <ouput triangle mesh>]\n" , Out.name );
	printf( "\t[--%s <ouput voxel grid>]\n" , VoxelGrid.name );

	printf( "\t[--%s <maximum reconstruction depth>=%d]\n" , Depth.name , Depth.value );
	printf( "\t\t Running at depth d corresponds to solving on a 2^d x 2^d x 2^d\n" );
	printf( "\t\t voxel grid.\n" );

	printf( "\t[--%s <full depth>=%d]\n" , FullDepth.name , FullDepth.value );
	printf( "\t\t This flag specifies the depth up to which the octree should be complete.\n" );

	printf( "\t[--%s <depth at which to extract the voxel grid>=<%s>]\n" , VoxelDepth.name , Depth.name );

	printf( "\t[--%s <conjugate-gradients depth>=%d]\n" , CGDepth.name , CGDepth.value );
	printf( "\t\t The depth up to which a conjugate-gradients solver should be used.\n");

	printf( "\t[--%s <scale factor>=%f]\n" , Scale.name , Scale.value );
	printf( "\t\t Specifies the factor of the bounding cube that the input\n" );
	printf( "\t\t samples should fit into.\n" );

	printf( "\t[--%s <minimum number of samples per node>=%f]\n" , SamplesPerNode.name, SamplesPerNode.value );
	printf( "\t\t This parameter specifies the minimum number of points that\n" );
	printf( "\t\t should fall within an octree node.\n" );

	printf( "\t[--%s <interpolation weight>=%f]\n" , PointWeight.name , PointWeight.value );
	printf( "\t\t This value specifies the weight that point interpolation constraints are\n" );
	printf( "\t\t given when defining the (screened) Poisson system.\n" );

	printf( "\t[--%s <iterations>=%d]\n" , Iters.name , Iters.value );
	printf( "\t\t This flag specifies the (maximum if CG) number of solver iterations.\n" );

	printf( "\t[--%s <pull factor>]\n" , Color.name );
	printf( "\t\t This flag specifies the pull factor for color interpolation\n" );

#ifdef _OPENMP
	printf( "\t[--%s <num threads>=%d]\n" , Threads.name , Threads.value );
	printf( "\t\t This parameter specifies the number of threads across which\n" );
	printf( "\t\t the solver should be parallelized.\n" );
#endif // _OPENMP

	printf( "\t[--%s]\n" , Confidence.name );
	printf( "\t\t If this flag is enabled, the size of a sample's normals is\n" );
	printf( "\t\t used as a confidence value, affecting the sample's\n" );
	printf( "\t\t constribution to the reconstruction process.\n" );

	printf( "\t[--%s]\n" , NormalWeights.name );
	printf( "\t\t If this flag is enabled, the size of a sample's normals is\n" );
	printf( "\t\t used as to modulate the interpolation weight.\n" );

#if 0
	printf( "\t[--%s]\n" , NonManifold.name );
	printf( "\t\t If this flag is enabled, the isosurface extraction does not add\n" );
	printf( "\t\t a planar polygon's barycenter in order to ensure that the output\n" );
	printf( "\t\t mesh is manifold.\n" );
#endif

	printf( "\t[--%s]\n" , PolygonMesh.name);
	printf( "\t\t If this flag is enabled, the isosurface extraction returns polygons\n" );
	printf( "\t\t rather than triangles.\n" );

#if 0
	printf( "\t[--%s <minimum depth>=%d]\n" , MinDepth.name , MinDepth.value );
	printf( "\t\t This flag specifies the coarsest depth at which the system is to be solved.\n" );

	printf( "\t[--%s <cg solver accuracy>=%g]\n" , CSSolverAccuracy.name , CSSolverAccuracy.value );
	printf( "\t\t This flag specifies the accuracy cut-off to be used for CG.\n" );

	printf( "\t[--%s <adaptive weighting exponent>=%d]\n", AdaptiveExponent.name , AdaptiveExponent.value );
	printf( "\t\t This flag specifies the exponent scale for the adaptive weighting.\n" );

#ifdef _WIN32
	printf( "\t[--%s]\n" , Performance.name );
	printf( "\t\t If this flag is enabled, the running time and peak memory usage\n" );
	printf( "\t\t is output after the reconstruction.\n" );
#endif // _WIN32
#endif

	printf( "\t[--%s]\n" , Density.name );
	printf( "\t\t If this flag is enabled, the sampling density is written out with the vertices.\n" );

#if 0
	printf( "\t[--%s]\n" , ASCII.name );
	printf( "\t\t If this flag is enabled, the output file is written out in ASCII format.\n" );
	
	printf( "\t[--%s]\n" , NoComments.name );
	printf( "\t\t If this flag is enabled, the output file will not include comments.\n" );
#endif
	
	printf( "\t[--%s]\n" , Double.name );
	printf( "\t\t If this flag is enabled, the reconstruction will be performed with double-precision floats.\n" );

	printf( "\t[--%s]\n" , Verbose.name );
	printf( "\t\t If this flag is enabled, the progress of the reconstructor will be output to STDOUT.\n" );
}

Point3D< unsigned char > ReadASCIIColor( FILE* fp )
{
	Point3D< unsigned char > c;
	if( fscanf( fp , " %c %c %c " , &c[0] , &c[1] , &c[2] )!=3 ) fprintf( stderr , "[ERROR] Failed to read color\n" ) , exit( 0 );
	return c;
}

PlyProperty PlyColorProperties[]=
{
	{ "r"     , PLY_UCHAR , PLY_UCHAR , int( offsetof( Point3D< unsigned char > , coords[0] ) ) , 0 , 0 , 0 , 0 } ,
	{ "g"     , PLY_UCHAR , PLY_UCHAR , int( offsetof( Point3D< unsigned char > , coords[1] ) ) , 0 , 0 , 0 , 0 } ,
	{ "b"     , PLY_UCHAR , PLY_UCHAR , int( offsetof( Point3D< unsigned char > , coords[2] ) ) , 0 , 0 , 0 , 0 } ,
	{ "red"   , PLY_UCHAR , PLY_UCHAR , int( offsetof( Point3D< unsigned char > , coords[0] ) ) , 0 , 0 , 0 , 0 } , 
	{ "green" , PLY_UCHAR , PLY_UCHAR , int( offsetof( Point3D< unsigned char > , coords[1] ) ) , 0 , 0 , 0 , 0 } ,
	{ "blue"  , PLY_UCHAR , PLY_UCHAR , int( offsetof( Point3D< unsigned char > , coords[2] ) ) , 0 , 0 , 0 , 0 }
};

bool ValidPlyColorProperties( const bool* props ){ return ( props[0] || props[3] ) && ( props[1] || props[4] ) && ( props[2] || props[5] ); }

template< class Real , class Vertex >
int Execute( int argc , char* argv[] )
{
        struct spr_options opts;  // conversion options
	int paramNum = sizeof(params)/sizeof(cmdLineReadable*);
	std::vector< char* > comments;
	opts.comments = (void *) &comments;
	
	if( Verbose.set ) echoStdout=1;

	XForm4x4< Real > xForm , iXForm;
	if( XForm.set )
	{
		FILE* fp = fopen( XForm.value , "r" );
		if( !fp )
		{
			fprintf( stderr , "[WARNING] Could not read x-form from: %s\n" , XForm.value );
			xForm = XForm4x4< Real >::Identity();
		}
		else
		{
			for( int i=0 ; i<4 ; i++ ) for( int j=0 ; j<4 ; j++ )
			{
				float f;
				if( fscanf( fp , " %f " , &f )!=1 ) fprintf( stderr , "[ERROR] Execute: Failed to read xform\n" ) , exit( 0 );
				xForm(i,j) = (Real)f;
			}
			fclose( fp );
		}
	}
	else xForm = XForm4x4< Real >::Identity();
	iXForm = xForm.inverse();

	DumpOutput2( comments , "Running Screened Poisson Reconstruction (Version 7.0)\n" );
	char str[1024];
	for( int i=0 ; i<paramNum ; i++ )
		if( params[i]->set )
		{
			params[i]->writeValue( str );
			if( strlen( str ) ) DumpOutput2( comments , "\t--%s %s\n" , params[i]->name , str );
			else                DumpOutput2( comments , "\t--%s\n" , params[i]->name );
		}

	if( !In.set )
	{
		ShowUsage(argv[0]);
		return 0;
	}

	int kernelDepth = KernelDepth.set ?  KernelDepth.value : Depth.value-2;
	if( kernelDepth>Depth.value )
	{
		fprintf( stderr,"[ERROR] %s can't be greater than %s: %d <= %d\n" , KernelDepth.name , Depth.name , KernelDepth.value , Depth.value );
		return EXIT_FAILURE;
	}
	
	if( !MaxSolveDepth.set ) MaxSolveDepth.value = Depth.value;

	// setup options
	memset(&opts, 0, sizeof(struct spr_options));
	opts.thread_cnt = Threads.value;
	opts.mindepth = MinDepth.value;
	opts.depth = Depth.value;
	opts.fulldepth = FullDepth.value;
	opts.kerneldepth = kernelDepth;
	opts.samples_per_node = Real(SamplesPerNode.value);
	opts.scale = Scale.value;
	opts.density = Density.set;
	opts.confidenceset = Confidence.set;
	opts.normalweightsset = NormalWeights.set;
	opts.pointweight = PointWeight.value;
	opts.adaptiveexponent = AdaptiveExponent.value;
	opts.colorset = Color.set;
	opts.color = Color.value;
	opts.boundarytype = BoundaryType.value;
	opts.complete = Complete.set;
	opts.showresidual = ShowResidual.set;
	opts.iters = Iters.value;
	opts.maxsolvedepth = MaxSolveDepth.value;
	opts.cgdepth = CGDepth.value;
	opts.cssolveraccuracy = CSSolverAccuracy.value;
	opts.nonManifold = NonManifold.set;
	opts.polygon = PolygonMesh.set;
	
	typedef typename Octree< Real >::template ProjectiveData< Point3D< Real > > ProjectiveColor;
	typename Octree< Real >::template SparseNodeData< ProjectiveColor > colorData;
	CoredFileMeshData< Vertex > mesh;

	char* ext = GetFileExtension( In.value );
	if( Color.set && Color.value>0 )
	{
		OrientedPointStreamWithData< float , Point3D< unsigned char > >* pointStream;
		if     ( !strcasecmp( ext , "bnpts" ) )
		  pointStream = new BinaryOrientedPointStreamWithData< float , Point3D< unsigned char > >( In.value );
		else if( !strcasecmp( ext , "ply"   ) )
		  pointStream = new PLYOrientedPointStreamWithData< float , Point3D< unsigned char > >( In.value , PlyColorProperties , 6 , ValidPlyColorProperties );
		else
		  pointStream = new ASCIIOrientedPointStreamWithData< float , Point3D< unsigned char > >( In.value , ReadASCIIColor );

		// new call
		spr_surface_build_cpp_with_data< Real, Vertex >(xForm, &pointStream, opts, &mesh);
	}
	else
	{
		OrientedPointStream< float >* pointStream;
		if     ( !strcasecmp( ext , "bnpts" ) )
		  pointStream = new BinaryOrientedPointStream< float >( In.value );
		else if( !strcasecmp( ext , "ply"   ) )
		  pointStream = new PLYOrientedPointStream< float >( In.value );
		else
		  pointStream = new ASCIIOrientedPointStream< float >( In.value );

		// new call
		spr_surface_build_cpp< Real, Vertex >(xForm, &pointStream, opts, &mesh);
	}
	delete[] ext;

	/*
	if( VoxelGrid.set )
	{
		double t = Time();
		FILE* fp = fopen( VoxelGrid.value , "wb" );
		if( !fp ) fprintf( stderr , "Failed to open voxel file for writing: %s\n" , VoxelGrid.value );
		else
		{
			int res = 0;
			Pointer( Real ) values = tree.Evaluate( ( ConstPointer( Real ) )solution , res , isoValue , VoxelDepth.value );
			fwrite( &res , sizeof(int) , 1 , fp );
			if( sizeof(Real)==sizeof(float) ) fwrite( values , sizeof(float) , res*res*res , fp );
			else
			{
				float *fValues = new float[res*res*res];
				for( int i=0 ; i<res*res*res ; i++ ) fValues[i] = float( values[i] );
				fwrite( fValues , sizeof(float) , res*res*res , fp );
				delete[] fValues;
			}
			fclose( fp );
			DeletePointer( values );
		}
		DumpOutput( "Got voxel grid in: %f\n" , Time()-t );
	}
	*/
	
	if( Out.set )
	{
		if( NoComments.set )
		{
			if( ASCII.set ) PlyWritePolygons( Out.value , &mesh , PLY_ASCII         , NULL , 0 , iXForm );
			else            PlyWritePolygons( Out.value , &mesh , PLY_BINARY_NATIVE , NULL , 0 , iXForm );
		}
		else
		{
			if( ASCII.set ) PlyWritePolygons( Out.value , &mesh , PLY_ASCII         , &comments[0] , (int)comments.size() , iXForm );
			else            PlyWritePolygons( Out.value , &mesh , PLY_BINARY_NATIVE , &comments[0] , (int)comments.size() , iXForm );
		}
		DumpOutput( "Vertices / Polygons: %d / %d\n" , mesh.outOfCorePointCount()+mesh.inCorePoints.size() , mesh.polygonCount() );
	}

	return 1;
}

#ifdef _WIN32
inline double to_seconds( const FILETIME& ft )
{
	const double low_to_sec=100e-9; // 100 nanoseconds
	const double high_to_sec=low_to_sec*4294967296.0;
	return ft.dwLowDateTime*low_to_sec+ft.dwHighDateTime*high_to_sec;
}
#endif // _WIN32

int main( int argc , char* argv[] )
{
#if defined(WIN32) && defined(MAX_MEMORY_GB)
	if( MAX_MEMORY_GB>0 )
	{
		SIZE_T peakMemory = 1;
		peakMemory <<= 30;
		peakMemory *= MAX_MEMORY_GB;
		printf( "Limiting memory usage to %.2f GB\n" , float( peakMemory>>30 ) );
		HANDLE h = CreateJobObject( NULL , NULL );
		AssignProcessToJobObject( h , GetCurrentProcess() );

		JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
		jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_JOB_MEMORY;
		jeli.JobMemoryLimit = peakMemory;
		if( !SetInformationJobObject( h , JobObjectExtendedLimitInformation , &jeli , sizeof( jeli ) ) )
			fprintf( stderr , "Failed to set memory limit\n" );
	}
#endif // defined(WIN32) && defined(MAX_MEMORY_GB)
	double t = Time();

	cmdLineParse( argc-1 , &argv[1] , sizeof(params)/sizeof(cmdLineReadable*) , params , 1 );
	if( Density.set )
		if( Color.set )
			if( Double.set ) Execute< double , PlyColorAndValueVertex< float > >( argc , argv );
			else             Execute< float  , PlyColorAndValueVertex< float > >( argc , argv );
		else
			if( Double.set ) Execute< double , PlyValueVertex< float > >( argc , argv );
			else             Execute< float  , PlyValueVertex< float > >( argc , argv );
	else
		if( Color.set )
			if( Double.set ) Execute< double , PlyColorVertex< float > >( argc , argv );
			else             Execute< float  , PlyColorVertex< float > >( argc , argv );
		else
			if( Double.set ) Execute< double , PlyVertex< float > >( argc , argv );
			else             Execute< float  , PlyVertex< float > >( argc , argv );
#ifdef _WIN32
	if( Performance.set )
	{
		HANDLE cur_thread=GetCurrentThread();
		FILETIME tcreat, texit, tkernel, tuser;
		if( GetThreadTimes( cur_thread , &tcreat , &texit , &tkernel , &tuser ) )
			printf( "Time (Wall/User/Kernel): %.2f / %.2f / %.2f\n" , Time()-t , to_seconds( tuser ) , to_seconds( tkernel ) );
		else printf( "Time: %.2f\n" , Time()-t );
		HANDLE h = GetCurrentProcess();
		PROCESS_MEMORY_COUNTERS pmc;
		if( GetProcessMemoryInfo( h , &pmc , sizeof(pmc) ) ) printf( "Peak Memory (MB): %d\n" , pmc.PeakWorkingSetSize>>20 );
	}
#endif // _WIN32
	return EXIT_SUCCESS;
}
