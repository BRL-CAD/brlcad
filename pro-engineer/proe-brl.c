#include <unistd.h>
#include <stdio.h>
#include "prodevelop.h"
#include "pro_hardware.h"
#include "pro_wchar_t.h"
#include "pd_prototype.h"
#include "prodev_const.h"
#include "prodev_size.h"
#include "profiles.h"
#include "proincludes.h"
#include "pro_unit.h"
#include "pro_feattype.h"
#include "prodev_feat.h"

extern char *tempnam();

extern int errno;

static int quality;
static int curr_win_id;
static int win_id;

void Output_object();	

static	wchar_t		  msgfil[PRODEV_NAME_SIZE];
#define MSGFIL		  pro_str_to_wstr(msgfil,"usermsg.txt")

#define	FILE_COPY_SIZE	2048
#define	MAX_LINE_LEN	512
#define DEFAULT_QUALITY 3

#define	V3ARGS(x)	(x)[0], (x)[1], (x)[2]

struct object_list
{
	char *obj;
	struct object_list *next;
} *obj_root;

int
Object_already_output( obj )
char *obj;
{
	int return_val;
	struct object_list *objl,*prev;
	char buf[1024];

	if( obj_root == (struct object_list *)NULL )
	{
		obj_root = (struct object_list *)malloc( sizeof( struct object_list ) );
		obj_root->next = (struct object_list *)NULL;
		obj_root->obj = obj;
		return_val = 0;
	}
	else
	{
		objl = obj_root;
		prev = (struct object_list *)NULL;
		while( objl != (struct object_list *)NULL && objl->obj != obj )
		{
			prev = objl;
			objl = objl->next;
		}

		if( objl == (struct object_list *)NULL )
		{
			return_val = 0;
			prev->next = (struct object_list *)malloc( sizeof( struct object_list ) );
			objl = prev->next;
			objl->next = (struct object_list *)NULL;
			objl->obj = obj;
		}
		else
			return_val = 1;
	}

	return( return_val );
}

