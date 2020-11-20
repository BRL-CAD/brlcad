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

#include <float.h>
#include <string.h>


///////////////////
//  SparseMatrix //
///////////////////
///////////////////////////////////////
// SparseMatrix Methods and Members //
///////////////////////////////////////

template< class T >
void SparseMatrix< T >::_init( void )
{
	_contiguous = false;
	_maxEntriesPerRow = 0;
	rows = 0;
	rowSizes = NullPointer< int >( );
	m_ppElements = NullPointer< Pointer( MatrixEntry< T > ) >( );
}

template< class T > SparseMatrix< T >::SparseMatrix( void ){  _init(); }

template< class T > SparseMatrix< T >::SparseMatrix( int rows                        ){ _init() , Resize( rows ); }
template< class T > SparseMatrix< T >::SparseMatrix( int rows , int maxEntriesPerRow ){ _init() , Resize( rows , maxEntriesPerRow ); }

template< class T >
SparseMatrix< T >::SparseMatrix( const SparseMatrix& M )
{
	_init();
	if( M._contiguous ) Resize( M.rows , M._maxEntriesPerRow );
	else                Resize( M.rows );
	for( int i=0 ; i<rows ; i++ )
	{
		SetRowSize( i , M.rowSizes[i] );
		memcpy( (*this)[i] , M[i] , sizeof( MatrixEntry< T > ) * rowSizes[i] );
	}
}
template<class T>
int SparseMatrix<T>::Entries( void ) const
{
	int e = 0;
	for( int i=0 ; i<rows ; i++ ) e += int( rowSizes[i] );
	return e;
}
template<class T>
SparseMatrix<T>& SparseMatrix<T>::operator = (const SparseMatrix<T>& M)
{
	if( M._contiguous ) Resize( M.rows , M._maxEntriesPerRow );
	else                Resize( M.rows );
	for( int i=0 ; i<rows ; i++ )
	{
		SetRowSize( i , M.rowSizes[i] );
		memcpy( (*this)[i] , M[i] , sizeof( MatrixEntry< T > ) * rowSizes[i] );
	}
	return *this;
}

template<class T>
SparseMatrix<T>::~SparseMatrix( void ){ Resize( 0 ); }

template< class T >
bool SparseMatrix< T >::write( const char* fileName ) const
{
	FILE* fp = fopen( fileName , "wb" );
	if( !fp ) return false;
	bool ret = write( fp );
	fclose( fp );
	return ret;
}
template< class T >
bool SparseMatrix< T >::read( const char* fileName )
{
	FILE* fp = fopen( fileName , "rb" );
	if( !fp ) return false;
	bool ret = read( fp );
	fclose( fp );
	return ret;
}
template< class T >
bool SparseMatrix< T >::write( FILE* fp ) const
{
	if( fwrite( &rows , sizeof( int ) , 1 , fp )!=1 ) return false;
	if( fwrite( rowSizes , sizeof( int ) , rows , fp )!=rows ) return false;
	for( int i=0 ; i<rows ; i++ ) if( fwrite( (*this)[i] , sizeof( MatrixEntry< T > ) , rowSizes[i] , fp )!=rowSizes[i] ) return false;
	return true;
}
template< class T >
bool SparseMatrix< T >::read( FILE* fp )
{
	int r;
	if( fread( &r , sizeof( int ) , 1 , fp )!=1 ) return false;
	Resize( r );
	if( fread( rowSizes , sizeof( int ) , rows , fp )!=rows ) return false;
	for( int i=0 ; i<rows ; i++ )
	{
		r = rowSizes[i];
		rowSizes[i] = 0;
		SetRowSize( i , r );
		if( fread( (*this)[i] , sizeof( MatrixEntry< T > ) , rowSizes[i] , fp )!=rowSizes[i] ) return false;
	}
	return true;
}


template< class T >
void SparseMatrix< T >::Resize( int r )
{
	if( rows>0 )
	{
		if( _contiguous ){ if( _maxEntriesPerRow ) FreePointer( m_ppElements[0] ); }
		else for( int i=0 ; i<rows ; i++ ){ if( rowSizes[i] ) FreePointer( m_ppElements[i] ); }
		FreePointer( m_ppElements );
		FreePointer( rowSizes );
	}
	rows = r;
	if( r )
	{
		rowSizes = AllocPointer< int >( r );
		m_ppElements = AllocPointer< Pointer( MatrixEntry< T > ) >( r );
		memset( rowSizes , 0 , sizeof( int ) * r );
	}
	_contiguous = false;
	_maxEntriesPerRow = 0;
}
template< class T >
void SparseMatrix< T >::Resize( int r , int e )
{
	if( rows>0 )
	{
		if( _contiguous ){ if( _maxEntriesPerRow ) FreePointer( m_ppElements[0] ); }
		else for( int i=0 ; i<rows ; i++ ){ if( rowSizes[i] ) FreePointer( m_ppElements[i] ); }
		FreePointer( m_ppElements );
		FreePointer( rowSizes );
	}
	rows = r;
	if( r )
	{
		rowSizes = AllocPointer< int >( r );
		m_ppElements = AllocPointer< Pointer( MatrixEntry< T > ) >( r );
		m_ppElements[0] = AllocPointer< MatrixEntry< T > >( r * e );
		memset( rowSizes , 0 , sizeof( int ) * r );
		for( int i=1 ; i<r ; i++ ) m_ppElements[i] = m_ppElements[i-1] + e;
	}
	_contiguous = true;
	_maxEntriesPerRow = e;
}

template<class T>
void SparseMatrix< T >::SetRowSize( int row , int count )
{
	if( _contiguous )
	{
		if( count>_maxEntriesPerRow ) fprintf( stderr , "[ERROR] Cannot set row size on contiguous matrix: %d<=%d\n" , count , _maxEntriesPerRow ) , exit( 0 );
		rowSizes[row] = count;
	}
	else if( row>=0 && row<rows )
	{
		if( rowSizes[row] ) FreePointer( m_ppElements[row] );
		if( count>0 ) m_ppElements[row] = AllocPointer< MatrixEntry< T > >( count );
		// [WARNING] Why wasn't this line here before???
		rowSizes[row] = count;
	}
}


template<class T>
void SparseMatrix<T>::SetZero()
{
	Resize(this->m_N, this->m_M);
}

template<class T>
SparseMatrix<T> SparseMatrix<T>::operator * (const T& V) const
{
	SparseMatrix<T> M(*this);
	M *= V;
	return M;
}

template<class T>
SparseMatrix<T>& SparseMatrix<T>::operator *= (const T& V)
{
	for( int i=0 ; i<rows ; i++ ) for( int ii=0 ; ii<rowSizes[i] ; i++ ) m_ppElements[i][ii].Value *= V;
	return *this;
}

template<class T>
template<class T2>
Vector<T2> SparseMatrix<T>::Multiply( const Vector<T2>& V ) const
{
	Vector<T2> R( rows );
	Multiply( V , R );
	return R;
}

