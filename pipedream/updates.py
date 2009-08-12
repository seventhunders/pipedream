#!/usr/bin/env python

def needs_update():
    from sys import path
    from os import chdir
    from commands import getstatusoutput
    chdir(path[0])
    (status,local) = getstatusoutput("git ls-remote . heads/public | cut -f 1")
    (status,remote) = getstatusoutput("git ls-remote . heads/public | cut -f 1")
    if local != remote:
        print local, remote
    return local!=remote
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
    