void
Output_part( fd_out , obj , name )
FILE *fd_out;
char *obj;
char *name;
{
	char *tmpnam_return;
	char tmp_file_name[256];
	wchar_t wtmp_file_name[256];
	FILE *fd_tmp;
	char copy_buf[FILE_COPY_SIZE];
	char line[MAX_LINE_LEN+1];
	int copy_len;
	int i,name_len;
	int c;
	char *tmp_obj;
	Pro_object_info obj_info;
	int status;
	char *attrib;
	wchar_t unit_name[PRODEV_NAME_SIZE];
	double conv_factor;
	int unit_subtype;
	int no_of_features;
	int *feature_ids;
	int assem_cuts;
	int feature_mask=0xffffffff;

	/* Create a temporary file to store the render format for this part */
	tmpnam_return = tempnam( NULL , "proe_brl" );
	if( tmpnam_return == NULL )
	{
		fprintf( stderr , "Cannot create temporary file name for part %s!!!\n" , name );
		promsg_print( MSGFIL , "USER_TEMPNAM" ,  name );
		return;
	}
	strcpy( tmp_file_name , tmpnam_return );

	/* It appears that pro_export_file_from_pro changes the file name given to all
	 * lower case, so change them first
	 */
	i = (-1);
	while( tmp_file_name[++i] != '\0' )
		tmp_file_name[i] = tolower( tmp_file_name[i] );

	pro_str_to_wstr( wtmp_file_name , tmp_file_name );

	/* Output the part in render format to the temporary file */
	pro_str_to_wstr( obj_info.name , name );
	pro_str_to_wstr( obj_info.type , "PRT" );
	tmp_obj = prodb_retrieve_object( &obj_info , &status );
	if( status )
	{
		fprintf( stderr, "Failed to retrieve object (%s)\n" , name );
		promsg_print( MSGFIL , "USER_EXPORT_FAILED" , tmp_file_name , name );
		return;
	}

	if( win_id == (-1) )
		win_id = pro_open_object_window( obj_info.name , obj_info.type , attrib );
	pro_set_current_window( win_id );
	progr_display_object( tmp_obj );
	if( !pro_export_file_from_pro( wtmp_file_name , tmp_obj , PRO_RENDER_FILE , NULL , &quality , NULL , NULL ) )
	{
		fprintf( stderr , "Failed to export part (%s) to file (%s)\n" , name , tmp_file_name );
		promsg_print( MSGFIL , "USER_EXPORT_FAILED" , tmp_file_name , name );
		return;
	}

	pro_clear_window( win_id );

	if( (fd_tmp = fopen( tmp_file_name , "r" )) == NULL )
	{
		fprintf( stderr , "Cannot open temporary file (%s) for reading" , tmp_file_name );
		perror( "proe-brl" );
		promsg_print( MSGFIL , "CANNOT_OPEN_TMP_FILE" , tmp_file_name );
		return;
	}

	/* write the start of the render format to the final output file */
	fprintf( fd_out , "solid %s %x\n" , name , obj );

	/* skip the first line in the temporary file (similar to above line, but less info */
	if( fgets( line, MAX_LINE_LEN, fd_tmp ) == NULL )
	{
		fprintf( stderr, "Failed to read tmp file (%s) containing exported part %s\n",
			tmp_file_name, name );
		promsg_print( MSGFIL , "USER_EXPORT_FAILED" , tmp_file_name , name );
		fprintf( fd_out, "endsolid %s\n", name );
		fclose( fd_tmp );
		return;
	}

	while( fgets( line, MAX_LINE_LEN, fd_tmp ) )
	{
		if( !strncmp( "endsolid", line, 8 ) )
		{
			int output_start=1;
			int output_count=0;

			/* Add any Assembly Cut info before end of solid */

			no_of_features = prodb_get_feature_ids( obj, feature_mask, &feature_ids );
			assem_cuts = 0;
			for( i=0 ; i<no_of_features ; i++ )
			{
				int feat_type;
				Pro_feature feat_info;

				if( (feat_type=prodb_get_feat_type( obj, feature_ids[i] )) == FT_ASSEM_CUT )
				{
					char **surfaces;
					int nsurfs;
					int j;

					nsurfs = prodb_get_feature_surfaces( obj, feature_ids[i], &surfaces );
					if( nsurfs < 0 )
					{
						fprintf( stderr, "Failed to get Assembly Cut feature surfaces for part %s\n", name );
						continue;
					}
					else if( nsurfs == 0 )
					{
						fprintf( stderr, "Assembly Cut in part %s has no surfaces\n", name );
						continue;
					}
					else if( nsurfs != 1 )
					{
						fprintf( stderr, "Assembly Cut in part %s is too complex to convert\n", name );
						fprintf( stderr, "\tYou will probably need to edit %s\n", name );
						
					}
					assem_cuts += nsurfs;

					if( output_start )
					{
						output_start = 0;
						fprintf( fd_out, "  modifiers\n" );
					}

					for( j=0 ; j<nsurfs ; j++ )
					{
						Ptc_surf *surf;
						Ptc_srfshape *shape;

						if( (surf = prodb_get_surface( surfaces[j] ) ) == NULL )
						{
							fprintf( stderr, "Cannot get surface #%d for Assembly Cut in part %s\n", j+1, name );
							continue;
						}
						shape = &surf->srf_shape;

						switch( surf->type )
						{

							case 34:	/* plane */
								fprintf( fd_out, "    plane\n" );
								fprintf( fd_out, "      %12e %12e %12e\n", V3ARGS( shape->plane.origin ) );
								fprintf( fd_out, "      %12e %12e %12e\n", V3ARGS( shape->plane.e1 ) );
								fprintf( fd_out, "      %12e %12e %12e\n", V3ARGS( shape->plane.e2 ) );
								fprintf( fd_out, "      %12e %12e %12e\n", V3ARGS( shape->plane.e3 ) );
								fprintf( fd_out, "      %12e %12e\n", surf->uv_min[0], surf->uv_min[1] );
								fprintf( fd_out, "      %12e %12e\n", surf->uv_max[0], surf->uv_max[1] );
								fprintf( fd_out, "      %d\n", surf->orient );
								fprintf( fd_out, "    endplane\n" );
								output_count++;
								break;
#if 0
							case 36:	/* cylinder */
								fprintf( fd_out, "    cylinder\n" );
								fprintf( fd_out, "      %12e %12e %12e\n", V3ARGS( shape->cylinder.origin ) );
								fprintf( fd_out, "      %12e %12e %12e\n", V3ARGS( shape->cylinder.e1 ) );
								fprintf( fd_out, "      %12e %12e %12e\n", V3ARGS( shape->cylinder.e2 ) );
								fprintf( fd_out, "      %12e %12e %12e\n", V3ARGS( shape->cylinder.e3 ) );
								fprintf( fd_out, "      %12e\n", shape->cylinder.radius );
								fprintf( fd_out, "      %12e %12e\n", surf->uv_min[0], surf->uv_min[1] );
								fprintf( fd_out, "      %12e %12e\n", surf->uv_max[0], surf->uv_max[1] );
								fprintf( fd_out, "      %d\n", surf->orient );
								fprintf( fd_out, "    endcylinder\n" );
								break;
#endif
							default:
								break;
						}
						prodb_rls_surface( surf );
					}
				}
			}
			if( !output_start )
				fprintf( fd_out, "  endmodifiers\n" );
			if( assem_cuts != output_count )
			{
				int cuts_not_done;

				cuts_not_done = assem_cuts - output_count;
				fprintf( stderr, "Part (%s) has %d assembly cuts that will not be reflected in the converted part\n", name, cuts_not_done );
				fprintf( stderr, "\tYou will probably need to edit this part\n" );
				promsg_clear();
				promsg_print( MSGFIL, "ASSEM_CUTS", name, &cuts_not_done );
			}

		}
		fprintf( fd_out, "%s", line );
	}

	copy_len = FILE_COPY_SIZE;
	copy_len = fread( copy_buf , sizeof( char ) , copy_len , fd_tmp );
	while( copy_len )
	{
		if( fwrite( copy_buf , sizeof( char ) , copy_len , fd_out ) != copy_len )
		{
			promsg_print( MSGFIL , "USER_COPY_ERROR" );
			break;
		}
		copy_len = fread( copy_buf , sizeof( char ) , FILE_COPY_SIZE , fd_tmp );
	}

	fclose( fd_tmp );

	(void)unlink( tmp_file_name );
}

