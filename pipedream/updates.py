#!/usr/bin/env python

#test
def what_branch():
    from commands import getstatusoutput
    from os import chdir
    from sys import path
    chdir(path[0])
    (status,output) = getstatusoutput("git branch | grep '*' | cut -f 2 -d ' '")
    return output
def whats_new():
    from sys import path
    from os import chdir
    from commands import getstatusoutput
    (status,lines) = getstatusoutput("git log HEAD..origin/" + what_branch() + " --format='%s'")
    return lines.split("\n")
def get_local_version():
    from sys import path
    from os import chdir
    from commands import getstatusoutput
    chdir(path[0])
    (status,local) = getstatusoutput("git ls-remote . HEAD | cut -f 1").split("\n")[0]
    return local
def needs_update():
    from sys import path
    from os import chdir
    from commands import getstatusoutput
    chdir(path[0])
    (status,whatever) = getstatusoutput("git fetch origin")
    local = get_local_version()
    (status,remote) = getstatusoutput("git ls-remote . origin/" + what_branch()+" | cut -f 1")
    #if local == remote:
        #print local, remote
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
    from os import system
    status = system("./superinstall.py")
    if status != 0:
        raise Exception("That didn't work")
#