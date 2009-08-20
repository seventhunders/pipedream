#!/usr/bin/env python
from subprocess import Popen, PIPE
import sys
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