#  Symbols -- Basic class definitions for Bison/FlexModule symbols
#
#        Copyright (c) 2002 by Tommy M. McGuire
#
#        Permission is hereby granted, free of charge, to any person
#        obtaining a copy of this software and associated documentation
#        files (the "Software"), to deal in the Software without
#        restriction, including without limitation the rights to use,
#        copy, modify, merge, publish, distribute, sublicense, and/or
#        sell copies of the Software, and to permit persons to whom
#        the Software is furnished to do so, subject to the following
#        conditions:
#
#        The above copyright notice and this permission notice shall be
#        included in all copies or substantial portions of the Software.
#
#        THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
#        KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
#        WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
#        AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
#        HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
#        WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#        FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
#        OTHER DEALINGS IN THE SOFTWARE.
#
#	Please report any problems to mcguire@cs.utexas.edu.
#
#	This is version 2.0.

def position(pos):
    "Return a string describing a token location."
    # pos = ((line,col),(line,col),file,[(file,line,col)])
    begin, end, file, stack = pos
    if file == "-":
	file = ""			# string
    else:
	file = "file '%s', " % (file)	# file name
    this = "%sline %d, col %d - line %d col %d" % \
           (file, begin[0], begin[1], end[0], end[1])
    if stack:
        for upfile, upline, upcol in stack:
            this = this + "\n    from file '%s', line %d" % (upfile, upline)
    return this

class Symbol:
    "Base class for Bison/Flex symbols and tokens. Subclass for "
    "specific non-terminals."
    names = {}                   # Replace with names from BisonModule
    def __init__(self, type, children, *rest):
        self.type = type
        self.children = children
    def append(self, object):
        self.children.append(object)
    def insert(self, idx, object):
	self.children.insert(idx, object)
    def __str__(self):
        import string
        try:
            name = self.names[self.type]
        except KeyError:
            name = "unnamed"
        kids = ""
        if self.children:
            kids = " (%s)" % (string.join(map(str, self.children), " "))
        return "(%s %s (%d)%s)" % \
	       (self.__class__.__name__, name, self.type, kids)

class Token(Symbol):
    "Base class for Flex tokens, based on the class Symbol above. "
    "Subclass for specific terminals."
    names = {}                   # Replace with names from FlexModule
    def __init__(self, type, string, position):
        Symbol.__init__(self, type, [])
        self.string = string
	self.position = position
    def __str__(self):
        import string
        try:
            name = self.names[self.type]
        except KeyError:
            name = "'%s'" % (self.string)
        kids = ""
        if self.children:
            kids = " (%s)" % (string.join(map(str, self.children), " "))
        return "(%s %s (%d) [%s] %s%s)" % \
            (self.__class__.__name__, name, self.type, self.location(), \
            self.string, kids)
    def location(self):
	return "%s" % (position(self.position))
