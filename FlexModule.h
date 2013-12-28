/*
        FlexModule.h -- Create Python modules from flex scanners

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

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "Python.h"

static int yylex(void);

/*
 * Utility function: Allocate memory, setting exception if needed.
 */
static void *
pxmalloc(int size)
{
  void *p;
  p = malloc(size);
  if (!p) { PyErr_NoMemory(); }
  return p;
}

/*
 * Track positions and handle flex buffers in the scanned text.
 */
typedef struct position_struct {
  struct position_struct *next;	/* Next stacked position */
  char *filename;		/* File name of position; "-" for strings */
  int cur_line;			/* Current line number in file */
  int cur_col;			/* Current character number within line */
  int pre_line;			/* Previous line number */
  int pre_col;			/* Previous column number */
  YY_BUFFER_STATE buf;		/* Flex buffer state */
  /* File */
  FILE *file;			/* File to be scanned */
  PyObject *file_object;	/* Saved file object reference */
  /* String */
  char *string;			/* String to be scanned */
} position;

/*
 * Set up a position.  Note: Don't call this function; use one of the
 * specific versions below.
 */
static position *
set_pos_base(char *fn)
{
  position *p = (position *) pxmalloc(sizeof(position));
  if (!p) { return NULL; }
  if (fn) {			/* Duplicate the file name */
    int len = strlen(fn);
    p->filename = (char *) pxmalloc(len + 1);
    if (!p->filename) {
      free(p);
      return NULL;
    }
    memcpy(p->filename, fn, len);
    p->filename[len] = 0;    
  }
  p->pre_line = p->cur_line = 1; /* Initialize the position */
  p->pre_col = p->cur_col = 1;
  p->file = NULL;		/* These will be dealt with below */
  p->file_object = NULL;
  p->string = NULL;
  p->next = NULL;
  return p;
}

/*
 * Grab a string and make it a flex buffer.
 */
static position *
set_pos_string(char *s, int s_len)
{
  position *p = set_pos_base("-");
  if (!p) { return NULL; }
				/* Duplicate the string as a flex buffer */
  p->string = (char *) pxmalloc(s_len + 2);
  if (!p->string) {
    free(p->filename);
    free(p);
    return 0;
  }
  memcpy(p->string, s, s_len);
				/* The last two characters are special */
  p->string[s_len] = p->string[s_len + 1] = YY_END_OF_BUFFER_CHAR;
				/* Point flex at the buffer; this
                                   creates the buffer and switches to it */
  p->buf = yy_scan_buffer(p->string, s_len + 2);
  return p;
}

/* 
 * Grab a file and point flex at it.  This function is for files which
 * are opened outside the Flex module.  It is also called by the function
 * below for owned files.
 */
static position *
set_pos_file_unowned(char * fn, FILE *f, PyObject *fileobj)
{
  position *p = set_pos_base(fn);
  if (!p) { return NULL; }
  p->file = f;			/* Remember the file */
				/* The fileobj may be NULL (see
                                   below), this doesn't care. */
  Py_XINCREF(fileobj);		/* Don't let anyone else close the file */
  p->file_object = fileobj;	/* while it's in use here. */
  p->buf = yy_create_buffer(f, YY_BUF_SIZE);
  yy_switch_to_buffer(p->buf);	/* Tell flex to use it */
  return p;
}

/*
 * Open a file and point flex at it.  This function opens the file for
 * input and marks the position record as owned.
 */
static position *
set_pos_file_owned(char *fn)
{
  position *p;
  FILE *f = fopen(fn, "r");	/* Open the file; may fail */
  if (!f) {
    return (position *) PyErr_SetFromErrno(PyExc_IOError);
  }
				/* Use previous function to set up record */
  p = set_pos_file_unowned(fn, f, NULL);
  return p;
}

/*
 * Advance the position based on the text scanned.  This will be
 * called from user actions immediately after a soon-to-be-token has
 * been seen.
 */
