#define RT_LEX_ANY	0	/* pseudo type */
struct rt_lex_t_int {
	int type;
	int value;
};
#define RT_LEX_INT	1
struct rt_lex_t_dbl {
	int	type;
	double	value;
};
#define RT_LEX_DOUBLE	2
struct rt_lex_t_key {
	int	type;
	int	value;
};
#define RT_LEX_SYMBOL	3
#define RT_LEX_KEYWORD	4
struct rt_lex_t_id {
	int	type;
	char 	*value;
};
#define RT_LEX_IDENT	5
#define RT_LEX_NUMBER	6	/* Pseudo type */
union rt_lex_token {
	int			type;
	struct	rt_lex_t_int	t_int;
	struct	rt_lex_t_dbl	t_dbl;
	struct	rt_lex_t_key	t_key;
	struct	rt_lex_t_id	t_id;
};
struct rt_lex_key {
	int	tok_val;
	char	*string;
};
#define RT_LEX_NEED_MORE	0
