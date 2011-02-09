/*                C O M B _ B O O L _ S C A N . L
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file comb_bool_scan.l
 *
 *	LEX(1) specifiation to scan Boolean expressions for
 *			the 'c' command
 *
 */

/*	Regular definitions	*/
open		"("
close		")"
closeall	"]"
vanilla		[^\(\)\] \t\n]
white		[ \t\n]+

%%

{open}		{
		    return TKN_LPAREN;
		}
{close}		{
		    return TKN_RPAREN;
		}
{vanilla}+	{
		    if ((*yytext == *bool_op_lexeme[OPN_UNION])
		     && (BU_STR_EQUAL(yytext, bool_op_lexeme[OPN_UNION])))
			return TKN_UNION;
		    if ((*yytext == *bool_op_lexeme[OPN_INTERSECTION])
		     && (BU_STR_EQUAL(yytext, bool_op_lexeme[OPN_INTERSECTION])))
			return TKN_INTERSECTION;
		    if ((*yytext == *bool_op_lexeme[OPN_DIFFERENCE])
		     && (BU_STR_EQUAL(yytext, bool_op_lexeme[OPN_DIFFERENCE])))
			return TKN_DIFFERENCE;
		    else
		    {
			talloc(yylval.val_string, char, yyleng + 1);
			sprintf(yylval.val_string, yytext);
			return TKN_OBJECT;
		    }
		}
{white}		;

%%

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
