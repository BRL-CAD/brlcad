/*
**	Table handling module.
**
**	This file supplies data manipulation routines to other modules;
**	it does not store any data itself. Its routines are generic,
**	applicable to the storage of any kind of data structure with
**	primary key and a hash function on it.
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"

/*
**	Initialize a table.
*/
void
_init_table(table)
reg	Table	*table;
{
	reg	int	i;

	table->ta_store = make_many(List *, table->ta_size);
	for (i = 0; i < table->ta_size; i++)
		table->ta_store[i] = NULL;
}

/*
**	Look up and return the entry corresponding to the key
**	in a table.
*/

Cast
_lookup_table(table, key)
reg	Table	*table;
reg	Cast	key;
{
	reg	List	*ptr;
	reg	int	h;

	put_trail("lookup_table", "start");
	h = tablehash(table)(key);
	for_list (ptr, table->ta_store[h])
	{
		if (tableequal(table)(key, tablekey(table)(ldata(ptr))))
		{
#ifdef	EXTRACHECK
			if (ldata(ptr) == NULL)
			{
				fprintf(stderr, "cake internal error: returning null in lookup_table\n");
				exit_cake(TRUE);
			}
#endif
			put_trail("lookup_table", "finish");
			return ldata(ptr);
		}
	}

	put_trail("lookup_table", "finish");
	return (Cast)NULL;
}

/*
**	Insert a new entry into the table.
**	Return whether it was there before.
*/

bool
_insert_table(table, entry)
reg	Table	*table;
reg	Cast	entry;
{
	reg	List	*ptr;
	reg	Cast	key;
	reg	int	h;

	put_trail("insert_table", "start");
	key = tablekey(table)(entry);
	h   = tablehash(table)(key);
	for_list (ptr, table->ta_store[h])
		if (tableequal(table)(key, tablekey(table)(ldata(ptr))))
		{
			put_trail("insert_table", "finish");
			return TRUE;
		}

	table->ta_store[h] = addhead(table->ta_store[h], entry);
	put_trail("insert_table", "finish");
	return FALSE;
}

/*
**	Hash str into the range 0 to SIZE-1.
*/

int
hash(s)
reg	char	*s;
{
#ifdef CRAY
	reg	long	h;
#else
	reg	int	h;
#endif

	for (h = 0; *s != '\0'; s++)
		h = (h << 1) + *s;

	h = (h >= 0? h: -h) % SIZE;
	if( h < 0 || h > SIZE )  {
		fprintf(stderr, "cake: hash error, h=%d\n", h);	/* BRL */
	}
	return h;
}

/*
**	Return a list of all the entries in a table.
*/

List *
_contents_table(table)
reg	Table	*table;
{
	reg	List	*all;
	reg	List	*ptr;
	reg	int	i;

	all = makelist0();
	for (i = 0; i < table->ta_size; i++)
		for_list (ptr, table->ta_store[i])
			addtail(all, ldata(ptr));	/* na */

	return all;
}
