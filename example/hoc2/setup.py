from distutils.core import setup, Extension
from subprocess import call

# I don't know enough about distutils to be able to automatically call flex and bison. Sorry.

# Call flex to generate lexer.c
# flex -ohoclexer.c hoclexer.l
call(['flex', '-o', 'hoclexer.c', 'hoclexer.l'])

# Call bison to generate hocgrammar.c and related files
# bison -v -t -d -o hocgrammar.c hocgrammar.y
call(['bison', '-v', '-t', '-d', '-o', 'hocgrammar.c', 'hocgrammar.y'])

hoclexer = Extension('hoclexer',
                     sources = ['hoclexer.c'],
                     include_dirs = ['../..'],
                     depends = ['hoclexer.l', 'hocgrammar.h'])

hocgrammar = Extension('hocgrammar',
                       sources = ['hocgrammar.c'],
                       include_dirs = ['../..'],
                       depends = ['hocgrammar.y', 'hocgrammar.h'])

setup (name = 'hoc',
       version = '1.0',
       description = 'calculator based on The Unix Programming Environment',
       author = 'Tommy M. McGuire',
       ext_modules = [hoclexer, hocgrammar])