static void
advance_pos(position *p, char *text, int len)
{
  int i;
  p->pre_line = p->cur_line;	/* Record the previous location */
  p->pre_col = p->cur_col;
  for (i = 0; i < len; i++) {	/* Advance the current location */
    if (text[i] == '\n') {
      p->cur_line++;
      p->cur_col = 1;
    } else {
      p->cur_col++;
    }
  }
}

/*
 * Macros for use in flex specification rules (for whitespace, etc.;
 * returned tokens already advance the position).  ADVANCE2 needs the
 * text and the length of the string.  ADVANCE is probably the easiest;
 * it uses yytext and yyleng.
 */
#define ADVANCE2(t,l) advance_pos(scanner.pstack,(t),(l))
#define ADVANCE       ADVANCE2(yytext, yyleng)

/*
 * Clean up a position.
 */
static void
close_pos(position *p)
{
  if (p->filename) {
    free(p->filename);
  }
  p->filename = 0;
  if (p->file && !p->file_object) {
    fclose(p->file);
  }
  p->file = NULL;
  if (p->file_object) {
    Py_DECREF(p->file_object);
  }
  p->file_object = NULL;
  if (p->string) {
    free(p->string);
  }
  p->string = NULL;
  if (p->buf) {
    yy_delete_buffer(p->buf);
  }
  p->buf = 0;
}

/*
 * The information needed by the scanner.
 */
struct scanner_struct {
  PyObject *maketoken;		/* Python function to make tokens */
  position *pstack;		/* Current positions */
  int lasttoken;		/* Return value of last call to yylex */
};
static struct scanner_struct scanner = { NULL, NULL, 0, };

/*
 * Check if we are currently scanning something.
 */
static int
scanning(void)
{
  return (scanner.maketoken && scanner.pstack);
}

/*
 * Push a file onto the position stack.
 */
static int
push_position(char *fn)
{
				/* Create a position and open the file */
  position *p = set_pos_file_owned(fn);
  if (!p) {
    return 0;
  }
  p->next = scanner.pstack;	/* Put it at the top of the stack */
  scanner.pstack = p;
  return 1;
}

/*
 * Push a position based on a char pointer and a length.
 */
static int
push_position2(char *begin, int len)
{
  int res = 0;
  char *buf = (char *) pxmalloc(len + 1);
  if (buf) {
    memcpy(buf, begin, len);
    buf[len] = 0;
    res = push_position(buf);
    free(buf);
  }
  return res;
}

/*
 * Macros for use in Flex rules.
 *
 * PUSH_FILE_YYTEXT takes two arguments: the index into yytext of the
 * beginning of the file name, and the index of the next position
 * after the end of the file name.  Just like a slice.
 */
#define PUSH_FILE_YYTEXT(b,e)  (ADVANCE, push_position2(yytext+(b), (e)-(b)))
#define PUSH_FILE_STRING(s)    push_position((s))
#define PUSH_FILE_STRING2(s,l) push_position2((s), (l))

/*
 * Pop the top of the position stack.
 *
 * yywrap is called when there is an end-of-file; this should transfer
 * back to previously pushed positions if there are any.  Otherwise,
 * it returns 1 to tell flex that we have reached the end of the
 * input.
 */
static int
yywrap(void)
{
  position *p = scanner.pstack;	/* Close and free the top of the stack */
  scanner.pstack = p->next;
  close_pos(p);
  free(p);
  if (!scanner.pstack) {	/* If that was the last position, quit */
     return 1;
  }
				/* Switch back to the previous buffer */
  yy_switch_to_buffer(scanner.pstack->buf);
  return 0;
}

/*
 * Python function to begin scanning a string.
 * Parameters are:
 * - Python function to create tokens; see the doc string.
 * - A string to scan.
 * This function uses goto's to handle clean-up after errors.
 */
static PyObject *
c_onstring(PyObject * self, PyObject * args)
{
  char *s;
  int s_len;
  Py_XDECREF(scanner.maketoken);
  if (!PyArg_ParseTuple(args, "Os#", &scanner.maketoken, &s, &s_len)) {
    return NULL;
  }
  if (scanning()) {
    PyErr_SetString(PyExc_ValueError, "Already scanning");
    return NULL;
  }
  scanner.pstack = set_pos_string(s, s_len);
  if (!scanner.pstack) {
    return NULL;
  }
  Py_INCREF(scanner.maketoken);	/* Grab maketoken */
  Py_INCREF(Py_None);		/* Return normally */
  return Py_None;
}

