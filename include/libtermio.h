/*
 *			T E R M I O . H
 *
 *  Externs for the BRL-CAD library LIBTERMIO
 *
 *  Author -
 *	Gary S. Moss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 *
 *  Include Sequencing -
 *	#include "config.h"
 *	#include <stdio.h>
 *	#include "termio.h"
 *
 *  $Header$
 */

void clr_Cbreak( int fd );
void set_Cbreak( int fd );
void clr_Raw( int fd );
void set_Raw( int fd );
void set_Echo( int fd );
void clr_Echo( int fd );
void set_Tabs( int fd );
void clr_Tabs( int fd );
void set_HUPCL( int fd );
void clr_CRNL( int fd );
unsigned short get_O_Speed( int fd );
void save_Tty( int fd );
void reset_Tty( int fd );
int save_Fil_Stat( int fd );
int reset_Fil_Stat( int	fd );
int set_O_NDELAY( int fd );
void prnt_Tio();	/* misc. types of args */
