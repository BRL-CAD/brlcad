/*
 *			I H O S T . H
 *
 *  Internal host table routines.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 *
 *  $Header$
 */

/* Internal Host table */
struct ihost {
	struct rt_list	l;
	char		*ht_name;	/* Official name of host */
	int		ht_flags;	/* Control info about this host */
#define HT_HOLD		0x1
	int		ht_when;	/* When to use this host */
#define HT_ALWAYS	1		/* Always try to use this one */
#define HT_NIGHT	2		/* Only use at night */
#define HT_PASSIVE	3		/* May call in, never initiated */
#define HT_RS		4		/* While rays/second > ht_rs */
#define HT_PASSRS	5		/* passive rays/second */
#define HT_HACKNIGHT	6		/* Only use during "hackers night" */
	int		ht_rs;		/* rays/second level */
	int		ht_rs_miss;	/* number of consecutive misses */
	int		ht_rs_wait;	/* # of auto adds to wait before */
					/* restarting this server */
	int		ht_where;	/* Where to find database */
#define HT_CD		1		/* cd to ht_path first */
#define HT_CONVERT	2		/* cd to ht_path, asc2g database */
#define HT_USE		3		/* cd to ht_path, use asc database */
					/* best of cd and convert */
	char		*ht_path;	/* remote directory to run in */
};
#define IHOST_MAGIC	0x69486f73
#define CK_IHOST(_p)	RT_CKMAG(_p, IHOST_MAGIC, "ihost")

extern struct rt_list	HostHead;

#define IHOST_NULL	((struct ihost *)0)

extern struct ihost	*host_lookup_by_name(char *name, int enter);
extern struct ihost	*host_lookup_by_addr(struct sockaddr_in *from, int enter);
extern struct ihost	*host_lookup_by_hostent(struct hostent *addr, int enter);
extern struct ihost	*make_default_host(char *name);
extern char		*get_our_hostname(void);
extern struct ihost	*host_lookup_of_fd(int fd);

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