/*
 * Python function to begin scanning a file.
 * Parameters are:
 * - Python function to create tokens; see the doc string.
 * - The file name.
 * This function uses goto's to handle clean-up after errors.
 */
static PyObject *
c_onfile(PyObject * self, PyObject * args)
{
  PyObject *fileobj;
  if (!PyArg_ParseTuple(args, "OO", &scanner.maketoken, &fileobj)) {
    return NULL;
  }
  if (scanning()) {
    PyErr_SetString(PyExc_ValueError, "Already scanning");
    return NULL;
  }
  if (PyString_Check(fileobj)) {
				/* It's a file name, ours to close */
    scanner.pstack = set_pos_file_owned(PyString_AsString(fileobj));
  } else if (PyFile_Check(fileobj)) {
				/* It's a Python file object; let the
				   caller close the damn thing.  We
				   do need to keep a reference, in case
				   it disappears while we aren't looking. */
    scanner.pstack =
      set_pos_file_unowned(PyString_AsString(PyFile_Name(fileobj)),
			   PyFile_AsFile(fileobj), fileobj);
  } else {
    PyErr_SetString(PyExc_ValueError, "Need filename or file object");
    return NULL;
  }
  if (!scanner.pstack) {	/* If set_pos_file failed, head for hills */
    return NULL;
  }
  Py_INCREF(scanner.maketoken);	/* Grab maketoken */
  Py_INCREF(Py_None);		/* Return normally */
  return Py_None;
}

/*
 * Python function to shut down scanner.
 * Has no parameters.
 */
static PyObject *
c_close(PyObject * self, PyObject * args)
{
  if (!PyArg_ParseTuple(args, "")) { return NULL; }
  if (scanner.maketoken) {	/* Shut down maketoken */
    Py_DECREF(scanner.maketoken);
    scanner.maketoken = NULL;
  }
  while (scanner.pstack) {	/* Clean out the position stack */
    position *next = scanner.pstack->next;
    close_pos(scanner.pstack);
    free(scanner.pstack);
    scanner.pstack = next;
  }
  scanner.lasttoken = 0;	/* Clear the last token value */
  Py_INCREF(Py_None);
  return Py_None;
}

/*
 * Call maketoken.
 */
static PyObject *
maketoken(void)
{
  position *p;
  PyObject *token, *ptuple, *list = PyList_New(0);
  if (!list) {
    return NULL;
  }
				/* Set up the list of open positions,
                                   starting from the second-to-last */
  for (p = scanner.pstack->next; p; p = p->next) {
    ptuple = Py_BuildValue("(s,i,i)", p->filename, p->cur_line, p->cur_col);
    if (!ptuple || (PyList_Append(list, ptuple) < 0)) {
      Py_DECREF(list);
      return NULL;
    }
    Py_DECREF(ptuple);
  }
				/* Set up the current position,
                                   including line position, file name,
                                   and the list from above */
  ptuple = Py_BuildValue("((i,i),(i,i),s,O)",
			 scanner.pstack->pre_line, scanner.pstack->pre_col,
			 scanner.pstack->cur_line, scanner.pstack->cur_col-1,
			 scanner.pstack->filename, list);
  Py_DECREF(list);
				/* Finally, call maketoken */
  token = PyObject_CallFunction(scanner.maketoken, "(i,s#,O)",
				scanner.lasttoken, yytext, yyleng, ptuple);
  Py_DECREF(ptuple);
  return token;
}

/*
 * Python function to return the next token scanned and its type.
 * Has no parameters.
 */