template<class T>
template<class T2>
void SparseMatrix<T>::Multiply( const Vector<T2>& In , Vector<T2>& Out , int threads ) const
{
	for( int i=0 ; i<rows ; i++ )
	{
		T2 temp = T2();
		temp *= 0;
#if 1
		ConstPointer( MatrixEntry< T > ) start = m_ppElements[i];
		ConstPointer( MatrixEntry< T > ) end = start + rowSizes[i];
		ConstPointer( MatrixEntry< T > ) e;
		for( e=start ; e!=end ; e++ ) temp += In[ e->N ] * e->Value;
		Out[i] = temp;
#else
		for( int j=0 ; j<rowSizes[i] ; j++ ) temp += m_ppElements[i][j].Value * In.m_pV[m_ppElements[i][j].N];
		Out.m_pV[i] = temp;
#endif
	}
}

template<class T>
template<class T2>
Vector<T2> SparseMatrix<T>::operator * (const Vector<T2>& V) const
{
	return Multiply(V);
}

template<class T>
template<class T2>
int SparseMatrix<T>::SolveSymmetric( const SparseMatrix<T>& M , const Vector<T2>& b , int iters , Vector<T2>& solution , const T2 eps , int reset , int threads )
{
	if( reset )
	{
		solution.Resize( b.Dimensions() );
		solution.SetZero();
	}
	Vector< T2 > r;
	r.Resize( solution.Dimensions() );
	M.Multiply( solution , r );
	r = b - r;
	Vector< T2 > d = r;
	double delta_new , delta_0;
	for( int i=0 ; i<r.Dimensions() ; i++ ) delta_new += r.m_pV[i] * r.m_pV[i];
	delta_0 = delta_new;
	if( delta_new<eps ) return 0;
	int ii;
	Vector< T2 > q;
	q.Resize( d.Dimensions() );
	for( ii=0; ii<iters && delta_new>eps*delta_0 ; ii++ )
	{
		M.Multiply( d , q , threads );
        double dDotQ = 0 , alpha = 0;
		for( int i=0 ; i<d.Dimensions() ; i++ ) dDotQ += d.m_pV[i] * q.m_pV[i];
		alpha = delta_new / dDotQ;
		for( int i=0 ; i<r.Dimensions() ; i++ ) solution.m_pV[i] += d.m_pV[i] * T2( alpha );
		if( !(ii%50) )
		{
			r.Resize( solution.Dimensions() );
			M.Multiply( solution , r , threads );
			r = b - r;
		}
		else
			for( int i=0 ; i<r.Dimensions() ; i++ ) r.m_pV[i] = r.m_pV[i] - q.m_pV[i] * T2(alpha);

		double delta_old = delta_new , beta;
		delta_new = 0;
		for( int i=0 ; i<r.Dimensions() ; i++ ) delta_new += r.m_pV[i]*r.m_pV[i];
		beta = delta_new / delta_old;
		for( int i=0 ; i<d.Dimensions() ; i++ ) d.m_pV[i] = r.m_pV[i] + d.m_pV[i] * T2( beta );
	}
	return ii;
}

// Solve for x s.t. M(x)=b by solving for x s.t. M^tM(x)=M^t(b)
template<class T>
int SparseMatrix<T>::Solve(const SparseMatrix<T>& M,const Vector<T>& b,int iters,Vector<T>& solution,const T eps){
	SparseMatrix mTranspose=M.Transpose();
	Vector<T> bb=mTranspose*b;
	Vector<T> d,r,Md;
	T alpha,beta,rDotR;
	int i;

	solution.Resize(M.Columns());
	solution.SetZero();

	d=r=bb;
	rDotR=r.Dot(r);
	for(i=0;i<iters && rDotR>eps;i++){
		T temp;
		Md=mTranspose*(M*d);
		alpha=rDotR/d.Dot(Md);
		solution+=d*alpha;
		r-=Md*alpha;
		temp=r.Dot(r);
		beta=temp/rDotR;
		rDotR=temp;
		d=r+d*beta;
	}
	return i;
}




///////////////////////////
// SparseSymmetricMatrix //
///////////////////////////
template<class T>
template<class T2>
Vector<T2> SparseSymmetricMatrix<T>::operator * (const Vector<T2>& V) const {return Multiply(V);}
template<class T>
template<class T2>
Vector<T2> SparseSymmetricMatrix<T>::Multiply( const Vector<T2>& V ) const
{
	Vector<T2> R( SparseMatrix< T >::rows );

	for(int i=0; i<SparseMatrix< T >::rows; i++){
		for(int ii=0;ii<SparseMatrix< T >::rowSizes[i];ii++)
		{
			int j=SparseMatrix< T >::m_ppElements[i][ii].N;
			R(i) += SparseMatrix< T >::m_ppElements[i][ii].Value * V.m_pV[j];
			R(j) += SparseMatrix< T >::m_ppElements[i][ii].Value * V.m_pV[i];
		}
	}
	return R;
}

