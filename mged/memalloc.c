/*	SCCSID	%W%	%E%	*/
/*
 *	memalloc.c -- displaylist memory allocation routines
 */

/*
 * Structure of the coremap array.
 * Consists of non-zero count and base address of that many contiguous units.
 * The addresses are increasing and the list is terminated with the
 * first zero link.
 *
 * memalloc() and memfree() use these tables to allocate memory
 */
struct map {
	struct map	*m_nxtp;	/* Linking pointer to next element */
	unsigned	 m_size;	/* Size of this free element */
	unsigned	 m_addr;	/* Address of start of this element */
};
#define MAP_NULL	((struct map *) 0)

/* Allocation/Free spaces */
static struct map *coremap = MAP_NULL;	/* Working map queues -- freespace */
static struct map *freemap = MAP_NULL;	/* Freelist of buffers */

/*
 *	A little better memory allocator		=GET=
 *
 *	July 4, 1980
 *
 *	For each Memory Map there exists a queue (coremap).
 *	There also exists a queue of free buffers which are enqueued
 *	on to either of the previous queues.  Initially all of the buffers
 *	are placed on the `freemap' queue.  Whenever a buffer is freed
 *	because of coallescing ends in memfree() or zero size in memalloc()
 *	the mapping buffer is taken off from the respective queue and
 *	returned to the `freemap' queue.
 *
 *
 *		R E V I S I O N   H I S T O R Y
 *
 *	07/04/80  GET	Original version  (JHU 6.79)
 *
 *	07/15/80  MJM	Modified to have unique names, since calling
 *			sequences are not unique  (BRL 6.85)
 *
 *	09-Sep-83 DAG	Overhauled.
 */


	/* Flags used by `type' in memfree() */

#define	M_TMTCH	00001	/* Top match */
#define	M_BMTCH	00002	/* Bottom match */
#define	M_TOVFL	00004	/* Top overflow */
#define	M_BOVFL	00010	/* Bottom overflow */
#define M_TWRAR	00020	/* Top wrap-around */
#define	M_BWRAR	00040	/* Bottom wrap-around */

#define	MAXINT	((unsigned long)((int)(-1)))	/* Maximum integer */

/*
 *			M E M A L L O C
 *
 *	Takes:		& pointer of map,
 *			size.
 *
 *	Returns:	NULL	Error
 *			<addr>	Othewise
 *
 *	Comments:
 *	Algorithm is first fit.
 */
unsigned
memalloc( size )
register unsigned size;
{
	register struct map *prevp = MAP_NULL;
	register struct map *curp;
	unsigned	addr;

	if( size == 0 )
		return( 1 );	/* Anything non-zero */

	for( curp = coremap; curp; curp = (prevp=curp)->m_nxtp )  {
		if( curp->m_size >= size )
			break;
	}

	if( curp == MAP_NULL )  {
		printf("memalloc(%d):  no more space\n", size );
		return(0);		/* No more space */
	}

	addr = curp->m_addr;
	curp->m_addr += size;

	/* If the element size goes to zero, put it on the freelist */

	if( (curp->m_size -= size) == 0 )  {
		if( prevp )
			prevp->m_nxtp = curp->m_nxtp;
		else
			coremap = curp->m_nxtp;	/* Click list down at start */
		curp->m_nxtp = freemap;		/* Link it in */
		freemap = curp;			/* Make it the start */
	}

	return( addr );
}

/*
 *			M E M F R E E
 *
 *	Takes:
 *			size,
 *			address.
 *
 *	Comments:
 *	The routine does not check for wrap around when increasing sizes
 *	or changing addresses.  Other wrap-around conditions are flagged.
 */
void
memfree( size, addr )
unsigned size;
unsigned addr;
{
	register int type = {0};
	register struct map *prevp = MAP_NULL;
	register struct map *curp;
	long il;
	struct map *tmap;

	if( size == 0 )
		return;		/* Nothing to free */

	/* Find the position in the list such that (prevp)<(addr)<(curp) */
	for( curp = coremap; curp; curp = (prevp=curp)->m_nxtp )
		if( addr < curp->m_addr )
			break;

	/* Make up the `type' variable */

	if( prevp )  {
		if( (il=prevp->m_addr+prevp->m_size) > addr )
			type |= M_BOVFL;
		if( il == addr )
			type |= M_BMTCH;
		if( il > MAXINT )
			type |= M_BWRAR;
	}
	if( curp )  {
		if( (il=addr+size) > curp->m_addr )
			type |= M_TOVFL;
		if( il == curp->m_addr )
			type |= M_TMTCH;
		if( il > MAXINT )
			type |= M_TWRAR;
	}

	if( type & (M_TOVFL|M_BOVFL|M_TWRAR|M_BWRAR) )  {
		printf("mfree(addr=%d,size=%d)  error type=0%o\n",
			addr, size, type );
		if( prevp )
			printf("prevp: m_addr=%d, m_size=%d\n",
				prevp->m_addr, prevp->m_size );
		if( curp )
			printf("curp: m_addr=%d, m_size=%d\n",
				curp->m_addr, curp->m_size );
		printf("display memory dropped, continuing\n");
		return;
	}

	/*
 	 * Now we do the surgery:
	 * If there are no matches on boundaries we allocate a buffer
	 * If there is one match we expand the appropriate buffer
	 * If there are two matches we will have a free buffer returned.
	 */

	switch( type & (M_BMTCH|M_TMTCH) )  {
	case M_TMTCH|M_BMTCH:	/* Deallocate top element and expand bottom */
		prevp->m_size += size + curp->m_size;
		prevp->m_nxtp = curp->m_nxtp;
		curp->m_nxtp = freemap;		/* Link into freemap */
		freemap = curp;
		break;

	case M_BMTCH:		/* Expand bottom element */
		prevp->m_size += size;
		break;

	case M_TMTCH:		/* Expand top element downward */
		curp->m_size += size;
		curp->m_addr -= size;
		break;

	default:		/* No matches; allocate and insert */
		if( (tmap=freemap) == MAP_NULL )
			tmap = (struct map *)malloc(sizeof(struct map));
		else
			freemap = freemap->m_nxtp;	/* Click one off */

		if( prevp )
			prevp->m_nxtp = tmap;
		else
			coremap = tmap;

		tmap->m_size = size;
		tmap->m_addr = addr;
		tmap->m_nxtp = curp;
	}
}
