// Geogram is licensed under the 3-clauses BSD License (also called "Revised
// BSD License", "New BSD License", or "Modified BSD License"):
//
// Copyright (c) 2012-2014, Bruno Levy All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer. Redistributions in binary
// form must reproduce the above copyright notice, this list of conditions and
// the following disclaimer in the documentation and/or other materials
// provided with the distribution. Neither the name of the ALICE Project-Team
// nor the names of its contributors may be used to endorse or promote products
// derived from this software without specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// If you modify this software, you should include a notice giving the name of
// the person performing the modification, the date of modification, and the
// reason for such modification.

// [Bruno Levy 2016]: replacement class for Ply vertices, so that we
// do not need importing all the Ply I/O code.
// Adapted from Ply.h
//
// https://github.com/alicevision/geogram/blob/master/src/lib/geogram/third_party/PoissonRecon/PlyVertexMini.h
//
// See https://github.com/mkazhdan/PoissonRecon/issues/143

// The "Wrapper" class indicates the class to cast to/from in order to support linear operations.
template< class Real >
class PlyVertex
{
public:
        typedef PlyVertex Wrapper;

        const static int ReadComponents=3;
        const static int WriteComponents=3;
        Point3D< Real > point;

        PlyVertex( void ) { ; }
        PlyVertex( Point3D< Real > p ) { point=p; }
        PlyVertex operator + ( PlyVertex p ) const { return PlyVertex( point+p.point ); }
        PlyVertex operator - ( PlyVertex p ) const { return PlyVertex( point-p.point ); }
        template< class _Real > PlyVertex operator * ( _Real s ) const { return PlyVertex( point*s ); }
        template< class _Real > PlyVertex operator / ( _Real s ) const { return PlyVertex( point/s ); }
        PlyVertex& operator += ( PlyVertex p ) { point += p.point ; return *this; }
        PlyVertex& operator -= ( PlyVertex p ) { point -= p.point ; return *this; }
        template< class _Real > PlyVertex& operator *= ( _Real s ) { point *= s ; return *this; }
        template< class _Real > PlyVertex& operator /= ( _Real s ) { point /= s ; return *this; }
};

template< class Real , class _Real > PlyVertex< Real > operator * ( XForm4x4< _Real > xForm , PlyVertex< Real > v ) { return PlyVertex< Real >( xForm * v.point ); }

template< class Real >
class PlyValueVertex
{
public:
        typedef PlyValueVertex Wrapper;

        const static int ReadComponents=4;
        const static int WriteComponents=4;

        Point3D<Real> point;
        Real value;

        PlyValueVertex( void ) : value( Real(0) ) { ; }
        PlyValueVertex( Point3D< Real > p , Real v ) : point(p) , value(v) { ; }
        PlyValueVertex operator + ( PlyValueVertex p ) const { return PlyValueVertex( point+p.point , value+p.value ); }
        PlyValueVertex operator - ( PlyValueVertex p ) const { return PlyValueVertex( point-p.value , value-p.value ); }
        template< class _Real > PlyValueVertex operator * ( _Real s ) const { return PlyValueVertex( point*s , Real(value*s) ); }
        template< class _Real > PlyValueVertex operator / ( _Real s ) const { return PlyValueVertex( point/s , Real(value/s) ); }
        PlyValueVertex& operator += ( PlyValueVertex p ) { point += p.point , value += p.value ; return *this; }
        PlyValueVertex& operator -= ( PlyValueVertex p ) { point -= p.point , value -= p.value ; return *this; }
        template< class _Real > PlyValueVertex& operator *= ( _Real s ) { point *= s , value *= Real(s) ; return *this; }
        template< class _Real > PlyValueVertex& operator /= ( _Real s ) { point /= s , value /= Real(s) ; return *this; }
};
template< class Real , class _Real > PlyValueVertex< Real > operator * ( XForm4x4< _Real > xForm , PlyValueVertex< Real > v ) { return PlyValueVertex< Real >( xForm * v.point , v.value ); }

