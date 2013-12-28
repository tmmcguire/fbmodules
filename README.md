fbmodules
=========

FlexModule and BisonModule: create Python modules from flex-based lexical scanners and bison-based parsers

## News

28 Dec 2013 - Moved to Github and re-released for Python 2.7.4. Note that the code itself has not received any particular attention, although I have verified that it appears to work.

22 Mar 2002 - Releasing  version  2.1.   Cleans  up push_position  in  FlexModule.h based on experience with APC and try to handle some weird error rule behavior by Bison.

28 Jan 2002 - Releasing version  2.0.   Major  upgrade, with  complete  rewrite  of FlexModule.h.  It no longer uses C++ and has several new features including  better  position handling  and inserting sub-files into the token stream (ala C's #include).  BisonModule.h has  much  better  error  handling, using FM's positions.

## RATIONALE

* Some people *like* using Flex and Bison.  (Yes, I pity them, too. (Actually, fooling with Flex is kind of fun, though.))

* Flex and bison are both reasonably standard, reasonably popular, and pretty well documented.  (See O'Reilly's *lex & yacc* by John R. Levine, Tony Mason and Doug Brown (preferably the second edition, which is much improved over the first) as well as the Flex and Bison docs.)

* Flex and bison are pretty fast (in their own way, of course).  If more speed is needed when using both, it should be possible to merge the two modules into a single C module without changing any of the scanning or grammar rules.

* Flex and bison are flexible.  If the speed of the Python program is not sufficient, you'll have debugged grammar rules which can be used to rewrite the program entirely in C.

## Requirements

This software has been tested with:

* Flex *2.5.4* and 2.5.35
* Bison *1.28* and 2.5
* gcc *2.95.2*, *3.0.3*, and 4.7.3
* Python *1.5.2*, *2.0*, and 2.7.4

under Linux.  BisonModule uses GCC's variable argument macros, if nothing else, and therefore probably won't work with another C compiler.  FlexModule uses flex's buffer calls and will not work with lex.

Version numbers in italics are ridiculously out of date, but represent the last time the modules received any serious love.

## Files

* **BisonModule.h** C header file which should be included by a Bison grammar specification.

* **FlexModule.h** Similarly, a C header file used by a Flex scanner specification.

* **Symbols.py** Sample Python code for Symbol (as in a non-terminal Bison grammar symbol) and Token classes (a subclass of Symbol, for terminal Flex symbols).

* **example/hoc2** Example based on hoc from  *The UNIX Programming Environment* by Brian Kernighan and Rob Pike.

## Installation

Copy **BisonModule.h**, **FlexModule.h**, and **Symbols.py** to the directory where you will build the modules.  Create a **setup.py** based on the lexer and grammar files, then run

    python setup.py build

## Use

The two .h files are used similarly, by including them into the bison or flex input and adding some declarations and a macro call at the end.

The following two sections describe how to use FlexModule from the viewpoint of the flex file (which is in C, remember) and the viewpoint of the Python user of the resulting module (i.e., in Python). The subsequent two sections describe how to use BisonModule from the viewpoint of the bison file (which is in C again) and the viewpoint of the Python user of the resulting module (i.e., in Python).

### Compiling with FlexModule

The actual flex rules for tokens are pretty much as normal for flex; just return a unique token type integer (here, `NUMBER` is defined by the bison grammar in the normal way).

    num     [0-9]*\.?[0-9]+
    ... 
    {num}   { return(NUMBER); }


For rules which do not return, like whitespace (spaces, tabs, newlines, etc.) and comments, call the macro **ADVANCE**:

    ws     [ \t\n]+ 
    ... 
    {ws}   { ADVANCE; /* and skip */ }
    
To insert a sub-file into the token stream, use the **PUSH_FILE** macros:

    str \"([^"\\\n]|\\.)*\" 
    ... 
    "input "{str} { PUSH_FILE_YYTEXT(7,yyleng-1); }

for a construction like ’input "file"’. **PUSH_FILE_YYTEXT**’s arguments are slice-like offsets into `yytext`; the 7 is the length of ’input "’ and the `yyleng-1` is the index of the last quote mark. A **PUSH_FILE_STRING** macro takes a zero-terminated file name argument. Both of these call `ADVANCE` to update the position.

At the end of the flex input file, add two things: An array of **TokenValues**, which provide a map between strings and the numerical token types, and a call to the **FLEXMODULEINIT** macro. For example:

    ... 
    %%
    TokenValues module_tokens[] = { 
    {"NUMBER", NUMBER}, 
    {"VAR", VAR}, 
    {0,0} 
    }; 
    FLEXMODULEINIT(hoclexer, module_tokens);

where `NUMBER` and `VAR` are constants and `hoclexer` is the name of the module.

See *example/hoc2/hoclexer.l* for a reasonably complete example.

### Using FlexModule in Python

After importing the module, it gives access to the functions:

* **onstring(maketoken, string)** begin scanning string
* **onfile(maketoken, file)** begin scanning a file (name or object)
* **readtoken()** read the next token, returning a pair consisting of the token value and the object returned by `maketoken`.
* **lasttoken()** re-call `maketoken` on the last token
* **close()** free resources and stop scanning

