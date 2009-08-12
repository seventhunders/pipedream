#!/usr/bin/env python

def do_update():
    from commands import getstatusoutput
    (status,output) = getstatusoutput("whoami")
    if output != "root":
        print "Not root.  Giving up..."
        return
    from os import chdir
    from sys import path
    chdir(path[0])
    (status,output) = getstatusoutput("git pull --rebase")
    if status != 0:
        raise Exception(output)
    (status,output) = getstatusoutput("superinstall.py")
    