void
Output_assembly( fd_out, obj, name )
FILE *fd_out;
char *obj;
char *name;
{
	int i;
	int memb_id;
	wchar_t unit_name[PRODEV_NAME_SIZE];
	double conv_factor;
	int unit_subtype;
	int no_of_features;
	int *feature_ids;
	int assem_cuts;
	int feature_mask=0xffffffff;

	no_of_features = prodb_get_feature_ids( obj, feature_mask, &feature_ids );
	assem_cuts = 0;
	for( i=0 ; i<no_of_features ; i++ )
	{
		int feat_type;

		if( (feat_type=prodb_get_feat_type( obj, feature_ids[i] )) == FT_ASSEM_CUT )
			assem_cuts++;
	}

	if( assem_cuts )
	{
		fprintf( stderr, "Assembly (%s) has %d assembly cuts that will not be reflected in the converted part\n", name, assem_cuts );
		fprintf( stderr, "\tYou will probably need to edit this assembly\n" );
		promsg_clear();
		promsg_print( MSGFIL, "ASSEM_CUTS", name, &assem_cuts );
	}


	fprintf( fd_out , "assembly %s %x\n" , name , obj );

	memb_id = prodb_first_member( obj );
	while( memb_id != (-1) )
	{
		char memb_name[80];
		char *memb_obj;
		Pro_object_info memb_info;
		double x[3],y[3],z[3],origin[3];

		memb_obj = prodb_member_to_object( obj , memb_id );
		if( prodb_get_object_info( memb_obj , &memb_info ) )
		{
			pro_wstr_to_str( memb_name , memb_info.name );
			fprintf( fd_out , " member %s %x\n" , memb_name , memb_obj );

			if( prodb_member_transform( obj , memb_id , x , y , z , origin ) )
			{
				fprintf( fd_out , " matrix\n" );
				fprintf( fd_out , " %f %f %f 0.0\n" , x[0] , x[1] , x[2] );
				fprintf( fd_out , " %f %f %f 0.0\n" , y[0] , y[1] , y[2] );
				fprintf( fd_out , " %f %f %f 0.0\n" , z[0] , z[1] , z[2] );
				fprintf( fd_out , " %f %f %f 1.0\n" , origin[0] , origin[1] , origin[2] );
			}
		}
		memb_id = prodb_next_member( obj , memb_id );
	}
	fprintf( fd_out , "endassembly %s\n" , name );

	memb_id = prodb_first_member( obj );
	while( memb_id != (-1) )
	{
		char *memb_obj;

		memb_obj = prodb_member_to_object( obj , memb_id );
		Output_object( fd_out , memb_obj );
		memb_id = prodb_next_member( obj , memb_id );
	}
}