template<class T>
template<class T2>
void SparseSymmetricMatrix<T>::Multiply( const Vector<T2>& In , Vector<T2>& Out , bool addDCTerm ) const
{
	Out.SetZero();
	const T2* in = &In[0];
	T2* out = &Out[0];
	T2 dcTerm = T2( 0 );
	if( addDCTerm )
	{
		for( int i=0 ; i<SparseMatrix< T >::rows ; i++ ) dcTerm += in[i];
		dcTerm /= SparseMatrix< T >::rows;
	}
	for( int i=0 ; i<SparseMatrix< T >::rows ; i++ )
	{
		const MatrixEntry<T>* temp = SparseMatrix< T >::m_ppElements[i];
		const MatrixEntry<T>* end = temp + SparseMatrix< T >::rowSizes[i];
		const T2& in_i_ = in[i];
		T2 out_i = T2(0);
		for( ; temp!=end ; temp++ )
		{
			int j=temp->N;
			T2 v=temp->Value;
			out_i += v * in[j];
			out[j] += v * in_i_;
		}
		out[i] += out_i;
	}
	if( addDCTerm ) for( int i=0 ; i<SparseMatrix< T >::rows ; i++ ) out[i] += dcTerm;
}
template<class T>
template<class T2>
void SparseSymmetricMatrix<T>::Multiply( const Vector<T2>& In , Vector<T2>& Out , MapReduceVector< T2 >& OutScratch , bool addDCTerm ) const
{
	int dim = int( In.Dimensions() );
	const T2* in = &In[0];
	int threads = OutScratch.threads();
	if( addDCTerm )
	{
		T2 dcTerm = 0;
		for( int t=0 ; t<threads ; t++ )
		{
			T2* out = OutScratch[t];
			memset( out , 0 , sizeof( T2 ) * dim );
			for( int i=(SparseMatrix< T >::rows*t)/threads ; i<(SparseMatrix< T >::rows*(t+1))/threads ; i++ )
			{
				const T2& in_i_ = in[i];
				T2& out_i_ = out[i];
				ConstPointer( MatrixEntry< T > ) temp;
				ConstPointer( MatrixEntry< T > ) end;
				for( temp = SparseMatrix< T >::m_ppElements[i] , end = temp+SparseMatrix< T >::rowSizes[i] ; temp!=end ; temp++ )
				{
					int j = temp->N;
					T2 v = temp->Value;
					out_i_ += v * in[j];
					out[j] += v * in_i_;
				}
				dcTerm += in_i_;
			}
		}
		dcTerm /= dim;
		dim = int( Out.Dimensions() );
		T2* out = &Out[0];
		for( int i=0 ; i<dim ; i++ )
		{
			T2 _out = dcTerm;
			for( int t=0 ; t<threads ; t++ ) _out += OutScratch[t][i];
			out[i] = _out;
		}
	}
	else
	{
		for( int t=0 ; t<threads ; t++ )
		{
			T2* out = OutScratch[t];
			memset( out , 0 , sizeof( T2 ) * dim );
			for( int i=(SparseMatrix< T >::rows*t)/threads ; i<(SparseMatrix< T >::rows*(t+1))/threads ; i++ )
			{
				T2 in_i_ = in[i];
				T2 out_i_ = T2();
				ConstPointer( MatrixEntry< T > ) temp;
				ConstPointer( MatrixEntry< T > ) end;
				for( temp = SparseMatrix< T >::m_ppElements[i] , end = temp+SparseMatrix< T >::rowSizes[i] ; temp!=end ; temp++ )
				{
					int j = temp->N;
					T2 v = temp->Value;
					out_i_ += v * in[j];
					out[j] += v * in_i_;
				}
				out[i] += out_i_;
			}
		}
		dim = int( Out.Dimensions() );
		T2* out = &Out[0];
		for( int i=0 ; i<dim ; i++ )
		{
			T2 _out = T2(0);
			for( int t=0 ; t<threads ; t++ ) _out += OutScratch[t][i];
			out[i] = _out;
		}
	}
}
template<class T>
template<class T2>
void SparseSymmetricMatrix<T>::Multiply( const Vector<T2>& In , Vector<T2>& Out , std::vector< T2* >& OutScratch , const std::vector< int >& bounds ) const
{
	int dim = In.Dimensions();
	const T2* in = &In[0];
	int threads = OutScratch.size();
	for( int t=0 ; t<threads ; t++ )
		for( int i=0 ; i<dim ; i++ ) OutScratch[t][i] = T2(0);
	for( int t=0 ; t<threads ; t++ )
	{
		T2* out = OutScratch[t];
		for( int i=bounds[t] ; i<bounds[t+1] ; i++ )
		{
			const MatrixEntry<T>* temp = SparseMatrix< T >::m_ppElements[i];
			const MatrixEntry<T>* end = temp + SparseMatrix< T >::rowSizes[i];
			const T2& in_i_ = in[i];
			T2& out_i_ = out[i];
			for(  ; temp!=end ; temp++ )
			{
				int j = temp->N;
				T2 v = temp->Value;
				out_i_ += v * in[j];
				out[j] += v * in_i_;
			}
		}
	}
	T2* out = &Out[0];
	for( int i=0 ; i<Out.Dimensions() ; i++ )
	{
		T2& _out = out[i];
		_out = T2(0);
		for( int t=0 ; t<threads ; t++ ) _out += OutScratch[t][i];
	}
}
#ifdef WIN32
#ifndef _AtomicIncrement_
#define _AtomicIncrement_
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
inline void AtomicIncrement( volatile float* ptr , float addend )
{
	float newValue = *ptr;
	LONG& _newValue = *( (LONG*)&newValue );
	LONG  _oldValue;
	for( ;; )
	{
		_oldValue = _newValue;
		newValue += addend;
		_newValue = InterlockedCompareExchange( (LONG*) ptr , _newValue , _oldValue );
		if( _newValue==_oldValue ) break;
	}
}
inline void AtomicIncrement( volatile double* ptr , double addend )
//inline void AtomicIncrement( double* ptr , double addend )
{
	double newValue = *ptr;
	LONGLONG& _newValue = *( (LONGLONG*)&newValue );
	LONGLONG  _oldValue;
	do
	{
		_oldValue = _newValue;
		newValue += addend;
		_newValue = InterlockedCompareExchange64( (LONGLONG*) ptr , _newValue , _oldValue );
	}
	while( _newValue!=_oldValue );
}
#endif // _AtomicIncrement_
template< class T >
void MultiplyAtomic( const SparseSymmetricMatrix< T >& A , const Vector< float >& In , Vector< float >& Out , int threads , const int* partition=NULL )
{
	Out.SetZero();
	const float* in = &In[0];
	float* out = &Out[0];
	if( partition )
		for( int t=0 ; t<threads ; t++ )
			for( int i=partition[t] ; i<partition[t+1] ; i++ )
			{
				const MatrixEntry< T >* temp = A[i];
				const MatrixEntry< T >* end = temp + A.rowSizes[i];
				const float& in_i = in[i];
				float out_i = 0.;
				for( ; temp!=end ; temp++ )
				{
					int j = temp->N;
					float v = temp->Value;
					out_i += v * in[j];
					AtomicIncrement( out+j , v * in_i );
				}
				AtomicIncrement( out+i , out_i );
			}
	else
		for( int i=0 ; i<A.rows ; i++ )
		{
			const MatrixEntry< T >* temp = A[i];
			const MatrixEntry< T >* end = temp + A.rowSizes[i];
			const float& in_i = in[i];
			float out_i = 0.f;
			for( ; temp!=end ; temp++ )
			{
				int j = temp->N;
				float v = temp->Value;
				out_i += v * in[j];
				AtomicIncrement( out+j , v * in_i );
			}
			AtomicIncrement( out+i , out_i );
		}
}
template< class T >
void MultiplyAtomic( const SparseSymmetricMatrix< T >& A , const Vector< double >& In , Vector< double >& Out , int threads , const int* partition=NULL )
{
	Out.SetZero();
	const double* in = &In[0];
	double* out = &Out[0];

	if( partition )
		for( int t=0 ; t<threads ; t++ )
			for( int i=partition[t] ; i<partition[t+1] ; i++ )
			{
				const MatrixEntry< T >* temp = A[i];
				const MatrixEntry< T >* end = temp + A.rowSizes[i];
				const double& in_i = in[i];
				double out_i = 0.;
				for( ; temp!=end ; temp++ )
				{
					int j = temp->N;
					T v = temp->Value;
					out_i += v * in[j];
					AtomicIncrement( out+j , v * in_i );
				}
				AtomicIncrement( out+i , out_i );
			}
	else
		for( int i=0 ; i<A.rows ; i++ )
		{
			const MatrixEntry< T >* temp = A[i];
			const MatrixEntry< T >* end = temp + A.rowSizes[i];
			const double& in_i = in[i];
			double out_i = 0.;
			for( ; temp!=end ; temp++ )
			{
				int j = temp->N;
				T v = temp->Value;
				out_i += v * in[j];
				AtomicIncrement( out+j , v * in_i );
			}
			AtomicIncrement( out+i , out_i );
		}
}

