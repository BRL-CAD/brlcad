#define CSG_MODE		1
#define FACET_MODE		2
#define	TRIMMED_SURF_MODE	3

#define NAMESIZE	16	/* from db.h */

struct iges_properties
{
	char			name[NAMESIZE+1];
	char			material_name[32];
	char			material_params[60];
	char			region_flag;
	short			ident;
	short			air_code;
	short			material_code;
	short			los_density;
	short			inherit;
	short			color_defined;
	unsigned char		color[3];

};