template< class Real >
class PlyOrientedVertex
{
public:
        typedef PlyOrientedVertex Wrapper;

        const static int ReadComponents=6;
        const static int WriteComponents=6;

        Point3D<Real> point , normal;

        PlyOrientedVertex( void ) { ; }
        PlyOrientedVertex( Point3D< Real > p , Point3D< Real > n ) : point(p) , normal(n) { ; }
        PlyOrientedVertex operator + ( PlyOrientedVertex p ) const { return PlyOrientedVertex( point+p.point , normal+p.normal ); }
        PlyOrientedVertex operator - ( PlyOrientedVertex p ) const { return PlyOrientedVertex( point-p.value , normal-p.normal ); }
        template< class _Real > PlyOrientedVertex operator * ( _Real s ) const { return PlyOrientedVertex( point*s , normal*s ); }
        template< class _Real > PlyOrientedVertex operator / ( _Real s ) const { return PlyOrientedVertex( point/s , normal/s ); }
        PlyOrientedVertex& operator += ( PlyOrientedVertex p ) { point += p.point , normal += p.normal ; return *this; }
        PlyOrientedVertex& operator -= ( PlyOrientedVertex p ) { point -= p.point , normal -= p.normal ; return *this; }
        template< class _Real > PlyOrientedVertex& operator *= ( _Real s ) { point *= s , normal *= s ; return *this; }
        template< class _Real > PlyOrientedVertex& operator /= ( _Real s ) { point /= s , normal /= s ; return *this; }
};
template< class Real , class _Real > PlyOrientedVertex< Real > operator * ( XForm4x4< _Real > xForm , PlyOrientedVertex< Real > v ) { return PlyOrientedVertex< Real >( xForm * v.point , xForm.inverse().transpose() * v.normal ); }

template< class Real >
class PlyColorVertex
{
public:
        struct _PlyColorVertex
        {
                Point3D< Real > point , color;
                _PlyColorVertex( void ) { ; }
                _PlyColorVertex( Point3D< Real > p , Point3D< Real > c ) : point(p) , color(c) { ; }
                _PlyColorVertex( PlyColorVertex< Real > p ){ point = p.point ; for( int c=0 ; c<3 ; c++ ) color[c] = (Real) p.color[c]; }
                operator PlyColorVertex< Real > ()
                {
                        PlyColorVertex< Real > p;
                        p.point = point;
                        for( int c=0 ; c<3 ; c++ ) p.color[c] = (unsigned char)std::max< int >( 0 , std::min< int >( 255 , (int)( color[c]+0.5 ) ) );
                        return p;
                }

                _PlyColorVertex operator + ( _PlyColorVertex p ) const { return _PlyColorVertex( point+p.point , color+p.color ); }
                _PlyColorVertex operator - ( _PlyColorVertex p ) const { return _PlyColorVertex( point-p.value , color-p.color ); }
                template< class _Real > _PlyColorVertex operator * ( _Real s ) const { return _PlyColorVertex( point*s , color*s ); }
                template< class _Real > _PlyColorVertex operator / ( _Real s ) const { return _PlyColorVertex( point/s , color/s ); }
                _PlyColorVertex& operator += ( _PlyColorVertex p ) { point += p.point , color += p.color ; return *this; }
                _PlyColorVertex& operator -= ( _PlyColorVertex p ) { point -= p.point , color -= p.color ; return *this; }
                template< class _Real > _PlyColorVertex& operator *= ( _Real s ) { point *= s , color *= s ; return *this; }
                template< class _Real > _PlyColorVertex& operator /= ( _Real s ) { point /= s , color /= s ; return *this; }
        };

        typedef _PlyColorVertex Wrapper;

        const static int ReadComponents=9;
        const static int WriteComponents=6;

        Point3D< Real > point;
        unsigned char color[3];