template< class T >
template< class T2 >
int SparseSymmetricMatrix< T >::SolveCGAtomic( const SparseSymmetricMatrix< T >& A , const Vector< T2 >& b , int iters , Vector< T2 >& x , T2 eps , int reset , int threads , bool solveNormal )
{
	eps *= eps;
	int dim = b.Dimensions();
	if( reset )
	{
		x.Resize( dim );
		x.SetZero();
	}
	Vector< T2 > r( dim ) , d( dim ) , q( dim );
	Vector< T2 > temp;
	if( solveNormal ) temp.Resize( dim );
	T2 *_x = &x[0] , *_r = &r[0] , *_d = &d[0] , *_q = &q[0];
	const T2* _b = &b[0];

	std::vector< int > partition( threads+1 );
	{
		int eCount = 0;
		for( int i=0 ; i<A.rows ; i++ ) eCount += A.rowSizes[i];
		partition[0] = 0;
		for( int t=0 ; t<threads ; t++ )
		{
			int _eCount = 0;
			for( int i=0 ; i<A.rows ; i++ )
			{
				_eCount += A.rowSizes[i];
				if( _eCount*threads>=eCount*(t+1) )
				{
					partition[t+1] = i;
					break;
				}
			}
		}
		partition[threads] = A.rows;
	}
	if( solveNormal )
	{
		MultiplyAtomic( A , x , temp , threads , &partition[0] );
		MultiplyAtomic( A , temp , r , threads , &partition[0] );
		MultiplyAtomic( A , b , temp , threads , &partition[0] );
		for( int i=0 ; i<dim ; i++ ) _d[i] = _r[i] = temp[i] - _r[i];
	}
	else
	{
		MultiplyAtomic( A , x , r , threads , &partition[0] );
		for( int i=0 ; i<dim ; i++ ) _d[i] = _r[i] = _b[i] - _r[i];
	}
	double delta_new = 0 , delta_0;
	for( size_t i=0 ; i<dim ; i++ ) delta_new += _r[i] * _r[i];
	delta_0 = delta_new;
	if( delta_new<eps )
	{
//		fprintf( stderr , "[WARNING] Initial residual too low: %g < %f\n" , delta_new , eps );
		return 0;
	}
	int ii;
	for( ii=0; ii<iters && delta_new>eps*delta_0 ; ii++ )
	{
		if( solveNormal ) MultiplyAtomic( A , d , temp , threads , &partition[0] ) , MultiplyAtomic( A , temp , q , threads , &partition[0] );
		else              MultiplyAtomic( A , d , q , threads , &partition[0] );
        double dDotQ = 0;
		for( int i=0 ; i<dim ; i++ ) dDotQ += _d[i] * _q[i];
		T2 alpha = T2( delta_new / dDotQ );
		for( int i=0 ; i<dim ; i++ ) _x[i] += _d[i] * alpha;
		if( (ii%50)==(50-1) )
		{
			r.Resize( dim );
			if( solveNormal ) MultiplyAtomic( A , x , temp , threads , &partition[0] ) , MultiplyAtomic( A , temp , r , threads , &partition[0] );
			else              MultiplyAtomic( A , x , r , threads , &partition[0] );
			for( int i=0 ; i<dim ; i++ ) _r[i] = _b[i] - _r[i];
		}
		else
			for( int i=0 ; i<dim ; i++ ) _r[i] -= _q[i] * alpha;

		double delta_old = delta_new;
		delta_new = 0;
		for( size_t i=0 ; i<dim ; i++ ) delta_new += _r[i] * _r[i];
		T2 beta = T2( delta_new / delta_old );
		for( int i=0 ; i<dim ; i++ ) _d[i] = _r[i] + _d[i] * beta;
	}
	return ii;
}
#endif // WIN32
template< class T >
template< class T2 >
int SparseSymmetricMatrix< T >::SolveCG( const SparseSymmetricMatrix<T>& A , const Vector<T2>& b , int iters , Vector<T2>& x , MapReduceVector< T2 >& scratch , T2 eps , int reset , bool addDCTerm , bool solveNormal )
{
	int threads = scratch.threads();
#if 0
	int dim = b.Dimensions();
#if 1
	Vector< T2 > r( dim ) , d( dim ) , Ad( dim ) , temp( dim );
	if( reset ) x.Resize( dim );
#else
	Vector< T2 > r ,d , Ad , temp;
#endif

	double delta_new = 0 , delta_0;

	////////////////////////
	// d = r = b - M * x
	// \delta_new = ||r||^2
#if 1
	A.Multiply( x , temp , scratch , addDCTerm );
	d = r = b - temp;
#else
	d = r = b - Multiply< AddAverage >( A , x );
#endif
	for( int i=0 ; i<dim ; i++ ) delta_new += r[i] * r[i];
	////////////////////////

	delta_0 = delta_new;
	if( delta_new<eps )
	{
//		fprintf( stderr , "[WARNING] Initial residual too low: %g < %g\n" , delta_new , eps );
		return 0;
	}
	int ii;
	for( ii=0 ; ii<iters && delta_new>eps*delta_0 ; ii++ )
	{
		////////////////////////////////////
		// \alpha = ||r||^2 / (d^t * M * d)
#if 1
		A.Multiply( d , Ad , scratch , addDCTerm );
#else
		Ad = Multiply< AddAverage >( A , d );
#endif
		double dDotMd = 0;
		for( int i=0 ; i<dim ; i++ ) dDotMd += Ad[i] * d[i];
		double alpha = double( delta_new / dDotMd );
		double delta_old = delta_new;
		////////////////////////////////////

		delta_new = 0;

		if( (ii%50)==(50-1) )
		{
			////////////////////////////////
			// x = x + d * alpha
			// r = b - M * ( x + d * alpha )
			//   = r - M * d * alpha
			// \delta_new = ||r||^2
			for( int i=0 ; i<dim ; i++ ) x[i] += d[i] * alpha;
			r.SetZero();
#if 1
			A.Multiply( x , temp , scratch , addDCTerm );
			r = b - temp;
#else
			r = b - Multiply< AddAverage >( A , x );
#endif
			for( int i=0 ; i<dim ; i++ ) delta_new += r[i] * r[i];
			////////////////////////////////
		}
		else
		{
			////////////////////////////////
			// x = x + d * alpha
			// r = r - M * d * alpha
			// \delta_new = ||r||^2
			r -= Ad * alpha;
			for( int i=0 ; i<dim ; i++ ) delta_new += r[i] * r[i] , x[i] += d[i] * alpha;
			////////////////////////////////
		}
		////////////////////////////////
		// beta = ||r||^2 / ||r_old||^2
		// d = r + d * beta
		double beta = delta_new / delta_old;
		for( int i=0 ; i<dim ; i++ ) d[i] = r[i] + d[i] * beta;
		////////////////////////////////
	}
	return ii;
#else
	eps *= eps;
	int dim = int( b.Dimensions() );
	Vector< T2 > r( dim ) , d( dim ) , q( dim ) , temp;
	if( reset ) x.Resize( dim );
	if( solveNormal ) temp.Resize( dim );
	T2 *_x = &x[0] , *_r = &r[0] , *_d = &d[0] , *_q = &q[0];
	const T2* _b = &b[0];

	double delta_new = 0 , delta_0;
	if( solveNormal )
	{
		A.Multiply( x , temp , scratch , addDCTerm ) , A.Multiply( temp , r , scratch , addDCTerm ) , A.Multiply( b , temp , scratch , addDCTerm );
		for( int i=0 ; i<dim ; i++ ) _d[i] = _r[i] = temp[i] - _r[i] , delta_new += _r[i] * _r[i];
	}
	else
	{
		 A.Multiply( x , r , scratch , addDCTerm );
		for( int i=0 ; i<dim ; i++ ) _d[i] = _r[i] = _b[i] - _r[i] , delta_new += _r[i] * _r[i];
	}
	delta_0 = delta_new;
	if( delta_new<eps )
	{
//		fprintf( stderr , "[WARNING] Initial residual too low: %g < %f\n" , delta_new , eps );
		return 0;
	}
	int ii;
	for( ii=0 ; ii<iters && delta_new>eps*delta_0 ; ii++ )
	{
		if( solveNormal ) A.Multiply( d , temp , scratch , addDCTerm ) , A.Multiply( temp , q , scratch , addDCTerm );
		else              A.Multiply( d , q , scratch , addDCTerm );
        double dDotQ = 0;
		for( int i=0 ; i<dim ; i++ ) dDotQ += _d[i] * _q[i];
		T2 alpha = T2( delta_new / dDotQ );
		double delta_old = delta_new;
		delta_new = 0;
		if( (ii%50)==(50-1) )
		{
			for( int i=0 ; i<dim ; i++ ) _x[i] += _d[i] * alpha;
			r.Resize( dim );
			if( solveNormal ) A.Multiply( x , temp , scratch , addDCTerm ) , A.Multiply( temp , r , scratch , addDCTerm );
			else              A.Multiply( x , r , scratch , addDCTerm );
			for( int i=0 ; i<dim ; i++ ) _r[i] = _b[i] - _r[i] , delta_new += _r[i] * _r[i] , _x[i] += _d[i] * alpha;
		}
		else
			for( int i=0 ; i<dim ; i++ ) _r[i] -= _q[i] * alpha , delta_new += _r[i] * _r[i] ,  _x[i] += _d[i] * alpha;

		T2 beta = T2( delta_new / delta_old );
		for( int i=0 ; i<dim ; i++ ) _d[i] = _r[i] + _d[i] * beta;
	}
	return ii;
#endif
}
template< class T >
template< class T2 >
int SparseSymmetricMatrix<T>::SolveCG( const SparseSymmetricMatrix<T>& A , const Vector<T2>& b , int iters , Vector<T2>& x , T2 eps , int reset , int threads , bool addDCTerm , bool solveNormal )
{
	eps *= eps;
	int dim = int( b.Dimensions() );
	MapReduceVector< T2 > outScratch;
	if( threads<1 ) threads = 1;
	if( threads>1 ) outScratch.resize( threads , dim );
	if( reset ) x.Resize( dim );
	Vector< T2 > r( dim ) , d( dim ) , q( dim );
	Vector< T2 > temp;
	if( solveNormal ) temp.Resize( dim );
	T2 *_x = &x[0] , *_r = &r[0] , *_d = &d[0] , *_q = &q[0];
	const T2* _b = &b[0];

	double delta_new = 0 , delta_0;

	if( solveNormal )
	{
		if( threads>1 ) A.Multiply( x , temp , outScratch , addDCTerm ) , A.Multiply( temp , r , outScratch , addDCTerm ) , A.Multiply( b , temp , outScratch , addDCTerm );
		else            A.Multiply( x , temp , addDCTerm )              , A.Multiply( temp , r , addDCTerm )              , A.Multiply( b , temp , addDCTerm );
		for( int i=0 ; i<dim ; i++ ) _d[i] = _r[i] = temp[i] - _r[i] , delta_new += _r[i] * _r[i];
	}
	else
	{
		if( threads>1 ) A.Multiply( x , r , outScratch , addDCTerm );
		else            A.Multiply( x , r , addDCTerm );
		for( int i=0 ; i<dim ; i++ ) _d[i] = _r[i] = _b[i] - _r[i] , delta_new += _r[i] * _r[i];
	}

	delta_0 = delta_new;
	if( delta_new<eps )
	{
//		fprintf( stderr , "[WARNING] Initial residual too low: %g < %f\n" , delta_new , eps );
		return 0;
	}
	int ii;
	for( ii=0 ; ii<iters && delta_new>eps*delta_0 ; ii++ )
	{
		if( solveNormal )
		{
			if( threads>1 ) A.Multiply( d , temp , outScratch , addDCTerm ) , A.Multiply( temp , q , outScratch , addDCTerm );
			else            A.Multiply( d , temp , addDCTerm )              , A.Multiply( temp , q , addDCTerm );
		}
		else
		{
			if( threads>1 ) A.Multiply( d , q , outScratch , addDCTerm );
			else            A.Multiply( d , q , addDCTerm );
		}
        double dDotQ = 0;
		for( int i=0 ; i<dim ; i++ ) dDotQ += _d[i] * _q[i];
		T2 alpha = T2( delta_new / dDotQ );
		double delta_old = delta_new;
		delta_new = 0;

		if( (ii%50)==(50-1) )
		{
			for( int i=0 ; i<dim ; i++ ) _x[i] += _d[i] * alpha;
			r.SetZero();
			if( solveNormal )
			{
				if( threads>1 ) A.Multiply( x , temp , outScratch , addDCTerm ) , A.Multiply( temp , r , outScratch , addDCTerm );
				else            A.Multiply( x , temp , addDCTerm )              , A.Multiply( temp , r , addDCTerm );
			}
			else
			{
				if( threads>1 ) A.Multiply( x , r , outScratch , addDCTerm );
				else            A.Multiply( x , r , addDCTerm );
			}
			for( int i=0 ; i<dim ; i++ ) _r[i] = _b[i] - _r[i] , delta_new += _r[i] * _r[i] , _x[i] += _d[i] * alpha;
		}
		else
		{
			for( int i=0 ; i<dim ; i++ ) _r[i] -= _q[i] * alpha , delta_new += _r[i] * _r[i] , _x[i] += _d[i] * alpha;
		}

		T2 beta = T2( delta_new / delta_old );
		for( int i=0 ; i<dim ; i++ ) _d[i] = _r[i] + _d[i] * beta;
	}
	return ii;
}

