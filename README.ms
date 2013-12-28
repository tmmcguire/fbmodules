.\" FlexModule and BisonModule README file.
.\" Process with "groff -Tascii -P-u -P-b -ms README.ms" to get ASCII
.\" version, and "groff -Thtml -P-l -ms README.ms" to get a HTML
.\" version.

.TL
FlexBisonModule-2.0

.PP
.DS C
http://www.cs.utexas.edu/users/mcguire/software/fbmodule/
.DE

.PP
Use Flex and Bison to create Python modules for parsing strings.

.SH
NEWS

.PP
22 Mar 2002 - Releasing version 2.1.  Cleans up push_position in
FlexModule.h based on experience with APC and try to handle some
weird error rule behavior by Bison.

.PP
28 Jan 2002 - Releasing version 2.0.  Major upgrade, with complete
rewrite of FlexModule.h.  It no longer uses C++ and has several new
features including better position handling and inserting sub-files
into the token stream (ala C's #include).  BisonModule.h has much
better error handling, using FM's positions.


.SH
RATIONALE

.IP \(bu
Some people \fIlike\fR using Flex and Bison.  (Yes, I pity them, too.
(Actually, fooling with Flex is kind of fun, though.))

.IP \(bu
Flex and Bison are both reasonably standard, reasonably popular,
and pretty well documented.  (See O'Reilly's \fIlex & yacc\fR by John
R. Levine, Tony Mason and Doug Brown (preferrably the second edition,
which is much improved over the first) as well as the Flex and Bison
docs.)

.IP \(bu
Flex and Bison are pretty fast (in their own way, of course).  If
more speed is needed when using both, it should be possible to merge
the two modules into a single C module without changing any of the
scanning or grammar rules.

.IP \(bu
Flex and Bison are flexible.  If the speed of the Python program is
not sufficient, you'll have debugged grammar rules which can be used
to rewrite the program entirely in C.


.SH
REQUIREMENTS

.PP
This sofware has been tested with:

.IP \(bu
Flex 2.5.4
.IP \(bu
Bison 1.28
.IP \(bu
gcc 2.95.2 and 3.0.3
.IP \(bu
Python 1.5.2 and 2.0

.LP
under Linux.  BisonModule uses GCC's variable argument macros, if
nothing else, and therefore probably won't work with another C
compiler.  FlexModule uses Flex's buffer calls and will not work with
lex.


.SH
FILES

.IP BisonModule.h
C header file which should be included by a Bison
grammar specification.

.IP FlexModule.h
Similarly, a C header file used by a Flex scanner
specification.

.IP C++FlexModule.h
For reference, a copy of the original C++
FlexModule.  It does not follow the interface below.

.IP Makefile.pre.in
Modified version of Makefile.pre.in from Python
1.5.1; changed to add rules to run Flex and Bison to produce .c files.

.IP Setup.in
Template for a Setup.in file.

.IP Symbols.py
Sample Python code for Symbol (as in a non-terminal Bison
grammar symbol) and Token classes (a subclass of Symbol, for terminal
Flex symbols).

.IP example/hoc2
Example based on hoc from 
\fIThe UNIX Programming Environment.\fR


.SH
INSTALLATION

.PP
Copy BisonModule.h, FlexModule.h, Symbols.py, and Makefile.pre.in to
the directory where you will build the modules.  Create a Setup.in and
type 
.DS
make -f Makefile.pre.in boot
make
.DE


.SH
USE

.PP
The two .h files are used similarly, by including them into the Bison
or Flex input and adding some declarations and a macro call at the
end.

.SH 2
Compiling with FlexModule

.PP
The actual Flex rules for tokens are pretty much as normal for flex;
just return a unique token type integer.  For rules which do not
return, like whitespace (spaces, tabs, newlines, etc.) and comments,
call the macro ADVANCE:

.DS
ws      [ \et\en]+
 ...
{ws}    { ADVANCE; /* and skip */ }
.DE

.PP
To insert a sub-file into the token stream, use the PUSH_FILE macros:

.DS
str           \e"([^"\e\e\en]|\e\e.)*\e"
 ...
"input "{str} { PUSH_FILE_YYTEXT(7,yyleng-1); }
.DE

for things like 'input "file"'.  PUSH_FILE_YYTEXT's arguments are
slice-like offsets into yytext; the 7 is the length of 'input "' and
the yyleng-1 is the index of the last quote mark.

.PP
A PUSH_FILE_STRING macro takes a zero-terminated file name argument.
Both of these call ADVANCE to update the position.

.PP
At the end of the Flex input file, add two things: An array of
TokenValues, which provide a map between strings and the numerical
token types, and a call to the FLEXMODULEINIT macro.  For example:

.DS
 ...
%%

TokenValues module_tokens[] = {
        {"NUMBER", NUMBER},
        {"VAR", VAR},
        {0,0}
};
FLEXMODULEINIT(hoclexer, module_tokens);
.DE

where NUMBER and VAR are constants and "hoclexer" is the name of the
module.

.SH 2
Using FlexModule in Python

.PP
After importing the module, it gives access to the functions:

.IP "onstring(maketoken, string)"
begin scanning string
.IP "onfile(maketoken, file)"
begin scanning a file (name or object)
.IP "readtoken()"
read the next token, returning a pair consisting of
the token value and the object returned by maketoken.
.IP "lasttoken()"
re-call maketoken on the last token
.IP "close()"
free resources and stop scanning

.LP
and the dictionaries:

.IP names
a map between numeric types and the string names of the
tokens.  This is created from the TokenValues array.
.IP types
a map between string names and numeric types, also from
TokenValues.

.PP
maketoken should be a function with three parameters:
.IP  \(bu 
the type of the token, an integer
.IP  \(bu 
the text of the token, a string
.IP  \(bu 
the postion of the token

.PP
A position is a tuple of
.IP  \(bu 
a pair with the beginning line and column
.IP  \(bu 
a pair with the ending line and column
.IP  \(bu 
the filename
.IP  \(bu 
a list of tuples, giving the file name, line, and column of stacked,
yet-to-be finished positions.  The list does not include the current
position.

.SH 2
Compiling with BisonModule

.PP
Each call to the readtoken and makesymbol functions below should
return a pair (like the readtoken function described above) of the
integer symbol type and the symbol object.  The only requirements of
the object are the methods "append" (for use by the REDUCELEFT macro)
and "insert" (for use by the REDUCERIGHT macro).  These objects are
passed around by the rules to build a parse tree, where each
non-terminal symbol has a list of child symbols.

.PP
The grammar file read by Bison is fairly normal, but the code associated
with the rules should be fairly limited:

.IP  \(bu 
The start rule of the grammar should, when reduced, call the
RETURNTREE macro with the value of the top of the tree.  The argument
of RETURNTREE is the symbol object that represents the top of the parse
tree.  For example:

.DS C
start:   list  { RETURNTREE($1); }
.DE

where start is the initial symbol, sets the parse tree to the list
symbol.

.IP  \(bu 
A rule can call the REDUCE macro to create a new non-terminal
symbol object.  The first argument to REDUCE should be a numerical
type for the symbol; the remaining arguments will be appended to the
new symbol object's children list.  For example:

.DS C
list:    /* nothing */  { $$ = REDUCE(LIST); }
.DE

(where LIST is an integer constant) creates a LIST symbol with no
children and passes it up the tree.

.IP  \(bu 
A rule can call the REDUCELEFT macro.  This macro does not create a
new symbol, but adds the second and subsequent arguments to its first
argument's children list.  This macro is useful for left-recursive rules
and for reusing lower level symbols.  For example,

.DS C
| list expr '\en' { $$ = REDUCELEFT($1, $2); }
.DE

adds an expression to an existing list of expressions.  Also,

.DS C
expr: expr '+' expr { $$ = REDUCELEFT($2, $1, $3); }
.DE

reuses the '+' token returned by the scanner, adding the two
subexpressions as its children.

The APPEND macro is a synonym for REDUCELEFT for use in the first
case.

.IP  \(bu 
There are also REDUCERIGHT and PREPEND macros, which prepends its
arguments, but it hasn't been tested since I have not used
right-recursive rules.

.IP  \(bu 
A REDUCEERROR macro is available to handle syntax errors.  It
creates a SYNTAXERROR symbol (whose numerical type is -1), which can
be incorporated into the parse tree like any other symbol.  Python
code can subsequently walk the tree to report syntax errors.  For
example,

.DS C
| list error '\en' { $$ = REDUCELEFT($1, REDUCEERROR); }
.DE

creates a syntax error symbol and adds it to a list of expressions.

The SYNTAXERROR symbols are created with special arguments.  The
makesymbol function gets passed a -1 as the type, and a list
consisting of the last token object read from the scanner (which
should include its location), and a string error message from Bison.
This error message may or may not be useful, as in something other
than "parse error".

.PP
Like FlexModule, a BisonModule needs an array associating numeric
types and strings and a final macro call to set everything up:

.DS
static SymbolValues module_symbols[] = {
        {"LIST", LIST},
        {0,0}
};
BISONMODULEINIT(hocgrammar, module_symbols);
.DE

.SH 2
Using BisonModule in Python

.PP
Each Bison module exports into Python:

.IP ParserError
an exception object, used when the parser cannot handle
a syntax error in the input.  (In general, for good error handling, I
am given to understand that this should not occur and thus this
exception should not be thrown.  It won't be if all syntax errors are
handled by error rules calling the REDUCEERROR macro.)

.IP "parse(makesymbol, readtoken)"
a function, which takes two functional arguments:  
a makesymbol function to create
symbols similar to the maketoken function above and a readtoken
function to return token pairs.  It returns the object set by
RETURNTREE.

The makesymbol function should match the Symbols.Symbol constructor in
takeing a type, and a list of children.  The readtoken function should
return a pair of token type and object.

.IP "names and types"
dictionaries, like FlexModule above.

.IP debug()
a function which toggles the Bison parser's debug flag.


.SH
BUGS

.PP
Ok, so I don't really understand Bison error handling.  If someone
could explain it to me (use small words, I'm not too bright) in such a
way as to improve the error handling of BisonModule, I'd appreciate
it.  (Version 2.0: I'm getting better.)

.PP
Neither FlexModule nor BisonModule appears to leak memory when I've
tested it, but the behavior of BisonModule when it throws a
ParserError is not entirely well tested.

.PP
The older FlexModule used Flex's C++ support to create more than one
scanner at a time.  The current one can't.  Sorry.


.SH
FURTHER INFORMATION

.PP
For more information, the code, and an example or two, see
.DS C
http://www.cs.utexas.edu/users/mcguire/software/fbmodule
.DE

.DS
Tommy M. McGuire
mcguire@cs.utexas.edu
.DE