and the dictionaries:

* **names** a map between numeric types and the string names of the tokens. This is created from the `TokenValues` array.
* **types** a map between string names and numeric types, also from `TokenValues`.

**maketoken** should be a function with three parameters:

* the type of the token, an integer
* the text of the token, a string
* the position of the token

A position is a tuple of:

* a pair with the beginning line and column
* a pair with the ending line and column
* the filename
* a list of tuples, giving the file name, line, and column of stacked, yet-to-be finished positions, created by the **PUSH_FILE** macros. The list does not include the current position.

### Compiling with BisonModule

Each call to the **readtoken** and **makesymbol** functions below should return a pair (like the **readtoken** function described above) of the integer symbol type and the symbol object. The only requirements of the object are the methods `append` (for use by the **REDUCELEFT** macro) and `insert` (for use by the **REDUCERIGHT** macro). These objects are passed around by the rules to build a parse tree, where each non-terminal symbol has a list of child symbols.

The grammar file read by Bison is fairly normal, but the code associated with the rules should be fairly limited:

* The start rule of the grammar should, when reduced, call the **RETURNTREE** macro with the value of the top of the tree. The argument of `RETURNTREE` is the symbol object that represents the top of the parse tree. For example:

        start: list { RETURNTREE($1); }

    where `start` is the initial symbol, sets the parse tree to the list symbol.

* A rule can call the **REDUCE** macro to create a new non-terminal symbol object. The first argument to `REDUCE` should be a numerical type for the symbol; the remaining arguments will be appended to the new symbol object’s children list. For example:

        list: /* nothing */ { $$ = REDUCE(LIST); }

    (where `LIST` is an integer constant) creates a `LIST` symbol with no children and passes it up the tree.

* A rule can call the **REDUCELEFT** macro. This macro does not create a new symbol, but adds the second and subsequent arguments to its first argument’s children list. This macro is useful for left-recursive rules and for reusing lower level symbols. For example,

        | list expr ’\n’ { $$ = REDUCELEFT($1, $2); }
    
    adds an expression to an existing list of expressions. Also,

        expr: expr ’+’ expr { $$ = REDUCELEFT($2, $1, $3); }
    
    reuses the ’+’ token returned by the scanner, adding the two subexpressions as its children.

    The **APPEND** macro is a synonym for `REDUCELEFT` for use in the first case.

* There are also **REDUCERIGHT** and **PREPEND** macros, which prepends its arguments, but it has not been tested since I have not used right-recursive rules.

* A **REDUCEERROR** macro is available to handle syntax errors. It creates a **SYNTAXERROR** symbol (whose numerical type is -1), which can be incorporated into the parse tree like any other symbol. Python code can subsequently walk the tree to report syntax errors. For example,

        | list error ’\n’ { $$ = REDUCELEFT($1, REDUCEERROR); }
        
    creates a syntax error symbol and adds it to a list of expressions.
    
    The `SYNTAXERROR` symbols are created with special arguments. The `makesymbol` function gets passed a -1 as the type, a list consisting of the last token object read from the scanner (which should include its location), and a string error message from bison. This error message may or may not be useful, as in something other than "parse error".
    
Like FlexModule, a BisonModule needs an array associating numeric types and strings and a final macro call to set everything up:

    static SymbolValues module_symbols[] = { 
    {"LIST", LIST}, 
    {0,0} 
    }; 
    BISONMODULEINIT(hocgrammar, module_symbols);

### Using BisonModule in Python

Each Bison module exports into Python:

* **parse(makesymbol, readtoken)**

    A function which takes two functional arguments: a `makesymbol` function to create symbols similar to the `maketoken` function above and a `readtoken` function to return token pairs. It returns the object set by `RETURNTREE`.
    
    The `makesymbol` function should match the **Symbols.Symbol** constructor in taking a type and a list of children. The `readtoken` function should return a pair of token type and object.
    
* `names` and `types` dictionaries, like FlexModule above.

* **debug()**

    A function which toggles the bison parser’s debug flag.

* **ParserError**

    An exception object used when the parser cannot handle a syntax error in the input. (In general, for good error handling, I am given to understand that this should not occur and thus this exception should not be thrown. It won’t be if all syntax errors are handled by error rules calling the `REDUCEERROR` macro.)

## Bugs

Ok, so I don’t really understand Bison error handling. If someone could explain it to me (use small words, I’m not too bright) in such a way as to improve the error handling of BisonModule, I’d appreciate it. (As of Version 2.0, I’m getting better.)

Neither FlexModule nor BisonModule appears to leak memory when I have tested it, but the behavior of BisonModule when it throws a `ParserError` is not entirely well tested.

The original FlexModule used flex's C++ support to create more than one scanner at a time. The current one can’t. Sorry.

## Further information

These components were written to support the [Austin Protocol Compiler](http://www.crsr.net/Software/APC.html), and have not used much outside of it by me. If you wish to look at that for a more complete example, please remember that it *has not* been updated recently.

Tommy M. McGuire

mcguire@crsr.net (née mcguire@cs.utexas.edu)
