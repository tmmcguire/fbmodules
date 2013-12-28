/*
	BisonModule.h -- Create Python modules from Bison parsers

        Copyright (c) 2000 by Tommy M. McGuire

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

#include <Python.h>

#define YYSTYPE PyObject *
YYSTYPE yylval;
int yydebug;
int yyparse(void);

static PyObject **symbolbuffer = NULL;	/* Buffer used to store
					   symbols while generating
					   parse tree */
static int maxslot = 1;			/* Current size of buffer */
static int slot;			/* Current index into buffer */

/*
 * Toolkits for using Bison (or any yacc) are a little difficult.
 * There are no really good automatic hooks to execute when a rule is
 * reduced that provide access to the non-terminals on the
 * right-hand-side of the rule---that has to be provided by the
 * grammar writer using the $-notation.  Unfortunately that results in
 * losing the non-terminals that were not chosen.
 *
 * The way out taken here is to notice that there are two sources of
 * symbols (terminals and non-terminals) being created: first, the
 * tokens that are read from the scanner, and second, the rules that
 * are reduced.  This file creates a buffer of symbols and inserts
 * into the buffer all of the tokens as they are read.  When a rule is
 * reduced, creating a non-terminal symbol, that is also inserted into
 * the buffer.  The buffer "owns" the symbols, in the Python sense
 * that the pointers in the buffer are "new" rather than borrowed.
 * The pointers returned to the parser and used in the $-notation are
 * "borrowed" from the buffer.
 *
 * When a rule is reduced, the children that are chosen by the grammar
 * writer using the $-notation are inserted into the new parent
 * symbol, thus creating a "new" pointer to those children that is
 * owned by the parent.
 *
 * When the whole parse tree is finished, another "new" pointer to the
 * root of the tree is saved.  Then, the buffer is deleted, by
 * DECREFing all the symbols in the buffer.  The symbols that are not
 * part of the tree are freed, and those that are part of tree are
 * returned.
 *
 * From the reference count standpoint, a token entering the parser
 * from the scanner has a count of 1 (the original pointer to it);
 * this reference is taken over by the buffer.  If that token is
 * incorporated into the tree by a REDUCE macro, its count is
 * increased to 2 (the pointer owned by the buffer and the pointer
 * owned by the parent non-terminal symbol).  When the buffer is
 * flushed, the count is reduced to 1, and the token is returned.  If
 * the token is not incorporated into the tree, its count remains 1
 * until the buffer is flushed, then is reduced to 0 and the token is
 * freed.
 */

/*
 * Buffer management: Initialize and clear buffer
 *
 * The buffer will be dynamically resized below as needed; it is
 * initially small.
 */
static void
clearbuffer (void)
{
  if (!symbolbuffer) {		/* Allocate initial symbolbuffer */
    symbolbuffer = (PyObject **) malloc(sizeof(PyObject *) * maxslot);
  }
  if (!symbolbuffer) {
    PyErr_NoMemory();
    return;
  }
  for (slot = 0; slot < maxslot; slot++) {
    symbolbuffer[slot] = 0;	/* All initial entries are 0 */
  }
  slot = 0;			/* Reset the next slot to be used */
}

/*
 * Buffer management: DECREF symbols in buffer and clear buffer
 *
 * The buffer initially owns references to all of the symbols inserted
 * into it.  These symbols are linked into a parse tree by the "reduce"
 * functions below, which generates extra references between them.
 * Setparsetree (below) creates a reference to the top of the parse
 * tree.  Flushing the buffer after this clears the references owned by
 * the buffer, leaving only the references in the parse tree and freeing
 * any symbols that are not linked into the tree.
 */
static void
flushbuffer (void)
{
  if (!symbolbuffer) {
    return;
  }
  for (slot = 0; slot < maxslot; slot++) {
    if (symbolbuffer[slot]) {
      Py_DECREF(symbolbuffer[slot]);
      symbolbuffer[slot] = 0;
    }
    else {
      break;			/* All used slots are first in buffer */
    }
  }
  slot = 0;
}

/*
 * Buffer management: Insert a symbol into the buffer
 * 
 * If the buffer is out of space, dynamically resize it.
 * 
 * The buffer takes over the reference from the calling function.
 */
