/*
**	Definitions for the table module.
**
**	$Header$
*/

typedef	struct	s_table
{
	int	ta_size;	/* the size of the store	*/
	List	**ta_store;	/* the store, an array of lists	*/
	Cast	(*ta_key)();	/* applied to entries		*/
	int	(*ta_hash)();	/* applied to keys		*/
	bool	(*ta_equal)();	/* applied to two keys		*/
} Table;

#define	init_table(t)		_init_table(&t)
#define	lookup_table(t, k)	_lookup_table(&t, (Cast) k)
#define	insert_table(t, e)	_insert_table(&t, (Cast) e)
#define	contents_table(t)	_contents_table(&t)

#define	tablekey(table)		(*(table->ta_key))
#define	tablehash(table)	(*(table->ta_hash))
#define	tableequal(table)	(*(table->ta_equal))

extern	void	_init_table();
extern	Cast	_lookup_table();
extern	bool	_insert_table();
extern	List	*_contents_table();
