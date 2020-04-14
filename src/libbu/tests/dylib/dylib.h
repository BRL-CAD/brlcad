struct dylib_contents {
	const char *const name;
	double version;
	int (*calc)(char **result, int rmaxlen, int input);
};

struct dylib_plugin {
	const struct dylib_contents * const i;
};

extern int dylib_load_plugins();