static void
buffersymbol (PyObject * symb)
{
  if (!symbolbuffer) {
    return;
  }
  while (slot >= maxslot) {	/* Resize the buffer */
    int i;
    maxslot *= 2;
    symbolbuffer =
      (PyObject **) realloc(symbolbuffer, sizeof(PyObject *) * maxslot);
    if (!symbolbuffer) {
      PyErr_NoMemory();
      slot = 0;
      return;
    }
    for (i = slot; i < maxslot; i++) {
      symbolbuffer[i] = 0;	/* Zero the new slots */
    }
  }
  symbolbuffer[slot] = symb;	/* Put the symbol in the buffer */
  slot++;			/* and go to the next slot */
  return;
}

/*
 * Parser data.
 */
static PyObject *ParserError = NULL;	/* Exception raised by parser */

static PyObject *makesymbol = NULL;	/* Function to create symbols */
static PyObject *readtoken = NULL;	/* Function to generate tokens */

static PyObject *lasttoken = NULL;      /* Last token read from scanner */
static PyObject *errtoken = NULL;       /* Last token when yyerror called */
static char *errmsg = NULL;	        /* Bison-generated error message */
static PyObject *errsymb = NULL;        /* Most recent error symbol */

static PyObject *parsetree = NULL;	/* Top node of parse tree */

#define SYNTAXERROR -1			/* Syntax error symbol type */

/*
 * Clear the existing parse tree reference and create a new one
 */
static void
setparsetree (PyObject * symbol)
{
  Py_XDECREF (parsetree);
  parsetree = symbol;
  Py_INCREF (parsetree);
}

/*
 * Create a new symbol and insert it into the buffer
 *
 * This function is called from the grammar rules, and sets the new
 * symbol's children to the arguments of this function.  These
 * arguments will be the "$" things from the grammar, which are
 * non-owned, non-authoritative references.  Thus are created the
 * owned references between the nodes of the tree.  The returned
 * value is a non-authoritative reference to the new symbol.
 */
static PyObject *
reduce (int symboltype, ...)
{
  va_list args;
  PyObject *list, *ob;
  list = PyList_New (0);	/* Create the list of children */
  if (!list) {
    Py_INCREF(Py_None);
    return Py_None;
  }
  va_start(args, symboltype);	/* Put the children in the list */
  for (ob = va_arg (args, PyObject *); ob; ob = va_arg (args, PyObject *)) {
    PyList_Append(list, ob);
  }
  va_end(args);
				/* Call makesymbol */
  ob = PyObject_CallFunction(makesymbol, "iO", symboltype, list);
  Py_DECREF (list);		/* Free the list */
  if (!ob) {
    Py_INCREF(Py_None);
    ob = Py_None;
  }
  buffersymbol(ob);
  return ob;
}

/*
 * Add any arguments to the end of the children of the first argument.
 *
 * Both this function and the next treat the existing symbol as a list
 * (by calling insert and append).
 * 
 * This rule handles left-recursion in the grammar and using
 * an existing symbol as an interior node of the tree.
 */
static PyObject *
reduceleft (PyObject * listsymbol, ...)
{
  va_list args;
  PyObject *ob;
  va_start(args, listsymbol);
				/* Append each argument to the list */
  for (ob = va_arg(args, PyObject *); ob; ob = va_arg(args, PyObject *)) {
    PyObject_CallMethod(listsymbol, "append", "O", ob);
  }
  va_end(args);
  return listsymbol;
}

/*
 * Add any arguments to the start of the children of the first arg
 * 
 * This rule handles right-recursion and is currently untested.  It
 * will probably add them in reverse order.
 */
static PyObject *
reduceright (PyObject * listsymbol, ...)
{
  va_list args;
  PyObject *ob;
  va_start(args, listsymbol);
				/* Prepend each argument to the list */
  for (ob = va_arg(args, PyObject *); ob; ob = va_arg(args, PyObject *)) {
    PyObject_CallMethod(listsymbol, "insert", "iO", 0, ob);
  }
  va_end(args);
  return listsymbol;
}

/*
 * Function needed by Bison-generated parser.  Defining
 * YYERROR_VERBOSE sometimes makes the string interesting, if not
 * necessarily useful.
 */