template< class T >
template< class T2 >
int SparseMatrix<T>::SolveJacobi( const SparseMatrix<T>& M , const Vector<T2>& diagonal , const Vector<T2>& b , Vector<T2>& x , Vector<T2>& Mx , T2 sor , int threads , int offset )
{
	M.Multiply( x , Mx , threads );
#if ZERO_TESTING_JACOBI
	for( int j=0 ; j<int(M.rows) ; j++ ) if( diagonal[j] ) x[j+offset] += ( b[j+offset]-Mx[j] ) * sor / diagonal[j];
#else // !ZERO_TESTING_JACOBI
	for( int j=0 ; j<int(M.rows) ; j++ ) x[j+offset] += ( b[j+offset]-Mx[j] ) * sor / diagonal[j];
#endif // ZERO_TESTING_JACOBI
	return M.rows;
}
template< class T >
template< class T2 >
int SparseMatrix<T>::SolveJacobi( const SparseMatrix<T>& M , const Vector<T2>& b , Vector<T2>& x , Vector<T2>& Mx , T2 sor , int threads , int offset )
{
	M.Multiply( x , Mx , threads );
#if ZERO_TESTING_JACOBI
	for( int j=0 ; j<int(M.rows) ; j++ )
	{
		T diagonal = M[j][0].Value;
		if( diagonal ) x[j+offset] += ( b[j+offset]-Mx[j] ) * sor / diagonal;
	}
#else // !ZERO_TESTING_JACOBI
	for( int j=0 ; j<int(M.rows) ; j++ ) x[j+offset] += ( b[j+offset]-Mx[j] ) * sor / M[j][0].Value;
#endif // ZERO_TESTING_JACOBI
	return M.rows;
}
template< class T >
template< class T2 >
int SparseSymmetricMatrix<T>::SolveJacobi( const SparseSymmetricMatrix<T>& M , const Vector<T2>& diagonal , const Vector<T2>& b , Vector<T2>& x , Vector<T2>& Mx , T2 sor , int reset )
{
	if( reset ) x.Resize( b.Dimensions() ) , x.SetZero();
	M.Multiply( x , Mx );
	// solution_new[j] * diagonal[j] + ( Md[j] - solution_old[j] * diagonal[j] ) = b[j]
	// solution_new[j] = ( b[j] - ( Md[j] - solution_old[j] * diagonal[j] ) ) / diagonal[j]
	// solution_new[j] = ( b[j] - Md[j] ) / diagonal[j] + solution_old[j]
	//		for( int j=0 ; j<int(M.rows) ; j++ ) x[j] += ( b[j]-Mx[j] ) / diagonal[j];
#if ZERO_TESTING_JACOBI
	for( int j=0 ; j<int(M.rows) ; j++ ) if( diagonal[j] ) x[j] += ( b[j]-Mx[j] ) * sor / diagonal[j];
#else // !ZERO_TESTING_JACOBI
	for( int j=0 ; j<int(M.rows) ; j++ ) x[j] += ( b[j]-Mx[j] ) * sor / diagonal[j];
#endif // ZERO_TESTING_JACOBI
	return M.rows;
}
template< class T >
template< class T2 >
int SparseSymmetricMatrix<T>::SolveJacobi( const SparseSymmetricMatrix<T>& M , const Vector<T2>& diagonal , const Vector<T2>& b , Vector<T2>& x , MapReduceVector<T2>& scratch , Vector<T2>& Mx , T2 sor , int reset )
{
	if( reset ) x.Resize( b.Dimensions() ) , x.SetZero();
	M.Multiply( x , Mx , scratch );
	// solution_new[j] * diagonal[j] + ( Md[j] - solution_old[j] * diagonal[j] ) = b[j]
	// solution_new[j] = ( b[j] - ( Md[j] - solution_old[j] * diagonal[j] ) ) / diagonal[j]
	// solution_new[j] = ( b[j] - Md[j] ) / diagonal[j] + solution_old[j]
	//		for( int j=0 ; j<int(M.rows) ; j++ ) x[j] += ( b[j]-Mx[j] ) / diagonal[j];
#if ZERO_TESTING_JACOBI
	for( int j=0 ; j<int(M.rows) ; j++ ) if( diagonal[j] ) x[j] += ( b[j]-Mx[j] ) * sor / diagonal[j];
#else // !ZERO_TESTING_JACOBI
	for( int j=0 ; j<int(M.rows) ; j++ ) x[j] += ( b[j]-Mx[j] ) * sor / diagonal[j];
#endif // ZERO_TESTING_JACOBI
	return M.rows;
}
template< class T >
template< class T2 >
int SparseSymmetricMatrix<T>::SolveJacobi( const SparseSymmetricMatrix<T>& M , const Vector<T2>& b , int iters , Vector<T2>& x , T2 sor , int reset )
{
	Vector< T2 > diagonal , Mx;
	M.getDiagonal( diagonal );
	Mx.Resize( M.rows );
	for( int i=0 ; i<iters ; i++ ) SolveJacobi( M , diagonal , b , x , Mx , sor , reset );
	return iters;
}
template< class T >
template< class T2 >
int SparseSymmetricMatrix<T>::SolveJacobi( const SparseSymmetricMatrix<T>& M , const Vector<T2>& b , int iters , Vector<T2>& x , MapReduceVector<T2>& scratch , T2 sor , int reset )
{
	Vector< T2 > diagonal , Mx;
	M.getDiagonal( diagonal , scratch.threads() );
	Mx.Resize( M.rows );
	for( int i=0 ; i<iters ; i++ ) SolveJacobi( M , diagonal , b , x , scratch , Mx , sor , reset );
	return iters;
}
template<class T>
template<class T2>
int SparseMatrix<T>::SolveGS( const SparseMatrix<T>& M , const Vector<T2>& diagonal , const Vector<T2>& b , Vector<T2>& x , bool forward , int offset )
{
#define ITERATE                                                         \
	{                                                                   \
		ConstPointer( MatrixEntry< T > ) start = M[j];                  \
		ConstPointer( MatrixEntry< T > ) end = start + M.rowSizes[j];   \
		ConstPointer( MatrixEntry< T > ) e;                             \
		T2 _b = b[j+offset];                                            \
		for( e=start ; e!=end ; e++ ) _b -= x[ e->N ] * e->Value;       \
		x[j+offset] += _b / diagonal[j];                                \
	}

#if ZERO_TESTING_JACOBI
	if( forward ) for( int j=0 ; j<int(M.rows)    ; j++ ){ if( diagonal[j] ){ ITERATE; } }
	else          for( int j=int(M.rows)-1 ; j>=0 ; j-- ){ if( diagonal[j] ){ ITERATE; } }
#else // !ZERO_TESTING_JACOBI
	if( forward ) for( int j=0 ; j<int(M.rows) ; j++ ){ ITERATE; }
	else          for( int j=int(M.rows)-1 ; j>=0 ; j-- ){ ITERATE; }
#endif // ZERO_TESTING_JACOBI
#undef ITERATE
	return M.rows;
}
template<class T>
template<class T2>
int SparseMatrix<T>::SolveGS( const std::vector< std::vector< int > >& mcIndices , const SparseMatrix<T>& M , const Vector<T2>& diagonal , const Vector<T2>& b , Vector<T2>& x , bool forward , int threads , int offset )
{
	int sum=0;
#if ZERO_TESTING_JACOBI
#define ITERATE( indices )                                                        \
	{                                                                             \
		for( int k=0 ; k<int( indices.size() ) ; k++ ) if( diagonal[indices[k]] ) \
		{                                                                         \
			int jj = indices[k];                                                  \
			ConstPointer( MatrixEntry< T > ) start = M[jj];                       \
			ConstPointer( MatrixEntry< T > ) end = start + M.rowSizes[jj];        \
			ConstPointer( MatrixEntry< T > ) e;                                   \
			T2 _b = b[jj+offset];                                                 \
			for( e=start ; e!=end ; e++ ) _b -= x[ e->N ] * e->Value;             \
			x[jj+offset] += _b / diagonal[jj];                                    \
		}                                                                         \
	}
#else // !ZERO_TESTING_JACOBI
#define ITERATE( indices )                                                  \
	{                                                                       \
		for( int k=0 ; k<int( indices.size() ) ; k++ )                      \
		{                                                                   \
			int jj = indices[k];                                            \
			ConstPointer( MatrixEntry< T > ) start = M[jj];                 \
			ConstPointer( MatrixEntry< T > ) end = start + M.rowSizes[jj];  \
			ConstPointer( MatrixEntry< T > ) e;                             \
			T2 _b = b[jj+offset];                                           \
			for( e=start ; e!=end ; e++ ) _b -= x[ e->N ] * e->Value;       \
			x[jj+offset] += _b / diagonal[jj];                              \
		}                                                                   \
	}
#endif // ZERO_TESTING_JACOBI

	if( forward ) for( int j=0 ; j<mcIndices.size()  ; j++ ){ sum += int( mcIndices[j].size() ) ; ITERATE( mcIndices[j] ); }
	else for( int j=int( mcIndices.size() )-1 ; j>=0 ; j-- ){ sum += int( mcIndices[j].size() ) ; ITERATE( mcIndices[j] ); }
#undef ITERATE
	return sum;
}
template<class T>
template<class T2>
int SparseMatrix<T>::SolveGS( const SparseMatrix<T>& M , const Vector<T2>& b , Vector<T2>& x , bool forward , int offset )
{
	int start = forward ? 0 : M.rows-1 , end = forward ? M.rows : -1 , dir = forward ? 1 : -1;
	for( int j=start ; j!=end ; j+=dir )
	{
		T diagonal = M[j][0].Value;
#if ZERO_TESTING_JACOBI
		if( diagonal )
#endif // ZERO_TESTING_JACOBI
		{
			ConstPointer( MatrixEntry< T > ) start = M[j];
			ConstPointer( MatrixEntry< T > ) end = start + M.rowSizes[j];
			ConstPointer( MatrixEntry< T > ) e;
			start++;
			T2 _b = b[j+offset];
			for( e=start ; e!=end ; e++ ) _b -= x[ e->N ] * e->Value;
			x[j+offset] = _b / diagonal;
		}
	}
	return M.rows;
}
template<class T>
template<class T2>
int SparseMatrix<T>::SolveGS( const std::vector< std::vector< int > >& mcIndices , const SparseMatrix<T>& M , const Vector<T2>& b , Vector<T2>& x , bool forward , int threads , int offset )
{
	int sum=0 , start = forward ? 0 : int( mcIndices.size() )-1 , end = forward ? int( mcIndices.size() ) : -1 , dir = forward ? 1 : -1;
	for( int j=start ; j!=end ; j+=dir )
	{
		const std::vector< int >& _mcIndices = mcIndices[j];
		sum += int( _mcIndices.size() );
		{
			for( int k=0 ; k<int( _mcIndices.size() ) ; k++ )
			{
				int jj = _mcIndices[k];
				T diagonal = M[jj][0].Value;
#if ZERO_TESTING_JACOBI
				if( diagonal )
#endif // ZERO_TESTING_JACOBI
				{
					ConstPointer( MatrixEntry< T > ) start = M[jj];
					ConstPointer( MatrixEntry< T > ) end = start + M.rowSizes[jj];
					ConstPointer( MatrixEntry< T > ) e;
					start++;
					T2 _b = b[jj+offset];
					for( e=start ; e!=end ; e++ ) _b -= x[ e->N ] * e->Value;
					x[jj+offset] = _b / diagonal;
				}
			}
		}
	}
	return sum;
}


