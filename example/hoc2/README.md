# hoc2

Example of using FlexModule and BisonModule to create Python modules using flex and bison.

This example is loosely based on the second version of hoc in *The UNIX Programming Environment* by Brian W. Kernighan and Rob Pike.  It implements something like a four function command line calculator with one character variables.

The interesting bits are:

* hocgrammar.y: The Bison grammar
* hoclexer.l:   The Flex lexical analyzer
* hoc:          The Python code to implement the calculator
* setup.py:     Distutils configuration to build modules

Other files are:

* hocinput, hocinputb, hocinputc: test input files
* memtest-bison, memtest-flex: scripts to run many scans or parses, watching for memory leaks

To build it, run

    python setup.py build

To run it, build (I mean do)

    python hoc hocinput

You will probably need to modify hoc to match the directory where distutils places the modules. Look for sys.path.

For questions or comments, send mail to mcguire@crsr.net.

Tommy M. McGuire