void
Output_object( fd_out , obj )
FILE *fd_out;
char *obj;
{
	Pro_object_info obj_info;
	char name[80];
	char type[10];
	char tmp_str[80];
	int nlayers;
	wchar_t **layer_array;
	int i;

	if( !prodb_get_object_info( obj , &obj_info ) )
	{
		promsg_print( MSGFIL , "NO_INFO_FOR_OBJECT" );
		return;
	}

	if( Object_already_output( obj ) )
		return;

	pro_wstr_to_str( name , obj_info.name );
	pro_wstr_to_str( type , obj_info.type );

#if 0
	nlayers = prolayer_get_names( obj, &layer_array );
	for( i=0 ; i<nlayers; i++ )
	{
		pro_wstr_to_str( tmp_str, layer_array[i] );
		printf( "%s is in layer %s\n" , name, tmp_str );
		free( layer_array[i] );
	}
#endif

	if( !strncmp( type , "ASM" , 3 ) )
	{
		Output_assembly( fd_out, obj, name );
	}
	else if( !strncmp( type , "PRT" , 3 ) )
	{
#if 0
		wchar_t material_name[PRODEV_NAME_SIZE];

		if( prodb_get_material_name( obj, material_name ) != PRODEV_NO_ERROR )
			printf( "Could not get material name for %s\n" , name );
		else
		{
			pro_wstr_to_str( tmp_str, material_name );
			printf( "\t%s is made of %s\n" , name, tmp_str );
		}
#endif
		Output_part( fd_out , obj , name );
	}
	else
	{
		promsg_print( MSGFIL , "UNKNOWN_OBJECT_TYPE" , type , name);
	}
}

int
proe_brl()
{
	FILE *fd_out;
	char *obj;
	Pro_object_info obj_info;
	char output_file[PRODEV_NAME_SIZE+4];
	wchar_t buff[256];
	int range[2];
	int unit_subtype;
	wchar_t unit_name[PRODEV_NAME_SIZE];
	double conv_factor;

	range[0] = 1;
	range[1] = 10;

	win_id = (-1);

	curr_win_id = pro_get_current_window();
	if( curr_win_id == (-1) )
	{
		fprintf( stderr , "Cannot get current window id\n" );
		return( 1 );
	}

	obj_root = (struct object_list *)NULL;

	obj = pro_get_current_object();
	if( !obj )
	{
		promsg_print( MSGFIL , "NO_CURRENT_OBJECT" );
		return( 1 );
	}

	if( !prodb_get_object_info( obj , &obj_info ) )
	{
		promsg_print( MSGFIL , "NO_INFO_FOR_OBJECT" );
		return( 1 );
	}

	if( !prodb_get_model_units( obj, LENGTH_UNIT, &unit_subtype, unit_name, &conv_factor ) )
	{
		promsg_print( MSGFIL, "NO_UNITS_FOR_MODEL" );
		conv_factor = 1.0;
	}
	else
		conv_factor *= 25.4;

	pro_wstr_to_str( output_file , obj_info.name );
	strcat( output_file , ".brl" );
	promsg_print( MSGFIL , "USER_GET_OUTPUT_FILE_NAME" , output_file );

	if( !promsg_getstring( buff , 255 ) )
		pro_wstr_to_str( output_file , buff );

	quality = DEFAULT_QUALITY;
	promsg_print( MSGFIL , "USER_GET_QUALITY" , &quality );

	if( promsg_getint( &quality , range ) )
		quality = DEFAULT_QUALITY;

	if( (fd_out = fopen( output_file , "w" )) == NULL )
	{
		promsg_print( MSGFIL , "OPEN_FILE_ERROR" , output_file );
		return( 1 );
	}

	fprintf( fd_out, "%g\n", conv_factor );

	Output_object( fd_out , obj );

	pro_set_current_window( curr_win_id );
	if( win_id != (-1) )
		pro_close_object_window( win_id );

	pro_refresh_window( -1 );

	fclose( fd_out );

	promsg_print( MSGFIL , "PROE_TO_BRL_DONE" );

	return( 0 );
}

int
user_initialize(argc, argv, version, build, errbuf)
int argc;
char *argv[];
char *version;
char *build;
wchar_t errbuf[80];
{
	if( pro_wchar_t_check ( sizeof (wchar_t) ) )
	{
		promsg_print( MSGFIL , "WCHAR_ERROR_MESSAGE" , sizeof(wchar_t),  pro_wchar_t_check ( sizeof (wchar_t) ) );
		return( -1 );
	}

	promenu_create( "export" , "intf_exprt.mnu" );
	promenu_expand( "export" , "intf_exprt.aux" );
	promenu_on_button( "export" , "BRL-CAD" , proe_brl , 0 , 0 );

	promsg_print( MSGFIL , "VERSION_BUILD" , version , build );
	return( 0 );
}

void
user_terminate()
{
	return;
}
