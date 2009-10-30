#!/usr/bin/env python
from subprocess import Popen, PIPE
import sys
if sys.platform == "win32":
   process = Popen(["c:\python26\python.exe",sys.path[0]+"\pipe.py","acceptotk","--service="+sys.argv[1]],stdout=PIPE,stderr=PIPE)
else:
    process = Popen([sys.path[0]+"/pipe.py","acceptotk","--service="+sys.argv[1]],stdout=PIPE,stderr=PIPE)
#process.wait()
prev_line = ""
while True:
    line = process.stdout.readline().strip()
 #   print "WTF LINE %s" % line
    if line=="":
        print prev_line
        break
    prev_line = line