static void
yyerror (char *s)
{
  if (errmsg) { free(errmsg); }	/* Free an existing error message */
				/* and copy the new one */
  errmsg = malloc(strlen(s) + 1);
  if (!errmsg) {
    PyErr_NoMemory();
  } else {
    strcpy(errmsg, s);
  }
  errtoken = lasttoken;		/* Save a copy of the last token */
}
#define YYERROR_VERBOSE 1

/*
 * For error rules, create a syntax error token
 * 
 * The first child of the error will be the last token seen from the
 * scanner; hopefully that will be somewhere near the syntax error.
 * The second child will be a string, describing the error message,
 * maybe.
 *
 * Bison appears to be repeatedly reducing the error rule as it
 * discards tokens until it finds one that is acceptable.  When it
 * does so, errmsg is null.  Also, having a grammar action "$$ =
 * ...REDUCEERROR..." repeatedly overwrites the $$ stack position.
 * So I am going out on an limb here and return the previous error
 * symbol if errmsg is not set.
 */
static PyObject *
reduceerror (void)
{
  if (!errmsg && errsymb) {
    return errsymb;		/* Re-use previous error */
  } else if (!errmsg) {
    yyerror("parser error: reducing error with no message");
  }
  if (!errtoken) {
    errtoken = lasttoken;
  }
				/* Call makesymbol for a syntax error
				   symbol with the last token seen
				   before the error and the error
				   message that was reported. */
  errsymb = PyObject_CallFunction (makesymbol, "i[Os]", 
				   SYNTAXERROR, errtoken, errmsg);
  free(errmsg);			/* Clean up and return the syntax error */
  errmsg = NULL;
  if (!errsymb) {
    Py_INCREF (Py_None);
    errsymb = Py_None;
  }
  buffersymbol (errsymb);
  return errsymb;
}

/*
 * Macros for simplifying rules.  Note the snazzy variable-argument
 * preprocessor stuff.
 *
 * APPEND and PREPEND do the same thing as REDUCELEFT and REDUCERIGHT,
 * but are named to be a little clearer when using a token as an
 * internal symbol and adding children to it.
 */
#define REDUCE(type, symbols...) reduce(type, ## symbols, 0)
#define REDUCELEFT(symbol, symbols...) reduceleft(symbol, ## symbols, 0)
#define APPEND(symbol, symbols...) reduceleft(symbol, ## symbols, 0)
#define REDUCERIGHT(symbol, symbols...) reduceright(symbol, ## symbols, 0)
#define PREPEND(symbol, symbols...) reduceright(symbol, ## symbols, 0)
#define REDUCEERROR reduceerror()
#define RETURNTREE(symbol) setparsetree(symbol)

/*
 * Buffer and return the next token from the scanner
 * 
 * The return value is the "type" value of the token object.
 */
static int
yylex (void)
{
  PyObject *pair, *type, *token;
  int typevalue;
				/* readtoken() and pick out the type
                                   and token */
  pair = PyObject_CallFunction(readtoken, NULL);
  if (!pair || pair == Py_None || 
      !(type = PySequence_GetItem(pair, 0)) ||
      !(token = PySequence_GetItem(pair, 1))) {
    Py_XDECREF(pair);		/* XDECREF safe from nulls */
    yylval = 0;
    return 0;
  }
  Py_DECREF(pair);
  buffersymbol(token);		/* Insert into buffer */
  lasttoken = token;		/* Save the last token in case of errors */
  yylval = token;		/* Return the token as a rule's $n */
				/* return token type */
  typevalue = (int) PyInt_AsLong(type);
  Py_DECREF(type);
  return typevalue;
}

/*
 * "parse" function visible from Python
 *
 * First argument is a function to make symbols; second is a function to
 * read tokens from the input stream.
 *
 * Clear the buffer, call Bison's yyparse, flush the buffer, and return
 * the parse tree top node.
 */