static PyObject *
c_readtoken(PyObject * self, PyObject * args)
{
  PyObject *token, *value;
  if (!PyArg_ParseTuple(args, "")) { return NULL; }
  if (!scanning()) {
    PyErr_SetString(PyExc_ValueError, "Not scanning anything");
    return NULL;
  }
  scanner.lasttoken = yylex();	/* Call flex for the next token */
  if (!scanner.lasttoken) {	/* We're out of tokens; return None */
    Py_INCREF(Py_None);
    return Py_None;
  }
  ADVANCE;			/* Automatically advance position */
  token = maketoken();		/* Call maketoken and build return pair */
  value = Py_BuildValue("(i,O)", scanner.lasttoken, token);
  Py_XDECREF(token);		/* maketoken may have failed */
  return value;
}

/*
 * Python function to return the most recent token scanned.
 * Has no parameters.
 */
static PyObject *
c_lasttoken(PyObject *self, PyObject *args)
{
  if (!PyArg_ParseTuple(args, "")) { return NULL; }
  if (!scanning() || !scanner.lasttoken) {
    PyErr_SetString(PyExc_ValueError, "No token available");
    return NULL;
  }
  return maketoken();		/* Call maketoken on the last info we had */
}

#define MAKETOKENDOC                                       \
"maketoken should be a function with three parameters: \n" \
"- the type of the token, an integer\n"                    \
"- the text of the token, a string\n"                      \
"- the postion of the token\n"                             \
"a position is a tuple of\n"                               \
"- a pair with the beginning line and column\n"            \
"- a pair with the ending line and column\n"               \
"- the filename\n"                                         \
"- a list of tuples, giving the file name, line, and\n"    \
"  column of stacked, yet-to-be finished positions."

/*
 * Function table for scanner module.
 */
static PyMethodDef module_methods[] = {
  {"onstring", c_onstring, METH_VARARGS,
   "onstring(maketoken, string) : begin scanning string\n" MAKETOKENDOC},
  {"onfile", c_onfile, METH_VARARGS,
   "onfile(maketoken, file) : begin scanning a file (name or object)\n"
   MAKETOKENDOC},
  {"readtoken", c_readtoken, METH_VARARGS,
   "readtoken() : read the next token, returning a pair of the token value\n"
   "              and the token returned by maketoken."},
  {"lasttoken", c_lasttoken, METH_VARARGS,
   "lasttoken() : re-read the most-recent token"},
  {"close", c_close, METH_VARARGS,
   "close() : free resources and stop scanning"},
  {NULL, NULL, 0, 0}
};

#undef MAKETOKENDOC

/*
 * Type definition for table mapping token names to integer values.
 */
typedef struct {
  char *name;			/* string name of token */
  long value;			/* integer value of token */
} TokenValues;

/*
 * Create a Python dictionary mapping token names to values and another
 * mapping values to names and insert these dictionaries into the module.
 */
static void
maketokens(TokenValues * toks, PyObject * module)
{
  PyObject *moddict = PyModule_GetDict(module);
  PyObject *types = PyDict_New();
  PyObject *names = PyDict_New();
  PyObject *ptype;
  PyObject *pname;
  int j;
				/* Put the module's TokenValues into dicts */
  for (j = 0; toks[j].name; j++) {
    ptype = PyInt_FromLong(toks[j].value);
    pname = PyString_FromString(toks[j].name);
    PyDict_SetItem(types, pname, ptype);
    PyDict_SetItem(names, ptype, pname);
    Py_DECREF(ptype);
    Py_DECREF(pname);
  }
				/* Insert the dicts into the module */
  PyMapping_SetItemString(moddict, "types", types);
  PyMapping_SetItemString(moddict, "names", names);
  Py_DECREF(types);
  Py_DECREF(names);
}

/*
 * Macro called with module name and TokenValues mapping; handles
 * Python InitModule chores.  Note the fancy preprocessor
 * stringification stuff.
 */
#define FLEXMODULEINIT(name, tokens)					\
void									\
init ## name (void) {							\
  PyObject *pmod = Py_InitModule4(#name, module_methods,		\
    "Flex-generated scanner module " #name, NULL, PYTHON_API_VERSION);	\
  maketokens(tokens, pmod);						\
  if (PyErr_Occurred()) {						\
    Py_FatalError("Error initializing scanner module " #name);		\
  }									\
}
