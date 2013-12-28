%{
/*
	hocgrammar.l -- Create Python parser module for hoc

        Copyright (c) 2002 by Tommy M. McGuire

        Permission is hereby granted, free of charge, to any person
        obtaining a copy of this software and associated documentation
        files (the "Software"), to deal in the Software without
        restriction, including without limitation the rights to use,
        copy, modify, merge, publish, distribute, sublicense, and/or
        sell copies of the Software, and to permit persons to whom
        the Software is furnished to do so, subject to the following
        conditions:

        The above copyright notice and this permission notice shall be
        included in all copies or substantial portions of the Software.

        THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
        KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
        WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
        AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
        HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
        WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
        FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
        OTHER DEALINGS IN THE SOFTWARE.

	Please report any problems to mcguire@cs.utexas.edu.

	This is version 2.0.
*/

/*
	This grammar is taken from _The UNIX Programming Environment_
	by Brian W. Kernighan and Rob Pike, Chapter 8, for the
	program hoc2.  Some changes have been made to the grammar
	to better work with BisonModule.
*/
#include "BisonModule.h"

	/* Symbol types */
#define LIST	1000
%}

	/* Note that this doesn't set any kind of type declaration for
	   the symbols.  They'll all be Python objects.  By the way,
	   this also handles operator precedence.  */
%token NUMBER
%token VAR
%right '='
%left '+' '-'
%left '*' '/'
%left UNARYMINUS

	/* Just to be unambiguous, start with "start". */
%start start

%%

	/* This rule provides a place to hang the RETURNTREE call,
	   which sets the root of the parse tree. */

start:		list			{ RETURNTREE($1); }

	/* The list symbol is initially created by the first branch
	   and expressions and syntax errors are added to it by the
	   third and fourth branches. */
list:		/* nothing */		{ $$ = REDUCE(LIST); }
		| list '\n'		{ $$ = $1; }
		| list expr '\n'	{ $$ = REDUCELEFT($1, $2); }
		| list error '\n'	{ $$ = REDUCELEFT($1, REDUCEERROR); }
		;

	/* An expr is a number, a variable reference, an assignment,
	   or one of the operators.  In these cases, the appropriate
	   token is reused as the symbol for the expression. */
expr:		NUMBER			{ $$ = $1; }
		| VAR			{ $$ = $1; }
		| VAR '=' expr		{ $$ = REDUCELEFT($2, $1, $3); }
		| expr '+' expr		{ $$ = REDUCELEFT($2, $1, $3); }
		| expr '-' expr		{ $$ = REDUCELEFT($2, $1, $3); }
		| expr '*' expr		{ $$ = REDUCELEFT($2, $1, $3); }
		| expr '/' expr		{ $$ = REDUCELEFT($2, $1, $3); }
		| '(' expr ')'		{ $$ = $2; }
		| '-' expr %prec UNARYMINUS
					{ $$ = REDUCELEFT($1, $2); }
		;

%%

static SymbolValues module_symbols[] = {
	{"LIST", LIST},
	{0,0}
};

BISONMODULEINIT(hocgrammar, module_symbols);
