import os,sys,inspect
sys.path.insert(0, os.path.join(os.path.realpath(os.path.curdir), 'lib'))

import libpyhal_s2 as hal

print 5*'=', "List of libpyhal_c's components", '='*5

indentStep = 8
widthLeft = 40

def prettify(i):
    prettyI = i
    prettyI = prettyI.replace("_scope_",    "::")
    prettyI = prettyI.replace("_less_",     "<")
    prettyI = prettyI.replace("_greater_",  ">")
    prettyI = prettyI.replace("_comma_",    ",")
    prettyI = prettyI.replace("_ptr_",      "*")
    prettyI = prettyI.replace("_unsigned",  "unsigned ")
    prettyI = prettyI.replace("_long_",     "long")
    prettyI = prettyI.replace("_int_",      "int")
    prettyI = prettyI.replace("_short_",    "short")
    prettyI = prettyI.replace("_char_",    "char")
    #prettyI = prettyI.replace("_",          " ")
    return prettyI

def showClass(dings, indent = 0):
#    print ' '*indent, prettify(dings)
    allIndent = indent+widthLeft
    for i in eval( "dir(%s)" % dings ):
        moduleName = "%s.%s" % (dings,i)
        if i[0:2] == '__' and i[-2:] == '__':
            prettyI = i
        else:
            prettyI = prettify(i)
        print ' '*indent, eval( "\"%%+%ds\" % allIndent" ) % prettyI, " "*32,  eval( "type( %s )" % moduleName )

        if i[0:2] == '__' and i[-2:] == '__':
            continue

        if eval( "inspect.isclass(%s)" % moduleName ):
            showClass( moduleName, indent + indentStep )

showClass("hal")