static PyObject *
c_parse (PyObject * self, PyObject * args)
{
				/* Initialize scanner */
  Py_XDECREF(makesymbol); makesymbol = NULL;
  Py_XDECREF(readtoken); readtoken = NULL;
  if (!PyArg_ParseTuple(args, "OO", &makesymbol, &readtoken)) { return NULL; }
  Py_INCREF(makesymbol);
  Py_INCREF(readtoken);
  Py_INCREF(Py_None);		/* Initialize returned parsetree */
  parsetree = Py_None;
  clearbuffer();		/* Set up the parsing buffer */
  if (yyparse()) {		/* Call parser */
    if (!PyErr_Occurred ()) {
      PyErr_SetString(ParserError, "syntax error");
    }
  }
  if (errmsg) { free(errmsg); }	/* Free remaining error message */
  flushbuffer();		/* Release unneeded symbols */
  if (PyErr_Occurred()) {
    return NULL;
  }
  return parsetree;		/* Return the top of the parse tree */
}

/*
 * Toggle Bison's debug flag
 */
static PyObject *
c_debug (PyObject * self, PyObject * args)
{
  if (!PyArg_ParseTuple(args, "")) {
    return NULL;
  }
  yydebug = ~yydebug;
  Py_INCREF(Py_None);
  return Py_None;
}

/*
 * Function table for the module
 */
static PyMethodDef module_methods[] = {
  {"parse", c_parse, METH_VARARGS, 
   "parse(makesymbol, readtoken) : parse tokens from an input stream\n"
   " - makesymbol should have the arguments\n"
   "   + a numeric symbol type\n"
   "   + a list of children\n"
   " - readtoken returns the next token's type and a Python object\n"
   "   which should have append (for REDUCELEFT) and insert (for\n"
   "   REDUCERIGHT) methods"},
  {"debug", c_debug, METH_VARARGS, 
   "debug() : toggle trace from parser to stderr"},
  {NULL, NULL, 0, 0}
};

/* 
 * Structure used to create dictionaries mapping names and symbol values.
 */
typedef struct
{
  char *name;
  long value;
}
SymbolValues;

/*
 * Create dictionaries mapping symbol types and names.
 */
static void
makesymboldicts (SymbolValues * symbs, PyObject * moddict)
{
  int j;
				/* Map symbol names to their values.  */
  PyObject *types = PyDict_New();
				/* Map integer values to their names.  */
  PyObject *names = PyDict_New();
  PyObject *ptype;
  PyObject *pname;
  for (j = 0; symbs[j].name; j++) {
    ptype = PyInt_FromLong(symbs[j].value);
    pname = PyString_FromString(symbs[j].name);
    PyDict_SetItem(types, pname, ptype);
    PyDict_SetItem(names, ptype, pname);
    Py_DECREF(ptype);
    Py_DECREF(pname);
  }
				/* Create error symbol information */
  ptype = PyInt_FromLong(SYNTAXERROR);
  pname = PyString_FromString("SYNTAXERROR");
  PyDict_SetItem(types, pname, ptype);
  PyDict_SetItem(names, ptype, pname);
  Py_DECREF(ptype);
  Py_DECREF(pname);
				/* Insert into module dictionary */
  PyMapping_SetItemString(moddict, "types", types);
  PyMapping_SetItemString(moddict, "names", names);
  Py_DECREF(types);
  Py_DECREF(names);
}

/*
 * Set up the syntax error exception
 */
static void
makesyntaxerror (char * modname, PyObject * moddict)
{
  int mlen = strlen(modname);
  char *buf = malloc(mlen + 13);
  strcpy(buf, modname);
  strcpy(buf + mlen, ".ParserError");
  ParserError = PyErr_NewException(buf, NULL, NULL);
  PyDict_SetItemString(moddict, "ParserError", ParserError);
  free(buf);
}

/*
 * Initialize the module based on the module name and the symbol
 * mapping.
 */
#define BISONMODULEINIT(name, symbols)				    \
void								    \
init ## name (void) {						    \
  PyObject *pmod = Py_InitModule4(#name, module_methods,	    \
                                  "Bison-generated parser module "  \
                                  #name, NULL, PYTHON_API_VERSION); \
  PyObject *moddict = PyModule_GetDict(pmod);		            \
  makesymboldicts(module_symbols, moddict);			    \
  makesyntaxerror(#name, moddict);				    \
  if (PyErr_Occurred()) {				      	    \
    Py_FatalError("Error initializing parser module #name");	    \
  }								    \
}
