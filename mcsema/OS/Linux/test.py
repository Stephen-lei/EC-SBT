#!/usr/bin/env python
import sys
import clang.cindex
function_calls = []           
function_declarations = []     
def traverse(node):

    for child in node.get_children():
        traverse(child)

    if node.kind == clang.cindex.CursorKind.CALL_EXPR:
        function_calls.append(node)

    if node.kind == clang.cindex.CursorKind.FUNCTION_DECL:
        function_declarations.append(node)

    str_t=" Found %s [line=%s, col=%s]" % (node.displayname, node.location.line, node.location.column)
    #print(str_t)

clang.cindex.Config.set_library_file("/usr/lib/llvm-11/lib/libclang-11.so.1")
index = clang.cindex.Index.create()

tu = index.parse(sys.argv[1])

root = tu.cursor        
traverse(root)
print(function_declarations)