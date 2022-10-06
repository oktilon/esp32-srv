#!/usr/bin/python3

f = open("main/index.html", "r")
t = f.read()
f.close()
e = t.replace("\n", "\\n")
e = e.replace("\"", "\\\"")
e = e.replace("%;", "%%;")

f = open("main/index.h", "w")
t = "/** \n * index.html\n */\n#ifndef _INDEX_H\n#define _INDEX_H 1\n\n#define INDEX_HTML = \"" + e + "\"\n\n#endif // _INDEX_H\n"
f.write(t)
f.close()

print(t)