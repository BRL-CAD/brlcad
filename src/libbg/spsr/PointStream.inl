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

template< class Real >
CVertexPointStream< Real >::CVertexPointStream( size_t pointCount , const struct cvertex **points ){ _points = points , _pointCount = pointCount , _current = 0; }
template< class Real >
CVertexPointStream< Real >::~CVertexPointStream( void ){ ; }
template< class Real >
void CVertexPointStream< Real >::reset( void ) { _current=0; }
template< class Real >
bool CVertexPointStream< Real >::nextPoint( Point3D< Real >& p , Point3D< Real >& n )
{
	while (_current < _pointCount && !_points[_current]) _current++;
	if( _current>=_pointCount ) return false;
	p[0] = _points[_current]->p[0] , p[1] = _points[_current]->p[1] , p[2] = _points[_current]->p[2];
	n[0] = _points[_current]->n[0] , n[1] = _points[_current]->n[1] , n[2] = _points[_current]->n[2];
	_current++;
	return true;
}

template< class Real >
MemoryPointStream< Real >::MemoryPointStream( size_t pointCount , std::pair< Point3D< Real > , Point3D< Real > >* points ){ _points = points , _pointCount = pointCount , _current = 0; }
template< class Real >
MemoryPointStream< Real >::~MemoryPointStream( void ){ ; }
template< class Real >
void MemoryPointStream< Real >::reset( void ) { _current=0; }
template< class Real >
bool MemoryPointStream< Real >::nextPoint( Point3D< Real >& p , Point3D< Real >& n )
{
	if( _current>=_pointCount ) return false;
	p = _points[_current].first , n = _points[_current].second;
	_current++;
	return true;
}

template< class Real >
ASCIIPointStream< Real >::ASCIIPointStream( const char* fileName )
{
	_fp = fopen( fileName , "r" );
	if( !_fp ) fprintf( stderr , "Failed to open file for reading: %s\n" , fileName ) , exit( 0 );
}
template< class Real >
ASCIIPointStream< Real >::~ASCIIPointStream( void )
{
	fclose( _fp );
	_fp = NULL;
}
template< class Real >
void ASCIIPointStream< Real >::reset( void ) { fseek( _fp , SEEK_SET , 0 ); }
template< class Real >
bool ASCIIPointStream< Real >::nextPoint( Point3D< Real >& p , Point3D< Real >& n )
{
	float c[2*DIMENSION];
	if( fscanf( _fp , " %f %f %f %f %f %f " , &c[0] , &c[1] , &c[2] , &c[3] , &c[4] , &c[5] )!=2*DIMENSION ) return false;
	p[0] = c[0] , p[1] = c[1] , p[2] = c[2];
	n[0] = c[3] , n[1] = c[4] , n[2] = c[5];
	return true;
}
template< class Real >
BinaryPointStream< Real >::BinaryPointStream( const char* fileName )
{
	_pointsInBuffer = _currentPointIndex = 0;
	_fp = fopen( fileName , "rb" );
	if( !_fp ) fprintf( stderr , "Failed to open file for reading: %s\n" , fileName ) , exit( 0 );
}
template< class Real >
BinaryPointStream< Real >::~BinaryPointStream( void )
{
	fclose( _fp );
	_fp = NULL;
}
template< class Real >
void BinaryPointStream< Real >::reset( void )
{
	fseek( _fp , SEEK_SET , 0 );
	_pointsInBuffer = _currentPointIndex = 0;
}
template< class Real >
bool BinaryPointStream< Real >::nextPoint( Point3D< Real >& p , Point3D< Real >& n )
{
	if( _currentPointIndex<_pointsInBuffer )
	{
		p[0] = _pointBuffer[ _currentPointIndex*6+0 ];
		p[1] = _pointBuffer[ _currentPointIndex*6+1 ];
		p[2] = _pointBuffer[ _currentPointIndex*6+2 ];
		n[0] = _pointBuffer[ _currentPointIndex*6+3 ];
		n[1] = _pointBuffer[ _currentPointIndex*6+4 ];
		n[2] = _pointBuffer[ _currentPointIndex*6+5 ];
		_currentPointIndex++;
		return true;
	}
	else
	{
		_currentPointIndex = 0;
		_pointsInBuffer = int( fread( _pointBuffer , sizeof( Real ) * 6 , POINT_BUFFER_SIZE , _fp ) );
		if( !_pointsInBuffer ) return false;
		else return nextPoint( p , n );
	}
}

