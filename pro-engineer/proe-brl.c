#include <unistd.h>
#include "prodevelop.h"

extern int errno;

static int quality;

static	wchar_t		  msgfil[PRODEV_NAME_SIZE];
#define MSGFIL		  pro_str_to_wstr(msgfil,"usermsg.txt")

#define	FILE_COPY_SIZE	2048
#define DEFAULT_QUALITY 3

struct object_list
{
	Pro_object_info info;
	struct object_list *next;
} *obj_root;

void
Copy_obj_info( to , from )
Pro_object_info *to,*from;
{
	int i;

	for( i=0 ; i<80 ; i++ )
		to->name[i] = from->name[i];

	for( i=0 ; i<10 ; i++ )
		to->type[i] = from->type[i];

	for( i=0 ; i<256 ; i++ )
		to->path[i] = from->path[i];

	for( i=0 ; i<20 ; i++ )
		to->device[i] = from->device[i];

	for( i=0 ; i<80 ; i++ )
		to->host[i] = from->host[i];

	to->version = from->version;
}

int
Same_object( info1 , info2 )
Pro_object_info *info1,*info2;
{
	char buf1[1024],buf2[1024];

	pro_wstr_to_str( buf1 , info1->name );
	pro_wstr_to_str( buf2 , info2->name );
	if( strcmp( buf1 , buf2 ) )
		return( 0 );

	pro_wstr_to_str( buf1 , info1->type );
	pro_wstr_to_str( buf2 , info2->type );
	if( strcmp( buf1 , buf2 ) )
		return( 0 );

	pro_wstr_to_str( buf1 , info1->path );
	pro_wstr_to_str( buf2 , info2->path );
	if( strcmp( buf1 , buf2 ) )
		return( 0 );

	pro_wstr_to_str( buf1 , info1->device );
	pro_wstr_to_str( buf2 , info2->device );
	if( strcmp( buf1 , buf2 ) )
		return( 0 );

	pro_wstr_to_str( buf1 , info1->host );
	pro_wstr_to_str( buf2 , info2->host );
	if( strcmp( buf1 , buf2 ) )
		return( 0 );

	if( info1->version != info2->version )
		return( 0 );

	return( 1 );
}

int
Object_already_output( info )
Pro_object_info *info;
{
	int return_val;
	struct object_list *objl,*prev;
	char buf[1024];

	if( obj_root == (struct object_list *)NULL )
	{
		obj_root = (struct object_list *)malloc( sizeof( struct object_list ) );
		obj_root->next = (struct object_list *)NULL;
		Copy_obj_info( &obj_root->info , info );
		return_val = 0;
	}
	else
	{
		objl = obj_root;
		prev = (struct object_list *)NULL;
		while( objl != (struct object_list *)NULL && !Same_object( &objl->info , info ) )
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
			Copy_obj_info( &objl->info , info );
		}
		else
			return_val = 1;
	}

	return( return_val );
}

void
Output_part( fd_out , obj , name , type , path )
FILE *fd_out;
char *obj;
char *name,*type,*path;
{
	char tmp_file_name[256];
	wchar_t wtmp_file_name[256];
	FILE *fd_tmp;
	char *copy_buf[FILE_COPY_SIZE];
	int copy_len;
	int i,name_len;
	int c;

	/* Create a temporary file to store the render format for this part */
	strcpy( tmp_file_name , tempnam( NULL , "proe_brl" ) );

	/* It appears that pro_export_file_from_pro changes the file name given to all
	 * lower case, so change them first
	 */
	i = (-1);
	while( tmp_file_name[++i] != '\0' )
		tmp_file_name[i] = tolower( tmp_file_name[i] );

	pro_str_to_wstr( wtmp_file_name , tmp_file_name );

	/* Output the part in render format to the temporary file */
	pro_export_file_from_pro( wtmp_file_name , obj , PRO_RENDER_FILE , NULL , &quality , NULL , NULL );

	if( (fd_tmp = fopen( tmp_file_name , "r" )) == NULL )
	{
		fprintf( stderr , "Cannot open temporary file (%s) for reading" , tmp_file_name );
		perror( "proe-brl" );
		promsg_print( MSGFIL , "CANNOT_OPEN_TMP_FILE" , tmp_file_name );
		return;
	}

	/* write the start of the render format to the final output file */
	fprintf( fd_out , "solid %s %s %s\n" , name , type , path );

	/* skip the first line in the temporary file (similar to above line, but less info */
	while( (c=fgetc( fd_tmp )) != '\n' && c != EOF );

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
Output_object( fd_out , obj )
FILE *fd_out;
char *obj;
{
	Pro_object_info obj_info;
	char name[80];
	char type[10];
	char path[256];

	if( !prodb_get_object_info( obj , &obj_info ) )
	{
		promsg_print( MSGFIL , "NO_INFO_FOR_OBJECT" );
		return;
	}

	if( Object_already_output( &obj_info ) )
		return;

	pro_wstr_to_str( name , obj_info.name );
	pro_wstr_to_str( type , obj_info.type );
	pro_wstr_to_str( path , obj_info.path );

	if( !strncmp( type , "ASM" , 3 ) )
	{
		int memb_id;

		fprintf( fd_out , "assembly %s %s %s\n" , name , type , path );
		memb_id = prodb_first_member( obj );
		while( memb_id != (-1) )
		{
			char memb_name[80];
			char memb_type[10];
			char memb_path[256];
			char *memb_obj;
			Pro_object_info memb_info;
			double x[3],y[3],z[3],origin[3];

			memb_obj = prodb_member_to_object( obj , memb_id );
			if( prodb_get_object_info( memb_obj , &memb_info ) )
			{
				pro_wstr_to_str( memb_name , memb_info.name );
				pro_wstr_to_str( memb_type , memb_info.type );
				pro_wstr_to_str( memb_path , memb_info.path );
				fprintf( fd_out , " member %s %s %s\n" , memb_name , memb_type , memb_path );

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
	else if( !strncmp( type , "PRT" , 3 ) )
	{
		Output_part( fd_out , obj , name , type , path );
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

	range[0] = 1;
	range[1] = 10;

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

	pro_wstr_to_str( output_file , obj_info.name );
	strcat( output_file , ".brl" );
	promsg_print( MSGFIL , "USER_GET_OUTPUT_FILE_NAME" , output_file );

	if( !promsg_getstring( buff , 255 ) )
		pro_wstr_to_str( output_file , buff );

fprintf( stderr , "filename = %s\n" , output_file );

	quality = DEFAULT_QUALITY;
	promsg_print( MSGFIL , "USER_GET_QUALITY" , &quality );
fprintf( stderr , "Printed message for quality\n" );
	if( promsg_getint( &quality , range ) )
		quality = DEFAULT_QUALITY;
fprintf( stderr , "Quality set to %d\n" , quality );

	if( (fd_out = fopen( output_file , "w" )) == NULL )
	{
		promsg_print( MSGFIL , "OPEN_FILE_ERROR" , output_file );
		return( 1 );
	}

	Output_object( fd_out , obj );

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
