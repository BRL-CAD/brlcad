/*									*/
/*		C O M B _ B O O L _ S C A N . L				*/
/*									*/
/*	LEX(1) specifiation to scan Boolean expressions for		*/
/*			the 'c' command					*/
/*									*/
/*  Author -								*/
/*	Paul Tanenbaum							*/
/*									*/
/*  Source -								*/
/*	The U. S. Army Research Laboratory				*/
/*	Aberdeen Proving Ground, Maryland  21005-5068  USA		*/
/*									*/
/*									*/

/*	Regular definitions	*/
open		"("
close		")"
closeall	"]"
vanilla		[^\(\)\] \t\n]
white		[ \t\n]+

%%

{open}		{
		    return (TKN_LPAREN);
		}
{close}		{
		    return (TKN_RPAREN);
		}
{vanilla}+	{
		    if ((*yytext == *bool_op_lexeme[OPN_UNION])
		     && (strcmp(yytext, bool_op_lexeme[OPN_UNION]) == 0))
			return (TKN_UNION);
		    if ((*yytext == *bool_op_lexeme[OPN_INTERSECTION])
		     && (strcmp(yytext, bool_op_lexeme[OPN_INTERSECTION]) == 0))
			return (TKN_INTERSECTION);
		    if ((*yytext == *bool_op_lexeme[OPN_DIFFERENCE])
		     && (strcmp(yytext, bool_op_lexeme[OPN_DIFFERENCE]) == 0))
			return (TKN_DIFFERENCE);
		    else
		    {
			talloc(yylval.val_string, char, yyleng + 1);
			sprintf(yylval.val_string, yytext);
			return (TKN_OBJECT);
		    }
		}
{white}		;

%%
