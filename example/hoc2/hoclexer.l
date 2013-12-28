/*
	hoclexer.l -- Create Python scanner module for hoc

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
	The lexer used in the original hoc2 from _The UNIX Programming
	Environment_, by Brian W. Kernighan and Rob Pike, was written
	directly in C.  To illustrate FlexModule, it has been rewritten
	using Flex.
*/

%{
#include "hocgrammar.h"
#include "FlexModule.h"
%}

num	[0-9]*\.?[0-9]+
id	[a-z]
ws	[ \t]+
str	\"([^"\\\n]|\\.)*\"

%%

"input "{str}			{ PUSH_FILE_YYTEXT(7,yyleng-1); }
{num}				{ return(NUMBER); }
{id}				{ return(VAR); }
{ws}				{ ADVANCE; /* and skip */ }
.|\n				{ return(yytext[0]); }

%%

TokenValues module_tokens[] = {
	{"NUMBER", NUMBER},
	{"VAR", VAR},
	{0,0}
};

FLEXMODULEINIT(hoclexer, module_tokens);