template<class T>
template<class T2>
int SparseSymmetricMatrix<T>::SolveGS( const SparseSymmetricMatrix<T>& M , const Vector<T2>& diagonal , const Vector<T2>& b , Vector<T2>& x , Vector<T2>& Mx , Vector<T2>& dx , bool forward , int reset , int ordering )
{
	if( reset ) x.Resize( b.Dimensions() ) , x.SetZero();
	dx.SetZero();
	M.Multiply( x , Mx );
#define ITERATE                                                         \
	{                                                                   \
		ConstPointer( MatrixEntry< T > ) start = M[j];                  \
		ConstPointer( MatrixEntry< T > ) end = start + M.rowSizes[j];   \
		ConstPointer( MatrixEntry< T > ) e;                             \
		T2 _Mx = Mx[j];                                                 \
		if( ordering!=ORDERING_UPPER_TRIANGULAR )                       \
			for( e=start ; e!=end ; e++ ) _Mx += dx[ e->N ] * e->Value; \
		dx[j] = ( b[j]-_Mx ) / diagonal[j];                             \
		x[j] += dx[j];                                                  \
		if( ordering!=ORDERING_LOWER_TRIANGULAR )                       \
		for( e=start ; e!=end ; e++ ) Mx[ e->N ] += dx[j] * e->Value;   \
	}

#if ZERO_TESTING_JACOBI
	if( forward ) for( int j=0 ; j<int(M.rows)    ; j++ ){ if( diagonal[j] ){ ITERATE; } }
	else          for( int j=int(M.rows)-1 ; j>=0 ; j-- ){ if( diagonal[j] ){ ITERATE; } }
#else // !ZERO_TESTING_JACOBI
	if( forward ) for( int j=0 ; j<int(M.rows)    ; j++ ){ ITERATE; }
	else          for( int j=int(M.rows)-1 ; j>=0 ; j-- ){ ITERATE; }
#endif // ZERO_TESTING_JACOBI
#undef ITERATE
	return M.rows;
}
template<class T>
template<class T2>
int SparseSymmetricMatrix<T>::SolveGS( const SparseSymmetricMatrix<T>& M , const Vector<T2>& diagonal , const Vector<T2>& b , Vector<T2>& x , MapReduceVector<T2>& scratch , Vector<T2>& Mx , Vector<T2>& dx , bool forward , int reset , int ordering )
{
	if( reset ) x.Resize( b.Dimensions() ) , x.SetZero();
	dx.SetZero();
	M.Multiply( x , Mx , scratch );
#define ITERATE                                                         \
	{                                                                   \
		ConstPointer( MatrixEntry< T > ) start = M[j];                  \
		ConstPointer( MatrixEntry< T > ) end = start + M.rowSizes[j];   \
		ConstPointer( MatrixEntry< T > ) e;                             \
		T2 _Mx = Mx[j];                                                 \
		if( ordering!=ORDERING_UPPER_TRIANGULAR )                       \
			for( e=start ; e!=end ; e++ ) _Mx += dx[ e->N ] * e->Value; \
		dx[j] = ( b[j]-_Mx ) / diagonal[j];                             \
		x[j] += dx[j];                                                  \
		if( ordering!=ORDERING_LOWER_TRIANGULAR )                       \
		for( e=start ; e!=end ; e++ ) Mx[ e->N ] += dx[j] * e->Value;   \
	}

#if ZERO_TESTING_JACOBI
	if( forward ) for( int j=0 ; j<int(M.rows)    ; j++ ){ if( diagonal[j] ){ ITERATE; } }
	else          for( int j=int(M.rows)-1 ; j>=0 ; j-- ){ if( diagonal[j] ){ ITERATE; } }
#else // !ZERO_TESTING_JACOBI
	if( forward ) for( int j=0 ; j<int(M.rows)    ; j++ ){ ITERATE; }
	else          for( int j=int(M.rows)-1 ; j>=0 ; j-- ){ ITERATE; }
#endif // ZERO_TESTING_JACOBI
#undef ITERATE
	return M.rows;
}
template<class T>
template<class T2>
int SparseSymmetricMatrix<T>::SolveGS( const std::vector< std::vector< int > >& mcIndices , const SparseSymmetricMatrix<T>& M , const Vector<T2>& diagonal , const Vector<T2>& b , Vector<T2>& x , MapReduceVector<T2>& scratch , Vector<T2>& Mx , Vector<T2>& dx , bool forward , int reset )
{
	int sum = 0;
	if( reset ) x.Resize( b.Dimensions() ) , x.SetZero();
	M.Multiply( x , Mx , scratch );
	dx.SetZero();
#if ZERO_TESTING_JACOBI
#define ITERATE( indices )                                                        \
	{                                                                             \
		for( int k=0 ; k<int( indices.size() ) ; k++ ) if( diagonal[indices[k]] ) \
		{                                                                         \
			int jj = indices[k];                                                  \
			ConstPointer( MatrixEntry< T > ) start = M[jj];                       \
			ConstPointer( MatrixEntry< T > ) end = start + M.rowSizes[jj];        \
			ConstPointer( MatrixEntry< T > ) e;                                   \
			T2 _Mx = Mx[jj];                                                      \
			for( e=start ; e!=end ; e++ ) _Mx += dx[ e->N ] * e->Value;           \
			Mx[jj] = _Mx;                                                         \
			dx[jj] = ( b[jj]-_Mx ) / diagonal[jj];                                \
			x[jj] += dx[jj];                                                      \
			for( e=start ; e!=end ; e++ ) Mx[ e->N ] += dx[jj] * e->Value;        \
		}                                                                         \
	}
#else // !ZERO_TESTING_JACOBI
#define ITERATE( indices )                                                 \
	{                                                                      \
		for( int k=0 ; k<int( indices.size() ) ; k++ )                     \
		{                                                                  \
			int jj = indices[k];                                           \
			ConstPointer( MatrixEntry< T > ) start = M[jj];                \
			ConstPointer( MatrixEntry< T > ) end = start + M.rowSizes[jj]; \
			ConstPointer( MatrixEntry< T > ) e;                            \
			T2 _Mx = Mx[jj];                                               \
			for( e=start ; e!=end ; e++ ) _Mx += dx[ e->N ] * e->Value;    \
			Mx[jj] = _Mx;                                                  \
			dx[jj] = ( b[jj]-_Mx ) / diagonal[jj];                         \
			x[jj] += dx[jj];                                               \
			for( e=start ; e!=end ; e++ ) Mx[ e->N ] += dx[jj] * e->Value; \
		}                                                                  \
	}
#endif // ZERO_TESTING_JACOBI

	if( forward ) for( int j=0 ; j<int( mcIndices.size() )    ; j++ ){ sum += int(mcIndices[j].size()) ; ITERATE( mcIndices[j] ); }
	else          for( int j=int( mcIndices.size() )-1 ; j>=0 ; j-- ){ sum += int(mcIndices[j].size()) ; ITERATE( mcIndices[j] ); }
#undef ITERATE
	return sum;
}
template<class T>
template<class T2>
int SparseSymmetricMatrix<T>::SolveGS( const SparseSymmetricMatrix<T>& M , const Vector<T2>& b , int iters , Vector<T2>& solution , bool forward , int reset , int ordering )
{
	Vector< T2 > diagonal , Mx , dx;
	M.getDiagonal( diagonal );
	Mx.Resize( M.rows ) , dx.Resize( M.rows );
	for( int i=0 ; i<iters ; i++ ) SolveGS( M , diagonal , b , solution , Mx , dx , forward , reset , ordering );
	return iters;
}
template<class T>
template<class T2>
int SparseSymmetricMatrix<T>::SolveGS( const SparseSymmetricMatrix<T>& M , const Vector<T2>& b , int iters , Vector<T2>& solution , MapReduceVector<T2>& scratch , bool forward , int reset , int ordering )
{
	Vector< T2 > diagonal , Mx , dx;
	M.getDiagonal( diagonal , scratch.threads() );
	Mx.Resize( M.rows ) , dx.Resize( M.rows );
	for( int i=0 ; i<iters ; i++ ) SolveGS( M , diagonal , b , solution , scratch , Mx , dx , forward , reset , ordering );
	return iters;
}
template<class T>
template<class T2>
int SparseSymmetricMatrix<T>::SolveGS( const std::vector< std::vector< int > >& mcIndices , const SparseSymmetricMatrix<T>& M , const Vector<T2>& b , int iters , Vector<T2>& solution , MapReduceVector<T2>& scratch , bool forward , int reset )
{
	Vector< T2 > diagonal , Mx , dx;
	M.getDiagonal( diagonal , scratch.threads() );
	Mx.Resize( M.rows ) , dx.Resize( M.rows );
	for( int i=0 ; i<iters ; i++ ) SolveGS( mcIndices , M , diagonal , b , solution , scratch , Mx , dx , forward , reset );
	return iters;
}
template< class T >
template< class T2 >
void SparseMatrix< T >::getDiagonal( Vector< T2 >& diagonal , int threads , int offset ) const
{
	diagonal.Resize( SparseMatrix< T >::rows );
	for( int i=0 ; i<rows ; i++ )
	{
		int ii = i+offset;
		T2 d = 0.;
		ConstPointer( MatrixEntry< T > ) start = m_ppElements[i];
		ConstPointer( MatrixEntry< T > ) end = start + rowSizes[i];
		ConstPointer( MatrixEntry< T > ) e;
		for( e=start ; e!=end ; e++ ) if( e->N==ii ) d += e->Value;
		diagonal[i] = d;
	}
}
template< class T >
template< class T2 >
void SparseSymmetricMatrix< T >::getDiagonal( Vector< T2 >& diagonal , int threads ) const
{
	diagonal.Resize( SparseMatrix< T >::rows );
	for( int i=0 ; i<SparseMatrix< T >::rows ; i++ )
	{
		T2 d = 0.;
		ConstPointer( MatrixEntry< T > ) start = SparseMatrix< T >::m_ppElements[i];
		ConstPointer( MatrixEntry< T > ) end = start + SparseMatrix< T >::rowSizes[i];
		ConstPointer( MatrixEntry< T > ) e;
		for( e=start ; e!=end ; e++ ) if( e->N==i ) d += e->Value;
		diagonal[i] = d * T2(2);
	}
}