        operator Point3D< Real >& (){ return point; }
        operator const Point3D< Real >& () const { return point; }
        PlyColorVertex( void ) { point.coords[0] = point.coords[1] = point.coords[2] = 0 , color[0] = color[1] = color[2] = 0; }
        PlyColorVertex( const Point3D<Real>& p ) { point=p; }
        PlyColorVertex( const Point3D< Real >& p , const unsigned char c[3] ) { point = p , color[0] = c[0] , color[1] = c[1] , color[2] = c[2]; }
};
template< class Real , class _Real > PlyColorVertex< Real > operator * ( XForm4x4< _Real > xForm , PlyColorVertex< Real > v ) { return PlyColorVertex< Real >( xForm * v.point , v.color ); }

template< class Real >
class PlyColorAndValueVertex
{
public:
        struct _PlyColorAndValueVertex
        {
                Point3D< Real > point , color;
                Real value;
                _PlyColorAndValueVertex( void ) : value(0) { ; }
                _PlyColorAndValueVertex( Point3D< Real > p , Point3D< Real > c , Real v ) : point(p) , color(c) , value(v) { ; }
                _PlyColorAndValueVertex( PlyColorAndValueVertex< Real > p ){ point = p.point ; for( int c=0 ; c<3 ; c++ ) color[c] = (Real) p.color[c] ; value = p.value; }
                operator PlyColorAndValueVertex< Real > ()
                {
                        PlyColorAndValueVertex< Real > p;
                        p.point = point;
                        for( int c=0 ; c<3 ; c++ ) p.color[c] = (unsigned char)std::max< int >( 0 , std::min< int >( 255 , (int)( color[c]+0.5 ) ) );
                        p.value = value;
                        return p;
                }

                _PlyColorAndValueVertex operator + ( _PlyColorAndValueVertex p ) const { return _PlyColorAndValueVertex( point+p.point , color+p.color , value+p.value ); }
                _PlyColorAndValueVertex operator - ( _PlyColorAndValueVertex p ) const { return _PlyColorAndValueVertex( point-p.value , color-p.color , value+p.value ); }
                template< class _Real > _PlyColorAndValueVertex operator * ( _Real s ) const { return _PlyColorAndValueVertex( point*s , color*s , value*s ); }
                template< class _Real > _PlyColorAndValueVertex operator / ( _Real s ) const { return _PlyColorAndValueVertex( point/s , color/s , value/s ); }
                _PlyColorAndValueVertex& operator += ( _PlyColorAndValueVertex p ) { point += p.point , color += p.color , value += p.value ; return *this; }
                _PlyColorAndValueVertex& operator -= ( _PlyColorAndValueVertex p ) { point -= p.point , color -= p.color , value -= p.value ; return *this; }
                template< class _Real > _PlyColorAndValueVertex& operator *= ( _Real s ) { point *= s , color *= s , value *= (Real)s ; return *this; }
                template< class _Real > _PlyColorAndValueVertex& operator /= ( _Real s ) { point /= s , color /= s , value /= (Real)s ; return *this; }
        };

        typedef _PlyColorAndValueVertex Wrapper;

        const static int ReadComponents=10;
        const static int WriteComponents=7;

        Point3D< Real > point;
        unsigned char color[3];
        Real value;

        operator Point3D< Real >& (){ return point; }
        operator const Point3D< Real >& () const { return point; }
        PlyColorAndValueVertex( void ) { point.coords[0] = point.coords[1] = point.coords[2] = (Real)0 , color[0] = color[1] = color[2] = 0 , value = (Real)0; }
        PlyColorAndValueVertex( const Point3D< Real >& p ) { point=p; }
        PlyColorAndValueVertex( const Point3D< Real >& p , const unsigned char c[3] , Real v) { point = p , color[0] = c[0] , color[1] = c[1] , color[2] = c[2] , value = v; }
};
template< class Real , class _Real > PlyColorAndValueVertex< Real > operator * ( XForm4x4< _Real > xForm , PlyColorAndValueVertex< Real > v ) { return PlyColorAndValueVertex< Real >( xForm * v.point , v.color , v.value ); }


